#include "ui/shaderlightprefs.h"
#include "ui/canvas.h"
#include "ui/window.h"
#include <QApplication>
#include <QRadioButton>
#include <QColorDialog>
#include <QPainter>
#include <QMouseEvent>
#include <algorithm>

class AmbientColorPlane : public QWidget
{
public:
    explicit AmbientColorPlane(ShaderLightPrefs* prefs, QWidget* parent = nullptr)
        : QWidget(parent), prefs(prefs) {}

protected:
    void paintEvent(QPaintEvent*) override {
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
    void mouseMoveEvent(QMouseEvent* ev) override {
        if (ev->buttons() & Qt::LeftButton) handle(ev);
    }

private:
    ShaderLightPrefs* prefs;

    void handle(QMouseEvent* ev) {
        if (!prefs) return;
        int w = width();
        int h = height();
        if (w <= 0 || h <= 0) return;
        int x = std::max(0, std::min(ev->pos().x(), w - 1));
        int y = std::max(0, std::min(ev->pos().y(), h - 1));
        double hf = w > 1 ? double(x) / double(w - 1) : 0.0;
        double v  = h > 1 ? 1.0 - double(y) / double(h - 1) : 1.0;
        QColor c;
        c.setHsvF(hf, 1.0, v);
        prefs->applyAmbientFromPlane(c);
    }
};

const QString ShaderLightPrefs::PREFS_GEOM = "shaderPrefsGeometry";

ShaderLightPrefs::ShaderLightPrefs(QWidget *parent, Canvas *_canvas) : QWidget(parent)
{
    canvas = _canvas;

#ifdef Q_OS_ANDROID
    // Style the widget to appear as a solid, non-transparent panel on Android
    this->setAutoFillBackground(true);
    QPalette p = this->palette();
    p.setColor(QPalette::Window, QColor(45, 45, 45)); // Dark gray background
    this->setPalette(p);
#endif

    QVBoxLayout* prefsLayout = new QVBoxLayout;
    prefsLayout->setContentsMargins(8, 8, 8, 8);
    prefsLayout->setSpacing(6);
    this->setLayout(prefsLayout);

    ambientPlane = nullptr;

    QLabel* title = new QLabel("Shader preferences");
    QFont boldFont = QApplication::font();
    boldFont.setWeight(QFont::Bold);
    title->setFont(boldFont);
    title->setAlignment(Qt::AlignCenter);
    prefsLayout->addWidget(title);

    QWidget* middleWidget = new QWidget;
    QGridLayout* middleLayout = new QGridLayout;
    middleLayout->setHorizontalSpacing(6);
    middleLayout->setVerticalSpacing(4);
    middleWidget->setLayout(middleLayout);
    this->layout()->addWidget(middleWidget);

    // labels
    middleLayout->addWidget(new QLabel("Ambient Color"),0,0);
    middleLayout->addWidget(new QLabel("Directive Color"),1,0);
    //middleLayout->addWidget(new QLabel("Direction"),2,0);

    QPixmap dummy(20, 20);

    dummy.fill(canvas->getAmbientColor());
    buttonAmbientColor = new QPushButton;
    buttonAmbientColor->setIcon(QIcon(dummy));
    middleLayout->addWidget(buttonAmbientColor,0,1);
    buttonAmbientColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonAmbientColor,SIGNAL(clicked(bool)),this,SLOT(buttonAmbientColorClicked()));

    editAmbientFactor = new QLineEdit;
    editAmbientFactor->setValidator(new QDoubleValidator);
    editAmbientFactor->setText(QString("%1").arg(canvas->getAmbientFactor()));

#ifdef Q_OS_ANDROID
    // On Android, provide a slider + numeric field for ambient factor
    sliderAmbientFactor = new QSlider(Qt::Horizontal);
    sliderAmbientFactor->setRange(0, 200); // maps to 0.0 - 2.0
    sliderAmbientFactor->setSingleStep(1);
    sliderAmbientFactor->setPageStep(5);
    sliderAmbientFactor->setValue(static_cast<int>(canvas->getAmbientFactor() * 100.0));
    sliderAmbientFactor->setFocusPolicy(Qt::NoFocus);

    QWidget* ambientFactorWidget = new QWidget;
    QHBoxLayout* ambientFactorLayout = new QHBoxLayout;
    ambientFactorLayout->setContentsMargins(0, 0, 0, 0);
    ambientFactorLayout->setSpacing(4);
    ambientFactorWidget->setLayout(ambientFactorLayout);
    ambientFactorLayout->addWidget(editAmbientFactor);
    ambientFactorLayout->addWidget(sliderAmbientFactor);

    middleLayout->addWidget(ambientFactorWidget,0,2,1,2);
    connect(sliderAmbientFactor,&QSlider::valueChanged,this,&ShaderLightPrefs::sliderAmbientFactorChanged);
#else
    sliderAmbientFactor = nullptr;
    middleLayout->addWidget(editAmbientFactor,0,2,1,2);
#endif

    connect(editAmbientFactor,SIGNAL(editingFinished()),this,SLOT(editAmbientFactorFinished()));

    QPushButton* buttonResetAmbientColor = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetAmbientColor,0,4);
    buttonResetAmbientColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetAmbientColor,SIGNAL(clicked(bool)),this,SLOT(resetAmbientColorClicked()));


    dummy.fill(canvas->getDirectiveColor());
    buttonDirectiveColor = new QPushButton;
    buttonDirectiveColor->setIcon(QIcon(dummy));
    middleLayout->addWidget(buttonDirectiveColor,1,1);
    buttonDirectiveColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonDirectiveColor,SIGNAL(clicked(bool)),this,SLOT(buttonDirectiveColorClicked()));

    editDirectiveFactor = new QLineEdit;
    editDirectiveFactor->setValidator(new QDoubleValidator);
    editDirectiveFactor->setText(QString("%1").arg(canvas->getDirectiveFactor()));
    middleLayout->addWidget(editDirectiveFactor,1,2,1,2);
    connect(editDirectiveFactor,SIGNAL(editingFinished()),this,SLOT(editDirectiveFactorFinished()));

    QPushButton* buttonResetDirectiveColor = new QPushButton("Reset");
    middleLayout->addWidget(buttonResetDirectiveColor,1,4);
    buttonResetDirectiveColor->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetDirectiveColor,SIGNAL(clicked(bool)),this,SLOT(resetDirectiveColorClicked()));

#ifdef Q_OS_ANDROID
    // Inline ambient color plane for Android (shows when user taps swatch)
    ambientPlane = new AmbientColorPlane(this, middleWidget);
    ambientPlane->setMinimumHeight(120);
    ambientPlane->setVisible(false);
    middleLayout->addWidget(ambientPlane,2,0,1,5);
#endif

    // Fill in directions

    QFrame* lightSourceWidget = new QFrame;
    lightSourceWidget->setFrameStyle(QFrame::Raised | QFrame::Box);
    QGridLayout* lightSourceWidgetLayout = new QGridLayout;
    lightSourceWidget->setLayout(lightSourceWidgetLayout);

    QLabel* sourcePositionLabel = new QLabel("Light\nSource\nPosition");
    sourcePositionLabel->setAlignment(Qt::AlignCenter);
    lightSourceWidgetLayout->addWidget(sourcePositionLabel,0,0,4,1);

    comboDirections = new QComboBox;
    comboDirections->setFocusPolicy(Qt::NoFocus);
    lightSourceWidgetLayout->addWidget(comboDirections,0,1,1,3);
    comboDirections->addItems(canvas->getNameDir());
    comboDirections->setCurrentIndex(canvas->getCurrentLightDirection());
    connect(comboDirections,SIGNAL(currentIndexChanged(int)),this,SLOT(comboDirectionsChanged(int)));

    leftRight = new QButtonGroup;
    topBottom = new QButtonGroup;
    rearFront = new QButtonGroup;

    QRadioButton* radioLeftButton = new QRadioButton;
    radioLeftButton->setText("Left");
    QRadioButton* radioRightButton = new QRadioButton;
    radioRightButton->setText("Right");
    QRadioButton* radioLRNoneButton = new QRadioButton;
    radioLRNoneButton->setText("off");
    leftRight->addButton(radioLeftButton,2);
    leftRight->addButton(radioRightButton,0);
    leftRight->addButton(radioLRNoneButton,1);
    leftRight->button(0)->setChecked(true);


    lightSourceWidgetLayout->addWidget(radioLeftButton,1,1);
    lightSourceWidgetLayout->addWidget(radioRightButton,1,2);
    lightSourceWidgetLayout->addWidget(radioLRNoneButton,1,3);

    QRadioButton* radioTopButton = new QRadioButton;
    radioTopButton->setText("Top");
    QRadioButton* radioBottomButton = new QRadioButton;
    radioBottomButton->setText("Bottom");
    QRadioButton* radioTBNoneButton = new QRadioButton;
    radioTBNoneButton->setText("off");
    topBottom->addButton(radioTopButton,0);
    topBottom->addButton(radioBottomButton,2);
    topBottom->addButton(radioTBNoneButton,1);
    topBottom->button(0)->setChecked(true);

    lightSourceWidgetLayout->addWidget(radioTopButton,2,1);
    lightSourceWidgetLayout->addWidget(radioBottomButton,2,2);
    lightSourceWidgetLayout->addWidget(radioTBNoneButton,2,3);

    QRadioButton* radioRearButton = new QRadioButton;
    radioRearButton->setText("Rear");
    QRadioButton* radioFrontButton = new QRadioButton;
    radioFrontButton->setText("Front");
    QRadioButton* radioRFNoneButton = new QRadioButton;
    radioRFNoneButton->setText("off");
    rearFront->addButton(radioRearButton,0);
    rearFront->addButton(radioFrontButton,2);
    rearFront->addButton(radioRFNoneButton,1);
    rearFront->button(0)->setChecked(true);


    lightSourceWidgetLayout->addWidget(radioRearButton,3,1);
    lightSourceWidgetLayout->addWidget(radioFrontButton,3,2);
    lightSourceWidgetLayout->addWidget(radioRFNoneButton,3,3);

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    // Qt 5.15+ and Qt 6: use idClicked signal with modern syntax
    connect(leftRight, &QButtonGroup::idClicked, this, &ShaderLightPrefs::radioSourceClicked);
    connect(topBottom, &QButtonGroup::idClicked, this, &ShaderLightPrefs::radioSourceClicked);
    connect(rearFront, &QButtonGroup::idClicked, this, &ShaderLightPrefs::radioSourceClicked);
#else
    // Qt 5.14 and below: use buttonClicked signal
    connect(leftRight, SIGNAL(buttonClicked(int)), this, SLOT(radioSourceClicked(int)));
    connect(topBottom, SIGNAL(buttonClicked(int)), this, SLOT(radioSourceClicked(int)));
    connect(rearFront, SIGNAL(buttonClicked(int)), this, SLOT(radioSourceClicked(int)));
#endif
    setRadio(canvas->getCurrentLightDirection());

    labelPix = new QLabel;
    setPix(canvas->getCurrentLightDirection());
    lightSourceWidgetLayout->addWidget(labelPix,1,4,3,1);

    QPushButton* buttonResetDirection = new QPushButton("Reset");
    lightSourceWidgetLayout->addWidget(buttonResetDirection,0,4,1,1);
    buttonResetDirection->setFocusPolicy(Qt::NoFocus);
    connect(buttonResetDirection,SIGNAL(clicked(bool)),this,SLOT(resetDirection()));

    middleLayout->addWidget(lightSourceWidget,3,0,4,5);

    groupWireFrame = new QFrame;
    QGridLayout* groupWireFrameLayout = new QGridLayout;
    groupWireFrame->setLayout(groupWireFrameLayout);

    checkboxUseWireFrame = new QCheckBox("Add wireframe");
    checkboxUseWireFrame->setChecked(canvas->getUseWire());
    groupWireFrameLayout->addWidget(checkboxUseWireFrame,0,0);
    checkboxUseWireFrame->setFocusPolicy(Qt::NoFocus);
    connect(checkboxUseWireFrame,SIGNAL(stateChanged(int)),this,SLOT(checkboxUseWireFrameChanged()));

    QLabel* labelWireColor = new QLabel("Wire Color");
    groupWireFrameLayout->addWidget(labelWireColor,1,0);
    dummy.fill(canvas->getWireColor());
    buttonWireColor = new QPushButton;
    buttonWireColor->setIcon(QIcon(dummy));
    groupWireFrameLayout->addWidget(buttonWireColor,1,1);
    buttonWireColor->setFocusPolicy(Qt::NoFocus);
    QPushButton* buttonResetWireColor = new QPushButton("Reset");
    buttonResetWireColor->setFocusPolicy(Qt::NoFocus);
    groupWireFrameLayout->addWidget(buttonResetWireColor,1,3);
    connect(buttonWireColor,SIGNAL(clicked(bool)),this,SLOT(buttonWireColorClicked()));
    connect(buttonResetWireColor,SIGNAL(clicked(bool)),this,SLOT(resetWireColorClicked()));

    labelWireWidth = new QLabel(QString("Wire Width : %1").arg((int)canvas->getWireWidth()));
    groupWireFrameLayout->addWidget(labelWireWidth,2,0);
    sliderWireWidth = new QSlider(Qt::Horizontal);
    sliderWireWidth->setFocusPolicy(Qt::NoFocus);
    sliderWireWidth->setRange(1,10);
    sliderWireWidth->setTickPosition(QSlider::TicksBelow);
    sliderWireWidth->setSingleStep(1);
    sliderWireWidth->setPageStep(1);
    sliderWireWidth->setValue((int)canvas->getWireWidth());
    groupWireFrameLayout->addWidget(sliderWireWidth,2,1,1,2);
    connect(sliderWireWidth,SIGNAL(valueChanged(int)),this,SLOT(sliderWireWidthChanged()));
    QPushButton* buttonResetLineWidth = new QPushButton("Reset");
    buttonResetLineWidth->setFocusPolicy(Qt::NoFocus);
    groupWireFrameLayout->addWidget(buttonResetLineWidth,2,3);
    connect(buttonResetLineWidth,SIGNAL(clicked(bool)),this,SLOT(resetWireWidthClicked()));

    middleLayout->addWidget(groupWireFrame,7,0,3,5);

#ifdef Q_OS_ANDROID
    // Simple view setting: show/hide the floating layer-peel button
    checkboxShowLayerButton = new QCheckBox("Show layer button (peel view)");
    checkboxShowLayerButton->setFocusPolicy(Qt::NoFocus);
    // Ask parent window for current state so dialog reflects real setting
    if (auto win = qobject_cast<Window*>(parentWidget())) {
        checkboxShowLayerButton->setChecked(win->isLayerButtonVisible());
    } else {
        checkboxShowLayerButton->setChecked(true);
    }
    middleLayout->addWidget(checkboxShowLayerButton,10,0,1,5);
    connect(checkboxShowLayerButton, &QCheckBox::stateChanged, this, [this](int state) {
        if (auto win = qobject_cast<Window*>(parentWidget())) {
            win->setLayerButtonVisible(state == Qt::Checked);
        }
    });
#endif

    // Ok button
    QWidget* boxButton = new QWidget;
    QHBoxLayout* boxButtonLayout = new QHBoxLayout;
    boxButton->setLayout(boxButtonLayout);
    QFrame *spacerL = new QFrame;
    spacerL->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Expanding));
    QPushButton* okButton = new QPushButton("Ok");
    boxButtonLayout->addWidget(spacerL);
    boxButtonLayout->addWidget(okButton);
    this->layout()->addWidget(boxButton);
    okButton->setFocusPolicy(Qt::NoFocus);
    connect(okButton,SIGNAL(clicked(bool)),this,SLOT(okButtonClicked()));

    QSettings settings;
#ifndef Q_OS_ANDROID
    // On desktop, restore previous position
    if (!settings.value(PREFS_GEOM).isNull()) {
        restoreGeometry(settings.value(PREFS_GEOM).toByteArray());
    }
#else
    // On Android, ignore saved geometry and use reasonable size
    resize(600, 800);
#endif

    connect(canvas,SIGNAL(fallbackGlslUpdated(bool)),this,SLOT(onFallbackGlslUpdated(bool)));
}

void ShaderLightPrefs::buttonAmbientColorClicked() {
#ifdef Q_OS_ANDROID
    // Android: toggle visibility of the inline color plane instead of
    // opening a separate dialog (avoids GL/accessibility deadlocks).
    if (ambientPlane) {
        ambientPlane->setVisible(!ambientPlane->isVisible());
    }
#else
    QColor newColor = QColorDialog::getColor(
        canvas->getAmbientColor(), this,
        QString("Choose ambient color"),
        QColorDialog::DontUseNativeDialog);
    if (newColor.isValid()) {
        canvas->setAmbientColor(newColor);
        QPixmap dummy(20, 20);
        dummy.fill(canvas->getAmbientColor());
        buttonAmbientColor->setIcon(QIcon(dummy));
        canvas->update();
    }
#endif
}

void ShaderLightPrefs::editAmbientFactorFinished() {
    double f = editAmbientFactor->text().toDouble();
    canvas->setAmbientFactor(f);
#ifdef Q_OS_ANDROID
    if (sliderAmbientFactor) {
        sliderAmbientFactor->setValue(static_cast<int>(f * 100.0));
    }
#endif
    canvas->update();
}

void ShaderLightPrefs::resetAmbientColorClicked() {
    canvas->resetAmbientColor();
    QPixmap dummy(20, 20);
    dummy.fill(canvas->getAmbientColor());
    buttonAmbientColor->setIcon(QIcon(dummy));
    editAmbientFactor->setText(QString("%1").arg(canvas->getAmbientFactor()));
#ifdef Q_OS_ANDROID
    if (sliderAmbientFactor) {
        sliderAmbientFactor->setValue(static_cast<int>(canvas->getAmbientFactor() * 100.0));
    }
    if (ambientPlane) ambientPlane->setVisible(false);
#endif
    canvas->update();
}

void ShaderLightPrefs::buttonDirectiveColorClicked() {
#ifdef Q_OS_ANDROID
    // Android: cycle through a preset directive palette instead of dialogs.
    static QVector<QColor> palette = {
        QColor(255, 255, 255),            // pure white
        QColor(240, 240, 240),            // soft white
        QColor(255, 230, 190),            // warm
        QColor(255, 210, 160),            // warmer
        QColor(210, 230, 255),            // cool
        QColor(200, 255, 220),            // greenish
        QColor(255, 230, 255)             // magenta tint
    };
    QColor current = canvas->getDirectiveColor();
    int idx = 0;
    for (int i = 0; i < palette.size(); ++i) {
        if (palette[i] == current) { idx = i; break; }
    }
    QColor newColor = palette[(idx + 1) % palette.size()];
#else
    QColor newColor = QColorDialog::getColor(
        canvas->getDirectiveColor(), this,
        QString("Choose directive color"),
        QColorDialog::DontUseNativeDialog);
#endif
    if (newColor.isValid() == true)
    {
        canvas->setDirectiveColor(newColor);
        QPixmap dummy(20, 20);
        dummy.fill(canvas->getDirectiveColor());
        buttonDirectiveColor->setIcon(QIcon(dummy));
        canvas->update();
    }
}

void ShaderLightPrefs::editDirectiveFactorFinished() {
    canvas->setDirectiveFactor(editDirectiveFactor->text().toDouble());
    canvas->update();
}

void ShaderLightPrefs::resetDirectiveColorClicked() {
    canvas->resetDirectiveColor();
    QPixmap dummy(20, 20);
    dummy.fill(canvas->getDirectiveColor());
    buttonDirectiveColor->setIcon(QIcon(dummy));
    editDirectiveFactor->setText(QString("%1").arg(canvas->getDirectiveFactor()));
    canvas->update();
}

void ShaderLightPrefs::applyAmbientFromPlane(const QColor& c) {
    canvas->setAmbientColor(c);
    QPixmap dummy(20, 20);
    dummy.fill(canvas->getAmbientColor());
    buttonAmbientColor->setIcon(QIcon(dummy));
    canvas->update();
}

void ShaderLightPrefs::sliderAmbientFactorChanged(int v) {
    double f = static_cast<double>(v) / 100.0;
    editAmbientFactor->setText(QString::number(f, 'f', 2));
    canvas->setAmbientFactor(f);
    canvas->update();
}

void ShaderLightPrefs::okButtonClicked() {
    this->close();
}

void ShaderLightPrefs::comboDirectionsChanged(int ind) {
    setRadio(ind);
    setPix(ind);
    canvas->setCurrentLightDirection(ind);
#ifdef Q_OS_ANDROID
    canvas->repaint();
#else
    canvas->update();
#endif
}

void ShaderLightPrefs::resetDirection() {
    canvas->resetCurrentLightDirection();
    comboDirections->setCurrentIndex(canvas->getCurrentLightDirection());
    canvas->update();
}

void ShaderLightPrefs::resizeEvent(QResizeEvent *event)
{
    QSettings().setValue(PREFS_GEOM, saveGeometry());
    QWidget::resizeEvent(event);
}

void ShaderLightPrefs::moveEvent(QMoveEvent *event)
{
    QSettings().setValue(PREFS_GEOM, saveGeometry());
    QWidget::moveEvent(event);
}

void ShaderLightPrefs::checkboxUseWireFrameChanged() {
    bool state = checkboxUseWireFrame->isChecked();
    canvas->setUseWire(state);
    canvas->update();
}

void ShaderLightPrefs::buttonWireColorClicked() {
#ifdef Q_OS_ANDROID
    // Android: cycle through a small set of wire colors.
    static QVector<QColor> palette = {
        QColor(255,128,0),   // original orange
        QColor(255,0,0),     // red
        QColor(0,255,0),     // green
        QColor(0,128,255),   // blue
        QColor(255,255,255)  // white
    };
    QColor current = canvas->getWireColor();
    int idx = 0;
    for (int i = 0; i < palette.size(); ++i) {
        if (palette[i] == current) { idx = i; break; }
    }
    QColor newColor = palette[(idx + 1) % palette.size()];
#else
    QColor newColor = QColorDialog::getColor(
        canvas->getWireColor(), this,
        QString("Choose wireframe color"),
        QColorDialog::DontUseNativeDialog);
#endif
    if (newColor.isValid() == true)
    {
        canvas->setWireColor(newColor);
        QPixmap dummy(20, 20);
        dummy.fill(canvas->getWireColor());
        buttonWireColor->setIcon(QIcon(dummy));
        canvas->update();
    }
}

void ShaderLightPrefs::resetWireColorClicked() {
    canvas->resetWireColor();
    QPixmap dummy(20, 20);
    dummy.fill(canvas->getWireColor());
    buttonWireColor->setIcon(QIcon(dummy));
    canvas->update();
}

void ShaderLightPrefs::sliderWireWidthChanged() {
    int lw = sliderWireWidth->value();
    canvas->setWireWidth((double) lw);
    labelWireWidth->setText(QString("Wire Width : %1").arg(lw));
    canvas->update();
}

void ShaderLightPrefs::resetWireWidthClicked() {
    canvas->resetWireWidth();
    sliderWireWidth->setValue((int)canvas->getWireWidth());
}

void ShaderLightPrefs::onFallbackGlslUpdated(bool b) {
        groupWireFrame->setDisabled(b);
}

void ShaderLightPrefs::toggleUseWire() {
    // toggle if enable, no sense to do so otherwise
    if (checkboxUseWireFrame->isEnabled())
        checkboxUseWireFrame->toggle();
}

void ShaderLightPrefs::radioSourceClicked(int ind) {
    int pos = leftRight->checkedId() * 9 + topBottom->checkedId() * 3 + rearFront->checkedId();
    // Forbidden : 13 off,off,off
    if (pos == 13) {
        resetDirection();
        return;
    }
    pos = (pos >= 14) ? pos - 1 : pos;
    
    // Update combo without triggering signal (avoid recursion)
    comboDirections->blockSignals(true);
    comboDirections->setCurrentIndex(pos);
    comboDirections->blockSignals(false);
    
    // Update cube icon and canvas directly
    setPix(pos);
    canvas->setCurrentLightDirection(pos);
#ifdef Q_OS_ANDROID
    // On Android, force immediate repaint since dialog may cover canvas
    canvas->repaint();
#else
    canvas->update();
#endif
}

void ShaderLightPrefs::setRadio(int ind) {
    leftRight->button(1 + QVariant(canvas->getListDir().at(ind).x()).toInt())->setChecked(true);
    topBottom->button(1 + QVariant(canvas->getListDir().at(ind).y()).toInt())->setChecked(true);
    rearFront->button(1 + QVariant(canvas->getListDir().at(ind).z()).toInt())->setChecked(true);
}

void ShaderLightPrefs::setPix(int ind) {
    labelPix->setPixmap(QPixmap(QString(":/qt/icons/lightSourcePosition/lsp_%1.png").arg(ind+1)).scaledToWidth(64,Qt::SmoothTransformation));
}
