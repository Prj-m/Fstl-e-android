#ifndef STLMESHLOADER_H
#define STLMESHLOADER_H

#include <QObject>
#include <QString>

class Mesh;

class StlMeshLoader : public QObject
{
    Q_OBJECT

public:
    explicit StlMeshLoader(QObject *parent = nullptr);
    ~StlMeshLoader();

public slots:
    void loadStl(const QString &filename);

signals:
    void meshLoaded(Mesh *mesh);
    void loadError(const QString &error);

private:
    Mesh *currentMesh;
};

#endif // STLMESHLOADER_H
