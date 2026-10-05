#include <future>
#include <cmath>
#include <limits>

#include "core/loader.h"
#include "core/importlimits.h"
#include "core/boundedzip.h"
#include "core/vertex.h"
#include "loaders/stepmeshloader.h"
#include "loaders/occtsteploader.h"
#include <QXmlStreamReader>
#include <QFile>
#include <QVector3D>

#ifdef Q_OS_ANDROID
#include <android/log.h>
#define LOG_TAG "FSTL_3MF"
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#define ALOG(...) qDebug(__VA_ARGS__)
#endif

Loader::Loader(QObject* parent, const QString& filename, bool is_reload)
    : QThread(parent), filename(filename), is_reload(is_reload)
{
    // Nothing to do here
}

void Loader::run()
try
{
    Mesh* mesh = nullptr;
    
    ALOG("Loader::run() called for file: %s", filename.toStdString().c_str());
    
    // Detect 3MF and STEP by inspecting the file header.
    // 3MF: ZIP container (magic bytes "PK")
    // STEP: ISO-10303-21 text header and/or HEADER/DATA markers
    QFile file(filename);
    bool is_3mf = false;
    bool is_step = false;
    
    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (file.size() > ImportLimits::SourceBytes)
        {
            emit error_bad_stl();
            return;
        }
        // Read a reasonably sized header window so we can detect both ZIP magic
        // and typical STEP headers even if there are leading comments.
        QByteArray header = file.read(4096);
        file.close();
        
        if (header.size() >= 2 && header[0] == 0x50 && header[1] == 0x4B)  // "PK" magic bytes
        {
            ALOG("Detected ZIP magic bytes - treating as 3MF");
            is_3mf = true;
        }
        
        if (!is_3mf)
        {
            QByteArray upper = header.toUpper();
            
            if (upper.contains("ISO-10303-21"))
            {
                ALOG("Detected ISO-10303-21 header - treating as STEP");
                is_step = true;
            }
            else if (upper.contains("HEADER;") && upper.contains("DATA;"))
            {
                ALOG("Detected HEADER/DATA markers - probable STEP file");
                is_step = true;
            }
        }
    }
    
    // Also check file extension as fallback when header sniff did not decide
    if (!is_3mf && !is_step)
    {
        if (filename.endsWith(".3mf", Qt::CaseInsensitive))
        {
            ALOG("Detected .3MF extension");
            is_3mf = true;
        }
        else if (filename.endsWith(".step", Qt::CaseInsensitive) ||
                 filename.endsWith(".stp", Qt::CaseInsensitive))
        {
            ALOG("Detected .STEP/.STP extension");
            is_step = true;
        }
    }
    
    if (is_3mf)
    {
        ALOG("Loading as 3MF file");
        mesh = load_3mf();
    }
    else if (is_step)
    {
        ALOG("Loading as STEP file");
        mesh = load_step();
    }
    else
    {
        ALOG("Loading as STL file");
        mesh = load_stl();
    }
    
    if (mesh)
    {
        if (mesh->empty())
        {
            emit error_empty_mesh();
            delete mesh;
        }
        else
        {
            emit got_mesh(mesh, is_reload);
            emit loaded_file(filename);
        }
    }
}

catch (...)
{
    ALOG("Import failed with an exception");
    emit error_bad_stl();
}

////////////////////////////////////////////////////////////////////////////////

void parallel_sort(Vertex* begin, Vertex* end, int threads)
{
    if (threads < 2 || end - begin < 2)
    {
        std::sort(begin, end);
    }
    else
    {
        const auto mid = begin + (end - begin) / 2;
        if (threads == 2)
        {
            auto future = std::async(parallel_sort, begin, mid, threads / 2);
            std::sort(mid, end);
            future.wait();
        }
        else
        {
            auto a = std::async(std::launch::async, parallel_sort, begin, mid, threads / 2);
            auto b = std::async(std::launch::async, parallel_sort, mid, end, threads / 2);
            a.wait();
            b.wait();
        }
        std::inplace_merge(begin, mid, end);
    }
}

Mesh* mesh_from_verts(uint32_t tri_count, QVector<Vertex>& verts)
{
#ifdef Q_OS_ANDROID
    // Android: Calculate per-triangle normals for flat shading
    std::vector<GLfloat> flat_verts;
    std::vector<GLfloat> flat_normals;
    flat_verts.reserve(size_t(tri_count)*9); // 3 vertices * 3 floats per triangle
    flat_normals.reserve(size_t(tri_count)*9); // 3 normals * 3 floats per triangle
    
    for (size_t i = 0; i < verts.size(); i += 3)
    {
        // Get triangle vertices
        float v0x = verts[i].x, v0y = verts[i].y, v0z = verts[i].z;
        float v1x = verts[i+1].x, v1y = verts[i+1].y, v1z = verts[i+1].z;
        float v2x = verts[i+2].x, v2y = verts[i+2].y, v2z = verts[i+2].z;
        
        // Calculate edge vectors
        float e1x = v1x - v0x, e1y = v1y - v0y, e1z = v1z - v0z;
        float e2x = v2x - v0x, e2y = v2y - v0y, e2z = v2z - v0z;
        
        // Calculate normal via cross product
        float nx = e1y * e2z - e1z * e2y;
        float ny = e1z * e2x - e1x * e2z;
        float nz = e1x * e2y - e1y * e2x;
        
        // Normalize
        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 0.0001f) {
            nx /= len; ny /= len; nz /= len;
        }
        
        // Store vertices
        flat_verts.push_back(v0x); flat_verts.push_back(v0y); flat_verts.push_back(v0z);
        flat_verts.push_back(v1x); flat_verts.push_back(v1y); flat_verts.push_back(v1z);
        flat_verts.push_back(v2x); flat_verts.push_back(v2y); flat_verts.push_back(v2z);
        
        // Store actual NORMALS (same for all 3 vertices of the triangle)
        for (int j = 0; j < 3; j++) {
            flat_normals.push_back(nx);
            flat_normals.push_back(ny);
            flat_normals.push_back(nz);
        }
    }
    
    // No indices - use non-indexed rendering
    std::vector<GLuint> empty_indices;
    return new Mesh(std::move(flat_verts), std::move(flat_normals), std::move(empty_indices));
#else
    // Desktop: Use indexed rendering with vertex deduplication
    // Save indicies as the second element in the array
    // (so that we can reconstruct triangle order after sorting)
    for (size_t i=0; i < tri_count*3; ++i)
    {
        verts[i].i = i;
    }

    // Check how many threads the hardware can safely support. This may return
    // 0 if the property can't be read so we shoud check for that too.
    auto threads = std::thread::hardware_concurrency();
    if (threads == 0)
    {
        threads = 8;
    }

    // Sort the set of vertices (to deduplicate)
    parallel_sort(verts.data(), verts.data() + verts.size(), threads);

    // This vector will store triangles as sets of 3 indices
    std::vector<GLuint> indices(tri_count*3);

    // Go through the sorted vertex list, deduplicating and creating
    // an indexed geometry representation for the triangles.
    // Unique vertices are moved so that they occupy the first vertex_count
    // positions in the verts array.
    size_t vertex_count = 0;
    for (auto v : verts)
    {
        if (!vertex_count || v != verts[vertex_count-1])
        {
            verts[vertex_count++] = v;
        }
        indices[v.i] = vertex_count - 1;
    }
    verts.resize(vertex_count);

    std::vector<GLfloat> flat_verts;
    flat_verts.reserve(vertex_count*3);
    for (auto v : verts)
    {
        flat_verts.push_back(v.x);
        flat_verts.push_back(v.y);
        flat_verts.push_back(v.z);
    }

    return new Mesh(std::move(flat_verts), std::move(indices));
#endif
}

////////////////////////////////////////////////////////////////////////////////

Mesh* Loader::load_stl()
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        emit error_missing_file();
        return NULL;
    }

    qint64 file_size, file_size_old;
    file_size = file.size();
    int stabilityChecks = 0;
    do {
        file_size_old = file_size;
        QThread::usleep(100000);
        file_size = file.size();
        if (file_size > ImportLimits::SourceBytes || ++stabilityChecks > 10)
        {
            emit error_bad_stl();
            return nullptr;
        }
    }
    while(file_size != file_size_old);

    // First, try to read the stl as an ASCII file
    if (file.read(5) == "solid")
    {
        file.readLine(); // skip solid name
        const auto line = file.readLine().trimmed();
        if (line.startsWith("facet") ||
            line.startsWith("endsolid"))
        {
            file.seek(0);
            return read_stl_ascii(file);
        }
        // Otherwise, this STL is a binary stl but contains 'solid' as
        // the first five characters.  This is a bad life choice, but
        // we can gracefully handle it by falling through to the binary
        // STL reader below.
    }

    file.seek(0);
    return read_stl_binary(file);
}

Mesh* Loader::read_stl_binary(QFile& file)
{
    QDataStream data(&file);
    data.setByteOrder(QDataStream::LittleEndian);
    data.setFloatingPointPrecision(QDataStream::SinglePrecision);

    // Load the triangle count from the .stl file
    file.seek(80);
    uint32_t tri_count = 0;
    data >> tri_count;

    // Verify that the file is the right size
    const qint64 payloadSize = qint64(tri_count) * 50;
    if (tri_count > ImportLimits::Triangles || data.status() != QDataStream::Ok ||
        file.size() != 84 + payloadSize ||
        quint64(tri_count) * 3 > quint64(std::numeric_limits<int>::max()))
    {
        emit error_bad_stl();
        return NULL;
    }

    // Extract vertices into an array of xyz, unsigned pairs
    QVector<Vertex> verts(qsizetype(tri_count) * 3);

    // Read each record exactly; providers can fail or truncate after size checks.
    uint8_t buffer[50 * 1024];
    int bufferedRecords = 0;
    int nextRecord = 0;
    for (auto v=verts.begin(); v != verts.end(); v += 3)
    {
        if (nextRecord == bufferedRecords)
        {
            bufferedRecords = int(std::min<qsizetype>(1024, (verts.end() - v) / 3));
            const int bytesToRead = bufferedRecords * 50;
            if (data.readRawData(reinterpret_cast<char*>(buffer), bytesToRead) != bytesToRead)
            {
                emit error_bad_stl();
                return nullptr;
            }
            nextRecord = 0;
        }
        auto b = buffer + nextRecord++ * 50;
        // Skip the face normal (first 3 floats) - we'll compute it from vertices
        b += 3 * sizeof(float);
        
        // Load vertex data from .stl file into vertices
        for (unsigned i=0; i < 3; ++i)
        {
            qFromLittleEndian<float>(b, 3, &v[i]);
            if (!std::isfinite(v[i].x) || !std::isfinite(v[i].y) || !std::isfinite(v[i].z))
            {
                emit error_bad_stl();
                return nullptr;
            }
            b += 3 * sizeof(float);
        }

        // Skip face attribute
        b += sizeof(uint16_t);
    }

    return mesh_from_verts(tri_count, verts);
}

Mesh* Loader::load_3mf()
{
    ALOG("load_3mf() START for: %s", filename.toStdString().c_str());
    
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly))
    {
        ALOG("FAILED to open 3MF file: %s", filename.toStdString().c_str());
        emit error_missing_file();
        return nullptr;
    }
    
    if (file.size() > ImportLimits::SourceBytes)
    {
        emit error_bad_stl();
        return nullptr;
    }
    ALOG("File opened successfully, size: %lld", file.size());
    
    QByteArray modelData;
    if (!readBounded3mfModel(file, modelData))
    {
        ALOG("Invalid or oversized 3MF model archive");
        emit error_bad_stl();
        return nullptr;
    }
    ALOG("Model data loaded, size: %d bytes", int(modelData.size()));
    
    // Parse XML content
    ALOG("Starting XML parse...");
    QXmlStreamReader xml(modelData);
    QVector<Vertex> verts;
    QVector<float> vertex_coords;  // Store all vertex coordinates
    uint32_t tri_count = 0;
    int vertex_count = 0;
    int object_count = 0, mesh_count = 0, item_count = 0;
    
    while (!xml.atEnd())
    {
        xml.readNext();
        if (xml.tokenType() == QXmlStreamReader::DTD)
        {
            emit error_bad_stl();
            return nullptr;
        }
        
        if (xml.isStartElement())
        {
            // Use localName() to ignore namespaces
            QString elemName = xml.name().toString();
            // This viewer supports a single untransformed mesh. Reject
            // assemblies instead of silently rendering the wrong geometry.
            if ((elemName == "object" && ++object_count > 1)
                || (elemName == "mesh" && ++mesh_count > 1)
                || (elemName == "item" && ++item_count > 1)
                || elemName == "components" || elemName == "component"
                || xml.attributes().hasAttribute("transform"))
            {
                emit error_bad_stl();
                return nullptr;
            }
            
            if (elemName == "vertex" || elemName.endsWith(":vertex"))
            {
                if (vertex_count >= ImportLimits::Coordinates)
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                // Read vertex coordinates
                QXmlStreamAttributes attrs = xml.attributes();
                bool validX = false, validY = false, validZ = false;
                float x = attrs.value("x").toFloat(&validX);
                float y = attrs.value("y").toFloat(&validY);
                float z = attrs.value("z").toFloat(&validZ);
                if (!validX || !validY || !validZ ||
                    !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                vertex_coords.push_back(x);
                vertex_coords.push_back(y);
                vertex_coords.push_back(z);
                vertex_count++;
            }
            else if (elemName == "triangle" || elemName.endsWith(":triangle"))
            {
                if (tri_count >= ImportLimits::Triangles)
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                // Read triangle vertex indices
                QXmlStreamAttributes attrs = xml.attributes();
                bool valid1 = false, valid2 = false, valid3 = false;
                int v1 = attrs.value("v1").toInt(&valid1);
                int v2 = attrs.value("v2").toInt(&valid2);
                int v3 = attrs.value("v3").toInt(&valid3);
                const qsizetype availableVertices = vertex_coords.size() / 3;
                if (!valid1 || !valid2 || !valid3 ||
                    v1 < 0 || v2 < 0 || v3 < 0 ||
                    v1 >= availableVertices || v2 >= availableVertices || v3 >= availableVertices)
                {
                    emit error_bad_stl();
                    return nullptr;
                }

                const qsizetype i1 = qsizetype(v1) * 3, i2 = qsizetype(v2) * 3, i3 = qsizetype(v3) * 3;
                verts.push_back(Vertex(vertex_coords[i1], vertex_coords[i1+1], vertex_coords[i1+2]));
                verts.push_back(Vertex(vertex_coords[i2], vertex_coords[i2+1], vertex_coords[i2+2]));
                verts.push_back(Vertex(vertex_coords[i3], vertex_coords[i3+1], vertex_coords[i3+2]));
                tri_count++;
            }
        }
    }
    
    ALOG("XML parse complete: vertices=%d, triangles=%d", vertex_count, tri_count);
    
    if (xml.hasError())
    {
        ALOG("XML parse ERROR: %s", xml.errorString().toStdString().c_str());
        emit error_bad_stl();
        return nullptr;
    }
    
    if (tri_count == 0)
    {
        ALOG("No triangles found in 3MF");
        emit error_empty_mesh();
        return nullptr;
    }
    
    ALOG("Creating mesh from %d triangles", tri_count);
    return mesh_from_verts(tri_count, verts);
}

Mesh* Loader::load_step()
{
    ALOG("load_step() START for: %s", filename.toStdString().c_str());

#ifdef FSTL_USE_OCCT_STEP
    ALOG("OCCT STEP support is compiled in");
#else
    ALOG("OCCT STEP support is NOT compiled in - using internal parser only");
#endif

    QVector<QVector3D> stepVerts;
    uint32_t tri_count = 0;

#ifdef FSTL_USE_OCCT_STEP
    {
        ALOG("Trying Open CASCADE STEP loader first...");
        OcctStepLoader occtLoader;
        unsigned int occtTriCount = 0;
        if (occtLoader.load(filename, stepVerts, occtTriCount) && !stepVerts.isEmpty())
        {
            tri_count = occtTriCount;
            ALOG("OCCT STEP loader succeeded with %d triangles", tri_count);
        }
        else
        {
            ALOG("OCCT STEP loader failed or returned empty geometry");
            emit error_bad_stl();
            return nullptr;
        }
    }
#endif

    if (stepVerts.isEmpty())
    {
        StepMeshLoader stepLoader;
        if (!stepLoader.parseFile(filename))
        {
            ALOG("Failed to parse STEP file with internal parser");
            emit error_bad_stl();
            return nullptr;
        }

        stepVerts = stepLoader.getVertices();
        tri_count = stepLoader.getTriangleCount();

        if (stepVerts.isEmpty())
        {
            ALOG("No geometry found in STEP file (internal parser)");
            emit error_empty_mesh();
            return nullptr;
        }
    }

    if (tri_count > ImportLimits::Triangles || stepVerts.size() != qsizetype(tri_count) * 3)
    {
        emit error_bad_stl();
        return nullptr;
    }

    // Convert QVector3D to Vertex
    QVector<Vertex> verts;
    verts.reserve(stepVerts.size());
    for (const QVector3D& v : stepVerts)
    {
        // OCCT uses double coordinates; narrowing to float can overflow even
        // when the source coordinate was finite. Never sort/render NaN or Inf.
        if (!std::isfinite(v.x()) || !std::isfinite(v.y()) || !std::isfinite(v.z()))
        {
            emit error_bad_stl();
            return nullptr;
        }
        verts.push_back(Vertex(v.x(), v.y(), v.z()));
    }

    ALOG("STEP: Creating mesh from %d triangles", tri_count);
    return mesh_from_verts(tri_count, verts);
}

Mesh* Loader::read_stl_ascii(QFile& file)
{
    file.readLine();
    uint32_t tri_count = 0;
    QVector<Vertex> verts(tri_count*3);

    bool okay = true;
    while (!file.atEnd() && okay)
    {
        const auto line = file.readLine().simplified();
        if (line.startsWith("endsolid"))
        {
            break;
        }
        else if (!line.startsWith("facet normal") ||
                 !file.readLine().simplified().startsWith("outer loop"))
        {
            okay = false;
            break;
        }

        if (tri_count >= ImportLimits::Triangles)
        {
            okay = false;
            break;
        }
        for (int i=0; i < 3; ++i)
        {
            auto line = file.readLine().simplified().split(' ');
            if (line.size() != 4 || line[0] != "vertex")
            {
                okay = false;
                break;
            }
            bool validX = false, validY = false, validZ = false;
            const float x = line[1].toFloat(&validX);
            const float y = line[2].toFloat(&validY);
            const float z = line[3].toFloat(&validZ);
            okay = validX && validY && validZ &&
                   std::isfinite(x) && std::isfinite(y) && std::isfinite(z);
            if (!okay)
                break;
            verts.push_back(Vertex(x, y, z));
        }
        if (!file.readLine().trimmed().startsWith("endloop") ||
            !file.readLine().trimmed().startsWith("endfacet"))
        {
            okay = false;
            break;
        }
        tri_count++;
    }

    if (okay)
    {
        return mesh_from_verts(tri_count, verts);
    }
    else
    {
        emit error_bad_stl();
        return NULL;
    }
}
