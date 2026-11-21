#pragma once
#include <QWidget>
#include <memory>

class DXRViewport final : public QWidget {
    Q_OBJECT
public:
    explicit DXRViewport(QWidget* parent = nullptr);
    ~DXRViewport() override;
    QString rendererStatus() const;
protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
private:
    class Renderer;
    std::unique_ptr<Renderer> renderer_;
};
