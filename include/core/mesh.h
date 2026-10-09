#ifndef MESH_H
#define MESH_H

#include <QString>
#include <QtOpenGL/QtOpenGL>

#include <vector>

class Mesh
{
public:
    Mesh(std::vector<GLfloat>&& vertices, std::vector<GLuint>&& indices);

    // Bounds are computed once in the constructor, on the loader thread.
    float min(size_t start) const { return start < 3 ? lower[start] : -1; }
    float max(size_t start) const { return start < 3 ? upper[start] : 1; }

    float xmin() const { return min(0); }
    float ymin() const { return min(1); }
    float zmin() const { return min(2); }
    float xmax() const { return max(0); }
    float ymax() const { return max(1); }
    float zmax() const { return max(2); }

    int triCount() const;
    bool empty() const;

private:
    std::vector<GLfloat> vertices;
    std::vector<GLuint> indices;
    float lower[3] = {-1, -1, -1};
    float upper[3] = {1, 1, 1};

    void computeBounds();

    friend class GLMesh;
};

#endif // MESH_H
