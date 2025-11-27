#include "rendering/backdrop.h"
#include <QVector3D>

Backdrop::Backdrop()
{
    initializeOpenGLFunctions();

    shader.addShaderFromSourceFile(QOpenGLShader::Vertex,   ":/gl/shaders/quad.vert");
    shader.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/gl/shaders/quad.frag");
    shader.link();

    // Fullscreen quad with UVs for 4-corner gradient
    // BL (-1,-1) -> (0,0), TL (-1,1) -> (0,1), BR (1,-1) -> (1,0), TR (1,1) -> (1,1)
    constexpr float vbuf[] = {
        -1.f, -1.f, 0.f, 0.f, // BL
        -1.f,  1.f, 0.f, 1.f, // TL
         1.f, -1.f, 1.f, 0.f, // BR
         1.f,  1.f, 1.f, 1.f  // TR
    };

    vertices.create();
    vertices.bind();
    vertices.allocate(vbuf, sizeof(vbuf));
    vertices.release();

    // Default to a subtle teal gradient similar to the classic fstl background
    tl = QColor::fromRgbF(0.03137255f, 0.20784314f, 0.25882353f);
    tr = QColor::fromRgbF(0.05882353f, 0.25882353f, 0.29803922f);
    bl = QColor::fromRgbF(0.00000000f, 0.10196078f, 0.15294118f);
    br = QColor::fromRgbF(0.00000000f, 0.12156863f, 0.18039216f);
}

void Backdrop::setColors(const QColor& topLeft,
                         const QColor& topRight,
                         const QColor& bottomLeft,
                         const QColor& bottomRight)
{
    tl = topLeft;
    tr = topRight;
    bl = bottomLeft;
    br = bottomRight;
}

void Backdrop::setTopLeft(const QColor& color)
{
    tl = color;
}

void Backdrop::setTopRight(const QColor& color)
{
    tr = color;
}

void Backdrop::setBottomLeft(const QColor& color)
{
    bl = color;
}

void Backdrop::setBottomRight(const QColor& color)
{
    br = color;
}

void Backdrop::draw()
{
    shader.bind();
    vertices.bind();

    const GLint vp = shader.attributeLocation("vertex_position");
    const GLint vt = shader.attributeLocation("vertex_uv");

    glEnableVertexAttribArray(vp);
    glEnableVertexAttribArray(vt);

    glVertexAttribPointer(vp, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), nullptr);
    glVertexAttribPointer(vt, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat),
                          reinterpret_cast<GLvoid*>(2 * sizeof(GLfloat)));

    auto toVec3 = [](const QColor& c) {
        return QVector3D{
            static_cast<float>(c.redF()),
            static_cast<float>(c.greenF()),
            static_cast<float>(c.blueF())
        };
    };

    shader.setUniformValue("colorTL", toVec3(tl));
    shader.setUniformValue("colorTR", toVec3(tr));
    shader.setUniformValue("colorBL", toVec3(bl));
    shader.setUniformValue("colorBR", toVec3(br));

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    vertices.release();
    shader.release();
}
