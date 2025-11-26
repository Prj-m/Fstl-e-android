#include <QApplication>
#include <QLocale>

#include "core/app.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_ANDROID
    // Disable threaded OpenGL to prevent accessibility deadlock on Android
    // Qt's accessibility system tries to access OpenGL from different thread causing crash
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts, false);
#endif

    // Force C locale to force decimal point
    QLocale::setDefault(QLocale::c());

    QCoreApplication::setOrganizationName("fstl-e-android");
    QCoreApplication::setOrganizationDomain("https://github.com/wdaniau/fstl-e");
    QCoreApplication::setApplicationName("fstl-e-android");
    QCoreApplication::setApplicationVersion("1.0.0");
    
    App a(argc, argv);

    return a.exec();
}
