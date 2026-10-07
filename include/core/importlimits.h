#pragma once
#include <QtGlobal>

namespace ImportLimits {
constexpr qint64 SourceBytes = 128 * 1024 * 1024;
constexpr qint64 ModelXmlBytes = 32 * 1024 * 1024;
constexpr quint32 Triangles = 1000000;
constexpr qsizetype Coordinates = 1000000;
// 3MF assemblies: model parts per archive, component nesting depth and the
// number of object instances expanded from build items and components.
constexpr int ModelParts = 256;
constexpr int ComponentDepth = 16;
constexpr qsizetype Instances = 100000;
}
