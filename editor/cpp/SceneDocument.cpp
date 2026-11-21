#include "SceneDocument.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

#include <cmath>

namespace {
QJsonArray vectorToJson(const QVector3D& value) {
    return QJsonArray{
        static_cast<double>(value.x()),
        static_cast<double>(value.y()),
        static_cast<double>(value.z())
    };
}

bool readVector(const QJsonValue& value, QVector3D& output) {
    if (!value.isArray()) return false;
    const QJsonArray array = value.toArray();
    if (array.size() != 3) return false;
    for (const QJsonValue& component : array) {
        if (!component.isDouble() || !std::isfinite(component.toDouble())) return false;
    }
    output = QVector3D(
        static_cast<float>(array.at(0).toDouble()),
        static_cast<float>(array.at(1).toDouble()),
        static_cast<float>(array.at(2).toDouble()));
    return true;
}

void setError(QString* target, const QString& message) {
    if (target) *target = message;
}
}

SceneDocument::SceneDocument() {
    reset();
}

void SceneDocument::reset() {
    objects_.clear();
    Object camera;
    camera.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    camera.name = QStringLiteral("Camera");
    camera.type = QStringLiteral("camera");
    objects_.push_back(camera);

    Object mesh;
    mesh.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    mesh.name = QStringLiteral("Triangle Mesh");
    mesh.type = QStringLiteral("triangle");
    objects_.push_back(mesh);

    Object light;
    light.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    light.name = QStringLiteral("Directional Light");
    light.type = QStringLiteral("directional_light");
    objects_.push_back(light);

    filePath_.clear();
    dirty_ = false;
}

SceneDocument::Object* SceneDocument::find(const QString& id) noexcept {
    for (Object& object : objects_) if (object.id == id) return &object;
    return nullptr;
}

const SceneDocument::Object* SceneDocument::find(const QString& id) const noexcept {
    for (const Object& object : objects_) if (object.id == id) return &object;
    return nullptr;
}

QString SceneDocument::addObject(const QString& name, const QString& type) {
    Object object;
    object.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    object.name = name.trimmed().isEmpty() ? QStringLiteral("Object") : name.trimmed();
    object.type = type;
    const QString id = object.id;
    objects_.push_back(object);
    dirty_ = true;
    return id;
}

bool SceneDocument::removeObject(const QString& id) {
    for (qsizetype i = 0; i < objects_.size(); ++i) {
        if (objects_.at(i).id == id) {
            objects_.removeAt(i);
            dirty_ = true;
            return true;
        }
    }
    return false;
}

bool SceneDocument::setObjectName(const QString& id, const QString& name) {
    Object* object = find(id);
    const QString cleanName = name.trimmed();
    if (!object || cleanName.isEmpty()) return false;
    if (object->name != cleanName) {
        object->name = cleanName;
        dirty_ = true;
    }
    return true;
}

bool SceneDocument::setTransformComponent(const QString& id, int component, double value) {
    Object* object = find(id);
    if (!object || component < 0 || component >= 9 || !std::isfinite(value)) return false;
    QVector3D* target = component < 3 ? &object->position
                        : component < 6 ? &object->rotation : &object->scale;
    const int axis = component % 3;
    const float converted = static_cast<float>(value);
    if (axis == 0 && target->x() != converted) { target->setX(converted); dirty_ = true; }
    if (axis == 1 && target->y() != converted) { target->setY(converted); dirty_ = true; }
    if (axis == 2 && target->z() != converted) { target->setZ(converted); dirty_ = true; }
    return true;
}

bool SceneDocument::load(const QString& filePath, QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(error, QStringLiteral("Cannot open scene: %1").arg(file.errorString()));
        return false;
    }

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("Invalid scene JSON: %1").arg(parseError.errorString()));
        return false;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("format")).toString() != QStringLiteral("FX3Scene") ||
        root.value(QStringLiteral("version")).toInt(-1) != 1 ||
        !root.value(QStringLiteral("objects")).isArray()) {
        setError(error, QStringLiteral("Unsupported scene format or version."));
        return false;
    }

    QVector<Object> loaded;
    QSet<QString> ids;
    const QJsonArray objects = root.value(QStringLiteral("objects")).toArray();
    for (qsizetype i = 0; i < objects.size(); ++i) {
        if (!objects.at(i).isObject()) {
            setError(error, QStringLiteral("Object %1 is not a JSON object.").arg(i));
            return false;
        }
        const QJsonObject value = objects.at(i).toObject();
        Object object;
        object.id = value.value(QStringLiteral("id")).toString();
        object.name = value.value(QStringLiteral("name")).toString();
        object.type = value.value(QStringLiteral("type")).toString();
        if (object.id.isEmpty() || object.name.trimmed().isEmpty() || object.type.isEmpty() ||
            ids.contains(object.id) ||
            !readVector(value.value(QStringLiteral("position")), object.position) ||
            !readVector(value.value(QStringLiteral("rotation")), object.rotation) ||
            !readVector(value.value(QStringLiteral("scale")), object.scale)) {
            setError(error, QStringLiteral("Object %1 has missing, duplicate or invalid fields.").arg(i));
            return false;
        }
        ids.insert(object.id);
        loaded.push_back(object);
    }

    objects_ = std::move(loaded);
    filePath_ = filePath;
    dirty_ = false;
    return true;
}

bool SceneDocument::save(const QString& filePath, QString* error) {
    if (filePath.isEmpty()) {
        setError(error, QStringLiteral("A scene file path is required."));
        return false;
    }

    QJsonArray objects;
    for (const Object& object : objects_) {
        QJsonObject value;
        value.insert(QStringLiteral("id"), object.id);
        value.insert(QStringLiteral("name"), object.name);
        value.insert(QStringLiteral("type"), object.type);
        value.insert(QStringLiteral("position"), vectorToJson(object.position));
        value.insert(QStringLiteral("rotation"), vectorToJson(object.rotation));
        value.insert(QStringLiteral("scale"), vectorToJson(object.scale));
        objects.append(value);
    }

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("FX3Scene"));
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("objects"), objects);

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setError(error, QStringLiteral("Cannot write scene: %1").arg(file.errorString()));
        return false;
    }
    const QByteArray bytes = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        setError(error, QStringLiteral("Could not write the complete scene file."));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        setError(error, QStringLiteral("Could not commit scene file: %1").arg(file.errorString()));
        return false;
    }

    filePath_ = filePath;
    dirty_ = false;
    return true;
}
