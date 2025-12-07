#include <QApplication>
#include <QGuiApplication>
#include <QLocale>
#include <QStyleFactory>

#include "core/app.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    // Disable threaded OpenGL to prevent accessibility deadlock on Android
    // Qt's accessibility system tries to access OpenGL from different thread causing crash
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
    
    // Force software rendering for widget backing stores to avoid RHI deadlock
    // Multiple workarounds for QTBUG-108762 QComboBox crash on Android
    qputenv("QT_WIDGETS_RHI", "0");
    qputenv("QT_ANDROID_DISABLE_ACCESSIBILITY", "1");
    
    // Prevent widgets from creating backing stores during event processing
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
#endif

    // Force C locale to force decimal point
    QLocale::setDefault(QLocale::c());

    QCoreApplication::setOrganizationName("fstl-e-android");
    QCoreApplication::setOrganizationDomain("https://github.com/wdaniau/fstl-e");
    QCoreApplication::setApplicationName("fstl-e-android");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    App a(argc, argv);

#ifdef Q_OS_ANDROID
    // Use simpler Fusion style on Android to avoid native widget conflicts with OpenGL
    a.setStyle(QStyleFactory::create("Fusion"));
#endif

    return a.exec();
}
