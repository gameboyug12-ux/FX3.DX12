#pragma once
#include <QMainWindow>
class DXRViewport;
class QTreeWidget;
class QPlainTextEdit;
class QDockWidget;
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
private:
    void createMenus();
    DXRViewport* viewport_ = nullptr;
    QTreeWidget* outliner_ = nullptr;
    QPlainTextEdit* log_ = nullptr;
};
