#ifndef GLMESH_H
#define GLMESH_H

#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QVector>

// forward declaration
class Mesh;

class GLMesh : protected QOpenGLFunctions
{
public:
    GLMesh(const Mesh* const mesh);
    void draw(GLint vp);
    void drawEdges(GLint vp);
private:
    void uploadEdges(const QVector<uint32_t>& edges);
    void buildEdges();

	QOpenGLBuffer vertices;
	QOpenGLBuffer indices;
	QOpenGLBuffer edge_indices;
	bool use_indices;
	bool has_edges;
	bool edges_built;
	size_t vertex_count;
	size_t index_count;
	size_t edge_count;
};

#endif // GLMESH_H
