#pragma once
#include <QUrl>

inline QString fileOpenPath(const QUrl& url)
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString(QUrl::FullyEncoded);
}
