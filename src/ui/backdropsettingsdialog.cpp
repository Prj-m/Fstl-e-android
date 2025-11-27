#include "ui/backdropsettingsdialog.h"
#include "ui/canvas.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QColorDialog>
#include <QPainter>
#include <QMouseEvent>

const QString BackdropSettingsDialog::BACKDROP_TOP_LEFT_CUSTOM = "Backdrop/topLeftCustomColor";
const QString BackdropSettingsDialog::BACKDROP_TOP_RIGHT_CUSTOM = "Backdrop/topRightCustomColor";
const QString BackdropSettingsDialog::BACKDROP_BOTTOM_LEFT_CUSTOM = "Backdrop/bottomLeftCustomColor";
const QString BackdropSettingsDialog::BACKDROP_BOTTOM_RIGHT_CUSTOM = "Backdrop/bottomRightCustomColor";
const QString BackdropSettingsDialog::SETTINGS_DIALOG_GEOMETRY = "Backdrop/settingsDialogGeometry";

namespace
{
    auto createColorPatch = [](const QColor& col)
    {
        QPixmap px(20, 20);
        px.fill(col);
        return QIcon(px);
    };
}

#ifdef Q_OS_ANDROID
class BackdropColorPlane : public QWidget
{
public:
    explicit BackdropColorPlane(BackdropSettingsDialog* dlg, QWidget* parent = nullptr)
        : QWidget(parent), dialog(dlg) {}

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        int w = width();
        int h = height();
        if (w <= 0 || h <= 0) return;

        QImage img(w, h, QImage::Format_RGB32);
        for (int y = 0; y < h; ++y) {
            double v = h > 1 ? 1.0 - double(y) / double(h - 1) : 1.0; // brightness
            for (int x = 0; x < w; ++x) {
                double hf = w > 1 ? double(x) / double(w - 1) : 0.0;  // hue
                QColor c;
                c.setHsvF(hf, 1.0, v);
                img.setPixelColor(x, y, c);
            }
        }
        p.drawImage(0, 0, img);
        p.setPen(QColor(80, 80, 80));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    void mousePressEvent(QMouseEvent* ev) override { handle(ev); }
    void mouseMoveEvent(QMouseEvent* ev) override
    {
        if (ev->buttons() & Qt::LeftButton) handle(ev);
    }

private:
    BackdropSettingsDialog* dialog;

    void handle(QMouseEvent* ev)
    {
        if (!dialog) return;
        int w = width();
        int h = height();
        if (w <= 0 || h <= 0) return;
        int x = std::max(0, std::min(ev->pos().x(), w - 1));
        int y = std::max(0, std::min(ev->pos().y(), h - 1));
        double hf = w > 1 ? double(x) / double(w - 1) : 0.0;
        double v  = h > 1 ? 1.0 - double(y) / double(h - 1) : 1.0;
        QColor c;
        c.setHsvF(hf, 1.0, v);
        dialog->applyPlaneColor(c);
    }
};
#endif

BackdropSettingsDialog::BackdropSettingsDialog(QWidget* parent, Canvas* _canvas) : QWidget(parent)
{
    canvas = _canvas;

    this->setMinimumWidth(400);

    auto* mainLayout = new QVBoxLayout(this);

    auto* title = new QLabel("Background Color Settings");
    QFont boldFont = QApplication::font();
    boldFont.setWeight(QFont::Bold);
    title->setFont(boldFont);
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    auto* mainGrid = new QGridLayout;
    mainGrid->setAlignment(Qt::AlignTop);

    auto* gridContainer = new QWidget;
    gridContainer->setLayout(mainGrid);
    mainLayout->addWidget(gridContainer, 0, Qt::AlignTop);

    mainLayout->addStretch();

    // Ok / Close button at the bottom (both desktop and Android)
    QWidget* boxButton = new QWidget;
    QHBoxLayout* boxButtonLayout = new QHBoxLayout;
    boxButton->setLayout(boxButtonLayout);
    QFrame* spacerL = new QFrame;
    spacerL->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));
    QPushButton* resetButton = new QPushButton("Reset");
    QPushButton* okButton = new QPushButton("Ok");
    boxButtonLayout->addWidget(spacerL);
    boxButtonLayout->addWidget(resetButton);
    boxButtonLayout->addWidget(okButton);
    mainLayout->addWidget(boxButton);
    resetButton->setFocusPolicy(Qt::NoFocus);
    okButton->setFocusPolicy(Qt::NoFocus);
    QObject::connect(resetButton, SIGNAL(clicked(bool)), this, SLOT(onResetButtonClicked()));
    QObject::connect(okButton, SIGNAL(clicked(bool)), this, SLOT(okButtonClicked()));

    QLabel* presetLabel = new QLabel("Preset:");
    presetLabel->setFont(boldFont);
    presetLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    mainGrid->addWidget(presetLabel, 0, 0);

    comboBackdropPresets = new QComboBox;
    comboBackdropPresets->setFocusPolicy(Qt::NoFocus);
    comboBackdropPresets->addItem("Custom Colors", 0);
    comboBackdropPresets->addItem("Standard", 1);
    comboBackdropPresets->addItem("Neutral", 2);
    comboBackdropPresets->addItem("Light Grey", 3);
    comboBackdropPresets->addItem("Blueprint", 4);
    comboBackdropPresets->addItem("Dark Studio", 5);
    comboBackdropPresets->addItem("Warm", 6);
    comboBackdropPresets->addItem("Neon Studio", 7);
    comboBackdropPresets->addItem("Sunset", 8);
    comboBackdropPresets->addItem("Cyber Tech", 9);
    comboBackdropPresets->addItem("Chocolate", 10);
    comboBackdropPresets->addItem("Extreme RGB", 11);
    comboBackdropPresets->setCurrentIndex(canvas->getBackdropPresetIndex());

    mainGrid->addWidget(comboBackdropPresets, 0, 1);
    connect(comboBackdropPresets,SIGNAL(currentIndexChanged(int)), this, SLOT(onPresetChanged(int)));

    canvas->loadBackdropFromSettings();

    const QString colorButtonStyleLeft = "QPushButton { text-align: left; padding: 8px;}";
    const QString colorButtonStyleRight = "QPushButton { text-align: left; padding: 8px;}";

    auto* colorLayoutTop = new QHBoxLayout;
    mainGrid->addLayout(colorLayoutTop, 1, 0, 1, 2); // spans 2 columns

    buttonColorTL = new QPushButton("Top Left");
    buttonColorTL->setStyleSheet(colorButtonStyleLeft);
    buttonColorTL->setIcon(createColorPatch(canvas->backdropTL));
    buttonColorTL->setFocusPolicy(Qt::NoFocus);
    connect(buttonColorTL,SIGNAL(clicked(bool)), this,SLOT(onTLColorButtonClicked()));

    buttonColorTR = new QPushButton("Top Right");
    buttonColorTR->setStyleSheet(colorButtonStyleRight);
    buttonColorTR->setIcon(createColorPatch(canvas->backdropTR));
    buttonColorTR->setFocusPolicy(Qt::NoFocus);
    connect(buttonColorTR,SIGNAL(clicked(bool)), this,SLOT(onTRColorButtonClicked()));

    colorLayoutTop->addWidget(buttonColorTL);
    colorLayoutTop->addWidget(buttonColorTR);

    auto* colorLayoutBottom = new QHBoxLayout;
    mainGrid->addLayout(colorLayoutBottom, 2, 0, 1, 2);

    buttonColorBL = new QPushButton("Bottom Left");
    buttonColorBL->setStyleSheet(colorButtonStyleLeft);
    buttonColorBL->setIcon(createColorPatch(canvas->backdropBL));
    buttonColorBL->setFocusPolicy(Qt::NoFocus);
    connect(buttonColorBL,SIGNAL(clicked(bool)), this,SLOT(onBLColorButtonClicked()));

    buttonColorBR = new QPushButton("Bottom Right");
    buttonColorBR->setStyleSheet(colorButtonStyleRight);
    buttonColorBR->setIcon(createColorPatch(canvas->backdropBR));
    buttonColorBR->setFocusPolicy(Qt::NoFocus);
    connect(buttonColorBR,SIGNAL(clicked(bool)), this,SLOT(onBRColorButtonClicked()));

    colorLayoutBottom->addWidget(buttonColorBL);
    colorLayoutBottom->addWidget(buttonColorBR);

#ifdef Q_OS_ANDROID
    // Android: inline color plane used when a corner button is tapped
    planeTarget = CornerTL;
    colorPlane = new BackdropColorPlane(this, gridContainer);
    colorPlane->setMinimumHeight(90);
    colorPlane->setVisible(false);
    mainGrid->addWidget(colorPlane, 3, 0, 1, 2);
#endif

    const QSettings settings;
    if (!settings.value(SETTINGS_DIALOG_GEOMETRY).isNull()) {
        restoreGeometry(settings.value(SETTINGS_DIALOG_GEOMETRY).toByteArray());
    }
}

void BackdropSettingsDialog::onPresetChanged(const int index)
{
    const int presetId = comboBackdropPresets->itemData(index).toInt();
    canvas->setBackdropPresetIndex(presetId);

    switch (presetId)
    {
    case 0:
        restoreCustomBackdropCorners();
        break;
    case 1:
        canvas->setBackdropCorners(
            canvas->tlStandardBackdrop, canvas->trStandardBackdrop,
            canvas->blStandardBackdrop, canvas->brStandardBackdrop
        );
        break;
    case 2:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.80f, 0.83f, 0.86f),
            QColor::fromRgbF(0.72f, 0.75f, 0.78f),
            QColor::fromRgbF(0.18f, 0.19f, 0.22f),
            QColor::fromRgbF(0.26f, 0.28f, 0.31f)
        );
        break;
    case 3:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.90f, 0.90f, 0.92f),
            QColor::fromRgbF(0.88f, 0.88f, 0.90f),
            QColor::fromRgbF(0.80f, 0.80f, 0.82f),
            QColor::fromRgbF(0.78f, 0.78f, 0.80f)
        );
        break;
    case 4:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.12f, 0.22f, 0.45f),
            QColor::fromRgbF(0.10f, 0.18f, 0.40f),
            QColor::fromRgbF(0.05f, 0.10f, 0.25f),
            QColor::fromRgbF(0.04f, 0.08f, 0.20f)
        );
        break;
    case 5:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.15f, 0.15f, 0.18f),
            QColor::fromRgbF(0.10f, 0.10f, 0.12f),
            QColor::fromRgbF(0.02f, 0.02f, 0.03f),
            QColor::fromRgbF(0.04f, 0.04f, 0.05f)
        );
        break;
    case 6:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.90f, 0.85f, 0.78f),
            QColor::fromRgbF(0.95f, 0.90f, 0.82f),
            QColor::fromRgbF(0.60f, 0.53f, 0.45f),
            QColor::fromRgbF(0.68f, 0.60f, 0.50f)
        );
        break;
    case 7:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.22f, 0.05f, 0.32f),
            QColor::fromRgbF(0.05f, 0.32f, 0.38f),
            QColor::fromRgbF(0.10f, 0.02f, 0.18f),
            QColor::fromRgbF(0.02f, 0.20f, 0.28f)
        );
        break;
    case 8:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.95f, 0.78f, 0.82f),
            QColor::fromRgbF(0.88f, 0.85f, 0.95f),
            QColor::fromRgbF(0.98f, 0.88f, 0.70f),
            QColor::fromRgbF(0.90f, 0.78f, 0.92f)
        );
        break;
    case 9:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.05f, 0.18f, 0.32f),
            QColor::fromRgbF(0.06f, 0.30f, 0.42f),
            QColor::fromRgbF(0.02f, 0.10f, 0.20f),
            QColor::fromRgbF(0.00f, 0.22f, 0.32f)
        );
        break;
    case 10:
        canvas->setBackdropCorners(
            QColor::fromRgbF(0.28f, 0.18f, 0.12f),
            QColor::fromRgbF(0.38f, 0.25f, 0.12f),
            QColor::fromRgbF(0.12f, 0.08f, 0.05f),
            QColor::fromRgbF(0.22f, 0.15f, 0.08f)
        );
        break;
    case 11:
        canvas->setBackdropCorners(
            QColor::fromRgbF(1.0f, 0.0f, 0.0f),
            QColor::fromRgbF(0.0f, 1.0f, 0.0f),
            QColor::fromRgbF(0.0f, 0.0f, 1.0f),
            QColor::fromRgbF(1.0f, 1.0f, 0.0f)
        );
        break;
    default: ;
    }

    buttonColorTL->setIcon(createColorPatch(canvas->backdropTL));
    buttonColorTR->setIcon(createColorPatch(canvas->backdropTR));
    buttonColorBL->setIcon(createColorPatch(canvas->backdropBL));
    buttonColorBR->setIcon(createColorPatch(canvas->backdropBR));
}

static QColor pickColor(const QColor& initial, QWidget* parent)
{
    return QColorDialog::getColor(initial, parent, "Choose color", QColorDialog::DontUseNativeDialog);
}

void BackdropSettingsDialog::onTLColorButtonClicked()
{
    if (!confirmCustomColorChange()) return;
#ifdef Q_OS_ANDROID
    planeTarget = CornerTL;
    if (colorPlane) {
        colorPlane->setVisible(true);
        colorPlane->raise();
    }
#else
    const QColor newColor = pickColor(canvas->backdropTL, this);
    if (newColor.isValid() != true) return;
    canvas->setBackdropTLCorner(newColor);
    buttonColorTL->setIcon(createColorPatch(newColor));
    canvas->update();
    applyCustomPreset();
#endif
}

void BackdropSettingsDialog::onTRColorButtonClicked()
{
    if (!confirmCustomColorChange()) return;
#ifdef Q_OS_ANDROID
    planeTarget = CornerTR;
    if (colorPlane) {
        colorPlane->setVisible(true);
        colorPlane->raise();
    }
#else
    const QColor newColor = pickColor(canvas->backdropTR, this);
    if (newColor.isValid() != true) return;
    canvas->setBackdropTRCorner(newColor);
    buttonColorTR->setIcon(createColorPatch(newColor));
    canvas->update();
    applyCustomPreset();
#endif
}

void BackdropSettingsDialog::onBLColorButtonClicked()
{
    if (!confirmCustomColorChange()) return;
#ifdef Q_OS_ANDROID
    planeTarget = CornerBL;
    if (colorPlane) {
        colorPlane->setVisible(true);
        colorPlane->raise();
    }
#else
    const QColor newColor = pickColor(canvas->backdropBL, this);
    if (newColor.isValid() != true) return;
    canvas->setBackdropBLCorner(newColor);
    buttonColorBL->setIcon(createColorPatch(newColor));
    canvas->update();
    applyCustomPreset();
#endif
}

void BackdropSettingsDialog::onBRColorButtonClicked()
{
    if (!confirmCustomColorChange()) return;
#ifdef Q_OS_ANDROID
    planeTarget = CornerBR;
    if (colorPlane) {
        colorPlane->setVisible(true);
        colorPlane->raise();
    }
#else
    const QColor newColor = pickColor(canvas->backdropBR, this);
    if (newColor.isValid() != true) return;
    canvas->setBackdropBRCorner(newColor);
    buttonColorBR->setIcon(createColorPatch(newColor));
    canvas->update();
    applyCustomPreset();
#endif
}

void BackdropSettingsDialog::setCustomBackdropCorners(const QColor& tl, const QColor& tr,
                                                      const QColor& bl, const QColor& br)
{
    QSettings settings;
    settings.setValue(BACKDROP_TOP_LEFT_CUSTOM, tl);
    settings.setValue(BACKDROP_TOP_RIGHT_CUSTOM, tr);
    settings.setValue(BACKDROP_BOTTOM_LEFT_CUSTOM, bl);
    settings.setValue(BACKDROP_BOTTOM_RIGHT_CUSTOM, br);
}

void BackdropSettingsDialog::restoreCustomBackdropCorners() const
{
    const QSettings settings;
    const QColor tl = settings.value(BACKDROP_TOP_LEFT_CUSTOM, canvas->tlStandardBackdrop).value<QColor>();
    const QColor tr = settings.value(BACKDROP_TOP_RIGHT_CUSTOM, canvas->trStandardBackdrop).value<QColor>();
    const QColor bl = settings.value(BACKDROP_BOTTOM_LEFT_CUSTOM, canvas->blStandardBackdrop).value<QColor>();
    const QColor br = settings.value(BACKDROP_BOTTOM_RIGHT_CUSTOM, canvas->brStandardBackdrop).value<QColor>();
    canvas->setBackdropCorners(tl, tr, bl, br);
}

void BackdropSettingsDialog::applyCustomPreset() const
{
    setCustomBackdropCorners(canvas->backdropTL, canvas->backdropTR,
                             canvas->backdropBL, canvas->backdropBR);
    comboBackdropPresets->setCurrentIndex(0); // Custom Colors
}

#ifdef Q_OS_ANDROID
void BackdropSettingsDialog::applyPlaneColor(const QColor& c)
{
    switch (planeTarget)
    {
    case CornerTL:
        canvas->setBackdropTLCorner(c);
        buttonColorTL->setIcon(createColorPatch(c));
        break;
    case CornerTR:
        canvas->setBackdropTRCorner(c);
        buttonColorTR->setIcon(createColorPatch(c));
        break;
    case CornerBL:
        canvas->setBackdropBLCorner(c);
        buttonColorBL->setIcon(createColorPatch(c));
        break;
    case CornerBR:
        canvas->setBackdropBRCorner(c);
        buttonColorBR->setIcon(createColorPatch(c));
        break;
    }
    canvas->update();
    applyCustomPreset();
}
#endif

bool BackdropSettingsDialog::confirmCustomColorChange()
{
    // If current preset is not "Custom Colors", switching to a manual color
    // change will flip the preset back to Custom. We just do it silently.
    if (comboBackdropPresets->currentIndex() != 0) {
        comboBackdropPresets->setCurrentIndex(0);
        restoreCustomBackdropCorners();
    }
    return true;
}

void BackdropSettingsDialog::resizeEvent(QResizeEvent *event)
{
    QSettings().setValue(SETTINGS_DIALOG_GEOMETRY, saveGeometry());
    QWidget::resizeEvent(event);
}

void BackdropSettingsDialog::moveEvent(QMoveEvent *event)
{
    QSettings().setValue(SETTINGS_DIALOG_GEOMETRY, saveGeometry());
    QWidget::moveEvent(event);
}

void BackdropSettingsDialog::onResetButtonClicked()
{
    // Reset to the Standard preset (id 1) which restores default backdrop colors
    int standardIndex = -1;
    for (int i = 0; i < comboBackdropPresets->count(); ++i) {
        if (comboBackdropPresets->itemData(i).toInt() == 1) {
            standardIndex = i;
            break;
        }
    }
    if (standardIndex >= 0) {
        comboBackdropPresets->setCurrentIndex(standardIndex);
    }
}

void BackdropSettingsDialog::okButtonClicked()
{
    // Close/hide the settings panel after user confirms selection
    this->close();
}
