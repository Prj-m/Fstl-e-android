#ifndef GLMESH_H
#define GLMESH_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>

// forward declaration
class Mesh;

class GLMesh : protected QOpenGLFunctions
{
public:
    GLMesh(const Mesh* const mesh);
    void draw(GLuint vp, GLuint np);
    void drawEdges(GLuint vp);
private:
	QOpenGLBuffer vertices;
	QOpenGLBuffer normals;
	QOpenGLBuffer indices;
	QOpenGLBuffer edge_indices;
	bool use_indices;
	bool has_normals;
	bool has_edges;
	size_t vertex_count;
	size_t index_count;
	size_t edge_count;
};

#endif // GLMESH_H
