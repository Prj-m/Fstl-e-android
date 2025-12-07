#ifndef BACKDROPSETTINGSDIALOG_H
#define BACKDROPSETTINGSDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QPushButton>
#include <QSettings>
#include <QStringList>
#include <QListWidget>

class Canvas;
#ifdef Q_OS_ANDROID
class BackdropColorPlane;
#endif

class BackdropSettingsDialog final : public QWidget
{
    Q_OBJECT

public:
    BackdropSettingsDialog(QWidget* parent, Canvas* _canvas);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;

private slots:
    void onPresetChanged(int index);
    void onTLColorButtonClicked();
    void onTRColorButtonClicked();
    void onBLColorButtonClicked();
    void onBRColorButtonClicked();
    void onResetButtonClicked();
    void okButtonClicked();
#ifdef Q_OS_ANDROID
    void onPresetButtonClicked();
    void onPresetItemClicked(QListWidgetItem* item);
#endif

public:
#ifdef Q_OS_ANDROID
    void applyPlaneColor(const QColor& c);
#endif

private:
    static void setCustomBackdropCorners(const QColor& tl, const QColor& tr,
                                         const QColor& bl, const QColor& br);
    void restoreCustomBackdropCorners() const;
    void applyCustomPreset() const;
    bool confirmCustomColorChange();

private:
    Canvas* canvas;
    QPushButton* buttonColorTL;
    QPushButton* buttonColorTR;
    QPushButton* buttonColorBL;
    QPushButton* buttonColorBR;

#ifdef Q_OS_ANDROID
    enum PlaneTargetCorner { CornerTL, CornerTR, CornerBL, CornerBR };
    BackdropColorPlane* colorPlane;
    PlaneTargetCorner planeTarget;
    QStringList presetNames;
    int currentPresetIndex;
    QWidget* presetContainer;
    QPushButton* presetButton;
    QListWidget* presetListWidget;
#else
    QComboBox* comboBackdropPresets;
#endif

    const static QString BACKDROP_TOP_LEFT_CUSTOM;
    const static QString BACKDROP_TOP_RIGHT_CUSTOM;
    const static QString BACKDROP_BOTTOM_LEFT_CUSTOM;
    const static QString BACKDROP_BOTTOM_RIGHT_CUSTOM;
    const static QString SETTINGS_DIALOG_GEOMETRY;
};

#endif // BACKDROPSETTINGSDIALOG_H
