#include "loaders/occtsteploader.h"

#include <QDebug>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>

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
#include <Poly_Triangulation.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>
#endif

OcctStepLoader::OcctStepLoader() = default;

bool OcctStepLoader::load(const QString& filename, QVector<QVector3D>& outVerts, unsigned int& outTriCount)
{
    outVerts.clear();
    outTriCount = 0;

#ifdef FSTL_USE_OCCT_STEP
    // Full OCCT-based STEP import. This is compiled only when
    // FSTL_USE_OCCT_STEP is defined and Open CASCADE is linked in.

    // On Android, OCCT cannot read content:// URIs directly. If the filename
    // is a content URI or Qt resource, copy the data into a temporary file and
    // pass that real filesystem path to OCCT.
    QString occtFilename = filename;
    bool removeTempFile = false;

#ifdef Q_OS_ANDROID
    if (filename.startsWith("content://") || filename.startsWith(":/")) {
        QTemporaryFile tmp(QDir::tempPath() + "/fstl_step_XXXXXX.stp");
        tmp.setAutoRemove(false); // remove manually after OCCT is done
        if (!tmp.open()) {
            qWarning() << "OCCT STEP: Failed to create temporary STEP file" << tmp.errorString();
            return false;
        }

        QFile inFile(filename);
        if (!inFile.open(QIODevice::ReadOnly)) {
            qWarning() << "OCCT STEP: Failed to open source STEP file" << filename << inFile.errorString();
            return false;
        }

        QByteArray data = inFile.readAll();
        if (data.isEmpty()) {
            qWarning() << "OCCT STEP: Source STEP file appears empty" << filename;
            return false;
        }
        if (tmp.write(data) != data.size()) {
            qWarning() << "OCCT STEP: Failed to write full STEP data to temporary file";
            return false;
        }
        tmp.close();
        inFile.close();

        occtFilename = tmp.fileName();
        removeTempFile = true;
        qDebug() << "OCCT STEP: Copied" << filename << "to temp" << occtFilename;
    }
#endif

    STEPControl_Reader reader;
    IFSelect_ReturnStatus status = reader.ReadFile(occtFilename.toStdString().c_str());
    if (status != IFSelect_RetDone)
    {
        qWarning() << "OCCT STEP: ReadFile failed with status" << static_cast<int>(status);
        return false;
    }

    // Transfer all roots to build a unified shape
    if (reader.TransferRoots() <= 0)
    {
        qWarning() << "OCCT STEP: TransferRoots produced no shapes";
        return false;
    }

    TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull())
    {
        qWarning() << "OCCT STEP: OneShape is null";
        return false;
    }

    // Create triangulation for all faces in the shape.
    // Deflection value is a tradeoff between quality and speed.
    const double linDeflection = 0.1;   // you can tweak this later
    const bool isRelative = false;
    const double angDeflection = 0.5;   // radians

    try
    {
        BRepMesh_IncrementalMesh mesher(shape, linDeflection, isRelative, angDeflection, true /*parallel*/);
    }
    catch (...)
    {
        qWarning() << "OCCT STEP: BRepMesh_IncrementalMesh threw an exception";
        return false;
    }

    // Iterate over faces and extract triangulations
    for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next())
    {
        const TopoDS_Face& face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull())
            continue;

        gp_Trsf trsf = loc.Transformation();
        const Standard_Integer nbNodes = tri->NbNodes();
        const Standard_Integer nbTris  = tri->NbTriangles();

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
#ifdef Q_OS_ANDROID
        if (removeTempFile) {
            QFile::remove(occtFilename);
        }
#endif
        return false;
    }

    qDebug() << "OCCT STEP: Generated" << outTriCount << "triangles";
#ifdef Q_OS_ANDROID
    if (removeTempFile) {
        QFile::remove(occtFilename);
    }
#endif
    return true;
#else
    Q_UNUSED(filename);
    Q_UNUSED(outVerts);
    Q_UNUSED(outTriCount);
    // OCCT not enabled in this build.
    return false;
#endif
}
