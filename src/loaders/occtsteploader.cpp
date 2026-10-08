#include "loaders/occtsteploader.h"
#include "core/importlimits.h"

#include <QDebug>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <QStandardPaths>
#include <memory>

#ifdef FSTL_USE_OCCT_STEP
// Open CASCADE headers
#include <STEPControl_Reader.hxx>
#include <IFSelect_ReturnStatus.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <TopLoc_Location.hxx>
#include <BRep_Tool.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <IMeshTools_Parameters.hxx>
#include <Message_ProgressIndicator.hxx>
#include <Message_ProgressRange.hxx>
#include <Message_ProgressScope.hxx>
#include <Poly_Triangulation.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#include <Bnd_Box.hxx>
#include <BRepBndLib.hxx>
#include <algorithm>
#include <cmath>

namespace {
// Lets OCCT's transfer and meshing stages stop when the viewer is closing.
class CancelProgressIndicator : public Message_ProgressIndicator
{
public:
    explicit CancelProgressIndicator(const std::function<bool()>& cancel) : m_cancel(cancel) {}
    Standard_Boolean UserBreak() override { return m_cancel && m_cancel(); }
    void Show(const Message_ProgressScope&, const Standard_Boolean) override {}
    DEFINE_STANDARD_RTTI_INLINE(CancelProgressIndicator, Message_ProgressIndicator)
private:
    std::function<bool()> m_cancel;
};
}
#endif

OcctStepLoader::OcctStepLoader() = default;

bool OcctStepLoader::load(const QString& filename, QVector<QVector3D>& outVerts, unsigned int& outTriCount,
                          const std::function<bool()>& cancel)
{
    outVerts.clear();
    outTriCount = 0;
    const auto cancelled = [&cancel] { return cancel && cancel(); };

#ifdef FSTL_USE_OCCT_STEP
    // Full OCCT-based STEP import. This is compiled only when
    // FSTL_USE_OCCT_STEP is defined and Open CASCADE is linked in.

    // On Android, OCCT cannot read content:// URIs directly. If the filename
    // is a content URI or Qt resource, copy the data into a temporary file and
    // pass that real filesystem path to OCCT.
    QString occtFilename = filename;
    // Keep ownership until OCCT finishes; all return paths remove the copy.
    std::unique_ptr<QTemporaryFile> temporarySource;

#ifdef Q_OS_ANDROID
    if (filename.startsWith("content://") || filename.startsWith(":/")) {
        // Android's process temporary directory may be unwritable. Use the
        // application's private cache, which Qt resolves through getCacheDir().
        const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
        if (cachePath.isEmpty() || !QDir().mkpath(cachePath)) {
            qWarning() << "OCCT STEP: Application cache is unavailable";
            return false;
        }
        temporarySource = std::make_unique<QTemporaryFile>(cachePath + "/fstl_step_XXXXXX.stp");
        QTemporaryFile& tmp = *temporarySource;
        if (!tmp.open()) {
            qWarning() << "OCCT STEP: Failed to create temporary STEP file" << tmp.errorString();
            return false;
        }

        QFile inFile(filename);
        if (!inFile.open(QIODevice::ReadOnly)) {
            qWarning() << "OCCT STEP: Failed to open source STEP file" << filename << inFile.errorString();
            return false;
        }

        while (!inFile.atEnd()) {
            if (cancelled()) return false;
            const QByteArray chunk = inFile.read(1024 * 1024);
            if (chunk.isEmpty() || tmp.size() + chunk.size() > ImportLimits::SourceBytes ||
                tmp.write(chunk) != chunk.size()) {
                qWarning() << "OCCT STEP: Failed to copy source to temporary file";
                return false;
            }
        }
        if (inFile.error() != QFileDevice::NoError || tmp.size() == 0 || !tmp.flush()) {
            qWarning() << "OCCT STEP: Source read or temporary file write failed" << inFile.error()
                       << inFile.errorString() << "copied" << tmp.size() << "source size" << inFile.size();
            return false;
        }
        tmp.close();
        inFile.close();

        occtFilename = tmp.fileName();
        qDebug() << "OCCT STEP: Copied" << filename << "to temp" << occtFilename;
    }
#endif

    if (cancelled()) return false;
    Handle(CancelProgressIndicator) progress = new CancelProgressIndicator(cancel);

    TopoDS_Shape shape;
    {
        // Scoped so the reader's STEP entity model is freed before meshing.
        STEPControl_Reader reader;
        IFSelect_ReturnStatus status = reader.ReadFile(occtFilename.toStdString().c_str());
        if (status != IFSelect_RetDone)
        {
            qWarning() << "OCCT STEP: ReadFile failed with status" << static_cast<int>(status);
            return false;
        }
        if (cancelled()) return false;

        // Transfer all roots to build a unified shape
        if (reader.TransferRoots(progress->Start()) <= 0 || cancelled())
        {
            qWarning() << "OCCT STEP: TransferRoots produced no shapes or was cancelled";
            return false;
        }

        shape = reader.OneShape();
    }
    if (shape.IsNull())
    {
        qWarning() << "OCCT STEP: OneShape is null";
        return false;
    }

    // Create triangulation for all faces in the shape. A fixed 0.1 deflection
    // turns large parts into millions of triangles (and exhausts memory before
    // the triangle budget can be checked), so scale it with the model size.
    double linDeflection = 0.1;
    Bnd_Box bounds;
    BRepBndLib::Add(shape, bounds);
    if (!bounds.IsVoid())
        linDeflection = std::max(linDeflection, 0.001 * std::sqrt(bounds.SquareExtent()));
    const bool isRelative = false;
    const double angDeflection = 0.5;   // radians

    try
    {
        IMeshTools_Parameters meshParams;
        meshParams.Deflection = linDeflection;
        meshParams.Relative = isRelative;
        meshParams.Angle = angDeflection;
        meshParams.InParallel = true;
        BRepMesh_IncrementalMesh mesher(shape, meshParams, progress->Start());
    }
    catch (...)
    {
        qWarning() << "OCCT STEP: BRepMesh_IncrementalMesh threw an exception";
        return false;
    }
    if (cancelled()) return false;

    quint64 totalTris = 0;
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
    {
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(TopoDS::Face(exp.Current()), loc);
        if (!tri.IsNull())
            totalTris += quint64(std::max(0, tri->NbTriangles()));
    }
    if (totalTris > ImportLimits::Triangles)
    {
        qWarning() << "OCCT STEP: Triangle budget exceeded";
        return false;
    }
    outVerts.reserve(qsizetype(totalTris) * 3);

    // Iterate over faces and extract triangulations
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
    {
        if (cancelled()) return false;
        const TopoDS_Face& face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull())
            continue;

        gp_Trsf trsf = loc.Transformation();
        const Standard_Integer nbNodes = tri->NbNodes();
        const Standard_Integer nbTris  = tri->NbTriangles();

        if (nbTris < 0 || quint64(outTriCount) + quint64(nbTris) > ImportLimits::Triangles)
        {
            outVerts.clear();
            outTriCount = 0;
            qWarning() << "OCCT STEP: Triangle budget exceeded";
            return false;
        }
        for (Standard_Integer i = 1; i <= nbTris; ++i)
        {
            Poly_Triangle t = tri->Triangle(i);
            Standard_Integer n1 = 0, n2 = 0, n3 = 0;
            t.Get(n1, n2, n3);

            if (n1 < 1 || n1 > nbNodes ||
                n2 < 1 || n2 > nbNodes ||
                n3 < 1 || n3 > nbNodes)
            {
                continue;
            }

            gp_Pnt p1 = tri->Node(n1).Transformed(trsf);
            gp_Pnt p2 = tri->Node(n2).Transformed(trsf);
            gp_Pnt p3 = tri->Node(n3).Transformed(trsf);

            outVerts.append(QVector3D(p1.X(), p1.Y(), p1.Z()));
            outVerts.append(QVector3D(p2.X(), p2.Y(), p2.Z()));
            outVerts.append(QVector3D(p3.X(), p3.Y(), p3.Z()));
            ++outTriCount;
        }
    }

    if (outTriCount == 0)
    {
        qWarning() << "OCCT STEP: No triangles generated from shape";
        return false;
    }

    qDebug() << "OCCT STEP: Generated" << outTriCount << "triangles";
    return true;
#else
    Q_UNUSED(filename);
    Q_UNUSED(outVerts);
    Q_UNUSED(outTriCount);
    // OCCT not enabled in this build.
    return false;
#endif
}
