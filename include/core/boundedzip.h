#pragma once

#include "core/importlimits.h"
#include <QFile>
#include <QMap>
#include <QtEndian>
#include <zlib.h>

// Read the 3MF model parts of an ordinary single-disk ZIP. No extraction to
// disk, ZIP64, encryption or allocation based on untrusted decompressed sizes.
// Parts are keyed by lower-case archive path (for example "3d/3dmodel.model").
// The combined decompressed size of all parts is bounded by ModelXmlBytes.
inline bool readBounded3mfParts(QFile& file, QMap<QByteArray, QByteArray>& parts)
{
    parts.clear();
    if (file.size() < 22 || file.size() > ImportLimits::SourceBytes || !file.seek(0))
        return false;
    const QByteArray archive = file.read(ImportLimits::SourceBytes + 1);
    const qint64 size = archive.size();
    if (size != file.size() || size > ImportLimits::SourceBytes)
        return false;
    auto u16 = [&](qint64 p) { return qFromLittleEndian<quint16>(archive.constData() + p); };
    auto u32 = [&](qint64 p) { return qFromLittleEndian<quint32>(archive.constData() + p); };
    qint64 end = -1;
    for (qint64 p = size - 22; p >= qMax<qint64>(0, size - 65557); --p) {
        if (u32(p) == 0x06054b50 && p + 22 + u16(p + 20) == size) {
            end = p;
            break;
        }
    }
    if (end < 0 || u16(end + 4) != 0 || u16(end + 6) != 0
        || u16(end + 8) != u16(end + 10) || u16(end + 10) == 0xffff)
        return false;
    const qint64 directory = u32(end + 16);
    const qint64 directoryEnd = directory + u32(end + 12);
    if (directoryEnd > end)
        return false;

    QMap<QByteArray, qint64> entries;
    qint64 p = directory;
    for (int i = 0; i < u16(end + 10); ++i) {
        if (p + 46 > directoryEnd || u32(p) != 0x02014b50)
            return false;
        const qint64 nameSize = u16(p + 28);
        const qint64 next = p + 46 + nameSize + u16(p + 30) + u16(p + 32);
        if (next > directoryEnd || u16(p + 34) != 0)
            return false;
        const QByteArray name = archive.mid(p + 46, nameSize).toLower();
        if (name.startsWith("3d/") && name.endsWith(".model")) {
            if (entries.contains(name) || entries.size() >= ImportLimits::ModelParts)
                return false; // Ambiguous, duplicate or excessive model entries.
            entries.insert(name, p);
        }
        p = next;
    }
    if (p != directoryEnd || !entries.contains("3d/3dmodel.model"))
        return false;

    qint64 total = 0;
    for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
        const qint64 model = it.value();
        const quint16 flags = u16(model + 8), method = u16(model + 10);
        const qint64 compressed = u32(model + 20), expected = u32(model + 24);
        const qint64 local = u32(model + 42);
        if ((flags & ~quint16(0x080e)) != 0 || (method != 0 && method != 8)
            || expected <= 0 || total + expected > ImportLimits::ModelXmlBytes
            || compressed <= 0 || compressed > ImportLimits::SourceBytes
            || local + 30 > directory || u32(local) != 0x04034b50
            || u16(local + 6) != flags || u16(local + 8) != method)
            return false;
        const qint64 localNameSize = u16(local + 26);
        const qint64 data = local + 30 + localNameSize + u16(local + 28);
        if (data + compressed > directory || localNameSize != u16(model + 28)
            || archive.mid(local + 30, localNameSize) != archive.mid(model + 46, localNameSize))
            return false;
        if (!(flags & 8) && (u32(local + 18) != compressed || u32(local + 22) != expected
                            || u32(local + 14) != u32(model + 16)))
            return false;
        QByteArray output;
        output.reserve(expected); // bounded by ModelXmlBytes above; avoids regrowth copies
        if (method == 0) {
            if (compressed != expected)
                return false;
            output = archive.mid(data, compressed);
        } else {
            z_stream stream{};
            stream.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(archive.constData() + data));
            stream.avail_in = static_cast<uInt>(compressed);
            if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
                return false;
            char chunk[65536];
            bool valid = false;
            for (;;) {
                stream.next_out = reinterpret_cast<Bytef*>(chunk);
                stream.avail_out = sizeof(chunk);
                const int result = inflate(&stream, Z_NO_FLUSH);
                const qsizetype produced = sizeof(chunk) - stream.avail_out;
                if (output.size() + produced > expected)
                    break;
                output.append(chunk, produced);
                if (result == Z_STREAM_END) {
                    valid = stream.avail_in == 0 && output.size() == expected;
                    break;
                }
                if (result != Z_OK || produced == 0)
                    break;
            }
            inflateEnd(&stream);
            if (!valid)
                return false;
        }
        const quint32 actualCrc = crc32(0, reinterpret_cast<const Bytef*>(output.constData()),
                                       static_cast<uInt>(output.size()));
        if (actualCrc != u32(model + 16))
            return false;
        total += output.size();
        parts.insert(it.key(), output);
    }
    return true;
}

// Root model part only; kept for callers and tests that need just that part.
inline bool readBounded3mfModel(QFile& file, QByteArray& output)
{
    output.clear();
    QMap<QByteArray, QByteArray> parts;
    if (!readBounded3mfParts(file, parts))
        return false;
    output = parts.value("3d/3dmodel.model");
    return true;
}
