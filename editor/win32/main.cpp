#include "MainWindow.hpp"
#include <Windows.h>
#include <objbase.h>
#include <exception>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int showCommand){
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(com)&&com!=RPC_E_CHANGED_MODE){MessageBoxW(nullptr,L"COM initialization failed.",L"FX3 Editor",MB_ICONERROR);return 1;}
    int result=1;
    try{MainWindow app;if(!app.create(instance,showCommand)){MessageBoxW(nullptr,L"Could not create the FX3 Editor window.",L"FX3 Editor",MB_ICONERROR);}else result=app.run();}
    catch(const std::exception& e){std::string s=e.what();std::wstring w(s.begin(),s.end());MessageBoxW(nullptr,w.c_str(),L"FX3 Editor startup error",MB_ICONERROR);}
    if(SUCCEEDED(com))CoUninitialize();
    return result;
}
