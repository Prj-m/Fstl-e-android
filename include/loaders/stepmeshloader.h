#ifndef STEPMESHLOADER_H
#define STEPMESHLOADER_H

#include <QString>
#include <QVector>
#include <QMap>
#include <QVector3D>

struct StepEntity {
    int id;
    QString type;
    QStringList params;
};

class StepMeshLoader
{
public:
    StepMeshLoader();
    
    // Parse a STEP file and return vertices for triangulation
    bool parseFile(const QString& filename);
    
    // Get parsed vertices (simple triangles)
    QVector<QVector3D> getVertices() const { return vertices; }
    
    // Get triangle count
    int getTriangleCount() const { return vertices.size() / 3; }
    
private:
    // Parse STEP file format
    bool parseStepData(const QString& data);
    
    // Extract entity from STEP line
    StepEntity parseEntity(const QString& line);
    
    // Resolve entity reference (e.g., #123)
    StepEntity resolveRef(const QString& ref);
    
    // Extract CARTESIAN_POINT coordinates
    QVector3D extractPoint(const StepEntity& entity);
    
    // Simple tessellation for basic shapes
    void tessellateGeometry();
    
    QMap<int, StepEntity> entities;
    QVector<QVector3D> vertices;
    QString errorString;
};

#endif // STEPMESHLOADER_H
