#ifndef APP_H
#define APP_H

#include <QApplication>
#ifdef Q_OS_ANDROID
#include <QtCore/private/qjnihelpers_p.h>
#endif

class Window;

class App : public QApplication
#ifdef Q_OS_ANDROID
    , private QtAndroidPrivate::NewIntentListener
#endif
{
    Q_OBJECT
public:
    explicit App(int& argc, char *argv[]);
	~App();
protected:
    bool event(QEvent* e) override;
private:
    Window* const window;
#ifdef Q_OS_ANDROID
    bool handleNewIntent(JNIEnv* env, jobject intent) override;
#endif

};

#endif // APP_H
