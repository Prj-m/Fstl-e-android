#ifndef SHADERLIGHTPREFS_H
#define SHADERLIGHTPREFS_H

#include <QPushButton>
#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QFrame>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QGridLayout>

class Canvas;

class ShaderLightPrefs : public QWidget
{
    Q_OBJECT
public:
    ShaderLightPrefs(QWidget* parent, Canvas* _canvas);
    void toggleUseWire();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void buttonAmbientColorClicked();
    void editAmbientFactorFinished();
    void resetAmbientColorClicked();

    void buttonDirectiveColorClicked();
    void editDirectiveFactorFinished();
    void resetDirectiveColorClicked();

    void comboDirectionsChanged(int ind);
    void resetDirection();
    void radioSourceClicked(int ind);

    void checkboxUseWireFrameChanged();
    void buttonWireColorClicked();
    void resetWireColorClicked();
    void sliderWireWidthChanged();
    void resetWireWidthClicked();

    void okButtonClicked();
    void onFallbackGlslUpdated(bool b);

    void setRadio(int ind);
    void setPix(int ind);

private:
    Canvas* canvas;
    QPushButton* buttonAmbientColor;
    QLineEdit* editAmbientFactor;
    QPushButton* buttonDirectiveColor;
    QLineEdit* editDirectiveFactor;
    QComboBox* comboDirections;

    QFrame* groupWireFrame;
    QCheckBox* checkboxUseWireFrame;
    QPushButton* buttonWireColor;
    QLabel* labelWireWidth;
    QSlider* sliderWireWidth;
    QLabel* labelPix;

    QButtonGroup* leftRight;
    QButtonGroup* topBottom;
    QButtonGroup* rearFront;

    const static QString PREFS_GEOM;
};

#endif // SHADERLIGHTPREFS_H
