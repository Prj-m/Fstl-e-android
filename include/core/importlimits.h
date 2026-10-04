#pragma once
#include <QtGlobal>

namespace ImportLimits {
constexpr qint64 SourceBytes = 128 * 1024 * 1024;
constexpr qint64 ModelXmlBytes = 32 * 1024 * 1024;
constexpr quint32 Triangles = 1000000;
constexpr qsizetype Coordinates = 1000000;
}
