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
    // On Android, content:// URIs need special handling
    // Read the file content into memory first
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
    ALOG("STEP: Starting parse");
    
    // STEP files have sections: HEADER, DATA, END
    int dataStart = data.indexOf("DATA;");
    int dataEnd = data.indexOf("ENDSEC;", dataStart);
    
    if (dataStart == -1 || dataEnd == -1)
    {
        errorString = "Invalid STEP file format";
        ALOG("STEP: %s", errorString.toStdString().c_str());
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
    
    ALOG("STEP: Extracted %d vertices (%d triangles)", 
         vertices.size(), vertices.size() / 3);
    
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
    
    for (auto it = entities.begin(); it != entities.end(); ++it)
    {
        const StepEntity& entity = it.value();
        
        if (entity.type == "CARTESIAN_POINT")
        {
            QVector3D point = extractPoint(entity);
            points[entity.id] = point;
        }
    }
    
    ALOG("STEP: Found %d CARTESIAN_POINTs", points.size());
    
    // Look for triangulated geometry or face bounds
    // This is a simplified approach - full STEP parsing is very complex
    for (auto it = entities.begin(); it != entities.end(); ++it)
    {
        const StepEntity& entity = it.value();
        
        // Look for triangulated faces or polygons
        if (entity.type.contains("FACE") || 
            entity.type.contains("POLY_LOOP") ||
            entity.type.contains("FACE_BOUND"))
        {
            // Try to extract point references
            QVector<QVector3D> facePoints;
            
            for (const QString& param : entity.params)
            {
                if (param.startsWith("#"))
                {
                    StepEntity refEntity = resolveRef(param);
                    if (refEntity.type == "CARTESIAN_POINT")
                    {
                        facePoints.append(extractPoint(refEntity));
                    }
                    else if (refEntity.type.contains("VERTEX"))
                    {
                        // VERTEX may reference a point
                        for (const QString& vParam : refEntity.params)
                        {
                            if (vParam.startsWith("#"))
                            {
                                StepEntity pointEntity = resolveRef(vParam);
                                if (pointEntity.type == "CARTESIAN_POINT")
                                {
                                    facePoints.append(extractPoint(pointEntity));
                                }
                            }
                        }
                    }
                }
            }
            
            // Triangulate polygon (fan triangulation for simple case)
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
