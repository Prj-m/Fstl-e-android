#include "core/loader.h"
#include "core/importlimits.h"
#include "core/boundedzip.h"
#include "core/fileopenpath.h"
#include "core/fastfloat.h"
#include <cstring>
#include <cstdio>
#include <random>
#include <QtEndian>
#include <QCoreApplication>
#include <QDataStream>
#include <QTemporaryDir>
#include <QPair>
#include <cmath>
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
    {
        // Token-based ASCII reader: formatting variants real exporters produce.
        QByteArray crlfTabs = ascii;
        crlfTabs.replace("\n", "\r\n").replace(' ', '\t');
        check(crlfTabs, false, true, "ASCII STL with CRLF and tabs");
        check(ascii.chopped(QByteArray("endsolid test\n").size()), false, true, "ASCII STL without endsolid");
        check(ascii.chopped(1), false, true, "ASCII STL without trailing newline");
        QByteArray blankLines = ascii;
        blankLines.replace("endfacet\n", "endfacet\n\n  \n");
        check(blankLines, false, true, "ASCII STL with blank lines");
        check(ascii + ascii, false, true, "ASCII STL with two solid blocks", 2);
        const QByteArray facet = ascii.mid(ascii.indexOf("facet"), ascii.indexOf("endsolid") - ascii.indexOf("facet"));
        QByteArray chunked = "solid big\n";
        const int facets = 12000; // > 1 MiB, crosses the reader's chunk boundary
        for (int i = 0; i < facets; ++i) chunked += facet;
        check(chunked + "endsolid big\n", false, true, "ASCII STL across read chunks", facets);
        QByteArray longToken = ascii;
        longToken.replace("vertex 1 0 0", "vertex 1" + QByteArray(300, '0') + " 0 0");
        check(longToken, false, false, "ASCII STL oversized token");
        QByteArray solidHeader = binaryStl(1);
        solidHeader.replace(0, 5, "solid");
        check(solidHeader, false, true, "binary STL whose header starts with solid");
    }
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
    check(binaryStl(1) + QByteArray(16, '\0'), false, true, "binary STL with trailing padding");
    check(binaryStl(1) + QByteArray(5000, '\0'), false, false, "binary STL with excessive trailing data");
    {
        QByteArray zeroCount = binaryStl(1);
        zeroCount.replace(80, 4, QByteArray(4, '\0'));
        check(zeroCount, false, true, "binary STL with zero triangle count");
    }
    check(QByteArray(82, '\0'), false, false, "truncated binary count");
    // 50 * count wraps to 50 in 32-bit arithmetic: old parser accepts 134 bytes.
    check(binaryStl(0x80000001U), false, false, "overflowed binary count");
    check(binaryStl(1, true, false), false, false, "nonfinite binary coordinates");
    const QByteArray model = "<model><resources><object><mesh><vertices><vertex x='0' y='0' z='0'/><vertex x='1' y='0' z='0'/><vertex x='0' y='1' z='0'/></vertices><triangles><triangle v1='0' v2='1' v3='2'/></triangles></mesh></object></resources></model>";
    check(model, true, true, "valid 3MF");
    {
        // 3MF units: coordinates are converted to millimetres.
        QByteArray inches = model;
        inches.replace("<model>", "<model unit='inch'>");
        const QString path = dir.filePath("units.3mf");
        QZipWriter writer(path);
        writer.addFile("3D/3dmodel.model", inches);
        writer.close();
        TestLoader loader(path);
        std::unique_ptr<Mesh> mesh(loader.load_3mf());
        ++checks;
        if (!mesh || std::abs(mesh->xmax() - 25.4f) > 1e-4f) { ++failures; std::cerr << "FAIL: 3MF inch units\n"; }
    }
    checkCancelled(model, true, "cancelled 3MF stops silently");
    check(model, true, false, "declared oversized ZIP XML", 1, ImportLimits::ModelXmlBytes + 1);
    check(model, true, false, "understated ZIP XML size", 1, 1);
    check(model, true, false, "overstated ZIP XML size", 1, model.size() + 1);
    check("<!DOCTYPE model [<!ENTITY shape 'triangle'>]>" + model, true, false, "3MF DTD rejected");
    // Slicer-style assemblies: multiple objects, components, transforms and
    // meshes stored in separate model parts (3MF production extension).
    auto checkAssembly = [&](const QList<QPair<QByteArray, QByteArray>>& files, bool expected,
                             const char* name, int triangles = 0, float xmin = 0, float xmax = 0) {
        const QString path = dir.filePath("assembly.3mf");
        QFile::remove(path);
        QZipWriter writer(path);
        for (const auto& f : files) writer.addFile(QString::fromUtf8(f.first), f.second);
        writer.close();
        if (writer.status() != QZipWriter::NoError) std::abort();
        TestLoader loader(path);
        int errors = 0;
        QObject::connect(&loader, &Loader::error_bad_stl, [&] { ++errors; });
        QObject::connect(&loader, &Loader::error_empty_mesh, [&] { ++errors; });
        std::unique_ptr<Mesh> mesh(loader.load_3mf());
        ++checks;
        bool passed = expected ? mesh && errors == 0 && mesh->triCount() == triangles
                                 && std::abs(mesh->xmin() - xmin) < 1e-3f && std::abs(mesh->xmax() - xmax) < 1e-3f
                               : !mesh && errors == 1;
        if (!passed) { ++failures; std::cerr << "FAIL: " << name << '\n'; }
    };
    const QByteArray tri = "<mesh><vertices><vertex x='0' y='0' z='0'/><vertex x='1' y='0' z='0'/><vertex x='0' y='1' z='0'/></vertices><triangles><triangle v1='0' v2='1' v3='2'/></triangles></mesh>";
    auto rootModel = [&](const QByteArray& resources, const QByteArray& build) {
        return "<model xmlns='http://schemas.microsoft.com/3dmanufacturing/core/2015/02' xmlns:p='http://schemas.microsoft.com/3dmanufacturing/production/2015/06'><resources>"
               + resources + "</resources><build>" + build + "</build></model>";
    };
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'>" + tri + "</object>",
        "<item objectid='1' transform='1 0 0 0 1 0 0 0 1 10 0 0'/>")}},
        true, "3MF build item transform applied", 1, 10, 11);
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'>" + tri + "</object><object id='2'><components><component objectid='1' transform='2 0 0 0 1 0 0 0 1 0 0 0'/></components></object>",
        "<item objectid='2' transform='1 0 0 0 1 0 0 0 1 100 0 0'/>")}},
        true, "3MF slicer component with nested transforms", 1, 100, 102);
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'>" + tri + "</object><object id='2'>" + tri + "</object>",
        "<item objectid='1'/><item objectid='2' transform='1 0 0 0 1 0 0 0 1 5 0 0'/>")}},
        true, "3MF multiple build items", 2, 0, 6);
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='2'><components><component p:path='/3D/Objects/part.model' objectid='1'/></components></object>",
        "<item objectid='2'/>")}, {"3D/Objects/part.model", "<model><resources><object id='1'>" + tri + "</object></resources></model>"}},
        true, "3MF production-extension part referenced by path", 1, 0, 1);
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1' type='support'>" + tri + "</object><object id='2'>" + tri + "</object>",
        "<item objectid='1'/><item objectid='2'/>")}},
        true, "3MF support objects not drawn", 1, 0, 1);
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'><components><component objectid='2'/></components></object><object id='2'><components><component objectid='1'/></components></object>",
        "<item objectid='1'/>")}}, false, "3MF component cycle rejected");
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'>" + tri + "</object>", "<item objectid='9'/>")}},
        false, "3MF missing object reference rejected");
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'>" + tri + "</object>", "<item objectid='1' transform='1 0 0 0 1 0 0 0 1 10 0'/>")}},
        false, "3MF malformed transform rejected");
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'>" + tri + "</object>", "<item objectid='1' transform='1 0 0 0 1 0 0 0 1 nan 0 0'/>")}},
        false, "3MF nonfinite transform rejected");
    checkAssembly({{"3D/3dmodel.model", rootModel("<object id='1'>" + tri + "</object><object id='1'>" + tri + "</object>", "<item objectid='1'/>")}},
        false, "3MF duplicate object id rejected");
    {
        // 18 levels deep exceeds the nesting limit of 16.
        QByteArray chain = "<object id='0'>" + tri + "</object>";
        for (int i = 1; i <= 18; ++i)
            chain += "<object id='" + QByteArray::number(i) + "'><components><component objectid='" + QByteArray::number(i - 1) + "'/></components></object>";
        checkAssembly({{"3D/3dmodel.model", rootModel(chain, "<item objectid='18'/>")}}, false, "3MF excessive component depth rejected");
    }
    {
        // Each level references the previous one ten times: 10^6 instances exceed the instance budget.
        QByteArray fan = "<object id='0'>" + tri + "</object>";
        for (int i = 1; i <= 6; ++i) {
            fan += "<object id='" + QByteArray::number(i) + "'><components>";
            for (int k = 0; k < 10; ++k) fan += "<component objectid='" + QByteArray::number(i - 1) + "'/>";
            fan += "</components></object>";
        }
        checkAssembly({{"3D/3dmodel.model", rootModel(fan, "<item objectid='6'/>")}}, false, "3MF instance explosion rejected");
    }
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
    {
        // The fast decimal path must accept/reject exactly like Qt and agree
        // with it to within one float ulp (double-then-float rounding).
        auto ulps = [](float a, float b) {
            int32_t ia, ib;
            std::memcpy(&ia, &a, 4);
            std::memcpy(&ib, &b, 4);
            if ((ia < 0) != (ib < 0)) return (a == b) ? 0LL : 1LL << 40;
            return std::llabs(qint64(ia) - qint64(ib));
        };
        std::vector<QByteArray> inputs = {"0", "-0", "1.", ".5", "+3", "1e", "e5", "-", ".", "", "1e-50", "1e50",
            "nan", "inf", "-inf", "1.5e+38", "3.5e38", "123456789012345678", " 1", "1 ", "0x10", "1e+", "--1",
            "0.000000000000000000000000001", "9999999999999999", "1.234560e+01", "-5.000000e-01", "007", "1e400"};
        std::mt19937 random(42);
        std::uniform_real_distribution<double> mantissa(-1.0, 1.0);
        std::uniform_int_distribution<int> exponent(-12, 12);
        for (int i = 0; i < 20000; ++i) {
            const double v = mantissa(random) * std::pow(10.0, exponent(random));
            for (const char* format : {"%e", "%.6f", "%g", "%.9g", "%.3f"})
            {
                char text[64];
                std::snprintf(text, sizeof(text), format, v);
                inputs.push_back(QByteArray(text));
            }
        }
        int mismatches = 0;
        for (const QByteArray& input : inputs) {
            bool qtOk = false, fastOk = false;
            const float qtValue = QByteArrayView(input).toFloat(&qtOk);
            const float fastValue = FastFloat::toFloat(QByteArrayView(input), &fastOk);
            bool utf16Ok = false;
            const QString wide = QString::fromLatin1(input);
            const float utf16Value = FastFloat::toFloat(QStringView(wide), &utf16Ok);
            const bool same = qtOk == fastOk && fastOk == utf16Ok
                && (!qtOk || ((std::isnan(qtValue) && std::isnan(fastValue)) ||
                              (ulps(qtValue, fastValue) <= 1 && fastValue == utf16Value)));
            if (!same && ++mismatches <= 5)
                std::cerr << "FastFloat mismatch for '" << input.constData() << "': qt " << qtOk << ' ' << qtValue
                          << " fast " << fastOk << ' ' << fastValue << '\n';
        }
        ++checks;
        if (mismatches) { ++failures; std::cerr << "FAIL: fast float parsing (" << mismatches << " mismatches)\n"; }
    }
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
