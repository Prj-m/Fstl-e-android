#include "core/loader.h"
#include "core/importlimits.h"
#include "core/boundedzip.h"
#include "core/fileopenpath.h"
#include <QtEndian>
#include <QCoreApplication>
#include <QDataStream>
#include <QTemporaryDir>
#include <QtCore/private/qzipwriter_p.h>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>

class TestLoader : public Loader {
public:
    explicit TestLoader(const QString& path) : Loader(nullptr, path, false) {}
    using Loader::load_stl;
    using Loader::load_3mf;
    using Loader::load_step;
};

static QByteArray binaryStl(quint32 count, bool triangle = true, bool finite = true) {
    QByteArray bytes(80, '\0');
    QDataStream stream(&bytes, QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    stream << count;
    if (triangle) {
        for (int i = 0; i < 12; ++i)
            stream << (finite ? float(i % 3) : std::numeric_limits<float>::quiet_NaN());
        stream << quint16(0);
    }
    return bytes;
}

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 2;
    int failures = 0, checks = 0;
    for (const QByteArray& coordinate : {QByteArray("0"), QByteArray("nan"), QByteArray("inf"), QByteArray("1e100")}) {
        const QString path = dir.filePath("coordinate.step");
        QFile file(path);
        const QByteArray bytes = "ISO-10303-21;\nDATA;\n#1=CARTESIAN_POINT('',(" + coordinate
            + ",0,0));\n#2=CARTESIAN_POINT('',(1,0,0));\n#3=CARTESIAN_POINT('',(0,1,0));\nENDSEC;\nEND-ISO-10303-21;\n";
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) std::abort();
        file.close();
        TestLoader loader(path);
        int errors = 0;
        QObject::connect(&loader, &Loader::error_bad_stl, [&] { ++errors; });
        std::unique_ptr<Mesh> mesh(loader.load_step());
        const bool expected = coordinate == "0";
        ++checks;
        if (expected ? !mesh || errors : mesh || errors != 1) {
            ++failures; std::cerr << "FAIL: STEP coordinate output validation\n";
        }
    }
    auto check = [&](const QByteArray& bytes, bool zip, bool expected, const char* name, int expectedTriangles = 1, quint32 declaredXmlSize = 0) {
        const QString path = dir.filePath(zip ? "model.3mf" : "model.stl");
        if (zip) {
            QZipWriter writer(path);
            writer.addFile("3D/3dmodel.model", bytes);
            writer.close();
            if (writer.status() != QZipWriter::NoError) std::abort();
            if (declaredXmlSize) {
                QFile zipFile(path);
                if (!zipFile.open(QIODevice::ReadWrite)) std::abort();
                const QByteArray data = zipFile.readAll();
                const qsizetype central = data.indexOf(QByteArray("PK\x01\x02", 4));
                if (central < 0) std::abort();
                quint32 size = qToLittleEndian(declaredXmlSize);
                if (!zipFile.seek(central + 24) || zipFile.write(reinterpret_cast<const char*>(&size), 4) != 4) std::abort();
                // Lie consistently in both headers so this exercises the
                // decompressor's limit, rather than header disagreement.
                if (!zipFile.seek(22) || zipFile.write(reinterpret_cast<const char*>(&size), 4) != 4) std::abort();
            }
        } else {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) std::abort();
        }
        TestLoader loader(path);
        int errors = 0;
        QObject::connect(&loader, &Loader::error_bad_stl, [&] { ++errors; });
        std::unique_ptr<Mesh> mesh(zip ? loader.load_3mf() : loader.load_stl());
        ++checks;
        const bool passed = expected ? mesh && mesh->triCount() == expectedTriangles && errors == 0 : !mesh && errors == 1;
        if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    // A cancelled import stops without a result and without an error signal;
    // the window joins the worker on close instead of destroying a running QThread.
    auto checkCancelled = [&](const QByteArray& bytes, bool zip, const char* name) {
        const QString path = dir.filePath(zip ? "cancel.3mf" : "cancel.stl");
        if (zip) {
            QZipWriter writer(path);
            writer.addFile("3D/3dmodel.model", bytes);
            writer.close();
            if (writer.status() != QZipWriter::NoError) std::abort();
        } else {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) std::abort();
        }
        TestLoader loader(path);
        int emitted = 0;
        QObject::connect(&loader, &Loader::error_bad_stl, [&] { ++emitted; });
        QObject::connect(&loader, &Loader::error_missing_file, [&] { ++emitted; });
        QObject::connect(&loader, &Loader::error_empty_mesh, [&] { ++emitted; });
        QObject::connect(&loader, &Loader::got_mesh, [&](Mesh*, bool) { ++emitted; });
        loader.cancel();
        if (!loader.isCancelled()) std::abort();
        std::unique_ptr<Mesh> mesh(zip ? loader.load_3mf() : loader.load_stl());
        ++checks;
        if (mesh || emitted) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    const QByteArray ascii = "solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid test\n";
    checkCancelled(binaryStl(1), false, "cancelled binary STL stops silently");
    checkCancelled(ascii, false, "cancelled ASCII STL stops silently");
    check(ascii, false, true, "valid ASCII STL");
    for (const QByteArray& invalid : {QByteArray("vertex"), QByteArray("vertex 0"), QByteArray("vertex 0 0"), QByteArray("vertex invalid 0 0"), QByteArray("vertex 0 invalid 0"), QByteArray("vertex nan 0 0"), QByteArray("vertex 0 0 inf")}) {
        QByteArray malformed = ascii;
        malformed.replace("vertex 0 0 0", invalid);
        check(malformed, false, false, "malformed ASCII vertex");
    }
    check(binaryStl(1), false, true, "valid binary STL");
    QByteArray multipleRecords = binaryStl(1025, false);
    const QByteArray record = binaryStl(1).mid(84);
    for (int i = 0; i < 1025; ++i) multipleRecords += record;
    check(multipleRecords, false, true, "binary record buffer boundary", 1025);
    check(binaryStl(1).chopped(1), false, false, "truncated binary STL");
    check(QByteArray(82, '\0'), false, false, "truncated binary count");
    // 50 * count wraps to 50 in 32-bit arithmetic: old parser accepts 134 bytes.
    check(binaryStl(0x80000001U), false, false, "overflowed binary count");
    check(binaryStl(1, true, false), false, false, "nonfinite binary coordinates");
    const QByteArray model = "<model><resources><object><mesh><vertices><vertex x='0' y='0' z='0'/><vertex x='1' y='0' z='0'/><vertex x='0' y='1' z='0'/></vertices><triangles><triangle v1='0' v2='1' v3='2'/></triangles></mesh></object></resources></model>";
    check(model, true, true, "valid 3MF");
    checkCancelled(model, true, "cancelled 3MF stops silently");
    check(model, true, false, "declared oversized ZIP XML", 1, ImportLimits::ModelXmlBytes + 1);
    check(model, true, false, "understated ZIP XML size", 1, 1);
    check(model, true, false, "overstated ZIP XML size", 1, model.size() + 1);
    check("<!DOCTYPE model [<!ENTITY shape 'triangle'>]>" + model, true, false, "3MF DTD rejected");
    QByteArray assembly = model;
    assembly.replace("</resources>", "<object><mesh/></object></resources>");
    check(assembly, true, false, "3MF multiple objects rejected");
    assembly = model;
    assembly.replace("</model>", "<build><item objectid='1' transform='1 0 0 0 1 0 0 0 1 10 0 0'/></build></model>");
    check(assembly, true, false, "3MF unsupported transform rejected");
    assembly = model;
    assembly.replace("</object>", "<components><component objectid='2'/></components></object>");
    check(assembly, true, false, "3MF components rejected");
    {
        const QString path = dir.filePath("archive.3mf");
        auto archiveCheck = [&](QByteArray data, bool expected, const char* name) {
            QFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) std::abort();
            file.close();
            if (!file.open(QIODevice::ReadOnly)) std::abort();
            QByteArray extracted;
            ++checks;
            const bool valid = readBounded3mfModel(file, extracted);
            if (valid != expected || (expected && extracted != model)) {
                ++failures; std::cerr << "FAIL: " << name << '\n';
            }
        };
        QZipWriter writer(path);
        writer.setCompressionPolicy(QZipWriter::NeverCompress);
        writer.addFile("3d/3dmodel.model", model);
        writer.close();
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly)) std::abort();
        const QByteArray original = source.readAll();
        source.close();
        archiveCheck(original, true, "stored lowercase ZIP model");
        archiveCheck(original.chopped(1), false, "truncated ZIP end record");
        const qsizetype central = original.indexOf(QByteArray("PK\x01\x02", 4));
        const qsizetype end = original.lastIndexOf(QByteArray("PK\x05\x06", 4));
        if (central < 0 || end < 0) std::abort();
        QByteArray damaged = original;
        damaged[30 + QByteArray("3d/3dmodel.model").size()] ^= 1;
        archiveCheck(damaged, false, "ZIP CRC mismatch");
        damaged = original;
        qToLittleEndian<quint32>(0xffffffffU, damaged.data() + central + 42);
        archiveCheck(damaged, false, "ZIP invalid local offset");
        damaged = original;
        qToLittleEndian<quint16>(0xffffU, damaged.data() + central + 28);
        archiveCheck(damaged, false, "ZIP invalid central name length");
        damaged = original;
        qToLittleEndian<quint16>(1, damaged.data() + end + 4);
        archiveCheck(damaged, false, "ZIP multidisk rejected");
        QZipWriter duplicate(path);
        duplicate.addFile("3D/3dmodel.model", model);
        duplicate.addFile("3d/3dmodel.model", model);
        duplicate.close();
        if (!source.open(QIODevice::ReadOnly)) std::abort();
        damaged = source.readAll();
        source.close();
        archiveCheck(damaged, false, "ZIP ambiguous model entries rejected");
    }
    for (const QByteArray& index : {QByteArray("-1"), QByteArray("-2147483648"), QByteArray("2147483647"), QByteArray("3"), QByteArray("invalid"), QByteArray("")}) {
        QByteArray malformed = model;
        malformed.replace("v1='0'", "v1='" + index + "'");
        check(malformed, true, false, "malformed 3MF index");
    }
    for (const QByteArray& coordinate : {QByteArray("nan"), QByteArray("inf"), QByteArray("invalid"), QByteArray("")}) {
        QByteArray malformed = model;
        malformed.replace("x='0'", "x='" + coordinate + "'");
        check(malformed, true, false, "malformed 3MF coordinate");
    }
    for (const bool triangleBudget : {false, true}) {
        const QString path = dir.filePath("oversized.stl");
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) std::abort();
        const quint32 count = ImportLimits::Triangles + 1;
        const QByteArray header = binaryStl(count, false);
        if (file.write(header) != header.size() ||
            !file.resize(triangleBudget ? 84 + qint64(count) * 50 : ImportLimits::SourceBytes + 1)) std::abort();
        file.close();
        TestLoader loader(path);
        int errors = 0;
        QObject::connect(&loader, &Loader::error_bad_stl, [&] { ++errors; });
        std::unique_ptr<Mesh> mesh(loader.load_stl());
        ++checks;
        if (mesh || errors != 1) { ++failures; std::cerr << "FAIL: sparse input budget\n"; }
    }
    for (const auto& item : {qMakePair(QString("file:///tmp/part%20one.stl"), QString("/tmp/part one.stl")),
                             qMakePair(QString("content://models.provider/document/part%2Fone.stp"), QString("content://models.provider/document/part%2Fone.stp"))}) {
        ++checks;
        if (fileOpenPath(QUrl(item.first)) != item.second) { ++failures; std::cerr << "FAIL: file-open URI conversion\n"; }
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
