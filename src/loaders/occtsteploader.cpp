#include "loaders/occtsteploader.h"

#include <QDebug>

OcctStepLoader::OcctStepLoader() = default;

bool OcctStepLoader::load(const QString& filename, QVector<QVector3D>& outVerts, unsigned int& outTriCount)
{
#ifdef FSTL_USE_OCCT_STEP
    // Full implementation is only compiled when Open CASCADE is available.
    // This stub is intentionally minimal and meant as a hook point for a
    // future OCCT integration on desktop or heavy Android builds.

    // Example (non-compiling sketch) of what would live here:
    //
    //   STEPControl_Reader reader;
    //   IFSelect_ReturnStatus status = reader.ReadFile(filename.toStdString().c_str());
    //   if (status != IFSelect_RetDone) { return false; }
    //   reader.TransferRoots();
    //   TopoDS_Shape shape = reader.OneShape();
    //   BRepMesh_IncrementalMesh mesher(shape, 0.1); // deflection
    //   for (TopExp_Explorer exp(shape, TopAbs_FACE); exp.More(); exp.Next()) {
    //       const TopoDS_Face& face = TopoDS::Face(exp.Current());
    //       TopLoc_Location loc;
    //       Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
    //       ... copy triangles into outVerts ...
    //   }

    Q_UNUSED(filename);
    Q_UNUSED(outVerts);
    Q_UNUSED(outTriCount);
    qWarning() << "OcctStepLoader::load called, but OCCT integration code is not yet implemented";
    return false;
#else
    Q_UNUSED(filename);
    Q_UNUSED(outVerts);
    Q_UNUSED(outTriCount);
    // OCCT not enabled in this build.
    return false;
#endif
}
