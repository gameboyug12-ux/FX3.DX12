#include "MainWindow.hpp"
#include "DXRViewport.hpp"
#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPlainTextEdit>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMenuBar>
#include <QStatusBar>
#include <QTimer>
#include <QAction>
#include <QApplication>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("FX3 Editor 0.3.7"));
    resize(1600,950);
    viewport_=new DXRViewport(this);
    setCentralWidget(viewport_);

    auto* sceneDock=new QDockWidget(QStringLiteral("Scene Outliner"),this);
    outliner_=new QTreeWidget(sceneDock); outliner_->setHeaderLabels({QStringLiteral("Scene")});
    auto* root=new QTreeWidgetItem(outliner_,{QStringLiteral("Scene")});
    new QTreeWidgetItem(root,{QStringLiteral("Camera")});
    new QTreeWidgetItem(root,{QStringLiteral("Triangle Mesh")});
    new QTreeWidgetItem(root,{QStringLiteral("Directional Light")});
    root->setExpanded(true); sceneDock->setWidget(outliner_);
    addDockWidget(Qt::LeftDockWidgetArea,sceneDock);

    auto* propertiesDock=new QDockWidget(QStringLiteral("Properties"),this);
    auto* panel=new QWidget(propertiesDock); auto* form=new QFormLayout(panel);
    form->addRow(QStringLiteral("Object"),new QLabel(QStringLiteral("Triangle Mesh"),panel));
    for(const auto& group : {QStringLiteral("Location"),QStringLiteral("Rotation"),QStringLiteral("Scale")}) {
        form->addRow(new QLabel(QStringLiteral("<b>%1</b>").arg(group),panel));
        for(const auto& axis : {QStringLiteral("X"),QStringLiteral("Y"),QStringLiteral("Z")}) {
            auto* value=new QDoubleSpinBox(panel); value->setRange(-100000,100000);
            value->setDecimals(3); value->setValue(group==QStringLiteral("Scale")?1.0:0.0);
            form->addRow(axis,value);
        }
    }
    propertiesDock->setWidget(panel); addDockWidget(Qt::RightDockWidgetArea,propertiesDock);

    auto* outputDock=new QDockWidget(QStringLiteral("Console"),this);
    log_=new QPlainTextEdit(outputDock); log_->setReadOnly(true);
    log_->appendPlainText(QStringLiteral("FX3 Editor 0.3.7 started."));
    log_->appendPlainText(QStringLiteral("DXR viewport dispatches a hardware ray-traced triangle."));
    outputDock->setWidget(log_); addDockWidget(Qt::BottomDockWidgetArea,outputDock);

    createMenus();
    auto* refresh=new QTimer(this);
    connect(refresh,&QTimer::timeout,this,[this] {
        if(viewport_) statusBar()->showMessage(viewport_->rendererStatus());
    });
    refresh->start(1000);
    statusBar()->showMessage(QStringLiteral("Initializing DirectX 12 / DXR viewport..."));
}
void MainWindow::createMenus() {
    auto* file=menuBar()->addMenu(QStringLiteral("&File"));
    auto* quit=file->addAction(QStringLiteral("Exit"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit,&QAction::triggered,qApp,&QApplication::quit);
    auto* view=menuBar()->addMenu(QStringLiteral("&View"));
    auto* fullscreen=view->addAction(QStringLiteral("Toggle Fullscreen"));
    fullscreen->setShortcut(Qt::Key_F11);
    connect(fullscreen,&QAction::triggered,this,[this] {
        isFullScreen()?showNormal():showFullScreen();
    });
    auto* help=menuBar()->addMenu(QStringLiteral("&Help"));
    auto* about=help->addAction(QStringLiteral("About FX3 Editor"));
    connect(about,&QAction::triggered,this,[this] {
        log_->appendPlainText(QStringLiteral("FX3 Editor 0.3.7 | Qt 6 | Direct3D 12 | DXR"));
    });
}
