#pragma once
#include "SceneDocument.hpp"
#include "DXRViewport.hpp"
#include <Windows.h>
#include <array>
#include <string>

class MainWindow final {
public:
    MainWindow()=default;
    ~MainWindow();
    bool create(HINSTANCE instance,int showCommand);
    int run();
private:
    static LRESULT CALLBACK windowProc(HWND,UINT,WPARAM,LPARAM);
    LRESULT handleMessage(UINT,WPARAM,LPARAM);
    void createMenus();
    void createChildren();
    void layout(int width,int height);
    void rebuildOutliner();
    void refreshProperties();
    void updateTitle();
    void appendLog(const std::wstring&);
    void setStatus(const std::wstring&);
    bool maybeSave();
    bool saveScene(bool saveAs);
    bool openScene();
    void newScene();
    void addTriangle();
    void deleteSelected();
    void selectFromTree();
    void commitProperty(int controlId);
    std::wstring chooseSceneFile(bool save);
    HWND hwnd_=nullptr,tree_=nullptr,properties_=nullptr,console_=nullptr,viewportWindow_=nullptr,status_=nullptr,toolbar_=nullptr;
    HWND nameEdit_=nullptr,typeLabel_=nullptr;
    std::array<HWND,9> transformEdits_{};
    HINSTANCE instance_=nullptr;
    SceneDocument scene_;
    DXRViewport viewport_;
    std::wstring selectedId_;
    bool rendererReady_=false;
};
