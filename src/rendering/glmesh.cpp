#include "rendering/glmesh.h"
#include "core/mesh.h"
#include <QOpenGLContext>

namespace {
// Reading the index buffer back (to build wireframe edges on demand) needs
// glMapBufferRange: core in OpenGL ES 3.0 and desktop OpenGL 3.0.
bool canMapBuffers()
{
    const QOpenGLContext* context = QOpenGLContext::currentContext();
    if (!context)
        return false;
    const auto version = context->format().version();
    return context->isOpenGLES() ? version.first >= 3
                                 : version >= qMakePair(3, 0) || context->hasExtension("GL_ARB_map_buffer_range");
}

// Each triangle contributes its three edges. Shared edges are drawn twice,
// which looks identical and avoids a slow deduplication pass on first use.
QVector<uint32_t> triangleEdges(const uint32_t* triangles, size_t index_count)
{
    QVector<uint32_t> edges;
    edges.reserve(qsizetype(index_count / 3 * 6));
    for (size_t i = 0; i + 2 < index_count; i += 3) {
        const uint32_t a = triangles[i], b = triangles[i + 1], c = triangles[i + 2];
        edges << a << b << b << c << c << a;
    }
    return edges;
}
} // namespace

GLMesh::GLMesh(const Mesh* const mesh)
    : vertices(QOpenGLBuffer::VertexBuffer),
      indices(QOpenGLBuffer::IndexBuffer), edge_indices(QOpenGLBuffer::IndexBuffer),
      use_indices(!mesh->indices.empty()), has_edges(false),
      edges_built(false), edge_count(0)
{
    initializeOpenGLFunctions();

    vertices.create();
    vertices.setUsagePattern(QOpenGLBuffer::StaticDraw);
    vertices.bind();
    vertices.allocate(mesh->vertices.data(),
                      mesh->vertices.size() * sizeof(float));
    vertices.release();

    if (use_indices)
    {
        indices.create();
        indices.setUsagePattern(QOpenGLBuffer::StaticDraw);
        indices.bind();
        indices.allocate(mesh->indices.data(),
                         mesh->indices.size() * sizeof(uint32_t));
        indices.release();
    }
    
    vertex_count = mesh->vertices.size() / 3;
    index_count = mesh->indices.size();

    // Wireframe edges are built on first use (most models are never shown as
    // wireframe). Without buffer mapping, build them now from the CPU copy.
    if (use_indices && !canMapBuffers())
    {
        uploadEdges(triangleEdges(mesh->indices.data(), index_count));
        edges_built = true;
    }
}

void GLMesh::uploadEdges(const QVector<uint32_t>& edges)
{
    if (edges.empty() || !edge_indices.create())
        return;
    edge_indices.setUsagePattern(QOpenGLBuffer::StaticDraw);
    if (edge_indices.bind()) {
        edge_indices.allocate(edges.data(), edges.size() * sizeof(uint32_t));
        edge_indices.release();
        edge_count = edges.size();
        has_edges = true;
    }
}

void GLMesh::buildEdges()
{
    edges_built = true;
    if (!use_indices)
    {
        // Non-indexed: vertices 3i, 3i+1, 3i+2 form triangle i.
        QVector<uint32_t> sequential(qsizetype(vertex_count - vertex_count % 3));
        for (qsizetype i = 0; i < sequential.size(); ++i)
            sequential[i] = uint32_t(i);
        uploadEdges(triangleEdges(sequential.constData(), size_t(sequential.size())));
        return;
    }
    indices.bind();
    const auto* triangles = static_cast<const uint32_t*>(
        indices.mapRange(0, int(index_count * sizeof(uint32_t)), QOpenGLBuffer::RangeRead));
    QVector<uint32_t> edges;
    if (triangles)
        edges = triangleEdges(triangles, index_count);
    else
        qWarning("Wireframe unavailable: index buffer could not be mapped");
    if (triangles)
        indices.unmap();
    indices.release();
    uploadEdges(edges);
}

void GLMesh::draw(GLint vp)
{
    vertices.bind();
    glVertexAttribPointer(vp, 3, GL_FLOAT, false, 3*sizeof(float), NULL);
    glEnableVertexAttribArray(vp);
    
    if (use_indices)
    {
        indices.bind();
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, NULL);
        indices.release();
    }
    else
    {
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
    }
    
    glDisableVertexAttribArray(vp);
    vertices.release();
}

void GLMesh::drawEdges(GLint vp)
{
    if (!edges_built)
        buildEdges();
    if (!has_edges)
        return;
        
    vertices.bind();
    glVertexAttribPointer(vp, 3, GL_FLOAT, false, 3*sizeof(float), NULL);
    glEnableVertexAttribArray(vp);
    
    edge_indices.bind();
    glDrawElements(GL_LINES, edge_count, GL_UNSIGNED_INT, NULL);
    edge_indices.release();
    
    glDisableVertexAttribArray(vp);
    vertices.release();
}
