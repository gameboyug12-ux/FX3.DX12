#include "MainWindow.hpp"
#include "DXRViewport.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QStringList>
#include <QTreeWidget>
#include <QTreeWidgetItem>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setMinimumSize(1000, 650);
    resize(1600, 950);

    viewport_ = new DXRViewport(this);
    setCentralWidget(viewport_);

    auto* sceneDock = new QDockWidget(QStringLiteral("Scene Outliner"), this);
    outliner_ = new QTreeWidget(sceneDock);
    outliner_->setHeaderLabels({QStringLiteral("Scene")});
    outliner_->setSelectionMode(QAbstractItemView::SingleSelection);
    sceneDock->setWidget(outliner_);
    addDockWidget(Qt::LeftDockWidgetArea, sceneDock);
    connect(outliner_, &QTreeWidget::currentItemChanged, this,
        [this](QTreeWidgetItem* current, QTreeWidgetItem*) {
            selectedObjectId_ = current ? current->data(0, Qt::UserRole).toString() : QString();
            refreshProperties();
        });

    createPropertiesDock();

    auto* outputDock = new QDockWidget(QStringLiteral("Console"), this);
    log_ = new QPlainTextEdit(outputDock);
    log_->setReadOnly(true);
    outputDock->setWidget(log_);
    addDockWidget(Qt::BottomDockWidgetArea, outputDock);

    scene_.reset();
    rebuildOutliner();
    createMenus();
    updateWindowTitle();
    appendLog(QStringLiteral("FX3 Editor 0.3.8 initialized."));
    appendLog(QStringLiteral("Scene files use the versioned .fx3scene JSON format."));
    appendLog(QStringLiteral("Note: scene transforms are saved; the current DXR preview remains a fixed test triangle."));
    statusBar()->showMessage(QStringLiteral("Ready | DX12 / DXR preview"));
}

void MainWindow::createPropertiesDock() {
    auto* dock = new QDockWidget(QStringLiteral("Properties"), this);
    propertiesPanel_ = new QWidget(dock);
    auto* form = new QFormLayout(propertiesPanel_);

    nameField_ = new QLineEdit(propertiesPanel_);
    typeLabel_ = new QLabel(propertiesPanel_);
    form->addRow(QStringLiteral("Name"), nameField_);
    form->addRow(QStringLiteral("Type"), typeLabel_);
    connect(nameField_, &QLineEdit::editingFinished, this, &MainWindow::onNameEdited);

    const QStringList groups{QStringLiteral("Location"), QStringLiteral("Rotation"), QStringLiteral("Scale")};
    const QStringList axes{QStringLiteral("X"), QStringLiteral("Y"), QStringLiteral("Z")};
    for (int group = 0; group < 3; ++group) {
        form->addRow(new QLabel(QStringLiteral("<b>%1</b>").arg(groups.at(group)), propertiesPanel_));
        for (int axis = 0; axis < 3; ++axis) {
            const int index = group * 3 + axis;
            auto* field = new QDoubleSpinBox(propertiesPanel_);
            field->setRange(-100000.0, 100000.0);
            field->setDecimals(3);
            field->setSingleStep(0.1);
            if (group == 2) field->setValue(1.0);
            transformFields_[static_cast<size_t>(index)] = field;
            form->addRow(axes.at(axis), field);
            connect(field, qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this, index](double value) { onTransformChanged(index, value); });
        }
    }
    dock->setWidget(propertiesPanel_);
    addDockWidget(Qt::RightDockWidgetArea, dock);
}

void MainWindow::createMenus() {
    auto* file = menuBar()->addMenu(QStringLiteral("&File"));
    auto* actionNew = file->addAction(QStringLiteral("&New Scene"));
    actionNew->setShortcut(QKeySequence::New);
    connect(actionNew, &QAction::triggered, this, &MainWindow::newScene);
    auto* actionOpen = file->addAction(QStringLiteral("&Open Scene..."));
    actionOpen->setShortcut(QKeySequence::Open);
    connect(actionOpen, &QAction::triggered, this, [this] { openScene(); });
    auto* actionSave = file->addAction(QStringLiteral("&Save"));
    actionSave->setShortcut(QKeySequence::Save);
    connect(actionSave, &QAction::triggered, this, [this] { saveScene(); });
    auto* actionSaveAs = file->addAction(QStringLiteral("Save &As..."));
    actionSaveAs->setShortcut(QKeySequence::SaveAs);
    connect(actionSaveAs, &QAction::triggered, this, [this] { saveSceneAs(); });
    file->addSeparator();
    auto* quit = file->addAction(QStringLiteral("E&xit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    auto* sceneMenu = menuBar()->addMenu(QStringLiteral("&Scene"));
    auto* addTriangleAction = sceneMenu->addAction(QStringLiteral("Add &Triangle Mesh"));
    addTriangleAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+T")));
    connect(addTriangleAction, &QAction::triggered, this, &MainWindow::addTriangle);
    auto* deleteAction = sceneMenu->addAction(QStringLiteral("&Delete Selected Object"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, this, &MainWindow::deleteSelected);

    auto* view = menuBar()->addMenu(QStringLiteral("&View"));
    auto* fullscreen = view->addAction(QStringLiteral("Toggle &Fullscreen"));
    fullscreen->setShortcut(Qt::Key_F11);
    connect(fullscreen, &QAction::triggered, this, [this] {
        isFullScreen() ? showNormal() : showFullScreen();
    });

    auto* help = menuBar()->addMenu(QStringLiteral("&Help"));
    auto* about = help->addAction(QStringLiteral("About FX3 Editor"));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, QStringLiteral("FX3 Editor 0.3.8"),
            QStringLiteral("FX3.DX12 Editor 0.3.8\nQt 6 Widgets | C++20 | Direct3D 12 | DXR"));
    });
}

void MainWindow::rebuildOutliner() {
    const QSignalBlocker treeBlocker(outliner_);
    if (!scene_.find(selectedObjectId_)) selectedObjectId_.clear();
    outliner_->clear();
    auto* root = new QTreeWidgetItem(outliner_, {QStringLiteral("Scene")});
    for (const SceneDocument::Object& object : scene_.objects()) {
        auto* item = new QTreeWidgetItem(root, {object.name});
        item->setData(0, Qt::UserRole, object.id);
        if (object.id == selectedObjectId_) outliner_->setCurrentItem(item);
    }
    root->setExpanded(true);
    if (selectedObjectId_.isEmpty() && !scene_.objects().isEmpty()) {
        selectedObjectId_ = scene_.objects().front().id;
        for (int i = 0; i < root->childCount(); ++i) {
            if (root->child(i)->data(0, Qt::UserRole).toString() == selectedObjectId_) {
                outliner_->setCurrentItem(root->child(i));
                break;
            }
        }
    }
    refreshProperties();
}

void MainWindow::refreshProperties() {
    const SceneDocument::Object* object = scene_.find(selectedObjectId_);
    const bool enabled = object != nullptr;
    propertiesPanel_->setEnabled(enabled);
    if (!object) {
        nameField_->clear();
        typeLabel_->clear();
        return;
    }
    const QSignalBlocker nameBlocker(nameField_);
    nameField_->setText(object->name);
    typeLabel_->setText(object->type);
    const float values[] = {
        object->position.x(), object->position.y(), object->position.z(),
        object->rotation.x(), object->rotation.y(), object->rotation.z(),
        object->scale.x(), object->scale.y(), object->scale.z()
    };
    for (size_t i = 0; i < transformFields_.size(); ++i) {
        const QSignalBlocker blocker(transformFields_[i]);
        transformFields_[i]->setValue(values[i]);
    }
}

void MainWindow::updateWindowTitle() {
    const QString path = scene_.filePath().isEmpty()
        ? QStringLiteral("Untitled") : scene_.filePath();
    setWindowTitle(QStringLiteral("FX3 Editor 0.3.8 — %1%2")
        .arg(path, scene_.isDirty() ? QStringLiteral(" *") : QString()));
}

void MainWindow::appendLog(const QString& message) {
    if (log_) log_->appendPlainText(message);
}

bool MainWindow::maybeSave() {
    if (!scene_.isDirty()) return true;
    const auto answer = QMessageBox::warning(this, QStringLiteral("Unsaved Scene"),
        QStringLiteral("The current scene has unsaved changes."),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Save) return saveScene();
    return answer == QMessageBox::Discard;
}

bool MainWindow::saveScene() {
    if (scene_.filePath().isEmpty()) return saveSceneAs();
    QString error;
    if (!scene_.save(scene_.filePath(), &error)) {
        QMessageBox::critical(this, QStringLiteral("Save Failed"), error);
        appendLog(QStringLiteral("Save failed: %1").arg(error));
        return false;
    }
    updateWindowTitle();
    appendLog(QStringLiteral("Scene saved: %1").arg(scene_.filePath()));
    statusBar()->showMessage(QStringLiteral("Scene saved"), 3000);
    return true;
}

bool MainWindow::saveSceneAs() {
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save FX3 Scene"),
        scene_.filePath().isEmpty() ? QStringLiteral("Untitled.fx3scene") : scene_.filePath(),
        QStringLiteral("FX3 Scene (*.fx3scene)"));
    if (path.isEmpty()) return false;
    QString error;
    if (!scene_.save(path, &error)) {
        QMessageBox::critical(this, QStringLiteral("Save Failed"), error);
        appendLog(QStringLiteral("Save failed: %1").arg(error));
        return false;
    }
    updateWindowTitle();
    appendLog(QStringLiteral("Scene saved: %1").arg(path));
    statusBar()->showMessage(QStringLiteral("Scene saved"), 3000);
    return true;
}

bool MainWindow::openScene() {
    if (!maybeSave()) return false;
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open FX3 Scene"),
        QString(), QStringLiteral("FX3 Scene (*.fx3scene);;JSON (*.json)"));
    if (path.isEmpty()) return false;
    QString error;
    if (!scene_.load(path, &error)) {
        QMessageBox::critical(this, QStringLiteral("Open Failed"), error);
        appendLog(QStringLiteral("Open failed: %1").arg(error));
        return false;
    }
    selectedObjectId_.clear();
    rebuildOutliner();
    updateWindowTitle();
    appendLog(QStringLiteral("Scene opened: %1").arg(path));
    return true;
}

void MainWindow::newScene() {
    if (!maybeSave()) return;
    scene_.reset();
    selectedObjectId_.clear();
    rebuildOutliner();
    updateWindowTitle();
    appendLog(QStringLiteral("Created a new scene."));
}

void MainWindow::addTriangle() {
    const QString id = scene_.addObject(QStringLiteral("Triangle Mesh %1").arg(scene_.objects().size()),
                                        QStringLiteral("triangle"));
    selectedObjectId_ = id;
    rebuildOutliner();
    updateWindowTitle();
    appendLog(QStringLiteral("Added triangle mesh object to scene document."));
}

void MainWindow::deleteSelected() {
    if (selectedObjectId_.isEmpty()) return;
    const SceneDocument::Object* object = scene_.find(selectedObjectId_);
    if (!object) return;
    const QString name = object->name;
    if (!scene_.removeObject(selectedObjectId_)) return;
    selectedObjectId_.clear();
    rebuildOutliner();
    updateWindowTitle();
    appendLog(QStringLiteral("Deleted object: %1").arg(name));
}

void MainWindow::onTransformChanged(int component, double value) {
    if (scene_.setTransformComponent(selectedObjectId_, component, value)) {
        updateWindowTitle();
    }
}

void MainWindow::onNameEdited() {
    if (scene_.setObjectName(selectedObjectId_, nameField_->text())) {
        rebuildOutliner();
        updateWindowTitle();
    } else {
        refreshProperties();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (maybeSave()) event->accept();
    else event->ignore();
}
