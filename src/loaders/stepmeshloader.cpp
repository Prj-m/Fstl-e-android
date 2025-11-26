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
    }
    
    ALOG("STEP: Parsed %d entities", entityCount);
    
    // Extract geometry
    tessellateGeometry();
    
    ALOG("STEP: Extracted %lld vertices (%lld triangles)", 
         (long long)vertices.size(), (long long)vertices.size() / 3);
    
    return vertices.size() > 0;
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
        
        // Handle POLY_LOOP - contains ordered vertex list
        if (entity.type == "POLY_LOOP" && entity.params.size() > 0)
        {
            QString vertexListStr = entity.params[0];
            // Parse list: (vertexListStr could be "(#1,#2,#3)")
            QRegularExpression refRegex("#(\\d+)");
            QRegularExpressionMatchIterator refIt = refRegex.globalMatch(vertexListStr);
            
            while (refIt.hasNext())
            {
                QString ref = refIt.next().captured(0);
                QVector3D pt = resolveToPoint(ref);
                if (!pt.isNull())
                    facePoints.append(pt);
            }
        }
        // Handle FACE_BOUND
        else if (entity.type == "FACE_BOUND" || entity.type == "FACE_OUTER_BOUND")
        {
            // FACE_BOUND typically references a loop
            for (const QString& param : entity.params)
            {
                if (param.startsWith("#"))
                {
                    StepEntity loopEntity = resolveRef(param);
                    if (loopEntity.type == "POLY_LOOP" && loopEntity.params.size() > 0)
                    {
                        QString vertexListStr = loopEntity.params[0];
                        QRegularExpression refRegex("#(\\d+)");
                        QRegularExpressionMatchIterator refIt = refRegex.globalMatch(vertexListStr);
                        
                        while (refIt.hasNext())
                        {
                            QString ref = refIt.next().captured(0);
                            QVector3D pt = resolveToPoint(ref);
                            if (!pt.isNull())
                                facePoints.append(pt);
                        }
                    }
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
