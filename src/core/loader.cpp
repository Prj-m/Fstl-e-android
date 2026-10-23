#include <future>
#include <cmath>
#include <limits>
#include <algorithm>

#include "core/loader.h"
#include "core/importlimits.h"
#include "core/boundedzip.h"
#include "core/fastfloat.h"
#include "core/vertex.h"
#include "loaders/stepmeshloader.h"
#include "loaders/occtsteploader.h"
#include <QXmlStreamReader>
#include <QBuffer>
#include <functional>
#include <QSet>
#include <QMap>
#include <QFile>
#include <QElapsedTimer>
#include <QDir>
#include <QStandardPaths>
#include <QVector3D>

#ifdef Q_OS_ANDROID
#include <android/log.h>
#define LOG_TAG "FSTL_3MF"
#define ALOG(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#define ALOG(...) qDebug(__VA_ARGS__)
#endif

Loader::Loader(QObject* parent, const QString& filename, bool is_reload)
    : QThread(parent), filename(filename), sourcePath(filename), is_reload(is_reload)
{
    // Nothing to do here
}

bool Loader::copyUnsizedSource()
{
    QFile in(filename);
    if (!in.open(QIODevice::ReadOnly))
    {
        emit error_missing_file();
        return false;
    }
    const QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cachePath.isEmpty() || !QDir().mkpath(cachePath))
    {
        emit error_missing_file();
        return false;
    }
    sourceCopy = std::make_unique<QTemporaryFile>(cachePath + "/fstl_source_XXXXXX");
    if (!sourceCopy->open())
    {
        emit error_missing_file();
        return false;
    }
    // Read until the stream ends: size() and atEnd() are meaningless here.
    for (;;)
    {
        if (isCancelled()) return false;
        const QByteArray chunk = in.read(1024 * 1024);
        if (chunk.isEmpty())
            break;
        if (sourceCopy->size() + chunk.size() > ImportLimits::SourceBytes ||
            sourceCopy->write(chunk) != chunk.size())
        {
            emit error_bad_stl();
            return false;
        }
    }
    if (in.error() != QFileDevice::NoError || !sourceCopy->flush() || sourceCopy->size() == 0)
    {
        emit error_bad_stl();
        return false;
    }
    sourceCopy->close();
    sourcePath = sourceCopy->fileName();
    ALOG("Copied unsized content URI to the cache");
    return true;
}

void Loader::cancel()
{
    cancelled.store(true);
    requestInterruption();
}

bool Loader::isCancelled() const
{
    return cancelled.load() || isInterruptionRequested();
}

void Loader::run()
try
{
    Mesh* mesh = nullptr;
    QElapsedTimer importTimer;
    importTimer.start();
    
    ALOG("Loader::run() called for file: %s", filename.toStdString().c_str());

    // Some providers (cloud, messaging, unscanned media) report no size.
    if (filename.startsWith(QLatin1String("content://")) && QFile(filename).size() <= 0 &&
        !copyUnsizedSource())
        return;
    
    // Detect 3MF and STEP by inspecting the file header.
    // 3MF: ZIP container (magic bytes "PK")
    // STEP: ISO-10303-21 text header and/or HEADER/DATA markers
    QFile file(sourcePath);
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
    
    if (isCancelled())
    {
        // The owner is shutting down; it only waits for finished.
        delete mesh;
        return;
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
            ALOG("Import finished in %lld ms (%d triangles)",
                 static_cast<long long>(importTimer.elapsed()), mesh->triCount());
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
    // Indexed rendering with vertex deduplication. Face normals are derived in
    // the fragment shaders, so shared vertices need no per-face data: a closed
    // mesh uploads ~half as many vertices as triangles plus 12 bytes of
    // indices per triangle, instead of 72 bytes per triangle non-indexed.
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
}

////////////////////////////////////////////////////////////////////////////////

Mesh* Loader::load_stl()
{
    QFile file(sourcePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        emit error_missing_file();
        return NULL;
    }

    qint64 file_size = file.size();
    if (file_size > ImportLimits::SourceBytes)
    {
        emit error_bad_stl();
        return nullptr;
    }
    // Watcher-triggered reloads can see a file that is still being written;
    // wait for its size to settle. First opens skip this delay.
    if (is_reload)
    {
        qint64 file_size_old;
        int stabilityChecks = 0;
        do {
            file_size_old = file_size;
            QThread::usleep(100000);
            if (isCancelled()) return nullptr;
            file_size = file.size();
            if (file_size > ImportLimits::SourceBytes || ++stabilityChecks > 10)
            {
                emit error_bad_stl();
                return nullptr;
            }
        }
        while(file_size != file_size_old);
    }

    // A size matching the binary triangle count is binary even when the
    // 80-byte header starts with "solid" (common with CAD exporters).
    bool binarySize = false;
    if (file_size >= 84 && file.seek(80))
    {
        uchar count[4];
        binarySize = file.read(reinterpret_cast<char*>(count), 4) == 4 &&
                     file_size == 84 + qint64(qFromLittleEndian<quint32>(count)) * 50;
    }
    file.seek(0);

    // First, try to read the stl as an ASCII file
    if (!binarySize && file.read(5) == "solid")
    {
        // Bounded reads: a binary file may contain no newline for megabytes.
        // The solid name may be long, so skip up to 64 KiB of it.
        for (qint64 skipped = 0; skipped < 64 * 1024;)
        {
            const QByteArray chunk = file.readLine(1024);
            skipped += chunk.size();
            if (chunk.isEmpty() || chunk.endsWith('\n'))
                break;
        }
        const auto line = file.readLine(1024).trimmed();
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

    // Some exporters leave the count at 0; recover it from an exact payload.
    const qint64 records = file.size() - 84;
    if (tri_count == 0 && records > 0 && records % 50 == 0)
        tri_count = uint32_t(std::min<qint64>(records / 50, qint64(ImportLimits::Triangles) + 1));

    // Verify the size. A little trailing data (padding some exporters append)
    // is ignored; the size check otherwise doubles as format validation.
    const qint64 payloadSize = qint64(tri_count) * 50;
    if (tri_count > ImportLimits::Triangles || data.status() != QDataStream::Ok ||
        file.size() < 84 + payloadSize || file.size() > 84 + payloadSize + 4096 ||
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
            if (isCancelled()) return nullptr;
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
    
    QFile file(sourcePath);
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
    
    QMap<QByteArray, QByteArray> parts;
    if (!readBounded3mfParts(file, parts))
    {
        ALOG("Invalid or oversized 3MF model archive");
        emit error_bad_stl();
        return nullptr;
    }
    ALOG("3MF model parts: %d", int(parts.size()));

    // 3MF stores row-vector affine transforms "m00 m01 m02 m10 m11 m12 m20 m21 m22 m30 m31 m32":
    // p' = (x, y, z, 1) * M.
    struct Affine {
        double m[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
        Affine then(const Affine& o) const {   // apply *this first, then o
            Affine r;
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    r.m[row * 3 + col] = m[row * 3] * o.m[col] + m[row * 3 + 1] * o.m[3 + col]
                                        + m[row * 3 + 2] * o.m[6 + col];
            for (int col = 0; col < 3; ++col)
                r.m[9 + col] = m[9] * o.m[col] + m[10] * o.m[3 + col] + m[11] * o.m[6 + col] + o.m[9 + col];
            return r;
        }
    };
    auto parseTransform = [](const QStringView text, Affine& out) {
        if (text.isEmpty())
            return true;
        const auto fields = text.trimmed().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() != 12)
            return false;
        for (int i = 0; i < 12; ++i) {
            bool ok = false;
            const double v = fields[i].toDouble(&ok);
            if (!ok || !std::isfinite(v) || std::abs(v) > 1e9)
                return false;
            out.m[i] = v;
        }
        return true;
    };
    struct Component { QByteArray part; QByteArray id; Affine transform; };
    struct Object {
        QVector<float> coords;
        QVector<qint32> indices;
        QVector<Component> components;
        bool drawable = true;
    };
    QMap<QByteArray, Object> objects;           // key: "<part>#<id>"
    QVector<Component> items;
    bool hasBuild = false;
    qsizetype totalVertices = 0, totalIndices = 0;
    const QByteArray root = "3d/3dmodel.model";
    auto partKey = [&](QStringView path, const QByteArray& current) {
        if (path.isEmpty())
            return current;
        QByteArray key = path.toUtf8().toLower();
        while (key.startsWith('/'))
            key.remove(0, 1);
        return key;
    };
    auto attr = [](const QXmlStreamAttributes& attrs, QLatin1StringView name) {
        for (const auto& a : attrs)
            if (a.name() == name)
                return a.value();
        return QStringView();
    };

    for (auto part = parts.constBegin(); part != parts.constEnd(); ++part)
    {
        // Read through a device: given a QByteArray, QXmlStreamReader decodes
        // the whole part to UTF-16 at once (2x the XML size); a device is
        // decoded in small chunks.
        QBuffer xmlSource;
        xmlSource.setData(part.value());
        xmlSource.open(QIODevice::ReadOnly);
        QXmlStreamReader xml(&xmlSource);
        Object* object = nullptr;
        int implicitId = 0;
        float unitScale = 1.0f; // coordinates are converted to millimetres
        while (!xml.atEnd())
        {
            if (isCancelled()) return nullptr;
            xml.readNext();
            if (xml.tokenType() == QXmlStreamReader::DTD)
            {
                emit error_bad_stl();
                return nullptr;
            }
            if (xml.isEndElement() && xml.name() == QLatin1StringView("object"))
                object = nullptr;
            if (!xml.isStartElement())
                continue;
            const QStringView name = xml.name();
            const QXmlStreamAttributes attrs = xml.attributes();
            if (name == QLatin1StringView("model"))
            {
                // 3MF core spec units; millimetre is the default.
                const QStringView unit = attr(attrs, QLatin1StringView("unit"));
                unitScale = unit == QLatin1StringView("micron") ? 0.001f
                          : unit == QLatin1StringView("centimeter") ? 10.0f
                          : unit == QLatin1StringView("inch") ? 25.4f
                          : unit == QLatin1StringView("foot") ? 304.8f
                          : unit == QLatin1StringView("meter") ? 1000.0f
                          : 1.0f;
            }
            else if (name == QLatin1StringView("object"))
            {
                QByteArray id = attr(attrs, QLatin1StringView("id")).toUtf8();
                if (id.isEmpty())
                    id = "implicit-" + QByteArray::number(++implicitId);
                const QByteArray key = part.key() + '#' + id;
                if (objects.contains(key))
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                const QStringView type = attr(attrs, QLatin1StringView("type"));
                object = &objects[key];
                object->drawable = type.isEmpty() || type == QLatin1StringView("model")
                                   || type == QLatin1StringView("solidsupport");
            }
            else if (name == QLatin1StringView("vertex") && object)
            {
                if (++totalVertices > ImportLimits::Coordinates)
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                bool vx = false, vy = false, vz = false;
                const float x = FastFloat::toFloat(attr(attrs, QLatin1StringView("x")), &vx);
                const float y = FastFloat::toFloat(attr(attrs, QLatin1StringView("y")), &vy);
                const float z = FastFloat::toFloat(attr(attrs, QLatin1StringView("z")), &vz);
                if (!vx || !vy || !vz || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                object->coords << x * unitScale << y * unitScale << z * unitScale;
            }
            else if (name == QLatin1StringView("triangle") && object)
            {
                totalIndices += 3;
                if (totalIndices > qsizetype(ImportLimits::Triangles) * 3)
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                bool ok1 = false, ok2 = false, ok3 = false;
                const int v1 = attr(attrs, QLatin1StringView("v1")).toInt(&ok1);
                const int v2 = attr(attrs, QLatin1StringView("v2")).toInt(&ok2);
                const int v3 = attr(attrs, QLatin1StringView("v3")).toInt(&ok3);
                const qsizetype available = object->coords.size() / 3;
                if (!ok1 || !ok2 || !ok3 || v1 < 0 || v2 < 0 || v3 < 0
                    || v1 >= available || v2 >= available || v3 >= available)
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                object->indices << v1 << v2 << v3;
            }
            else if (name == QLatin1StringView("component") && object)
            {
                Component c;
                c.part = partKey(attr(attrs, QLatin1StringView("path")), part.key());
                c.id = attr(attrs, QLatin1StringView("objectid")).toUtf8();
                if (c.id.isEmpty() || !parseTransform(attr(attrs, QLatin1StringView("transform")), c.transform))
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                // The translation is in this part's unit, like its vertices.
                for (int t = 9; t < 12; ++t)
                    c.transform.m[t] *= unitScale;
                object->components << c;
            }
            else if (name == QLatin1StringView("build"))
            {
                if (part.key() != root)
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                hasBuild = true;
            }
            else if (name == QLatin1StringView("item") && part.key() == root)
            {
                Component c;
                c.part = partKey(attr(attrs, QLatin1StringView("path")), root);
                c.id = attr(attrs, QLatin1StringView("objectid")).toUtf8();
                if (c.id.isEmpty() || !parseTransform(attr(attrs, QLatin1StringView("transform")), c.transform))
                {
                    emit error_bad_stl();
                    return nullptr;
                }
                // The translation is in this part's unit, like its vertices.
                for (int t = 9; t < 12; ++t)
                    c.transform.m[t] *= unitScale;
                items << c;
            }
        }
        if (xml.hasError())
        {
            ALOG("XML parse ERROR: %s", xml.errorString().toStdString().c_str());
            emit error_bad_stl();
            return nullptr;
        }
    }

    parts.clear(); // up to ModelXmlBytes of XML is no longer needed

    // Files without a <build> section draw every top-level object of the root part.
    if (!hasBuild)
    {
        QSet<QByteArray> referenced;
        for (const Object& o : objects)
            for (const Component& c : o.components)
                referenced.insert(c.part + '#' + c.id);
        for (auto it = objects.constBegin(); it != objects.constEnd(); ++it)
            if (it.key().startsWith(root + '#') && !referenced.contains(it.key()))
                items << Component{root, it.key().mid(root.size() + 1), Affine()};
    }

    QVector<Vertex> verts;
    verts.reserve(totalIndices); // exact without instancing; a lower bound otherwise
    uint32_t tri_count = 0;
    qsizetype instances = 0;
    QSet<QByteArray> stack;
    std::function<bool(const QByteArray&, const Affine&, int)> expand =
        [&](const QByteArray& key, const Affine& world, int depth) -> bool
    {
        if (isCancelled() || depth > ImportLimits::ComponentDepth || ++instances > ImportLimits::Instances
            || stack.contains(key))
            return false;
        const auto found = objects.constFind(key);
        if (found == objects.constEnd())
            return false;
        const Object& o = found.value();
        if (o.drawable)
        {
            if (quint64(tri_count) + quint64(o.indices.size() / 3) > ImportLimits::Triangles)
                return false;
            for (qsizetype i = 0; i < o.indices.size(); ++i)
            {
                const qsizetype v = qsizetype(o.indices[i]) * 3;
                const double x = o.coords[v], y = o.coords[v + 1], z = o.coords[v + 2];
                const double* m = world.m;
                const float tx = float(x * m[0] + y * m[3] + z * m[6] + m[9]);
                const float ty = float(x * m[1] + y * m[4] + z * m[7] + m[10]);
                const float tz = float(x * m[2] + y * m[5] + z * m[8] + m[11]);
                if (!std::isfinite(tx) || !std::isfinite(ty) || !std::isfinite(tz))
                    return false;
                verts.push_back(Vertex(tx, ty, tz));
            }
            tri_count += uint32_t(o.indices.size() / 3);
        }
        stack.insert(key);
        for (const Component& c : o.components)
            if (!expand(c.part + '#' + c.id, c.transform.then(world), depth + 1))
                return false;
        stack.remove(key);
        return true;
    };
    for (const Component& item : items)
    {
        if (!expand(item.part + '#' + item.id, item.transform, 0))
        {
            if (isCancelled()) return nullptr;
            ALOG("Invalid 3MF assembly (missing, cyclic, too deep or over budget)");
            emit error_bad_stl();
            return nullptr;
        }
    }

    ALOG("3MF: objects=%d items=%d instances=%d triangles=%d",
         int(objects.size()), int(items.size()), int(instances), int(tri_count));
    if (tri_count == 0)
    {
        ALOG("No triangles found in 3MF");
        emit error_empty_mesh();
        return nullptr;
    }
    objects.clear(); // release per-object coordinates before building the mesh
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
        const bool loaded = occtLoader.load(sourcePath, stepVerts, occtTriCount,
                                            [this] { return isCancelled(); });
        if (isCancelled())
        {
            return nullptr;
        }
        if (loaded && !stepVerts.isEmpty())
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
        if (!stepLoader.parseFile(sourcePath))
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

namespace {
// Streams whitespace-separated tokens from a file in large chunks without
// per-line allocations (the previous readLine/simplified/split approach
// allocated ~7 times per vertex line).
class StlTokenReader
{
public:
    explicit StlTokenReader(QFile& file) : file(file) {}

    // Next token, or empty at end of input. Tokens are views into the buffer
    // and stay valid only until the next call.
    QByteArrayView next()
    {
        skipSpace();
        qsizetype end = pos;
        for (;;)
        {
            // constData(): a non-const QByteArray::operator[] checks for detach per byte.
            const char* data = buffer.constData();
            const qsizetype size = buffer.size();
            while (end < size && !isSpace(data[end]))
                ++end;
            if (end < size || eof)
                break;
            // Token reaches the end of the buffer: keep it and read more.
            // At end of input the loop ends with the token complete.
            end -= pos;
            refill();
            end += pos;
        }
        if (end - pos > MaxToken)
        {
            failed = true;
            return {};
        }
        const QByteArrayView token(buffer.constData() + pos, end - pos);
        pos = end;
        return token;
    }

    // Skips the rest of the current line (bounded).
    void skipLine()
    {
        qsizetype skipped = 0;
        for (;;)
        {
            const char* data = buffer.constData();
            while (pos < buffer.size() && data[pos] != '\n' && skipped < MaxLine)
            {
                ++pos;
                ++skipped;
            }
            if (pos < buffer.size() || eof || skipped >= MaxLine || !refill())
                return;
        }
    }

    bool failed = false;

private:
    static constexpr qsizetype Chunk = 1 << 20;
    static constexpr qsizetype MaxToken = 256;
    static constexpr qsizetype MaxLine = 64 * 1024; // e.g. a long solid name

    static bool isSpace(char c) { return c == ' ' || c == '\n' || c == '\r' || c == '\t' || c == '\f' || c == '\v'; }

    void skipSpace()
    {
        for (;;)
        {
            const char* data = buffer.constData();
            const qsizetype size = buffer.size();
            while (pos < size && isSpace(data[pos]))
                ++pos;
            if (pos < buffer.size() || eof || !refill())
                return;
        }
    }

    bool refill()
    {
        buffer.remove(0, pos);
        pos = 0;
        const qsizetype kept = buffer.size();
        buffer.resize(kept + Chunk);
        const qint64 got = file.read(buffer.data() + kept, Chunk);
        if (got <= 0)
        {
            buffer.resize(kept);
            eof = true;
            failed = failed || got < 0;
            return false;
        }
        buffer.resize(kept + qsizetype(got));
        return true;
    }

    QFile& file;
    QByteArray buffer;
    qsizetype pos = 0;
    bool eof = false;
};
} // namespace

Mesh* Loader::read_stl_ascii(QFile& file)
{
    StlTokenReader tokens(file);
    tokens.skipLine(); // "solid name"

    uint32_t tri_count = 0;
    QVector<Vertex> verts;
    // Typical facets take ~250 bytes; reserving avoids repeated regrowth.
    verts.reserve(qsizetype(std::min<qint64>(file.size() / 250, ImportLimits::Triangles)) * 3);

    bool okay = true;
    for (;;)
    {
        if ((tri_count & 0x3FF) == 0 && isCancelled()) return nullptr;
        const QByteArrayView token = tokens.next();
        if (token.isEmpty())
        {
            break; // end of input (some files omit endsolid)
        }
        if (token == "endsolid")
        {
            // Some exporters write one solid block per body.
            tokens.skipLine();
            if (tokens.next() == "solid")
            {
                tokens.skipLine();
                continue;
            }
            break; // ignore trailing data after the last block, as before
        }
        if (token != "facet" || tokens.next() != "normal" || tri_count >= ImportLimits::Triangles)
        {
            okay = false;
            break;
        }
        // The stored facet normal is recomputed later; skip its values.
        QByteArrayView word;
        for (int i = 0; i < 4 && (word = tokens.next()) != "outer"; ++i) {}
        if (word != "outer" || tokens.next() != "loop")
        {
            okay = false;
            break;
        }
        for (int i = 0; i < 3 && okay; ++i)
        {
            if (tokens.next() != "vertex")
            {
                okay = false;
                break;
            }
            float xyz[3];
            for (float& value : xyz)
            {
                bool valid = false;
                value = FastFloat::toFloat(tokens.next(), &valid);
                okay = okay && valid && std::isfinite(value);
            }
            if (okay)
                verts.push_back(Vertex(xyz[0], xyz[1], xyz[2]));
        }
        if (!okay || tokens.next() != "endloop" || tokens.next() != "endfacet")
        {
            okay = false;
            break;
        }
        tri_count++;
    }

    if (okay && !tokens.failed)
    {
        return mesh_from_verts(tri_count, verts);
    }
    else
    {
        emit error_bad_stl();
        return NULL;
    }
}
