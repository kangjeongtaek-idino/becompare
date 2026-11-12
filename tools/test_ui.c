/* UI behaviour test: the whole application runs in this process with its real windows and RichEdit
   controls (kept hidden), and a second thread inspects what a user would see:
   row colours, word level highlight, F7 / Shift+F7 navigation, the light and dark palettes.
   WinMain of main.asm is linked as bc_winmain. Set BC_UITEST_SHOW=1 to watch the window. */
#include <windows.h>
#include <richedit.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
extern int bc_winmain(HINSTANCE);
extern uint64_t g_dark;

#define ID_RA 201
#define ID_RB 202
#define ID_BTN_PREV 241
#define ID_BTN_NEXT 242
#define ID_LBL_INFO 250
#define IDM_LIGHT 1030
#define IDM_DARK 1031

static int checks, failed, shiftDown, boxes;
static DWORD mainTid;
static HWND frame, ra, rb, view;
static wchar_t cmdline[1200];
static void check(int ok,const char*name){checks++;printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)failed++;fflush(stdout);}

/* ---- the application's surroundings */
BOOL WINAPI __real_ShowWindow(HWND,int);
SHORT WINAPI __real_GetKeyState(int);
LPWSTR WINAPI __wrap_GetCommandLineW(void){return cmdline;}
BOOL WINAPI __wrap_ShowWindow(HWND h,int c){
 if(!getenv("BC_UITEST_SHOW")&&!(GetWindowLongPtrW(h,GWL_STYLE)&WS_CHILD))c=SW_HIDE;
 return __real_ShowWindow(h,c);
}
SHORT WINAPI __wrap_GetKeyState(int vk){
 if(vk==VK_SHIFT)return shiftDown?(SHORT)0x8000:0;
 if(vk==VK_CONTROL||vk==VK_MENU)return 0;
 return __real_GetKeyState(vk);
}
int WINAPI __wrap_MessageBoxW(HWND h,LPCWSTR t,LPCWSTR c,UINT f){(void)h;(void)c;boxes++;printf("unexpected message box (flags %u, %d characters)\n",f,(int)wcslen(t));return (f&15)?IDNO:IDOK;}
void __wrap_update_start(void){}                      /* no network access from a test */

/* ---- reading the screen */
static BOOL CALLBACK onTop(HWND h,LPARAM l){wchar_t c[32];(void)l;GetClassNameW(h,c,32);if(!wcscmp(c,L"BCFrame")){frame=h;return FALSE;}return TRUE;}
static BOOL CALLBACK onChild(HWND h,LPARAM l){wchar_t c[32];(void)l;GetClassNameW(h,c,32);if(!wcscmp(c,L"RICHEDIT50W")){int id=GetDlgCtrlID(h);if(id==ID_RA)ra=h;if(id==ID_RB)rb=h;}return TRUE;}
static int lineText(HWND h,int row,wchar_t*out,int cap){
 out[0]=(wchar_t)(cap-1);int n=(int)SendMessageW(h,EM_GETLINE,row,(LPARAM)out);
 while(n>0&&(out[n-1]==L'\r'||out[n-1]==L'\n'))n--;
 out[n]=0;return n;
}
/* background of one character: 1 with the colour, 0 when it has none of its own */
static int back(HWND h,int row,int col,COLORREF*c){
 LONG base=(LONG)SendMessageW(h,EM_LINEINDEX,row,0);
 CHARRANGE r={base+col,base+col+1};SendMessageW(h,EM_EXSETSEL,0,(LPARAM)&r);
 CHARFORMAT2W f;memset(&f,0,sizeof f);f.cbSize=sizeof f;SendMessageW(h,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&f);
 if(!(f.dwMask&CFM_BACKCOLOR)||(f.dwEffects&CFE_AUTOBACKCOLOR))return 0;
 *c=f.crBackColor;return 1;
}
/* palette: m modified row, w changed word, d deleted, i inserted, g empty side, . no background */
static COLORREF pal[128];
static void palette(int dark){
 pal['m']=dark?0x4E3A2B:0xF9E8DC;pal['w']=dark?0x7A573D:0xF0C7A9;pal['i']=dark?0x2F4026:0xDCF1DC;
 pal['d']=dark?0x403D3A:0xE8E8E8;pal['g']=dark?0x1B1816:0xF2F0EE;
}
/* every character of the row has the background its pattern letter names */
static void row(HWND h,int r,const wchar_t*text,const char*pat,const char*name){
 wchar_t got[1024];lineText(h,r,got,1024);
 if(text&&wcscmp(got,text)){printf("row %d holds other text (%d characters)\n",r,(int)wcslen(got));check(0,name);return;}
 for(int i=0;pat[i];i++){
  COLORREF c=0;int has=back(h,r,i,&c);
  if(pat[i]=='.'?has:(!has||c!=pal[(int)pat[i]])){printf("row %d column %d: expected '%c', got %s%06X\n",r,i,pat[i],has?"":"none ",(unsigned)c);check(0,name);return;}
 }
 check(1,name);
}
static int caretRow(HWND h){CHARRANGE r;SendMessageW(h,EM_EXGETSEL,0,(LPARAM)&r);return (int)SendMessageW(h,EM_LINEFROMCHAR,r.cpMin,0);}
static void caretTo(int r){for(int i=0;i<2;i++){HWND h=i?rb:ra;LRESULT p=SendMessageW(h,EM_LINEINDEX,r,0);SendMessageW(h,EM_SETSEL,p,p);}}
static int bothAt(int r){
 for(int i=0;i<40;i++){if(caretRow(ra)==r&&caretRow(rb)==r)return 1;Sleep(50);}
 printf("caret rows %d / %d, expected %d\n",caretRow(ra),caretRow(rb),r);return 0;
}
static int key(int vk,int shift,int want){shiftDown=shift;PostMessageW(ra,WM_KEYDOWN,vk,0);int ok=bothAt(want);Sleep(shift?100:0);shiftDown=0;return ok;}
static int info(const wchar_t*part){wchar_t t[300]={0};GetWindowTextW(GetDlgItem(view,ID_LBL_INFO),t,300);return wcsstr(t,part)!=NULL;}

/* ---- fixture */
static char longA[700],longB[700],patA[700],patB[700];
static wchar_t wLongA[700],wLongB[700];
static void put(const char*name,const char*text){FILE*f=fopen(name,"wb");fputs(text,f);fclose(f);}
static void fixture(void){
 /* 281 tokens before the change: too many for the word comparison, so the prefix / suffix span is used */
 char*a=longA,*b=longB,*pa=patA,*pb=patB;
 for(int i=0;i<140;i++){memcpy(a,"x ",2);a+=2;memcpy(b,"x ",2);b+=2;memcpy(pa,"mm",2);pa+=2;memcpy(pb,"mm",2);pb+=2;}
 memcpy(a,"OLD",3);a+=3;memcpy(pa,"www",3);pa+=3;memcpy(b,"NEW!",4);b+=4;memcpy(pb,"wwww",4);pb+=4;
 for(int i=0;i<140;i++){memcpy(a," y",2);a+=2;memcpy(b," y",2);b+=2;memcpy(pa,"mm",2);pa+=2;memcpy(pb,"mm",2);pb+=2;}
 *a=*b=*pa=*pb=0;
 for(int i=0;i<700;i++){wLongA[i]=(unsigned char)longA[i];wLongB[i]=(unsigned char)longB[i];}
 char t[2000];
 sprintf(t,"same line\r\nint count = 10; // total\r\nmid\r\nremoved line\r\ntail\r\nend\r\n%s\r\n",longA);put("ui_a.txt",t);
 sprintf(t,"same line\r\nint total = 10; // sum\r\nmid\r\ntail\r\nadded line\r\nend\r\n%s\r\n",longB);put("ui_b.txt",t);
}
/* rows: 0 same, 1 modified, 2 same, 3 only left, 4 same, 5 only right, 6 same, 7 modified (long) */
static void colours(const char*theme){
 char n[100];
#define ROW(h,r,text,pat,what) sprintf(n,"%s: %s",theme,what);row(h,r,text,pat,n)
 ROW(ra,0,L"same line",".........","unchanged row has no background (left)");
 ROW(rb,0,L"same line",".........","unchanged row has no background (right)");
 ROW(ra,1,L"int count = 10; // total","mmmmwwwwwmmmmmmmmmmwwwww","two changed words are two spans (left)");
 ROW(rb,1,L"int total = 10; // sum","mmmmwwwwwmmmmmmmmmmwww","two changed words are two spans (right)");
 ROW(ra,3,L"removed line","dddddddddddd","row only on the left is deleted colour");
 ROW(rb,3,NULL,"g","its empty side on the right is gap colour");
 ROW(ra,5,NULL,"g","empty side on the left is gap colour");
 ROW(rb,5,L"added line","iiiiiiiiii","row only on the right is inserted colour");
 ROW(ra,6,L"end","...","row after the changes has no background");
 ROW(ra,7,wLongA,patA,"long line highlights the span between prefix and suffix (left)");
 ROW(rb,7,wLongB,patB,"long line highlights the span between prefix and suffix (right)");
#undef ROW
}
static DWORD WINAPI watcher(void*arg){
 (void)arg;
 for(int i=0;i<200&&!frame;i++){EnumThreadWindows(mainTid,onTop,0);if(!frame)Sleep(50);}
 check(frame!=NULL,"frame window created");
 if(!frame){PostThreadMessageW(mainTid,WM_QUIT,0,0);return 0;}
 for(int i=0;i<200;i++){
  EnumChildWindows(frame,onChild,0);
  if(ra&&rb){view=GetParent(ra);if(info(L"차이 4개"))break;}
  Sleep(50);
 }
 check(ra&&rb,"text session has both panes");
 if(!ra||!rb){PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 check(info(L"차이 4개"),"compare ran and reports 4 differences");
 check(!IsWindowVisible(frame)||getenv("BC_UITEST_SHOW"),"window stays hidden during the test");

 SendMessageW(frame,WM_COMMAND,IDM_LIGHT,0);
 check(g_dark==0,"light theme selected");palette(0);colours("light");
 SendMessageW(frame,WM_COMMAND,IDM_DARK,0);
 check(g_dark==1,"dark theme selected");palette(1);colours("dark");
 SendMessageW(frame,WM_COMMAND,IDM_LIGHT,0);
 check(g_dark==0,"back to the light theme");palette(0);colours("light again");

 /* F7 walks the differences downwards and stops at the last, Shift+F7 walks back */
 caretTo(0);
 check(key(VK_F7,0,1)&&info(L"현재 1/4"),"F7 from the top goes to the first difference");
 check(key(VK_F7,0,3)&&info(L"현재 2/4"),"F7 goes to the second difference");
 check(key(VK_F7,0,5)&&info(L"현재 3/4"),"F7 goes to the third difference");
 check(key(VK_F7,0,7)&&info(L"현재 4/4"),"F7 goes to the last difference");
 PostMessageW(ra,WM_KEYDOWN,VK_F7,0);Sleep(300);
 check(bothAt(7)&&info(L"현재 4/4"),"F7 on the last difference stays there");
 check(key(VK_F7,1,5)&&info(L"현재 3/4"),"Shift+F7 goes to the previous difference");
 check(key(VK_F7,1,3)&&key(VK_F7,1,1)&&info(L"현재 1/4"),"Shift+F7 walks back to the first difference");
 shiftDown=1;PostMessageW(ra,WM_KEYDOWN,VK_F7,0);Sleep(300);shiftDown=0;
 check(bothAt(1),"Shift+F7 on the first difference stays there");
 /* a caret inside a block: F7 leaves the block, Shift+F7 from below returns to its first row */
 caretTo(4);
 check(key(VK_F7,0,5),"F7 from an unchanged row goes to the next difference below");
 caretTo(4);
 check(key(VK_F7,1,3),"Shift+F7 from an unchanged row goes to the difference above");
 /* the toolbar buttons do the same */
 caretTo(0);
 SendMessageW(view,WM_COMMAND,ID_BTN_NEXT,0);check(bothAt(1),"next-difference button");
 SendMessageW(view,WM_COMMAND,ID_BTN_NEXT,0);check(bothAt(3),"next-difference button again");
 SendMessageW(view,WM_COMMAND,ID_BTN_PREV,0);check(bothAt(1),"previous-difference button");

 check(!SendMessageW(ra,EM_GETMODIFY,0,0)&&!SendMessageW(rb,EM_GETMODIFY,0,0),"colouring and navigation leave the panes unmodified");
 PostMessageW(frame,WM_CLOSE,0,0);
 return 0;
}
static DWORD WINAPI watchdog(void*arg){(void)arg;Sleep(60000);printf("FAIL timed out\n");fflush(stdout);ExitProcess(3);}
int main(void){
 wchar_t a[400],b[400];
 fixture();
 GetFullPathNameW(L"ui_a.txt",400,a,NULL);GetFullPathNameW(L"ui_b.txt",400,b,NULL);
 swprintf(cmdline,1200,L"bcgui.exe text \"%ls\" \"%ls\"",a,b);
 mainTid=GetCurrentThreadId();
 CloseHandle(CreateThread(NULL,0,watchdog,NULL,0,NULL));
 HANDLE w=CreateThread(NULL,0,watcher,NULL,0,NULL);
 bc_winmain(GetModuleHandleW(NULL));
 check(WaitForSingleObject(w,5000)==WAIT_OBJECT_0,"application closed normally");
 check(boxes==0,"no message box appeared");
 printf("%d checks, %d failures\n",checks,failed);fflush(stdout);
 ExitProcess(failed?1:0);
}
