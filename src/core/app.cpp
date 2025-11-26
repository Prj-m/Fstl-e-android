#include <QDebug>
#include <QFileOpenEvent>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

#include "core/app.h"
#include "ui/window.h"

App::App(int& argc, char *argv[]) :
    QApplication(argc, argv), window(new Window())
{
    QString fileToOpen;
    
#ifdef Q_OS_ANDROID
    QJniObject activity = QJniObject::callStaticObjectMethod(
        "org/qtproject/qt/android/QtNative",
        "activity",
        "()Landroid/app/Activity;");
    
    if (activity.isValid()) {
        QJniObject intent = activity.callObjectMethod(
            "getIntent",
            "()Landroid/content/Intent;");
        
        if (intent.isValid()) {
            QJniObject data = intent.callObjectMethod(
                "getData",
                "()Landroid/net/Uri;");
            
            if (data.isValid()) {
                QJniObject path = data.callObjectMethod(
                    "getPath",
                    "()Ljava/lang/String;");
                if (path.isValid()) {
                    fileToOpen = path.toString();
                }
            }
        }
    }
#endif
    
    // Don't load default file yet - let load_persist_settings handle it
    // if autoreload is enabled
    if (fileToOpen.isEmpty()) {
        if (argc > 1)
            fileToOpen = argv[1];
        // else: no default file - load_persist_settings will handle autoreload
    }
    
    // Only load if we have a file from command line or intent
    if (!fileToOpen.isEmpty()) {
        window->load_stl(fileToOpen);
    }
    window->show();
}

App::~App()
{
    delete window;
}

bool App::event(QEvent* e)
{
    if (e->type() == QEvent::FileOpen)
    {
        window->load_stl(static_cast<QFileOpenEvent*>(e)->file());
        return true;
    }
    else
    {
        return QApplication::event(e);
    }
}
