#include <QFile>
#include <QDataStream>
#include <QVector3D>

#include <cmath>

#include "core/mesh.h"

////////////////////////////////////////////////////////////////////////////////

Mesh::Mesh(std::vector<GLfloat>&& v, std::vector<GLuint>&& i)
    : vertices(std::move(v)), indices(std::move(i))
{
    computeBounds();
}


void Mesh::computeBounds()
{
    if (vertices.size() < 3)
        return;
    for (int axis = 0; axis < 3; ++axis)
        lower[axis] = upper[axis] = vertices[axis];
    // One pass on the loader thread instead of six on the UI thread.
    for (size_t i = 0; i + 2 < vertices.size(); i += 3)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            const float value = vertices[i + axis];
            lower[axis] = std::fmin(lower[axis], value);
            upper[axis] = std::fmax(upper[axis], value);
        }
    }
}

int Mesh::triCount() const
{
    // Non-indexed meshes store 3 vertices per triangle
    if (indices.empty()) {
        return vertices.size() / 9;  // 9 floats per triangle (3 vertices * 3 coords)
    }
    return indices.size() / 3;
}
bool Mesh::empty() const
{
    return vertices.size() == 0;
}
