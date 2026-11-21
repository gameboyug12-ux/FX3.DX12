#pragma once

#include "SceneDocument.hpp"

#include <QMainWindow>
#include <array>

class DXRViewport;
class QTreeWidget;
class QPlainTextEdit;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QCloseEvent;

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createMenus();
    void createPropertiesDock();
    void rebuildOutliner();
    void refreshProperties();
    void updateWindowTitle();
    void appendLog(const QString& message);
    bool maybeSave();
    bool saveScene();
    bool saveSceneAs();
    bool openScene();
    void newScene();
    void addTriangle();
    void deleteSelected();
    void onTransformChanged(int component, double value);
    void onNameEdited();

    DXRViewport* viewport_ = nullptr;
    QTreeWidget* outliner_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
    QWidget* propertiesPanel_ = nullptr;
    QLineEdit* nameField_ = nullptr;
    QLabel* typeLabel_ = nullptr;
    std::array<QDoubleSpinBox*, 9> transformFields_{};
    SceneDocument scene_;
    QString selectedObjectId_;
};
