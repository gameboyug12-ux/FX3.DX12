#include "MainWindow.hpp"
#include <QApplication>
#include <QMessageBox>
#include <exception>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("FX3 Editor"));
    QApplication::setApplicationVersion(QStringLiteral("0.3.8"));
    try {
        MainWindow window;
        window.show();
        return app.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, QStringLiteral("FX3 Editor startup error"),
            QString::fromLocal8Bit(error.what()));
        return 1;
    }
}
