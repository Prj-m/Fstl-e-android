#include <QDebug>
#include <QFileOpenEvent>
#include <QUrl>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

#include "core/app.h"
#include "core/fileopenpath.h"
#include "ui/window.h"

App::App(int& argc, char *argv[]) :
    QApplication(argc, argv), window(new Window())
{
#ifdef Q_OS_ANDROID
    // No global style override; handled per-toolbar in Window
#endif
    QString fileToOpen;
#ifdef Q_OS_ANDROID
    QtAndroidPrivate::registerNewIntentListener(this);
#endif
    
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
                    "toString",
                    "()Ljava/lang/String;");
                if (path.isValid()) {
                    fileToOpen = fileOpenPath(QUrl(path.toString()));
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
#ifdef Q_OS_ANDROID
    QtAndroidPrivate::unregisterNewIntentListener(this);
#endif
    delete window;
}

bool App::event(QEvent* e)
{
    if (e->type() == QEvent::FileOpen)
    {
        const auto* openEvent = static_cast<QFileOpenEvent*>(e);
        const QUrl url = openEvent->url();
        const QString filename = url.isEmpty() ? openEvent->file() : fileOpenPath(url);
        if (!filename.isEmpty())
            window->load_stl(filename);
        return true;
    }
    else
    {
        return QApplication::event(e);
    }
}

#ifdef Q_OS_ANDROID
bool App::handleNewIntent(JNIEnv*, jobject intentObject)
{
    const QJniObject intent(intentObject);
    const QString action = intent.callObjectMethod("getAction", "()Ljava/lang/String;").toString();
    if (action != QStringLiteral("android.intent.action.VIEW"))
        return false;
    const QJniObject data = intent.callObjectMethod("getData", "()Landroid/net/Uri;");
    if (!data.isValid())
        return false;
    const QUrl url(data.callObjectMethod("toString", "()Ljava/lang/String;").toString());
    if (url.scheme() != QStringLiteral("file") && url.scheme() != QStringLiteral("content"))
        return false;
    const QString filename = fileOpenPath(url);
    if (filename.isEmpty())
        return false;
    // JNI callbacks arrive outside the GUI thread. The context cancels delivery
    // if the application is destroyed before this queued operation runs.
    QMetaObject::invokeMethod(this, [this, filename] {
        window->load_stl(filename);
    }, Qt::QueuedConnection);
    return true;
}
#endif
