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

    enum ColorPlaneTarget { PlaneTargetAmbient, PlaneTargetDirective };

    // Called by the inline color plane to apply the picked color
    void applyColorFromPlane(const QColor& c);

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

    // Inline color plane (Android) and its current target
    AmbientColorPlane* ambientPlane;
    ColorPlaneTarget planeTarget;

#ifdef Q_OS_ANDROID
    QCheckBox* checkboxShowLayerButton;
#endif

    QButtonGroup* leftRight;
    QButtonGroup* topBottom;
    QButtonGroup* rearFront;

    const static QString PREFS_GEOM;

    friend class AmbientColorPlane;
};

#endif // SHADERLIGHTPREFS_H
