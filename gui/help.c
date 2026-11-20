/* Owned, resizable offline HTML help window. Windows' ATL hosts WebBrowser in-process. */
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <exdisp.h>
#include <oleidl.h>
#include <oleauto.h>
#include <wchar.h>

typedef BOOL (WINAPI *AxInit)(void);
typedef HRESULT (WINAPI *AxControl)(HWND,IUnknown**);
static HWND help_window, browser;
static IWebBrowser2 *web;
static HMODULE atl;
static wchar_t home_path[520];
static const GUID browser_iid={0xd30c1661,0xcdaf,0x11d0,{0x8a,0x3e,0x00,0xc0,0x4f,0xc9,0xe2,0x6e}};
static const GUID active_iid={0x00000117,0x0000,0x0000,{0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x46}};
enum {HELP_BACK=901,HELP_FORWARD,HELP_HOME,HELP_CLOSE};
static void home(void){
    VARIANT url,empty;VariantInit(&url);VariantInit(&empty);
    V_VT(&url)=VT_BSTR;V_BSTR(&url)=SysAllocString(home_path);
    if(V_BSTR(&url))IWebBrowser2_Navigate2(web,&url,&empty,&empty,&empty,&empty);
    VariantClear(&url);
}
static void layout(HWND hwnd){
    RECT r;GetClientRect(hwnd,&r);int x=10;
    for(int id=HELP_BACK;id<=HELP_CLOSE;id++,x+=100)MoveWindow(GetDlgItem(hwnd,id),x,8,92,28,TRUE);
    if(browser)MoveWindow(browser,0,44,r.right,r.bottom>44?r.bottom-44:0,TRUE);
}
static LRESULT CALLBACK help_proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_SIZE:layout(hwnd);return 0;
    case WM_GETMINMAXINFO:((MINMAXINFO*)l)->ptMinTrackSize=(POINT){480,320};return 0;
    case WM_COMMAND:
        switch(LOWORD(w)){
        case HELP_BACK:if(web)IWebBrowser2_GoBack(web);return 0;
        case HELP_FORWARD:if(web)IWebBrowser2_GoForward(web);return 0;
        case HELP_HOME:if(web)home();return 0;
        case HELP_CLOSE:DestroyWindow(hwnd);return 0;
        }break;
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:
        if(web){IWebBrowser2_Stop(web);IWebBrowser2_Release(web);web=NULL;}
        browser=NULL;help_window=NULL;return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
int help_translate(MSG *msg){
    if(!help_window||!(msg->hwnd==help_window||IsChild(help_window,msg->hwnd)))return 0;
    if(msg->message==WM_KEYDOWN&&msg->wParam==VK_F1)return 1;
    if(msg->message==WM_KEYDOWN&&msg->wParam==VK_ESCAPE){DestroyWindow(help_window);return 1;}
    if(web){IOleInPlaceActiveObject *active=NULL;
        if(SUCCEEDED(IWebBrowser2_QueryInterface(web,&active_iid,(void**)&active))){
            HRESULT result=IOleInPlaceActiveObject_TranslateAccelerator(active,msg);
            IOleInPlaceActiveObject_Release(active);if(result==S_OK)return 1;
        }
    }
    return IsDialogMessageW(help_window,msg)?1:2;
}
int help_show(HWND owner,const wchar_t *path){
    if(help_window){ShowWindow(help_window,SW_RESTORE);SetForegroundWindow(help_window);return 1;}
    if(wcslen(path)>=520)return 0;
    wcscpy(home_path,path);
    if(!atl){
        wchar_t dll[MAX_PATH];UINT n=GetSystemDirectoryW(dll,MAX_PATH);
        if(!n||n+9>=MAX_PATH)return 0;
        wcscat(dll,L"\\atl.dll");atl=LoadLibraryW(dll);
    }
    if(!atl)return 0;
    AxInit init=(AxInit)(void*)GetProcAddress(atl,"AtlAxWinInit");
    AxControl control=(AxControl)(void*)GetProcAddress(atl,"AtlAxGetControl");
    if(!init||!control||!init())return 0;
    HINSTANCE instance=GetModuleHandleW(NULL);
    WNDCLASSW cls={0};cls.lpfnWndProc=help_proc;cls.hInstance=instance;cls.hCursor=LoadCursorW(NULL,IDC_ARROW);cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);cls.lpszClassName=L"BCHelp";
    if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return 0;
    RECT work;SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    int width=1100,height=800;if(width>work.right-work.left)width=work.right-work.left;if(height>work.bottom-work.top)height=work.bottom-work.top;
    help_window=CreateWindowExW(WS_EX_CONTROLPARENT,L"BCHelp",L"BeCompare 사용 설명서",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,work.left+(work.right-work.left-width)/2,work.top+(work.bottom-work.top-height)/2,width,height,owner,NULL,instance,NULL);
    if(!help_window)return 0;
    const wchar_t *labels[]={L"뒤로",L"앞으로",L"목차",L"닫기"};
    for(int i=0;i<4;i++){
        HWND button=CreateWindowExW(0,L"BUTTON",labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,0,0,0,0,help_window,(HMENU)(INT_PTR)(HELP_BACK+i),instance,NULL);
        SendMessageW(button,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    }
    browser=CreateWindowExW(0,L"AtlAxWin",L"Shell.Explorer.2",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_CLIPCHILDREN,0,44,width,height-44,help_window,NULL,instance,NULL);
    IUnknown *unknown=NULL;
    if(!browser||FAILED(control(browser,&unknown))||!unknown){DestroyWindow(help_window);return 0;}
    HRESULT result=IUnknown_QueryInterface(unknown,&browser_iid,(void**)&web);IUnknown_Release(unknown);
    if(FAILED(result)||!web){DestroyWindow(help_window);return 0;}
    IWebBrowser2_put_Silent(web,VARIANT_TRUE);
    IWebBrowser2_put_RegisterAsBrowser(web,VARIANT_FALSE);
    home();layout(help_window);ShowWindow(help_window,SW_SHOW);SetForegroundWindow(help_window);SetFocus(browser);return 1;
}
