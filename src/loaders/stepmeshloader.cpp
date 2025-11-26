#include "loaders/stepmeshloader.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

#ifdef Q_OS_ANDROID
#include <android/log.h>
#define LOG_TAG "FSTL_STEP"
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#define ALOG(...) qDebug(__VA_ARGS__)
#endif

StepMeshLoader::StepMeshLoader()
{
}

bool StepMeshLoader::parseFile(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        errorString = "Cannot open file: " + filename;
        ALOG("STEP: %s", errorString.toStdString().c_str());
        return false;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return parseStepData(content);
}

bool StepMeshLoader::parseStepData(const QString& data)
{
    ALOG("STEP: Starting parse, file size: %lld bytes", (long long)data.size());
    
    // STEP files have sections: HEADER, DATA, END
    // Check for ISO-10303-21 header
    if (!data.contains("ISO-10303-21"))
    {
        ALOG("STEP: Warning - ISO-10303-21 header not found");
    }
    
    int dataStart = data.indexOf("DATA;");
    int dataEnd = data.indexOf("ENDSEC;", dataStart);
    
    ALOG("STEP: DATA section: start=%d, end=%d", dataStart, dataEnd);
    
    if (dataStart == -1 || dataEnd == -1)
    {
        errorString = "Invalid STEP file format - DATA section not found";
        ALOG("STEP: %s", errorString.toStdString().c_str());
        ALOG("STEP: File preview: %s", data.left(500).toStdString().c_str());
        return false;
    }
    
    QString dataSection = data.mid(dataStart + 5, dataEnd - dataStart - 5);
    
    // Parse entities (format: #ID=TYPE(params);)
    QRegularExpression entityRegex("#(\\d+)\\s*=\\s*([^(]+)\\(([^;]+)\\);");
    QRegularExpressionMatchIterator it = entityRegex.globalMatch(dataSection);
    
    int entityCount = 0;
    while (it.hasNext())
    {
        QRegularExpressionMatch match = it.next();
        int id = match.captured(1).toInt();
        QString type = match.captured(2).trimmed();
        QString params = match.captured(3);
        
        StepEntity entity;
        entity.id = id;
        entity.type = type;
        
        // Split parameters (simple split, doesn't handle nested parens perfectly)
        QStringList paramList;
        QString current;
        int depth = 0;
        for (QChar c : params)
        {
            if (c == '(' || c == '[') depth++;
            else if (c == ')' || c == ']') depth--;
            else if (c == ',' && depth == 0)
            {
                paramList.append(current.trimmed());
                current.clear();
                continue;
            }
            current += c;
        }
        if (!current.trimmed().isEmpty())
            paramList.append(current.trimmed());
        
        entity.params = paramList;
        entities[id] = entity;
        entityCount++;

        if (entityCount <= 12)
        {
            QString preview;
            if (!paramList.isEmpty())
                preview = paramList[0].left(80);
            ALOG("STEP: Entity %d type=%s param0=%s", id,
                 type.toStdString().c_str(), preview.toStdString().c_str());
        }
    }
    
    ALOG("STEP: Parsed %d entities", entityCount);
    
    if (entityCount == 0)
    {
        errorString = "No STEP entities found in DATA section";
        ALOG("STEP: %s", errorString.toStdString().c_str());
        return false;  // real parse failure
    }
    
    // Extract geometry
    tessellateGeometry();
    
    ALOG("STEP: Extracted %lld vertices (%lld triangles)", 
         (long long)vertices.size(), (long long)vertices.size() / 3);
    
    if (vertices.isEmpty())
    {
        // Parsing was syntactically OK but we couldn't tessellate geometry
        errorString = "STEP file parsed but no tessellatable geometry was found";
        ALOG("STEP: %s", errorString.toStdString().c_str());
        // Return true so caller can report an "empty mesh" instead of "invalid file"
        return true;
    }
    
    return true;
}

StepEntity StepMeshLoader::parseEntity(const QString& line)
{
    StepEntity entity;
    // This is handled by the regex in parseStepData
    return entity;
}

StepEntity StepMeshLoader::resolveRef(const QString& ref)
{
    if (ref.startsWith("#"))
    {
        int id = ref.mid(1).toInt();
        if (entities.contains(id))
            return entities[id];
    }
    return StepEntity();
}

QVector3D StepMeshLoader::extractPoint(const StepEntity& entity)
{
    if (entity.type == "CARTESIAN_POINT" && entity.params.size() >= 2)
    {
        // Format: CARTESIAN_POINT('', (x, y, z))
        QString coords = entity.params[1];
        coords = coords.remove('(').remove(')');
        QStringList parts = coords.split(',');
        
        if (parts.size() >= 3)
        {
            float x = parts[0].trimmed().toFloat();
            float y = parts[1].trimmed().toFloat();
            float z = parts[2].trimmed().toFloat();
            return QVector3D(x, y, z);
        }
    }
    return QVector3D(0, 0, 0);
}

void StepMeshLoader::tessellateGeometry()
{
    // Extract all CARTESIAN_POINTs first
    QMap<int, QVector3D> points;
    QMap<int, int> vertexToPoint;  // Map VERTEX_POINT to CARTESIAN_POINT
    
    for (auto it = entities.begin(); it != entities.end(); ++it)
    {
        const StepEntity& entity = it.value();
        
        if (entity.type == "CARTESIAN_POINT")
        {
            QVector3D point = extractPoint(entity);
            points[entity.id] = point;
        }
        else if (entity.type == "VERTEX_POINT" && entity.params.size() > 1)
        {
            // VERTEX_POINT references a CARTESIAN_POINT
            QString pointRef = entity.params[1].trimmed();
            if (pointRef.startsWith("#"))
            {
                int pointId = pointRef.mid(1).toInt();
                vertexToPoint[entity.id] = pointId;
            }
        }
    }
    
    ALOG("STEP: Found %lld CARTESIAN_POINTs, %lld VERTEX_POINTs", (long long)points.size(), (long long)vertexToPoint.size());
    
    // Helper function to resolve a reference to a point
    auto resolveToPoint = [&](const QString& ref) -> QVector3D {
        if (!ref.startsWith("#")) return QVector3D();
        int id = ref.mid(1).toInt();
        
        // Direct point?
        if (points.contains(id)) return points[id];
        
        // Vertex pointing to point?
        if (vertexToPoint.contains(id) && points.contains(vertexToPoint[id]))
            return points[vertexToPoint[id]];
        
        // Try to resolve entity - avoid recursion, just check one level
        if (entities.contains(id))
        {
            const StepEntity& e = entities[id];
            if (e.type == "CARTESIAN_POINT") return extractPoint(e);
            // For VERTEX_POINT, resolve its point reference directly
            if (e.type == "VERTEX_POINT" && e.params.size() > 1)
            {
                QString pointRef = e.params[1].trimmed();
                if (pointRef.startsWith("#"))
                {
                    int pointId = pointRef.mid(1).toInt();
                    if (points.contains(pointId))
                        return points[pointId];
                }
            }
        }
        return QVector3D();
    };
    
    // Look for various face/loop types
    for (auto it = entities.begin(); it != entities.end(); ++it)
    {
        const StepEntity& entity = it.value();
        QVector<QVector3D> facePoints;
        
        // Helper to extract a list of point refs from a loop-style entity
        auto collectPointsFromLoopEntity = [&](const StepEntity& loopEntity, QVector<QVector3D>& outPoints)
        {
            // Find the parameter that actually contains the list of references "(#1,#2,#3,...)"
            QString vertexListStr;
            for (const QString& p : loopEntity.params)
            {
                if (p.contains('#'))
                {
                    vertexListStr = p;
                    break;
                }
            }
            if (vertexListStr.isEmpty())
                return;
            
            QRegularExpression refRegex("#(\\d+)");
            QRegularExpressionMatchIterator refIt = refRegex.globalMatch(vertexListStr);
            while (refIt.hasNext())
            {
                QString ref = refIt.next().captured(0);
                QVector3D pt = resolveToPoint(ref);
                if (!pt.isNull())
                    outPoints.append(pt);
            }
        };
        
        // Handle POLY_LOOP - contains ordered vertex list
        if (entity.type == "POLY_LOOP")
        {
            collectPointsFromLoopEntity(entity, facePoints);
        }
        // Handle EDGE_LOOP - ordered list of ORIENTED_EDGE, which reference EDGE_CURVE/vertices
        else if (entity.type == "EDGE_LOOP")
        {
            // EDGE_LOOP parameters may include a name and a list of oriented edges
            // Extract all entity references from all params
            for (const QString& param : entity.params)
            {
                QRegularExpression refRegex("#(\\d+)");
                QRegularExpressionMatchIterator refIt = refRegex.globalMatch(param);
                while (refIt.hasNext())
                {
                    QString oeRef = refIt.next().captured(0);
                    StepEntity oeEntity = resolveRef(oeRef);
                    if (!oeEntity.type.contains("ORIENTED_EDGE"))
                        continue;
                    
                    // ORIENTED_EDGE params typically include an EDGE_CURVE reference
                    for (const QString& oeParam : oeEntity.params)
                    {
                        if (!oeParam.startsWith("#"))
                            continue;
                        StepEntity edgeEntity = resolveRef(oeParam);
                        if (!edgeEntity.type.contains("EDGE_CURVE"))
                            continue;
                        
                        // EDGE_CURVE usually references two vertices (#v1,#v2,...)
                        QVector<int> vertexIds;
                        QRegularExpression vRefRegex("#(\\d+)");
                        QRegularExpressionMatchIterator vIt = vRefRegex.globalMatch(oeParam);
                        while (vIt.hasNext())
                        {
                            int id = vIt.next().captured(1).toInt();
                            vertexIds.append(id);
                        }
                        if (vertexIds.isEmpty())
                        {
                            // Fallback: scan edgeEntity params for vertex refs
                            for (const QString& eParam : edgeEntity.params)
                            {
                                QRegularExpression eRefRegex("#(\\d+)");
                                QRegularExpressionMatchIterator eIt = eRefRegex.globalMatch(eParam);
                                while (eIt.hasNext())
                                {
                                    int id = eIt.next().captured(1).toInt();
                                    vertexIds.append(id);
                                }
                            }
                        }
                        
                        if (!vertexIds.isEmpty())
                        {
                            // Use the first vertex of the edge as part of the loop polygon
                            QString vRef = QString("#%1").arg(vertexIds.first());
                            QVector3D pt = resolveToPoint(vRef);
                            if (!pt.isNull())
                                facePoints.append(pt);
                        }
                        break; // only process first EDGE_CURVE per ORIENTED_EDGE
                    }
                }
            }
        }
        // Handle FACE_BOUND / FACE_OUTER_BOUND that reference loops
        else if (entity.type == "FACE_BOUND" || entity.type == "FACE_OUTER_BOUND")
        {
            // FACE_BOUND typically references a loop (POLY_LOOP or EDGE_LOOP)
            for (const QString& param : entity.params)
            {
                if (!param.startsWith("#"))
                    continue;
                StepEntity loopEntity = resolveRef(param);
                if (loopEntity.type == "POLY_LOOP" || loopEntity.type == "EDGE_LOOP")
                {
                    collectPointsFromLoopEntity(loopEntity, facePoints);
                }
            }
        }
        
        // Triangulate polygon (fan triangulation)
        if (facePoints.size() >= 3)
        {
            for (int i = 1; i < facePoints.size() - 1; i++)
            {
                vertices.append(facePoints[0]);
                vertices.append(facePoints[i]);
                vertices.append(facePoints[i + 1]);
            }
        }
    }
    
    // Fallback: if no geometry found, create a simple test cube
    if (vertices.isEmpty() && !points.isEmpty())
    {
        ALOG("STEP: No explicit faces found, attempting to create geometry from points");
        
        // Get all points and create simple triangles if we have enough points
        QList<QVector3D> pointList = points.values();
        if (pointList.size() >= 3)
        {
            // Create triangles from groups of 3 points
            for (int i = 0; i + 2 < pointList.size(); i += 3)
            {
                vertices.append(pointList[i]);
                vertices.append(pointList[i + 1]);
                vertices.append(pointList[i + 2]);
            }
        }
    }
}
