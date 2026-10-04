#include "core/loader.h"
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
    auto check = [&](const QByteArray& bytes, bool zip, bool expected, const char* name, int expectedTriangles = 1) {
        const QString path = dir.filePath(zip ? "model.3mf" : "model.stl");
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
        int errors = 0;
        QObject::connect(&loader, &Loader::error_bad_stl, [&] { ++errors; });
        std::unique_ptr<Mesh> mesh(zip ? loader.load_3mf() : loader.load_stl());
        ++checks;
        const bool passed = expected ? mesh && mesh->triCount() == expectedTriangles && errors == 0 : !mesh && errors == 1;
        if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    const QByteArray ascii = "solid test\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 1 0 0\nvertex 0 1 0\nendloop\nendfacet\nendsolid test\n";
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
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
