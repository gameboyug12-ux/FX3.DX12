#include "MainWindow.hpp"
#include <Windows.h>
#include <CommCtrl.h>
#include <ShObjIdl.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <string>

namespace {
constexpr wchar_t kClassName[]=L"FX3Win32EditorWindow";
constexpr int ID_NEW=1001,ID_OPEN=1002,ID_SAVE=1003,ID_SAVEAS=1004,ID_EXIT=1005;
constexpr int ID_ADD=1010,ID_DELETE=1011,ID_ABOUT=1020,ID_NAME=1100,ID_TRANSFORM=1200;
constexpr int ID_TREE=1300,ID_CONSOLE=1301,ID_VIEWPORT=1302,ID_STATUS=1303;
std::wstring controlText(HWND h){int n=GetWindowTextLengthW(h);std::wstring s((size_t)n+1,L'\0');if(n)GetWindowTextW(h,s.data(),n+1);s.resize((size_t)n);return s;}
void setFont(HWND h){SendMessageW(h,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);}
void appendText(HWND edit,const std::wstring& text){if(!edit)return;int n=GetWindowTextLengthW(edit);SendMessageW(edit,EM_SETSEL,n,n);std::wstring line=text+L"\r\n";SendMessageW(edit,EM_REPLACESEL,FALSE,(LPARAM)line.c_str());}
std::wstring formatFloat(float v){wchar_t b[64];swprintf_s(b,L"%.3f",v);return b;}
}
MainWindow::~MainWindow()=default;
bool MainWindow::create(HINSTANCE instance,int showCommand){
    instance_=instance;INITCOMMONCONTROLSEX ic{sizeof(ic),ICC_TREEVIEW_CLASSES|ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&ic);
    WNDCLASSEXW wc{sizeof(wc)};wc.style=CS_HREDRAW|CS_VREDRAW;wc.lpfnWndProc=windowProc;wc.hInstance=instance;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName=kClassName;
    if(!RegisterClassExW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return false;
    hwnd_=CreateWindowExW(0,kClassName,L"FX3 Editor 0.3.9 RC-1",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,1500,900,nullptr,nullptr,instance,this);
    if(!hwnd_)return false;ShowWindow(hwnd_,showCommand);UpdateWindow(hwnd_);return true;
}
int MainWindow::run(){MSG msg{};while(GetMessageW(&msg,nullptr,0,0)>0){TranslateMessage(&msg);DispatchMessageW(&msg);}return (int)msg.wParam;}
LRESULT CALLBACK MainWindow::windowProc(HWND h,UINT m,WPARAM w,LPARAM l){
    MainWindow* self=reinterpret_cast<MainWindow*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(m==WM_NCCREATE){auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);self=static_cast<MainWindow*>(cs->lpCreateParams);self->hwnd_=h;SetWindowLongPtrW(h,GWLP_USERDATA,(LONG_PTR)self);}
    return self?self->handleMessage(m,w,l):DefWindowProcW(h,m,w,l);
}
void MainWindow::createMenus(){
    HMENU root=CreateMenu(),file=CreatePopupMenu(),scene=CreatePopupMenu(),help=CreatePopupMenu();
    AppendMenuW(file,MF_STRING,ID_NEW,L"&New Scene\tCtrl+N");AppendMenuW(file,MF_STRING,ID_OPEN,L"&Open Scene...\tCtrl+O");AppendMenuW(file,MF_STRING,ID_SAVE,L"&Save\tCtrl+S");AppendMenuW(file,MF_STRING,ID_SAVEAS,L"Save &As...");AppendMenuW(file,MF_SEPARATOR,0,nullptr);AppendMenuW(file,MF_STRING,ID_EXIT,L"E&xit");
    AppendMenuW(scene,MF_STRING,ID_ADD,L"Add &Triangle Mesh\tCtrl+T");AppendMenuW(scene,MF_STRING,ID_DELETE,L"&Delete Selected\tDelete");AppendMenuW(help,MF_STRING,ID_ABOUT,L"&About FX3 Editor");
    AppendMenuW(root,MF_POPUP,(UINT_PTR)file,L"&File");AppendMenuW(root,MF_POPUP,(UINT_PTR)scene,L"&Scene");AppendMenuW(root,MF_POPUP,(UINT_PTR)help,L"&Help");SetMenu(hwnd_,root);
}
void MainWindow::createChildren(){
    toolbar_=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,0,0,0,0,hwnd_,nullptr,instance_,nullptr);
    const wchar_t* labels[]={L"New",L"Open",L"Save",L"+ Triangle",L"Delete"};const int ids[]={ID_NEW,ID_OPEN,ID_SAVE,ID_ADD,ID_DELETE};
    for(int i=0;i<5;++i){HWND b=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,6+i*86,5,80,28,hwnd_,(HMENU)(INT_PTR)ids[i],instance_,nullptr);setFont(b);}
    tree_=CreateWindowExW(WS_EX_CLIENTEDGE,WC_TREEVIEWW,L"",WS_CHILD|WS_VISIBLE|TVS_HASLINES|TVS_LINESATROOT|TVS_HASBUTTONS|TVS_SHOWSELALWAYS,0,0,0,0,hwnd_,(HMENU)ID_TREE,instance_,nullptr);setFont(tree_);
    properties_=CreateWindowExW(WS_EX_CLIENTEDGE,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_LEFT,0,0,0,0,hwnd_,nullptr,instance_,nullptr);
    SetWindowSubclass(properties_,[](HWND panel,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR,DWORD_PTR)->LRESULT{if(msg==WM_COMMAND){SendMessageW(GetParent(panel),msg,wp,lp);return 0;}return DefSubclassProc(panel,msg,wp,lp);},1,0);
    auto label=[&](const wchar_t* text,int x,int y,int w=74){HWND h=CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE|SS_LEFT,x,y,w,22,properties_,nullptr,instance_,nullptr);setFont(h);};
    auto edit=[&](int id,int x,int y,int w=130){HWND h=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,x,y,w,24,properties_,(HMENU)(INT_PTR)id,instance_,nullptr);setFont(h);return h;};
    label(L"Name",10,12);nameEdit_=edit(ID_NAME,82,9,160);label(L"Type",10,43);typeLabel_=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE,82,43,160,22,properties_,nullptr,instance_,nullptr);setFont(typeLabel_);
    const wchar_t* groups[]={L"Location",L"Rotation",L"Scale"};const wchar_t* axes[]={L"X",L"Y",L"Z"};
    for(int g=0;g<3;++g){label(groups[g],10,78+g*104,220);for(int a=0;a<3;++a){int i=g*3+a;label(axes[a],14,104+g*104,50);transformEdits_[i]=edit(ID_TRANSFORM+i,82,101+g*104,160);}}
    console_=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY|WS_VSCROLL,0,0,0,0,hwnd_,(HMENU)ID_CONSOLE,instance_,nullptr);setFont(console_);
    viewportWindow_=CreateWindowExW(WS_EX_CLIENTEDGE,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_BLACKRECT,0,0,0,0,hwnd_,(HMENU)ID_VIEWPORT,instance_,nullptr);
    status_=CreateWindowExW(0,STATUSCLASSNAMEW,L"Ready | DX12 / DXR",WS_CHILD|WS_VISIBLE|SBARS_SIZEGRIP,0,0,0,0,hwnd_,(HMENU)ID_STATUS,instance_,nullptr);
    rebuildOutliner();refreshProperties();appendLog(L"FX3 Editor 0.3.9 RC-1 initialized (native Win32).");appendLog(L"Scene files use the versioned .fx3scene JSON format.");appendLog(L"DXR preview currently uses the renderer's test triangle.");
}
void MainWindow::layout(int width,int height){
    constexpr int statusHeight=22,top=40,bottom=165,left=230,right=270;
    MoveWindow(toolbar_,0,0,width,top,TRUE);MoveWindow(tree_,0,top,left,std::max(100,height-top-bottom),TRUE);
    MoveWindow(properties_,width-right,top,right,std::max(100,height-top-bottom),TRUE);
    MoveWindow(viewportWindow_,left,top,std::max(100,width-left-right),std::max(100,height-top-bottom),TRUE);
    MoveWindow(console_,0,height-bottom,width,bottom-statusHeight,TRUE);MoveWindow(status_,0,height-statusHeight,width,statusHeight,TRUE);
    if(rendererReady_)try{viewport_.resize((UINT)std::max(1,width-left-right),(UINT)std::max(1,height-top-bottom));}catch(const std::exception& e){std::string s=e.what();appendLog(L"DXR resize failed: "+std::wstring(s.begin(),s.end()));}
}
void MainWindow::rebuildOutliner(){
    TreeView_DeleteAllItems(tree_);TVINSERTSTRUCTW root{};root.hParent=TVI_ROOT;root.hInsertAfter=TVI_LAST;root.item.mask=TVIF_TEXT|TVIF_PARAM;root.item.pszText=(LPWSTR)L"Scene";root.item.lParam=0;HTREEITEM rt=TreeView_InsertItem(tree_,&root);
    int i=0;HTREEITEM selected=nullptr;for(const auto& o:scene_.objects()){TVINSERTSTRUCTW t{};t.hParent=rt;t.hInsertAfter=TVI_LAST;t.item.mask=TVIF_TEXT|TVIF_PARAM;t.item.pszText=(LPWSTR)o.name.c_str();t.item.lParam=(LPARAM)(++i);HTREEITEM item=TreeView_InsertItem(tree_,&t);if(o.id==selectedId_)selected=item;}
    TreeView_Expand(tree_,rt,TVE_EXPAND);if(!selected&&!scene_.objects().empty()){selectedId_=scene_.objects().front().id;selected=TreeView_GetChild(tree_,rt);}if(selected)TreeView_SelectItem(tree_,selected);
}
void MainWindow::refreshProperties(){
    const auto* o=scene_.find(selectedId_);EnableWindow(nameEdit_,o!=nullptr);EnableWindow(typeLabel_,o!=nullptr);for(auto h:transformEdits_)EnableWindow(h,o!=nullptr);
    if(!o){SetWindowTextW(nameEdit_,L"");SetWindowTextW(typeLabel_,L"");for(auto h:transformEdits_)SetWindowTextW(h,L"");return;}
    SetWindowTextW(nameEdit_,o->name.c_str());SetWindowTextW(typeLabel_,o->type.c_str());
    for(int i=0;i<9;++i){const auto& v=i<3?o->position:i<6?o->rotation:o->scale;auto s=formatFloat(v[i%3]);SetWindowTextW(transformEdits_[i],s.c_str());}
}
void MainWindow::updateTitle(){std::wstring p=scene_.filePath().empty()?L"Untitled":scene_.filePath();SetWindowTextW(hwnd_,(L"FX3 Editor 0.3.9 RC-1 — "+p+(scene_.isDirty()?L" *":L"")).c_str());}
void MainWindow::appendLog(const std::wstring& s){appendText(console_,s);}
void MainWindow::setStatus(const std::wstring& s){SendMessageW(status_,SB_SETTEXTW,0,(LPARAM)s.c_str());}
bool MainWindow::maybeSave(){if(!scene_.isDirty())return true;int r=MessageBoxW(hwnd_,L"The current scene has unsaved changes. Save before continuing?",L"Unsaved Scene",MB_ICONWARNING|MB_YESNOCANCEL);if(r==IDCANCEL)return false;if(r==IDYES)return saveScene(false);return true;}
std::wstring MainWindow::chooseSceneFile(bool save){
    IFileDialog* dlg=nullptr;HRESULT hr=CoCreateInstance(save?CLSID_FileSaveDialog:CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dlg));if(FAILED(hr))return {};
    COMDLG_FILTERSPEC types[]={{L"FX3 Scene (*.fx3scene)",L"*.fx3scene"},{L"JSON (*.json)",L"*.json"}};dlg->SetFileTypes(2,types);dlg->SetDefaultExtension(L"fx3scene");if(save)dlg->SetFileName(L"Untitled.fx3scene");
    hr=dlg->Show(hwnd_);if(FAILED(hr)){dlg->Release();return {};}IShellItem* item=nullptr;hr=dlg->GetResult(&item);dlg->Release();if(FAILED(hr))return {};PWSTR path=nullptr;hr=item->GetDisplayName(SIGDN_FILESYSPATH,&path);item->Release();if(FAILED(hr))return {};std::wstring result=path;CoTaskMemFree(path);return result;
}
bool MainWindow::saveScene(bool saveAs){std::wstring path=scene_.filePath();if(saveAs||path.empty()){path=chooseSceneFile(true);if(path.empty())return false;}std::wstring error;if(!scene_.save(path,&error)){MessageBoxW(hwnd_,error.c_str(),L"Save Failed",MB_ICONERROR);appendLog(L"Save failed: "+error);return false;}updateTitle();appendLog(L"Scene saved: "+path);setStatus(L"Scene saved");return true;}
bool MainWindow::openScene(){if(!maybeSave())return false;auto path=chooseSceneFile(false);if(path.empty())return false;std::wstring error;if(!scene_.load(path,&error)){MessageBoxW(hwnd_,error.c_str(),L"Open Failed",MB_ICONERROR);appendLog(L"Open failed: "+error);return false;}selectedId_.clear();rebuildOutliner();refreshProperties();updateTitle();appendLog(L"Scene opened: "+path);return true;}
void MainWindow::newScene(){if(!maybeSave())return;scene_.reset();selectedId_.clear();rebuildOutliner();refreshProperties();updateTitle();appendLog(L"Created a new scene.");}
void MainWindow::addTriangle(){selectedId_=scene_.addObject(L"Triangle Mesh "+std::to_wstring(scene_.objects().size()),L"triangle");rebuildOutliner();refreshProperties();updateTitle();appendLog(L"Added triangle mesh object to scene document.");}
void MainWindow::deleteSelected(){if(selectedId_.empty())return;auto* o=scene_.find(selectedId_);if(!o)return;auto name=o->name;if(scene_.removeObject(selectedId_)){selectedId_.clear();rebuildOutliner();refreshProperties();updateTitle();appendLog(L"Deleted object: "+name);}}
void MainWindow::selectFromTree(){HTREEITEM item=TreeView_GetSelection(tree_);if(!item)return;TVITEMW t{};t.mask=TVIF_PARAM;t.hItem=item;if(!TreeView_GetItem(tree_,&t)||t.lParam<=0)return;size_t i=(size_t)t.lParam-1;if(i<scene_.objects().size()){selectedId_=scene_.objects()[i].id;refreshProperties();}}
void MainWindow::commitProperty(int id){if(id==ID_NAME){auto s=controlText(nameEdit_);if(!scene_.setObjectName(selectedId_,s)){refreshProperties();return;}rebuildOutliner();updateTitle();return;}int c=id-ID_TRANSFORM;if(c<0||c>=9)return;double v;if(!parseDouble(transformEdits_[c],v)){refreshProperties();return;}if(scene_.setTransformComponent(selectedId_,c,v))updateTitle();else refreshProperties();}
LRESULT MainWindow::handleMessage(UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE:createMenus();createChildren();SetTimer(hwnd_,1,16,nullptr);try{viewport_.initialize(viewportWindow_,960,600);rendererReady_=true;appendLog(L"DXR viewport initialized.");setStatus(viewport_.status());}catch(const std::exception& e){std::string s=e.what();appendLog(L"DXR initialization failed: "+std::wstring(s.begin(),s.end()));}return 0;
    case WM_SIZE:if(status_){RECT r{};GetClientRect(hwnd_,&r);layout(r.right,r.bottom);}return 0;
    case WM_KEYDOWN:if((GetKeyState(VK_CONTROL)&0x8000)!=0){switch(w){case 0x4E:newScene();return 0;case 0x4F:openScene();return 0;case 0x53:saveScene(false);return 0;case 0x54:addTriangle();return 0;}}if(w==VK_DELETE){deleteSelected();return 0;}break;
    case WM_COMMAND:{int id=LOWORD(w),notify=HIWORD(w);switch(id){case ID_NEW:newScene();return 0;case ID_OPEN:openScene();return 0;case ID_SAVE:saveScene(false);return 0;case ID_SAVEAS:saveScene(true);return 0;case ID_EXIT:SendMessageW(hwnd_,WM_CLOSE,0,0);return 0;case ID_ADD:addTriangle();return 0;case ID_DELETE:deleteSelected();return 0;case ID_ABOUT:MessageBoxW(hwnd_,L"FX3.DX12 Editor 0.3.9 RC-1\nNative Win32 | C++20 | Direct3D 12 | DXR",L"About FX3 Editor",MB_OK|MB_ICONINFORMATION);return 0;default:if(notify==EN_KILLFOCUS&&(id==ID_NAME||(id>=ID_TRANSFORM&&id<ID_TRANSFORM+9)))commitProperty(id);return 0;}}
    case WM_NOTIFY:if(((LPNMHDR)l)->idFrom==ID_TREE&&((LPNMHDR)l)->code==TVN_SELCHANGEDW){selectFromTree();return 0;}break;
    case WM_TIMER:if(w==1&&rendererReady_){try{viewport_.render();}catch(const std::exception& e){KillTimer(hwnd_,1);std::string s=e.what();appendLog(L"DXR render stopped: "+std::wstring(s.begin(),s.end()));}}return 0;
    case WM_CLOSE:if(!maybeSave())return 0;DestroyWindow(hwnd_);return 0;
    case WM_DESTROY:KillTimer(hwnd_,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd_,m,w,l);
}
