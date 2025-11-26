#ifndef OCCTSTEPLOADER_H
#define OCCTSTEPLOADER_H

#include <QString>
#include <QVector>
#include <QVector3D>

// Optional STEP loader that can use Open CASCADE when FSTL_USE_OCCT_STEP is defined.
// When the OCCT kernel is not available, this class simply returns false from load().
class OcctStepLoader
{
public:
    OcctStepLoader();

    // Loads a STEP file and appends triangles to outVerts (3 vertices per triangle).
    // Returns true on success, false if the file could not be loaded via OCCT.
    bool load(const QString& filename, QVector<QVector3D>& outVerts, unsigned int& outTriCount);
};

#endif // OCCTSTEPLOADER_H
