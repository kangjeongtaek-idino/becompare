/* UI behaviour test: the whole application runs in this process with its real windows and RichEdit
   controls (kept hidden), and a second thread inspects what a user would see:
   row colours, word level highlight, F7 / Shift+F7 navigation, the light and dark palettes.
   "test_ui merge" opens the same two files as a merge session and checks the colours of its panes.
   "test_ui fmerge" runs a folder merge into an output folder and checks what it reports and writes.
   "test_ui browse" opens the folder dialog from a window hanging off the screen and checks where it lands.
   "test_ui find" searches a small folder tree through the file search of the Home tab.
   "test_ui findmft" lists the whole drive of the working folder from its file table (needs administrator
   rights, skipped otherwise) and looks for a file created just before.
   Every mode also checks the buttons at the right end of the tab row and that each control has a tooltip.
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
extern uint64_t g_dark, g_nsess;
extern int browse_folder(HWND,wchar_t*);
extern uint64_t fi_state, fi_vis, fi_mode;
extern int find_query(const wchar_t*,uint32_t*,uint32_t);
extern int find_query2(const wchar_t*,uint32_t*,uint32_t,const void*);
extern int find_path(uint32_t,wchar_t*,uint32_t);
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
#define IDM_CLOSE_TAB 1020
#define IDM_EXIT 1021
#define ID_RECENT 3001
#define ID_RLABEL 3002
#define ID_FIND_EDIT 3010
#define ID_FIND_BTN 3011
#define ID_FIND_LIST 3012
#define ID_FIND_KIND 3013
#define ID_FIND_EXT 3014
#define ID_FIND_PATH 3015
#define IDM_LIGHT 1030
#define IDM_DARK 1031

static int checks, failed, shiftDown, boxes, mergeMode, fmergeMode, browseMode, findMode, mftMode, expectBoxes, shownBoxes;
static wchar_t findRoot[600], probeName[80], probePath[700];
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
/* opening a found item: nothing is started and the clipboard is left alone, the calls are recorded */
static wchar_t shVerb[32],shFile[1200],shArgs[1200],clipText[1200];static int shCalls,menuAnswer,menuShown;
HINSTANCE WINAPI __wrap_ShellExecuteW(HWND h,LPCWSTR verb,LPCWSTR file,LPCWSTR args,LPCWSTR dir,int show){
 (void)h;(void)dir;(void)show;shCalls++;wcsncpy(shVerb,verb?verb:L"",31);wcsncpy(shFile,file?file:L"",1199);wcsncpy(shArgs,args?args:L"",1199);
 return (HINSTANCE)(INT_PTR)42;
}
BOOL WINAPI __wrap_TrackPopupMenu(HMENU m,UINT f,int x,int y,int r,HWND h,const RECT*rc){(void)f;(void)x;(void)y;(void)r;(void)h;(void)rc;menuShown=GetMenuItemCount(m);return menuAnswer;}
BOOL WINAPI __wrap_EmptyClipboard(void){return TRUE;}
HANDLE WINAPI __wrap_SetClipboardData(UINT fmt,HANDLE mem){
 if(fmt==CF_UNICODETEXT){const wchar_t*t=GlobalLock(mem);if(t){wcsncpy(clipText,t,1199);GlobalUnlock(mem);}}
 GlobalFree(mem);return (HANDLE)(INT_PTR)1;
}

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
 printf("list rows: %d\n",list?(int)SendMessageW(list,LVM_GETITEMCOUNT,0,0):-1);
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
/* ---- tooltips: every button, check box, edit field and combo box of the frame has one */
static HWND tipWnds[32];static int nTipWnds,tipless,tipped;   /* list views and tab controls have tooltip windows of their own */
static BOOL CALLBACK onThreadTop(HWND h,LPARAM l){wchar_t c[40];(void)l;GetClassNameW(h,c,40);if(!wcscmp(c,L"tooltips_class32")&&nTipWnds<32)tipWnds[nTipWnds++]=h;return TRUE;}
static int tipText(HWND ctl,wchar_t*out){
 for(int i=0;i<nTipWnds;i++){
  TOOLINFOW ti;memset(&ti,0,sizeof ti);ti.cbSize=TTTOOLINFOW_V2_SIZE;ti.hwnd=GetParent(ctl);ti.uId=(UINT_PTR)ctl;ti.lpszText=out;out[0]=0;
  if(SendMessageW(tipWnds[i],TTM_GETTOOLINFOW,0,(LPARAM)&ti)){SendMessageW(tipWnds[i],TTM_GETTEXTW,400,(LPARAM)&ti);if(out[0])return 1;}
 }
 return 0;
}
static BOOL CALLBACK onControl(HWND h,LPARAM l){
 wchar_t c[40],t[400];(void)l;GetClassNameW(h,c,40);
 int want=!wcscmp(c,L"Button")||!wcscmp(c,L"Edit")||!wcscmp(c,L"ComboBox");
 if(!wcscmp(c,L"Edit")){GetClassNameW(GetParent(h),t,40);if(!wcscmp(t,L"ComboBox"))want=0;}   /* the edit inside a combo box */
 if(!want)return TRUE;
 if(tipText(h,t))tipped++;else{tipless++;printf("no tooltip: %ls id %d\n",c,GetDlgCtrlID(h));}
 return TRUE;
}
static void tooltipChecks(void){
 wchar_t t[400];
 nTipWnds=0;EnumThreadWindows(mainTid,onThreadTop,0);
 check(nTipWnds>0,"tooltip window exists");
 if(!nTipWnds)return;
 for(int i=0;i<nTipWnds;i++)printf("tooltip window %d: %d tools\n",i,(int)SendMessageW(tipWnds[i],TTM_GETTOOLCOUNT,0,0));
 tipless=tipped=0;EnumChildWindows(frame,onControl,0);
 printf("%d controls with a tooltip, %d without\n",tipped,tipless);
 check(tipped>0&&tipless==0,"every button, check box, edit field and combo box has a tooltip");
 check(tipText(GetDlgItem(frame,IDM_CLOSE_TAB),t)&&wcsstr(t,L"\xD0ED")!=NULL,"close-tab button explains itself");
 check(tipText(GetDlgItem(frame,IDM_EXIT),t)&&wcsstr(t,L"\xC885\xB8CC")!=NULL,"exit button explains itself");
}
/* ---- the two buttons at the right end of the tab row */
static void tabRowChecks(void){
 HWND tab=FindWindowExW(frame,NULL,L"SysTabControl32",NULL),bc=GetDlgItem(frame,IDM_CLOSE_TAB),bx=GetDlgItem(frame,IDM_EXIT);
 check(tab&&bc&&bx,"tab row has a close-tab and an exit button");
 if(!tab||!bc||!bx)return;
 RECT rt,rc,rx,cl,disp;GetWindowRect(tab,&rt);GetWindowRect(bc,&rc);GetWindowRect(bx,&rx);GetClientRect(frame,&cl);
 POINT o={cl.right,0};ClientToScreen(frame,&o);
 disp=rt;SendMessageW(tab,TCM_ADJUSTRECT,0,(LPARAM)&disp);          /* top of the page area = bottom of the tab row */
 check(rc.left>=rt.right&&rx.left>=rc.right&&rx.right<=o.x,"the buttons follow the tabs: close, then exit, inside the window");
 check(rc.top>=rt.top&&rc.bottom<=disp.top&&rx.top>=rt.top&&rx.bottom<=disp.top,"the buttons are in the tab row, above the pages");
 check(rc.right-rc.left>=16&&rc.bottom-rc.top>=12&&rx.right-rx.left>=16,"the buttons have a usable size");
}
/* close the current tab and then the application through those buttons */
static void closeThroughButtons(void){
 uint64_t before=g_nsess;
 SendMessageW(GetDlgItem(frame,IDM_CLOSE_TAB),BM_CLICK,0,0);
 for(int i=0;i<40&&g_nsess==before;i++)Sleep(50);
 check(before>1&&g_nsess==before-1,"close-tab button closes the current tab");
 SendMessageW(GetDlgItem(frame,IDM_CLOSE_TAB),BM_CLICK,0,0);Sleep(200);
 check(g_nsess==before-1,"close-tab button leaves the Home tab open");
 SendMessageW(GetDlgItem(frame,IDM_EXIT),BM_CLICK,0,0);              /* the run ends: checked by main */
}
/* ---- folder dialog: opened from a window that hangs off the bottom right corner of the screen,
   the dialog and its OK button must still be inside the work area */
static HWND dlgSeen;static RECT dlgRect,okRect,workRect;static int dlgOk;
static BOOL CALLBACK onDialog(HWND h,LPARAM l){
 wchar_t c[40];DWORD pid;(void)l;GetWindowThreadProcessId(h,&pid);GetClassNameW(h,c,40);
 if(pid==GetCurrentProcessId()&&!wcscmp(c,L"#32770")&&IsWindowVisible(h)&&GetWindow(h,GW_OWNER)==frame){dlgSeen=h;return FALSE;}
 return TRUE;
}
static DWORD WINAPI dialogWatcher(void*arg){
 (void)arg;
 for(int i=0;i<200&&!dlgSeen;i++){EnumWindows(onDialog,0);if(!dlgSeen)Sleep(50);}
 if(!dlgSeen)return 0;
 Sleep(1200);                                         /* let it finish restoring its size and position */
 HWND ok=GetDlgItem(dlgSeen,IDOK);MONITORINFO mi;mi.cbSize=sizeof mi;
 GetMonitorInfoW(MonitorFromWindow(frame,MONITOR_DEFAULTTONEAREST),&mi);workRect=mi.rcWork;
 GetWindowRect(dlgSeen,&dlgRect);dlgOk=ok&&IsWindowVisible(ok)&&GetWindowRect(ok,&okRect);
 PostMessageW(dlgSeen,WM_COMMAND,IDCANCEL,0);
 return 0;
}
static int inside(const RECT*r,const RECT*w){return r->left>=w->left&&r->top>=w->top&&r->right<=w->right&&r->bottom<=w->bottom;}
static void browseChecks(void){
 MONITORINFO mi;mi.cbSize=sizeof mi;GetMonitorInfoW(MonitorFromWindow(frame,MONITOR_DEFAULTTOPRIMARY),&mi);
 SetWindowPos(frame,NULL,mi.rcWork.right-260,mi.rcWork.bottom-120,900,700,SWP_NOZORDER|SWP_NOACTIVATE);
 HANDLE t=CreateThread(NULL,0,dialogWatcher,NULL,0,NULL);
 wchar_t out[520]={0};int r=browse_folder(frame,out);
 WaitForSingleObject(t,15000);CloseHandle(t);
 check(dlgSeen!=NULL,"folder dialog opened");
 if(!dlgSeen)return;
 printf("work %ld,%ld-%ld,%ld dialog %ld,%ld-%ld,%ld ok %ld,%ld-%ld,%ld\n",workRect.left,workRect.top,workRect.right,workRect.bottom,
  dlgRect.left,dlgRect.top,dlgRect.right,dlgRect.bottom,okRect.left,okRect.top,okRect.right,okRect.bottom);
 check(r==0,"cancelled folder dialog returns nothing");
 check(inside(&dlgRect,&workRect),"folder dialog is inside the work area of the screen");
 check(dlgOk&&inside(&okRect,&workRect),"its OK button is inside the work area");
}
/* ---- file search of the Home tab */
static HWND homeView(void){return FindWindowExW(frame,NULL,L"BCHome",NULL);}
static int shown(HWND h){return (GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE)!=0;}
/* name, kind (row of the combo box), extensions, folder text -> number of rows */
static int filtered(HWND home,const wchar_t*text,int kind,const wchar_t*ext,const wchar_t*folder){
 SendMessageW(GetDlgItem(home,ID_FIND_KIND),CB_SETCURSEL,kind,0);
 SetWindowTextW(GetDlgItem(home,ID_FIND_EXT),ext);SetWindowTextW(GetDlgItem(home,ID_FIND_PATH),folder);
 SetWindowTextW(GetDlgItem(home,ID_FIND_EDIT),text);
 SendMessageW(GetDlgItem(home,ID_FIND_BTN),BM_CLICK,0,0);
 return (int)SendMessageW(GetDlgItem(home,ID_FIND_LIST),LB_GETCOUNT,0,0);
}
static int search(HWND home,const wchar_t*text){        /* type, press the button, return the number of rows */
 SetWindowTextW(GetDlgItem(home,ID_FIND_EDIT),text);
 SendMessageW(GetDlgItem(home,ID_FIND_BTN),BM_CLICK,0,0);
 return (int)SendMessageW(GetDlgItem(home,ID_FIND_LIST),LB_GETCOUNT,0,0);
}
static int rowIs(HWND home,int i,const wchar_t*want){
 wchar_t t[1200]={0};HWND l=GetDlgItem(home,ID_FIND_LIST);
 if(SendMessageW(l,LB_GETTEXTLEN,i,0)>1100)return 0;
 SendMessageW(l,LB_GETTEXT,i,(LPARAM)t);
 if(_wcsicmp(t,want))printf("row %d is another path (%d characters)\n",i,(int)wcslen(t));
 return !_wcsicmp(t,want);
}
static int hasRow(HWND home,const wchar_t*want){
 HWND l=GetDlgItem(home,ID_FIND_LIST);int n=(int)SendMessageW(l,LB_GETCOUNT,0,0);wchar_t t[1200];
 for(int i=0;i<n;i++){if(SendMessageW(l,LB_GETTEXTLEN,i,0)>1100)continue;SendMessageW(l,LB_GETTEXT,i,(LPARAM)t);if(!_wcsicmp(t,want))return 1;}
 return 0;
}
static void findFixture(void){
 CreateDirectoryW(L"ff",NULL);CreateDirectoryW(L"ff\\Alpha",NULL);CreateDirectoryW(L"ff\\Alpha\\Deep",NULL);CreateDirectoryW(L"ff\\Beta",NULL);
 putfile(L"ff\\Alpha\\Report_2026.TXT","a");putfile(L"ff\\Beta\\report-draft.md","b");putfile(L"ff\\Beta\\other.bin","c");
 putfile(L"ff\\Alpha\\Deep\\\xBCF4\xACE0\xC11C \xCD5C\xC885.docx","d");
 GetFullPathNameW(L"ff",600,findRoot,NULL);
 SetEnvironmentVariableW(L"BC_FIND_ROOT",findRoot);
}
static void findChecks(void){
 HWND home=homeView();wchar_t want[900],label[200];uint32_t ids[10];
 check(home&&GetDlgItem(home,ID_FIND_EDIT)&&GetDlgItem(home,ID_FIND_BTN)&&GetDlgItem(home,ID_FIND_LIST),"Home has the file search field, button and list");
 if(!home)return;
 HWND list=GetDlgItem(home,ID_FIND_LIST),recent=GetDlgItem(home,ID_RECENT);
 check(!shown(list)&&shown(recent)&&fi_state==0,"before a search the recent sessions are shown and nothing is listed");
 search(home,L"report");
 for(int i=0;i<200&&fi_state!=2;i++)Sleep(50);
 check(fi_state==2,"the folder tree is listed");
 {int n=0;for(int i=0;i<60;i++){n=(int)SendMessageW(list,LB_GETCOUNT,0,0);if(n==2)break;Sleep(50);}
  check(n==2,"the first search fills in by itself once the listing is done");}
 {RECT re,rb2,rl;GetWindowRect(GetDlgItem(home,ID_FIND_EDIT),&re);GetWindowRect(GetDlgItem(home,ID_FIND_BTN),&rb2);GetWindowRect(list,&rl);
  check(re.bottom-re.top>=16&&re.right-re.left>=200&&rb2.bottom-rb2.top>=16&&rb2.right-rb2.left>=40,"the search field and button have a usable size");
  check(rb2.left>=re.right&&re.bottom<=rl.top&&rb2.bottom<=rl.top,"the field is left of the button, both above the list");}
 check(fi_mode==2,"it was listed by walking the folders");
 check(search(home,L"report")==2,"two names contain the text, whatever their case");
 swprintf(want,900,L"%ls\\Alpha\\Report_2026.TXT",findRoot);check(hasRow(home,want),"a result is the full path of the file");
 swprintf(want,900,L"%ls\\Beta\\report-draft.md",findRoot);check(hasRow(home,want),"the other result too");
 check(shown(list)&&!shown(recent),"the results take the place of the recent sessions");
 GetWindowTextW(GetDlgItem(home,ID_RLABEL),label,200);check(wcsstr(label,L"\xACB0\xACFC 2\xAC1C")!=NULL,"the heading shows the number of results");
 check(search(home,L"\xCD5C\xC885")==1,"a Korean name is found");
 swprintf(want,900,L"%ls\\Alpha\\Deep\\\xBCF4\xACE0\xC11C \xCD5C\xC885.docx",findRoot);check(rowIs(home,0,want),"with its full path");
 check(search(home,L"DEEP")==1,"folders are found too");
 check(search(home,L"no-such-name-anywhere")==0,"a text that no name contains gives no rows");
 check(find_query(L"REPORT",ids,10)==2&&find_query(L".",ids,10)==4&&find_query(L"",ids,10)==0,"searching the index directly agrees");
 /* filters. The tree: Alpha\\Report_2026.TXT, Alpha\\Deep\\(Korean).docx, Beta\\report-draft.md, Beta\\other.bin */
 check(GetDlgItem(home,ID_FIND_KIND)&&GetDlgItem(home,ID_FIND_EXT)&&GetDlgItem(home,ID_FIND_PATH),"Home has the kind, extension and folder filters");
 check(SendMessageW(GetDlgItem(home,ID_FIND_KIND),CB_GETCOUNT,0,0)==9,"the kind filter offers nine kinds");
 check(filtered(home,L"a",2,L"",L"")==2,"folders only: Alpha and Beta contain the letter");
 swprintf(want,900,L"%ls\\Alpha",findRoot);check(hasRow(home,want),"a folder row is the path of the folder");
 check(filtered(home,L"report",1,L"",L"")==2&&filtered(home,L"e",1,L"",L"")==3,"files only leaves the folders out");
 check(filtered(home,L"",0,L"md",L"")==1,"an extension alone lists every file of that type");
 swprintf(want,900,L"%ls\\Beta\\report-draft.md",findRoot);check(rowIs(home,0,want),"which is the .md file");
 check(filtered(home,L"",0,L"*.TXT; .docx",L"")==2,"several extensions, written with dots, stars and any case");
 check(filtered(home,L"report",0,L"txt;docx",L"")==1,"name and extension together");
 check(filtered(home,L"",3,L"",L"")==3,"the documents kind: txt, md and docx, not bin");
 check(filtered(home,L"",8,L"",L"")==0&&filtered(home,L"",3,L"bin",L"")==1,"typed extensions replace those of the kind");
 check(filtered(home,L"",0,L"",L"beta")==2,"the folder filter: the two files inside Beta");
 check(filtered(home,L"",0,L"bin",L"BETA")==1&&filtered(home,L"report",0,L"",L"alpha")==1,"folder filter with the other filters");
 check(filtered(home,L"",2,L"",L"alpha")==1,"folders only inside Alpha: Deep");
 check(filtered(home,L"",0,L"",L"no-such-folder")==0,"a folder text that no path contains gives no rows");
 {uint32_t f[6]={1,0,0,0,0,0};check(find_query2(L"",ids,10,f)==4,"searching the index directly with a filter agrees");}
 /* opening what was found */
 check(filtered(home,L"other",0,L"",L"")==1,"one row to open");
 swprintf(want,900,L"%ls\\Beta\\other.bin",findRoot);
 SendMessageW(list,LB_SETCURSEL,0,0);shCalls=0;
 SendMessageW(home,WM_COMMAND,MAKEWPARAM(ID_FIND_LIST,LBN_DBLCLK),(LPARAM)list);
 check(shCalls==1&&!wcscmp(shVerb,L"open")&&!_wcsicmp(shFile,want)&&!shArgs[0],"double click opens the file with its registered program");
 menuAnswer=0;menuShown=0;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 check(menuShown==6&&shCalls==0,"the menu has five actions and a separator; leaving it does nothing");
 menuAnswer=2;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 check(shCalls==1&&!wcscmp(shVerb,L"openas")&&!_wcsicmp(shFile,want),"menu: choose the program");
 menuAnswer=3;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 {wchar_t arg[1000];swprintf(arg,1000,L"/select,\"%ls\"",want);
  check(shCalls==1&&!_wcsicmp(shFile,L"explorer.exe")&&!_wcsicmp(shArgs,arg),"menu: show in its folder, selected");}
 menuAnswer=5;shCalls=0;clipText[0]=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 check(shCalls==0&&!_wcsicmp(clipText,want),"menu: copy the path");
 check(filtered(home,L"Report_2026",0,L"",L"")==1,"a text file to open in the application");
 SendMessageW(list,LB_SETCURSEL,0,0);
 {uint64_t before=g_nsess;menuAnswer=4;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
  for(int i=0;i<40&&g_nsess==before;i++)Sleep(50);
  check(shCalls==0&&g_nsess==before+1,"menu: open in a session of the application");}
 menuAnswer=0;
 filtered(home,L"",0,L"",L"");
 GetWindowTextW(GetDlgItem(home,ID_RLABEL),label,200);
 check(!shown(list)&&shown(recent)&&!wcscmp(label,L"\xCD5C\xADFC \xC138\xC158"),"empty fields and no filter bring the recent sessions back");
}
/* the whole drive, from its file table */
static int canOpenVolume(wchar_t letter){
 wchar_t v[8]=L"\\\\.\\X:";v[4]=letter;
 HANDLE h=CreateFileW(v,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
 if(h==INVALID_HANDLE_VALUE)return 0;
 CloseHandle(h);return 1;
}
static int mftFixture(void){                             /* 1 when the test can run */
 wchar_t cwd[600],drive[3]=L"X:";GetCurrentDirectoryW(600,cwd);
 if(cwd[1]!=L':'||!canOpenVolume(cwd[0]))return 0;
 drive[0]=cwd[0];SetEnvironmentVariableW(L"BC_FIND_ROOT",drive);
 swprintf(probeName,80,L"bc_find_probe_%X_%X.tmp",(unsigned)GetCurrentProcessId(),(unsigned)GetTickCount());
 swprintf(probePath,700,L"%ls\\%ls",cwd,probeName);putfile(probePath,"probe");
 return 1;
}
static void mftChecks(void){
 HWND home=homeView();uint32_t ids[4];wchar_t path[1200];
 if(!home){check(0,"Home view exists");return;}
 DWORD t0=GetTickCount();
 search(home,probeName);
 for(int i=0;i<2000&&fi_state!=2;i++)Sleep(50);
 printf("drive listed in %lu ms: %llu entries, mode %llu\n",(unsigned long)(GetTickCount()-t0),(unsigned long long)fi_vis,(unsigned long long)fi_mode);
 check(fi_state==2,"the drive is listed");
 check(fi_mode==1,"it was read from the file table of the volume");
 check(fi_vis>10,"the index holds the names of the drive");      /* a build machine's work drive can be nearly empty */
 t0=GetTickCount();int n=find_query(probeName,ids,4);DWORD dt=GetTickCount()-t0;
 printf("one search over the index: %lu ms\n",(unsigned long)dt);
 check(n==1,"the file created before the search is found, once");
 check(n==1&&find_path(ids[0],path,1200)>0&&!_wcsicmp(path,probePath),"its path is rebuilt from the folder records");
 check(search(home,probeName)==1&&rowIs(home,0,probePath),"the Home list shows it");
 DeleteFileW(probePath);
}
static DWORD WINAPI watcher(void*arg){
 (void)arg;
 for(int i=0;i<200&&!frame;i++){EnumThreadWindows(mainTid,onTop,0);if(!frame)Sleep(50);}
 check(frame!=NULL,"frame window created");
 if(!frame){PostThreadMessageW(mainTid,WM_QUIT,0,0);return 0;}
 if(browseMode){tabRowChecks();tooltipChecks();browseChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(findMode){findChecks();tooltipChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(mftMode){mftChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(mergeMode){mergeChecks();tooltipChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(fmergeMode){fmergeChecks();tooltipChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
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
 tabRowChecks();
 tooltipChecks();
 closeThroughButtons();
 return 0;
}
static DWORD WINAPI watchdog(void*arg){(void)arg;Sleep(mftMode?170000:60000);printf("FAIL timed out\n");fflush(stdout);ExitProcess(3);}
int main(int argc,char**argv){
 wchar_t a[400],b[400];
 mergeMode=argc>1&&!strcmp(argv[1],"merge");
 fmergeMode=argc>1&&!strcmp(argv[1],"fmerge");
 browseMode=argc>1&&!strcmp(argv[1],"browse");
 findMode=argc>1&&!strcmp(argv[1],"find");
 mftMode=argc>1&&!strcmp(argv[1],"findmft");
 if(findMode)findFixture();
 if(mftMode&&!mftFixture()){printf("SKIP the volume cannot be opened (no administrator rights)\n0 checks, 0 failures\n");return 0;}
 tempFixture();
 check(GetFileAttributesW(ownTemp)!=INVALID_FILE_ATTRIBUTES,"temporary folder of this process exists before the run");
 fixture();
 GetFullPathNameW(L"ui_a.c",400,a,NULL);GetFullPathNameW(L"ui_b.c",400,b,NULL);
 swprintf(cmdline,1200,L"bcgui.exe %ls \"%ls\" \"%ls\"",mergeMode?L"merge":L"text",a,b);
 if(browseMode||findMode||mftMode)wcscpy(cmdline,L"bcgui.exe");
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
