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
#include <QColor>

class Canvas;
class AmbientColorPlane;

class ShaderLightPrefs : public QWidget
{
    Q_OBJECT
public:
    ShaderLightPrefs(QWidget* parent, Canvas* _canvas);
    void toggleUseWire();

    // Helper used by AmbientColorPlane
    void applyAmbientFromPlane(const QColor& c);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void buttonAmbientColorClicked();
    void editAmbientFactorFinished();
    void sliderAmbientFactorChanged(int v);
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
    QSlider* sliderAmbientFactor;
    QPushButton* buttonDirectiveColor;
    QLineEdit* editDirectiveFactor;
    QComboBox* comboDirections;

    QFrame* groupWireFrame;
    QCheckBox* checkboxUseWireFrame;
    QPushButton* buttonWireColor;
    QLabel* labelWireWidth;
    QSlider* sliderWireWidth;
    QLabel* labelPix;
    QCheckBox* checkboxShowLayerButton;
    AmbientColorPlane* ambientPlane;

    QButtonGroup* leftRight;
    QButtonGroup* topBottom;
    QButtonGroup* rearFront;

    const static QString PREFS_GEOM;

    friend class AmbientColorPlane;
};

#endif // SHADERLIGHTPREFS_H
