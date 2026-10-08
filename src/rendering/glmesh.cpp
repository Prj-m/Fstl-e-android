#include "rendering/glmesh.h"
#include "core/mesh.h"
#include <QSet>
#include <QPair>
#include <climits>


GLMesh::GLMesh(const Mesh* const mesh)
    : vertices(QOpenGLBuffer::VertexBuffer), normals(QOpenGLBuffer::VertexBuffer),
      indices(QOpenGLBuffer::IndexBuffer), edge_indices(QOpenGLBuffer::IndexBuffer),
      use_indices(!mesh->indices.empty()), has_normals(!mesh->normals.empty()), has_edges(false),
      edges_built(false), edge_count(0)
{
    initializeOpenGLFunctions();

    vertices.create();
    vertices.setUsagePattern(QOpenGLBuffer::StaticDraw);
    vertices.bind();
    vertices.allocate(mesh->vertices.data(),
                      mesh->vertices.size() * sizeof(float));
    vertices.release();

    if (has_normals)
    {
        normals.create();
        normals.setUsagePattern(QOpenGLBuffer::StaticDraw);
        normals.bind();
        normals.allocate(mesh->normals.data(),
                         mesh->normals.size() * sizeof(float));
        normals.release();
    }

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
    
    // Indexed meshes need the CPU index list to find unique edges, so build them
    // now. Non-indexed (Android) edges depend only on vertex_count and are built
    // on first wireframe draw, saving memory for models never shown as wireframe.
    if (use_indices) {
        QSet<QPair<uint32_t, uint32_t>> unique_edges;
        unique_edges.reserve(int(qMin<size_t>(index_count, INT_MAX)));
        for (size_t i = 0; i + 2 < mesh->indices.size(); i += 3) {
            uint32_t i0 = mesh->indices[i];
            uint32_t i1 = mesh->indices[i + 1];
            uint32_t i2 = mesh->indices[i + 2];
            // Order vertices so shared edges are stored once
            unique_edges.insert(qMakePair(qMin(i0, i1), qMax(i0, i1)));
            unique_edges.insert(qMakePair(qMin(i1, i2), qMax(i1, i2)));
            unique_edges.insert(qMakePair(qMin(i2, i0), qMax(i2, i0)));
        }
        QVector<uint32_t> edges;
        edges.reserve(unique_edges.size() * 2);
        for (const auto& edge : unique_edges) {
            edges.push_back(edge.first);
            edges.push_back(edge.second);
        }
        uploadEdges(edges);
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

void GLMesh::buildSequentialEdges()
{
    edges_built = true;
    // Triangle edges: 0-1, 1-2, 2-0
    QVector<uint32_t> edges;
    edges.reserve(vertex_count / 3 * 6);
    for (uint32_t i = 0; i + 2 < vertex_count; i += 3) {
        edges.push_back(i);
        edges.push_back(i + 1);
        edges.push_back(i + 1);
        edges.push_back(i + 2);
        edges.push_back(i + 2);
        edges.push_back(i);
    }
    uploadEdges(edges);
}

void GLMesh::draw(GLint vp, GLint np)
{
    vertices.bind();
    glVertexAttribPointer(vp, 3, GL_FLOAT, false, 3*sizeof(float), NULL);
    glEnableVertexAttribArray(vp);
    
    if (has_normals && np >= 0)
    {
        normals.bind();
        glVertexAttribPointer(np, 3, GL_FLOAT, false, 3*sizeof(float), NULL);
        glEnableVertexAttribArray(np);
        normals.release();
    }
    
    if (use_indices)
    {
        indices.bind();
        glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, NULL);
        indices.release();
    }
    else
    {
        // Non-indexed rendering for flat shading
        glDrawArrays(GL_TRIANGLES, 0, vertex_count);
    }
    
    if (has_normals && np >= 0)
    {
        glDisableVertexAttribArray(np);
    }
    glDisableVertexAttribArray(vp);
    vertices.release();
}

void GLMesh::drawEdges(GLint vp)
{
    if (!use_indices && !edges_built)
        buildSequentialEdges();
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
