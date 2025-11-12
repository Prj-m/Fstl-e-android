#include "glmesh.h"
#include "mesh.h"
#include <QSet>
#include <QPair>

GLMesh::GLMesh(const Mesh* const mesh)
    : vertices(QOpenGLBuffer::VertexBuffer), normals(QOpenGLBuffer::VertexBuffer),
      indices(QOpenGLBuffer::IndexBuffer), edge_indices(QOpenGLBuffer::IndexBuffer),
      use_indices(!mesh->indices.empty()), has_normals(!mesh->normals.empty()), has_edges(false)
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
    
    // Generate edge indices for wireframe mode
    // Each triangle has 3 edges, we need to create unique edges
    QVector<uint32_t> edges;
    if (use_indices) {
        // Use a set to track unique edges
        QSet<QPair<uint32_t, uint32_t>> unique_edges;
        
        // Iterate through triangles
        for (size_t i = 0; i < mesh->indices.size(); i += 3) {
            uint32_t i0 = mesh->indices[i];
            uint32_t i1 = mesh->indices[i + 1];
            uint32_t i2 = mesh->indices[i + 2];
            
            // Add three edges of the triangle (order vertices to avoid duplicates)
            auto edge1 = qMakePair(qMin(i0, i1), qMax(i0, i1));
            auto edge2 = qMakePair(qMin(i1, i2), qMax(i1, i2));
            auto edge3 = qMakePair(qMin(i2, i0), qMax(i2, i0));
            
            unique_edges.insert(edge1);
            unique_edges.insert(edge2);
            unique_edges.insert(edge3);
        }
        
        // Convert set to vector of indices
        for (const auto& edge : unique_edges) {
            edges.push_back(edge.first);
            edges.push_back(edge.second);
        }
    } else {
        // For non-indexed meshes, create edges from sequential triangles
        for (size_t i = 0; i < vertex_count; i += 3) {
            // Triangle edges: 0-1, 1-2, 2-0
            edges.push_back(i);
            edges.push_back(i + 1);
            edges.push_back(i + 1);
            edges.push_back(i + 2);
            edges.push_back(i + 2);
            edges.push_back(i);
        }
    }
    
    if (!edges.empty()) {
        if (edge_indices.create()) {
            edge_indices.setUsagePattern(QOpenGLBuffer::StaticDraw);
            if (edge_indices.bind()) {
                edge_indices.allocate(edges.data(), edges.size() * sizeof(uint32_t));
                edge_indices.release();
                edge_count = edges.size();
                has_edges = true;
            }
        }
    }
}

void GLMesh::draw(GLuint vp, GLuint np)
{
    vertices.bind();
    glVertexAttribPointer(vp, 3, GL_FLOAT, false, 3*sizeof(float), NULL);
    glEnableVertexAttribArray(vp);
    
    if (has_normals && np != (GLuint)-1)
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
    
    if (has_normals && np != (GLuint)-1)
    {
        glDisableVertexAttribArray(np);
    }
    glDisableVertexAttribArray(vp);
    vertices.release();
}

void GLMesh::drawEdges(GLuint vp)
{
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
