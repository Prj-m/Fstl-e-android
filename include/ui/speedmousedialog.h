#ifndef SPEEDMOUSEDIALOG_H
#define SPEEDMOUSEDIALOG_H

#include <QDialog>
#include <QSlider>
#include <QLabel>
#include <QVBoxLayout>
#include <QStatusBar>

class Canvas;

class SpeedMouseDialog : public QDialog
{
    Q_OBJECT
public:
    SpeedMouseDialog(QWidget* parent, Canvas* _canvas, QStatusBar* _sbar);

private:

    Canvas* canvas;
    QSlider* speedSlider;
    QStatusBar* sbar;
};

#endif // SPEEDMOUSEDIALOG_H
