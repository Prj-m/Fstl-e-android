#ifndef HSVPLANE_H
#define HSVPLANE_H

#include <QColor>
#include <QImage>
#include <QSize>

// Hue (x) by brightness (y) gradient shared by the colour-picker planes.
// Regenerated only when the widget size changes rather than on every repaint
// (the planes repaint whenever the GL canvas behind them updates).
inline const QImage& hsvPlaneImage(const QSize& size, QImage& cache)
{
    if (cache.size() == size)
        return cache;
    const int w = size.width();
    const int h = size.height();
    cache = QImage(size, QImage::Format_RGB32);
    for (int y = 0; y < h; ++y) {
        const double v = h > 1 ? 1.0 - double(y) / double(h - 1) : 1.0; // brightness
        QRgb* line = reinterpret_cast<QRgb*>(cache.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const double hf = w > 1 ? double(x) / double(w - 1) : 0.0;  // hue
            line[x] = QColor::fromHsvF(float(hf), 1.0f, float(v)).rgb();
        }
    }
    return cache;
}

#endif // HSVPLANE_H
