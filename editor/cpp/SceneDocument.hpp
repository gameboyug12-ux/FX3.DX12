#pragma once

#include <QVector>
#include <QVector3D>
#include <QString>

class SceneDocument final {
public:
    struct Object {
        QString id;
        QString name;
        QString type;
        QVector3D position{0.0f, 0.0f, 0.0f};
        QVector3D rotation{0.0f, 0.0f, 0.0f};
        QVector3D scale{1.0f, 1.0f, 1.0f};
    };

    SceneDocument();

    void reset();
    bool load(const QString& filePath, QString* error = nullptr);
    bool save(const QString& filePath, QString* error = nullptr);

    const QVector<Object>& objects() const noexcept { return objects_; }
    Object* find(const QString& id) noexcept;
    const Object* find(const QString& id) const noexcept;

    QString addObject(const QString& name, const QString& type);
    bool removeObject(const QString& id);
    bool setObjectName(const QString& id, const QString& name);
    bool setTransformComponent(const QString& id, int component, double value);

    const QString& filePath() const noexcept { return filePath_; }
    bool isDirty() const noexcept { return dirty_; }
    void markClean() noexcept { dirty_ = false; }

private:
    QVector<Object> objects_;
    QString filePath_;
    bool dirty_ = false;
};
