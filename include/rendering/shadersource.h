#ifndef SHADERSOURCE_H
#define SHADERSOURCE_H

#include <QByteArray>
#include <QFile>
#include <QOpenGLContext>
#include <QString>

// The shaders are written in GLSL ES. On desktop OpenGL, Qt defines highp,
// mediump and lowp as empty macros, which turns "precision highp float;" into
// a syntax error, so no program linked and nothing rendered. Desktop builds
// get the matching desktop version line and no precision statements; OpenGL
// ES sources are passed through unchanged.
inline QByteArray shaderSource(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QByteArray source = file.readAll();
    const QOpenGLContext* context = QOpenGLContext::currentContext();
    if (!context || context->isOpenGLES())
        return source;

    QByteArray desktop;
    desktop.reserve(source.size());
    for (const QByteArray& line : source.split('\n')) {
        const QByteArray trimmed = line.trimmed();
        if (trimmed.startsWith("precision "))
            continue;
        if (trimmed == "#version 300 es")
            desktop += "#version 330 core";
        else if (trimmed == "#version 100")
            desktop += "#version 120";
        else
            desktop += line;
        desktop += '\n';
    }
    return desktop;
}

#endif // SHADERSOURCE_H
