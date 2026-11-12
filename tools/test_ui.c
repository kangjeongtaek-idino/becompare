/* UI behaviour test: the whole application runs in this process with its real windows and RichEdit
   controls (kept hidden), and a second thread inspects what a user would see:
   row colours, word level highlight, F7 / Shift+F7 navigation, the light and dark palettes.
   "test_ui merge" opens the same two files as a merge session and checks the colours of its panes.
   "test_ui fmerge" runs a folder merge into an output folder and checks what it reports and writes.
   WinMain of main.asm is linked as bc_winmain. Set BC_UITEST_SHOW=1 to watch the window. */
#include <windows.h>
#include <richedit.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
extern int bc_winmain(HINSTANCE);
extern uint64_t g_dark;
extern uint32_t th_lonly, th_ronly, th_conf;

#define ID_RA 201
#define ID_RB 202
#define ID_BTN_PREV 241
#define ID_BTN_NEXT 242
#define ID_LBL_INFO 250
#define ID_CHK_SYN 215
#define ID_RL 601
#define ID_RR 603
#define ID_RM 610
#define ID_BTN_RUNM 710
#define IDM_LIGHT 1030
#define IDM_DARK 1031

static int checks, failed, shiftDown, boxes, mergeMode, fmergeMode, expectBoxes, shownBoxes;
static wchar_t lastBox[400];
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
int WINAPI __wrap_MessageBoxW(HWND h,LPCWSTR t,LPCWSTR c,UINT f){(void)h;(void)c;
 if(expectBoxes){shownBoxes++;wcsncpy(lastBox,t,399);lastBox[399]=0;return (f&15)==MB_YESNO?IDYES:IDOK;}   /* the test answers Yes */
 boxes++;printf("unexpected message box (flags %u, %d characters)\n",f,(int)wcslen(t));return (f&15)?IDNO:IDOK;}
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
/* text colour of one character */
static COLORREF fore(HWND h,int row,int col){
 LONG base=(LONG)SendMessageW(h,EM_LINEINDEX,row,0);
 CHARRANGE r={base+col,base+col+1};SendMessageW(h,EM_EXSETSEL,0,(LPARAM)&r);
 CHARFORMAT2W f;memset(&f,0,sizeof f);f.cbSize=sizeof f;SendMessageW(h,EM_GETCHARFORMAT,SCF_SELECTION,(LPARAM)&f);
 return (f.dwEffects&CFE_AUTOCOLOR)?0xFF000000:f.crTextColor;
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
 sprintf(t,"same line\r\nint count = 10; // total\r\nmid\r\nremoved line\r\ntail\r\nend\r\n%s\r\n",longA);put("ui_a.c",t);
 sprintf(t,"same line\r\nint total = 10; // sum\r\nmid\r\ntail\r\nadded line\r\nend\r\n%s\r\n",longB);put("ui_b.c",t);
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
/* ---- merge session: the side panes are read-only, the output pane holds the conflict blocks */
static HWND wanted;static int wantedId;
static BOOL CALLBACK onId(HWND h,LPARAM l){(void)l;if(GetDlgCtrlID(h)==wantedId){wchar_t c[32];GetClassNameW(h,c,32);if(!wcscmp(c,L"RICHEDIT50W")){wanted=h;return FALSE;}}return TRUE;}
static HWND pane(int id){wanted=NULL;wantedId=id;EnumChildWindows(frame,onId,0);return wanted;}
static int is(HWND h,int r,int col,COLORREF want){COLORREF c=0;int has=back(h,r,col,&c);if(!has||c!=want)printf("row %d column %d: expected %06X, got %s%06X\n",r,col,(unsigned)want,has?"":"none ",(unsigned)c);return has&&c==want;}
static int none(HWND h,int r,int col){COLORREF c=0;return !back(h,r,col,&c);}
static void mergeChecks(void){
 HWND l=NULL,r=NULL,m=NULL;wchar_t t[1024];
 for(int i=0;i<200;i++){l=pane(ID_RL);r=pane(ID_RR);m=pane(ID_RM);if(l&&r&&m&&lineText(m,1,t,1024)>0)break;Sleep(50);}
 check(l&&r&&m,"merge session has its panes");
 if(!l||!r||!m)return;
 lineText(m,1,t,1024);check(!wcsncmp(t,L"<<<<<<<",7),"merge ran: the output starts its first conflict on row 1");
 /* left: rows 1, 3, 6 differ; right: rows 1, 4, 6 */
 check(is(l,1,0,th_lonly)&&is(l,1,20,th_lonly)&&is(l,3,0,th_lonly)&&is(l,6,5,th_lonly),"left pane: changed lines have the left colour");
 check(none(l,0,0)&&none(l,2,0)&&none(l,4,0),"left pane: other lines have no background");
 check(is(r,1,0,th_ronly)&&is(r,4,0,th_ronly)&&is(r,6,5,th_ronly),"right pane: changed lines have the right colour");
 check(none(r,0,0)&&none(r,2,0)&&none(r,3,0),"right pane: other lines have no background");
 /* output: same line / <<<<<<< / left line / ======= / right line / >>>>>>> / mid */
 check(is(m,1,0,th_conf)&&is(m,3,0,th_conf)&&is(m,5,0,th_conf),"output pane: marker lines have the conflict colour");
 check(is(m,2,0,th_lonly)&&is(m,4,0,th_ronly),"output pane: the two sides of a conflict have their colours");
 check(none(m,0,0)&&none(m,6,0),"output pane: lines outside a conflict have no background");
 check((GetWindowLongPtrW(l,GWL_STYLE)&ES_READONLY)&&(GetWindowLongPtrW(r,GWL_STYLE)&ES_READONLY),"side panes are read-only again after colouring");
 check(!(GetWindowLongPtrW(m,GWL_STYLE)&ES_READONLY),"output pane stays editable");
 SendMessageW(m,EM_SETMODIFY,0,0);                    /* nothing to save: no question on close */
}
/* ---- folder merge: left, right, base and an output folder that already holds two files
   c1: conflict, the output file is older than the inputs   -> still an unresolved conflict
   c2: conflict, the output file was written after the inputs -> counts as merged
   n : only on the left                                       -> copied */
static void putfile(const wchar_t*path,const char*text){FILE*f=_wfopen(path,L"wb");if(f){fputs(text,f);fclose(f);}}
static void stamp(const wchar_t*path,int minutes){
 HANDLE h=CreateFileW(path,FILE_WRITE_ATTRIBUTES,7,NULL,OPEN_EXISTING,0,NULL);FILETIME ft;GetSystemTimeAsFileTime(&ft);
 ULARGE_INTEGER u;u.LowPart=ft.dwLowDateTime;u.HighPart=ft.dwHighDateTime;u.QuadPart+=(LONGLONG)minutes*600000000LL;
 ft.dwLowDateTime=u.LowPart;ft.dwHighDateTime=u.HighPart;SetFileTime(h,NULL,NULL,&ft);CloseHandle(h);
}
static int holds(const wchar_t*path,const char*text){char b[64]={0};FILE*f=_wfopen(path,L"rb");if(!f)return 0;size_t n=fread(b,1,63,f);fclose(f);return n==strlen(text)&&!memcmp(b,text,n);}
static void fmergeFixture(void){
 CreateDirectoryW(L"fm",NULL);CreateDirectoryW(L"fm\\L",NULL);CreateDirectoryW(L"fm\\R",NULL);CreateDirectoryW(L"fm\\B",NULL);CreateDirectoryW(L"fm\\O",NULL);
 putfile(L"fm\\B\\c1.txt","base");putfile(L"fm\\L\\c1.txt","L");putfile(L"fm\\R\\c1.txt","R");
 putfile(L"fm\\B\\c2.txt","base");putfile(L"fm\\L\\c2.txt","L2");putfile(L"fm\\R\\c2.txt","R2");
 putfile(L"fm\\L\\n.txt","new");DeleteFileW(L"fm\\O\\n.txt");
 putfile(L"fm\\O\\c1.txt","stale");stamp(L"fm\\O\\c1.txt",-3*24*60);
 putfile(L"fm\\O\\c2.txt","fresh");stamp(L"fm\\O\\c2.txt",10);
}
static HWND wantedClass(const wchar_t*cls,int id);
static const wchar_t*wantCls;
static BOOL CALLBACK onClass(HWND h,LPARAM l){(void)l;wchar_t c[40];GetClassNameW(h,c,40);if(!wcscmp(c,wantCls)&&(wantedId<0||GetDlgCtrlID(h)==wantedId)){wanted=h;return FALSE;}return TRUE;}
static HWND wantedClass(const wchar_t*cls,int id){wanted=NULL;wantCls=cls;wantedId=id;EnumChildWindows(frame,onClass,0);return wanted;}
static void fmergeChecks(void){
 HWND list=NULL,run=NULL;
 for(int i=0;i<200;i++){
  run=wantedClass(L"Button",ID_BTN_RUNM);
  if(run){list=FindWindowExW(GetParent(run),NULL,L"SysListView32",NULL);if(list&&SendMessageW(list,LVM_GETITEMCOUNT,0,0)>=3)break;}
  Sleep(50);
 }
 check(run&&list&&SendMessageW(list,LVM_GETITEMCOUNT,0,0)==3,"folder merge lists the three files");
 if(!run||!list)return;
 expectBoxes=1;lastBox[0]=0;
 SendMessageW(run,BM_CLICK,0,0);                      /* asks about the non-empty output folder, then reports */
 expectBoxes=0;
 printf("summary has %d characters, %d boxes shown\n",(int)wcslen(lastBox),shownBoxes);
 check(shownBoxes==2,"one question about the output folder and one summary");
 check(wcsstr(lastBox,L"\xBCD1\xD569 \xD30C\xC77C 1")!=NULL,"an output file newer than the inputs counts as merged");
 check(wcsstr(lastBox,L"\xBBF8\xD574\xACB0 \xCDA9\xB3CC 1")!=NULL,"an output file older than the inputs is still a conflict");
 check(wcsstr(lastBox,L"\xBCF5\xC0AC \xC2E4\xD328 0")!=NULL,"no failed copies");
 check(holds(L"fm\\O\\n.txt","new"),"the file only on the left was copied to the output");
 check(holds(L"fm\\O\\c1.txt","stale")&&holds(L"fm\\O\\c2.txt","fresh"),"conflicting files in the output were not overwritten");
 check(holds(L"fm\\L\\c1.txt","L")&&holds(L"fm\\R\\c1.txt","R")&&holds(L"fm\\B\\c1.txt","base"),"the inputs were not changed");
}
/* ---- temporary files: what this process left under %TEMP%\\BeCompareAsm is gone when WinMain returns */
static wchar_t ownTemp[600];
static void tempFixture(void){
 wchar_t t[400],d[600];GetTempPathW(400,t);
 swprintf(ownTemp,600,L"%lsBeCompareAsm\\%X_TEST",t,(unsigned)GetCurrentProcessId());
 swprintf(d,600,L"%ls\\A\\sub",ownTemp);SHCreateDirectoryExW(NULL,d,NULL);
 wcscat(d,L"\\f.txt");putfile(d,"extracted");
}
static DWORD WINAPI watcher(void*arg){
 (void)arg;
 for(int i=0;i<200&&!frame;i++){EnumThreadWindows(mainTid,onTop,0);if(!frame)Sleep(50);}
 check(frame!=NULL,"frame window created");
 if(!frame){PostThreadMessageW(mainTid,WM_QUIT,0,0);return 0;}
 if(mergeMode){mergeChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(fmergeMode){fmergeChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 for(int i=0;i<200;i++){
  EnumChildWindows(frame,onChild,0);
  if(ra&&rb){view=GetParent(ra);if(info(L"차이 4개"))break;}
  Sleep(50);
 }
 check(ra&&rb,"text session has both panes");
 if(!ra||!rb){PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 check(info(L"차이 4개"),"compare ran and reports 4 differences");
 check(!SendMessageW(ra,EM_CANUNDO,0,0)&&!SendMessageW(rb,EM_CANUNDO,0,0),"colouring is not an undo step");
 check(!IsWindowVisible(frame)||getenv("BC_UITEST_SHOW"),"window stays hidden during the test");

 SendMessageW(frame,WM_COMMAND,IDM_LIGHT,0);
 check(g_dark==0,"light theme selected");palette(0);colours("light");
 SendMessageW(frame,WM_COMMAND,IDM_DARK,0);
 check(g_dark==1,"dark theme selected");palette(1);colours("dark");
 SendMessageW(frame,WM_COMMAND,IDM_LIGHT,0);
 check(g_dark==0,"back to the light theme");palette(0);colours("light again");

 /* syntax colours (the files are .c): a keyword and a comment get their own text colour on both panes,
    the backgrounds stay, and switching it off returns the plain text colour */
 {HWND syn=GetDlgItem(view,ID_CHK_SYN);
  if(SendMessageW(syn,BM_GETCHECK,0,0))SendMessageW(syn,BM_CLICK,0,0);
  COLORREF plainKw=fore(ra,1,0),plainId=fore(ra,0,0);
  check(plainKw==plainId,"without syntax colours a keyword looks like other text");
  SendMessageW(syn,BM_CLICK,0,0);
  check(SendMessageW(syn,BM_GETCHECK,0,0)==BST_CHECKED,"syntax colours switched on");
  check(fore(ra,1,0)!=fore(ra,0,0)&&fore(rb,1,0)!=fore(rb,0,0),"keyword has its own text colour");
  check(fore(ra,1,17)!=fore(ra,0,0)&&fore(ra,1,17)!=fore(ra,1,0),"comment has its own text colour");
  check(fore(ra,0,0)==plainId,"other text keeps the plain colour");
  colours("with syntax colours");
  SendMessageW(syn,BM_CLICK,0,0);
  check(fore(ra,1,0)==plainId&&fore(ra,1,17)==plainId,"switching syntax colours off restores the text colour");
  colours("syntax colours off again");}

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
int main(int argc,char**argv){
 wchar_t a[400],b[400];
 mergeMode=argc>1&&!strcmp(argv[1],"merge");
 fmergeMode=argc>1&&!strcmp(argv[1],"fmerge");
 tempFixture();
 check(GetFileAttributesW(ownTemp)!=INVALID_FILE_ATTRIBUTES,"temporary folder of this process exists before the run");
 fixture();
 GetFullPathNameW(L"ui_a.c",400,a,NULL);GetFullPathNameW(L"ui_b.c",400,b,NULL);
 swprintf(cmdline,1200,L"bcgui.exe %ls \"%ls\" \"%ls\"",mergeMode?L"merge":L"text",a,b);
 if(fmergeMode){
  wchar_t l[400],r[400],bs[400],o[400];fmergeFixture();
  GetFullPathNameW(L"fm\\L",400,l,NULL);GetFullPathNameW(L"fm\\R",400,r,NULL);GetFullPathNameW(L"fm\\B",400,bs,NULL);GetFullPathNameW(L"fm\\O",400,o,NULL);
  swprintf(cmdline,1200,L"bcgui.exe foldermerge \"%ls\" \"%ls\" \"%ls\" \"%ls\"",l,r,bs,o);
 }
 mainTid=GetCurrentThreadId();
 CloseHandle(CreateThread(NULL,0,watchdog,NULL,0,NULL));
 HANDLE w=CreateThread(NULL,0,watcher,NULL,0,NULL);
 bc_winmain(GetModuleHandleW(NULL));
 check(WaitForSingleObject(w,5000)==WAIT_OBJECT_0,"application closed normally");
 check(boxes==0,"no message box appeared");
 check(GetFileAttributesW(ownTemp)==INVALID_FILE_ATTRIBUTES,"temporary folder of this process is removed on exit");
 printf("%d checks, %d failures\n",checks,failed);fflush(stdout);
 ExitProcess(failed?1:0);
}
