/* UI behaviour test: the whole application runs in this process with its real windows and RichEdit
   controls (kept hidden), and a second thread inspects what a user would see:
   row colours, word level highlight, F7 / Shift+F7 navigation, the light and dark palettes.
   "test_ui merge" opens the same two files as a merge session and checks the colours of its panes.
   "test_ui folder" opens a folder compare and checks its toolbar, compare-by box, options rows and filters.
   "test_ui folderpv" checks the preview under the list: showing a pair, differences, files, merge, save, the strip.
   "test_ui fmerge" runs a folder merge into an output folder and checks what it reports and writes.
   "test_ui browse" opens the folder dialog from a window hanging off the screen and checks where it lands.
   "test_ui find" searches a small folder tree through the file search of the Home tab.
   "test_ui findmft" lists the whole drive of the working folder by parsing its file table (needs administrator
   rights, skipped otherwise) and looks for a file created just before; "test_ui findusn" does the same with the
   file table enumerated instead (names only).
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
extern int __real_browse_folder(HWND,wchar_t*);
static wchar_t fakeFolder[700];static int browseCalls,cancelFolder;
int __wrap_browse_folder(HWND h,wchar_t*out){
 browseCalls++;
 if(cancelFolder)return 0;
 if(fakeFolder[0]){wcscpy(out,fakeFolder);return 1;}
 return __real_browse_folder(h,out);
}
extern uint64_t fi_state, fi_vis, fi_mode;
extern int find_query(const wchar_t*,uint32_t*,uint32_t);
extern int find_query2(const wchar_t*,uint32_t*,uint32_t,const void*);
extern int find_path(uint32_t,wchar_t*,uint32_t);
extern uint64_t find_time(uint32_t),find_size(uint32_t);
extern uint64_t fi_ncloud;
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
#define IDM_SETTINGS 1060
#define ID_RECENT 3001
#define ID_RLABEL 3002
#define ID_FIND_EDIT 3010
#define ID_FIND_BTN 3011
#define ID_FIND_LIST 3012
#define ID_FIND_KIND 3013
#define ID_FIND_EXT 3014
#define ID_FIND_PATH 3015
#define ID_FIND_DRIVE 3018
#define ID_FIND_CLOUD 3019
#define ID_FIND_RESET 3020
#define ID_FIND_CLR 3021
#define ID_FIND_PBROWSE 3022
#define ID_FIND_SELALL 3023
#define ID_FIND_COPY 3024
#define ID_FIND_SHARE 3025
#define ID_FIND_EMAIL 3026
#define ID_FIND_DEL 3027
#define ID_FIND_PROPS 3028
#define IDM_LIGHT 1030
#define IDM_DARK 1031

static int checks, failed, shiftDown, ctrlDown, boxes, mergeMode, fmergeMode, browseMode, findMode, mftMode, usnMode, folderMode, pvMode, settingsMode, expectBoxes, shownBoxes;
static wchar_t findRoot[600], probeName[80], probePath[700];
static wchar_t lastBox[400];
static int boxAnswer, deleteCalls, deleteMode;
static UINT lastBoxFlags, deleteFlags;
static DWORD mainTid;
static HWND frame, ra, rb, view;
static wchar_t cmdline[1200];
static void check(int ok,const char*name){checks++;printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)failed++;fflush(stdout);}

/* ---- the application's surroundings */
BOOL WINAPI __real_ShowWindow(HWND,int);
HWND WINAPI __real_CreateWindowExW(DWORD,LPCWSTR,LPCWSTR,DWORD,int,int,int,int,HWND,HMENU,HINSTANCE,LPVOID);
HWND WINAPI __wrap_CreateWindowExW(DWORD ex,LPCWSTR cls,LPCWSTR title,DWORD style,int x,int y,int w,int h,HWND parent,HMENU menu,HINSTANCE inst,LPVOID data){
 if(!getenv("BC_UITEST_SHOW")&&(ULONG_PTR)cls>65535&&!wcscmp(cls,L"BCSettings"))x=y=-30000;
 return __real_CreateWindowExW(ex,cls,title,style,x,y,w,h,parent,menu,inst,data);
}
SHORT WINAPI __real_GetKeyState(int);
LPWSTR WINAPI __wrap_GetCommandLineW(void){return cmdline;}
BOOL WINAPI __wrap_ShowWindow(HWND h,int c){
 if(!getenv("BC_UITEST_SHOW")&&!(GetWindowLongPtrW(h,GWL_STYLE)&WS_CHILD))c=SW_HIDE;
 return __real_ShowWindow(h,c);
}
SHORT WINAPI __wrap_GetKeyState(int vk){
 if(vk==VK_SHIFT)return shiftDown?(SHORT)0x8000:0;
 if(vk==VK_CONTROL)return ctrlDown?(SHORT)0x8000:0;
 if(vk==VK_MENU)return 0;
 return __real_GetKeyState(vk);
}
int WINAPI __wrap_MessageBoxW(HWND h,LPCWSTR t,LPCWSTR c,UINT f){(void)h;(void)c;
 if(expectBoxes){shownBoxes++;lastBoxFlags=f;wcsncpy(lastBox,t,399);lastBox[399]=0;return boxAnswer?boxAnswer:(f&15)==MB_YESNO?IDYES:IDOK;}
 boxes++;{char u8[600];WideCharToMultiByte(CP_UTF8,0,t,-1,u8,600,NULL,NULL);printf("unexpected message box (flags %u): %s\n",f,u8);}return (f&15)?IDNO:IDOK;}
void __wrap_update_start(void){}                      /* no network access from a test */
/* Simulate recycling inside the isolated fixture, never use the real recycle bin. */
int WINAPI __real_co_SHFileOperationW(LPSHFILEOPSTRUCTW);
int WINAPI __wrap_co_SHFileOperationW(LPSHFILEOPSTRUCTW op){
 if(!findMode||!op->pFrom||wcsncmp(op->pFrom,findRoot,wcslen(findRoot)))return __real_co_SHFileOperationW(op);
 deleteCalls++;deleteFlags=op->fFlags;
 check(shownBoxes>0&&boxAnswer==IDYES,"recycling only runs after explicit confirmation");
 check(op->wFunc==FO_DELETE&&!op->pTo,"search delete uses a delete operation");
 if(deleteMode==1)return ERROR_ACCESS_DENIED;
 if(deleteMode==3){op->fAnyOperationsAborted=TRUE;return 0;}
 for(const wchar_t*p=op->pFrom;*p;p+=wcslen(p)+1){
  wchar_t dest[1300];swprintf(dest,1300,L"%ls.recycled",p);
  check(MoveFileW(p,dest),"recycle simulation moves the fixture item");
  if(deleteMode==2){op->fAnyOperationsAborted=TRUE;return 0;}
 }
 return 0;
}
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
 check(GetDlgItem(GetParent(m),661)&&GetDlgItem(GetParent(m),662)&&GetDlgItem(GetParent(m),663)&&SendMessageW(GetDlgItem(GetParent(m),664),BM_GETCHECK,0,0)==BST_CHECKED,"merge view has ignore-case/space/trim and sync-scroll options");
 SendMessageW(GetParent(m),WM_COMMAND,647,0);         /* ID_BTN_ALLL: single-pass resolve all conflicts to left */
 lineText(m,1,t,1024);check(!wcscmp(t,L"int count = 10; // total")&&none(m,1,0),"resolve all (left) replaces every conflict in one pass and clears conflict colours");
 GetWindowTextW(GetDlgItem(GetParent(m),650),t,1024);check(wcsstr(t,L"\xCDA9\xB3CC 0\xAC1C")!=NULL,"merge info reports 0 conflicts remaining");
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
 putfile(L"fm\\B\\auto.txt","top\r\nmid\r\nbot\r\n");putfile(L"fm\\L\\auto.txt","LEFT\r\nmid\r\nbot\r\n");putfile(L"fm\\R\\auto.txt","top\r\nmid\r\nRIGHT\r\n");
 DeleteFileW(L"fm\\O\\auto.txt");
 putfile(L"fm\\L\\n.txt","new");DeleteFileW(L"fm\\O\\n.txt");
 putfile(L"fm\\O\\c1.txt","stale");stamp(L"fm\\O\\c1.txt",-3*24*60);
 putfile(L"fm\\O\\c2.txt","fresh");stamp(L"fm\\O\\c2.txt",10);
}
static HWND wantedClass(const wchar_t*cls,int id);
static const wchar_t*wantCls;
static BOOL CALLBACK onClass(HWND h,LPARAM l){(void)l;wchar_t c[40];GetClassNameW(h,c,40);if(!wcscmp(c,wantCls)&&(wantedId<0||GetDlgCtrlID(h)==wantedId)){wanted=h;return FALSE;}return TRUE;}
static HWND wantedClass(const wchar_t*cls,int id){wanted=NULL;wantCls=cls;wantedId=id;EnumChildWindows(frame,onClass,0);return wanted;}
static void fmergeChecks(void){
 HWND list=NULL,run=NULL,fmv=NULL;wchar_t cell0[256]={0};
 for(int i=0;i<200;i++){
  run=wantedClass(L"Button",ID_BTN_RUNM);
  if(run){fmv=GetParent(run);list=FindWindowExW(fmv,NULL,L"SysListView32",NULL);if(list&&SendMessageW(list,LVM_GETITEMCOUNT,0,0)>=4)break;}
  Sleep(50);
 }
 printf("list rows: %d\n",list?(int)SendMessageW(list,LVM_GETITEMCOUNT,0,0):-1);
 check(run&&list&&SendMessageW(list,LVM_GETITEMCOUNT,0,0)==4,"folder merge lists the four files");
 if(!run||!list)return;
 SetWindowTextW(GetDlgItem(fmv,706),L"auto");
 check(SendMessageW(list,LVM_GETITEMCOUNT,0,0)==1,"folder merge path filter narrows the visible rows");
 SetWindowTextW(GetDlgItem(fmv,706),L"");
 check(SendMessageW(list,LVM_GETITEMCOUNT,0,0)==4,"clearing the folder merge filter restores all rows");
 {NMLISTVIEW nm;memset(&nm,0,sizeof nm);nm.hdr.hwndFrom=list;nm.hdr.idFrom=700;nm.hdr.code=LVN_COLUMNCLICK;nm.iItem=-1;nm.iSubItem=0;
  SendMessageW(fmv,WM_NOTIFY,700,(LPARAM)&nm);
  LVITEMW it;memset(&it,0,sizeof it);it.iSubItem=0;it.pszText=cell0;it.cchTextMax=255;
  SendMessageW(list,LVM_GETITEMTEXTW,0,(LPARAM)&it);
  check(!wcscmp(cell0,L"n.txt"),"clicking the path column header reverses sort order");
  SendMessageW(fmv,WM_NOTIFY,700,(LPARAM)&nm);
  memset(&it,0,sizeof it);it.iSubItem=0;it.pszText=cell0;it.cchTextMax=255;cell0[0]=0;
  SendMessageW(list,LVM_GETITEMTEXTW,0,(LPARAM)&it);
  check(!wcscmp(cell0,L"auto.txt"),"clicking the path column header again sorts A-Z");}
 expectBoxes=1;lastBox[0]=0;
 SendMessageW(run,BM_CLICK,0,0);                      /* asks about the non-empty output folder, then reports */
 expectBoxes=0;
 printf("summary has %d characters, %d boxes shown\n",(int)wcslen(lastBox),shownBoxes);
 check(shownBoxes==2,"one question about the output folder and one summary");
 check(wcsstr(lastBox,L"\xBCD1\xD569 \xD30C\xC77C 2")!=NULL,"an output file newer than the inputs and an auto-merged diff3 file both count as merged");
 check(wcsstr(lastBox,L"\xBBF8\xD574\xACB0 \xCDA9\xB3CC 1")!=NULL,"an output file older than the inputs is still a conflict");
 check(wcsstr(lastBox,L"\xBCF5\xC0AC \xC2E4\xD328 0")!=NULL,"no failed copies");
 check(holds(L"fm\\O\\n.txt","new"),"the file only on the left was copied to the output");
 check(holds(L"fm\\O\\auto.txt","LEFT\r\nmid\r\nRIGHT\r\n"),"non-conflicting 3-way text changes were automatically merged by diff3");
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
 check(tipText(GetDlgItem(frame,IDM_SETTINGS),t)&&wcsstr(t,L"\xD658\xACBD\xC124\xC815")!=NULL,"preferences button explains itself");
}
/* ---- the two buttons at the right end of the tab row */
static void tabRowChecks(void){
 HWND tab=FindWindowExW(frame,NULL,L"SysTabControl32",NULL),bc=GetDlgItem(frame,IDM_CLOSE_TAB),bx=GetDlgItem(frame,IDM_EXIT),bs=GetDlgItem(frame,IDM_SETTINGS);
 check(tab&&bc&&bx&&bs,"tab row has close-tab, preferences and exit buttons");
 if(!tab||!bc||!bx||!bs)return;
 RECT rt,rc,rx,rs,cl,disp;GetWindowRect(tab,&rt);GetWindowRect(bc,&rc);GetWindowRect(bx,&rx);GetWindowRect(bs,&rs);GetClientRect(frame,&cl);
 POINT o={cl.right,0};ClientToScreen(frame,&o);
 disp=rt;SendMessageW(tab,TCM_ADJUSTRECT,0,(LPARAM)&disp);          /* top of the page area = bottom of the tab row */
 check(rc.left>=rt.right&&rs.left>=rc.right&&rx.left>=rs.right&&rx.right<=o.x,"the buttons follow the tabs: close, preferences, exit, inside the window");
 check((GetWindowLongPtrW(bs,GWL_STYLE)&BS_TYPEMASK)==(GetWindowLongPtrW(bc,GWL_STYLE)&BS_TYPEMASK),"preferences keeps the existing native button style");
 check(rc.top>=rt.top&&rc.bottom<=disp.top&&rx.top>=rt.top&&rx.bottom<=disp.top,"the buttons are in the tab row, above the pages");
 check(rc.right-rc.left>=16&&rc.bottom-rc.top>=12&&rx.right-rx.left>=16,"the buttons have a usable size");
 /* Paint the actual native controls and compare their borders and glyph colour. */
 check((GetWindowLongPtrW(bc,GWL_STYLE)&BS_TYPEMASK)==(GetWindowLongPtrW(bx,GWL_STYLE)&BS_TYPEMASK),"exit and close-tab use the same native button style");
 {RECT bounds;GetClientRect(bx,&bounds);int w=bounds.right,h=bounds.bottom;
  HDC screen=GetDC(NULL),dc=CreateCompatibleDC(screen),peer=CreateCompatibleDC(screen);
  HBITMAP bitmap=CreateCompatibleBitmap(screen,w,h),peerBitmap=CreateCompatibleBitmap(screen,w,h);
  HGDIOBJ old=SelectObject(dc,bitmap),peerOld=SelectObject(peer,peerBitmap);
  for(int state=0;state<3;state++){
   EnableWindow(bx,state!=2);EnableWindow(bc,state!=2);
   SendMessageW(bx,BM_SETSTATE,state==1,0);SendMessageW(bc,BM_SETSTATE,state==1,0);
   FillRect(dc,&bounds,(HBRUSH)GetStockObject(WHITE_BRUSH));FillRect(peer,&bounds,(HBRUSH)GetStockObject(WHITE_BRUSH));
   SendMessageW(bx,WM_PRINTCLIENT,(WPARAM)dc,PRF_CLIENT|PRF_ERASEBKGND);
   SendMessageW(bc,WM_PRINTCLIENT,(WPARAM)peer,PRF_CLIENT|PRF_ERASEBKGND);
   int red=0,sameBorder=1;
   for(int y=0;y<h;y++)for(int x=0;x<w;x++){
    COLORREF c=GetPixel(dc,x,y);if(GetRValue(c)>150&&GetGValue(c)<80&&GetBValue(c)<80)red++;
    if((x<2||y<2||x>=w-2||y>=h-2)&&c!=GetPixel(peer,x,y))sameBorder=0;
   }
   check(red>0,"native exit button paints a red icon in normal, pressed and disabled states");
   check(sameBorder,"exit button border matches close-tab in normal, pressed and disabled states");
  }
  EnableWindow(bx,TRUE);EnableWindow(bc,TRUE);SendMessageW(bx,BM_SETSTATE,0,0);SendMessageW(bc,BM_SETSTATE,0,0);
  SelectObject(dc,old);SelectObject(peer,peerOld);DeleteObject(bitmap);DeleteObject(peerBitmap);DeleteDC(dc);DeleteDC(peer);ReleaseDC(NULL,screen);}
}
/* Preferences are driven through real controls while the main thread runs its modal loop. */
extern wchar_t g_ini[];
extern void lh_keep(const wchar_t*);
static HWND prefsWindow;
static void prefsSnapshot(HWND h,const wchar_t*path){
 RECT r;GetClientRect(h,&r);HDC screen=GetDC(h),dc=CreateCompatibleDC(screen);BITMAPINFO bi;memset(&bi,0,sizeof bi);bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=r.right;bi.bmiHeader.biHeight=-r.bottom;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 void*pixels=NULL;HBITMAP bitmap=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&pixels,NULL,0);if(bitmap){HGDIOBJ old=SelectObject(dc,bitmap);PrintWindow(h,dc,PW_CLIENTONLY);
  BITMAPFILEHEADER head={0};head.bfType=0x4d42;head.bfOffBits=sizeof head+sizeof(BITMAPINFOHEADER);head.bfSize=head.bfOffBits+r.right*r.bottom*4;FILE*f=_wfopen(path,L"wb");if(f){fwrite(&head,1,sizeof head,f);fwrite(&bi.bmiHeader,1,sizeof(BITMAPINFOHEADER),f);fwrite(pixels,1,r.right*r.bottom*4,f);fclose(f);}SelectObject(dc,old);DeleteObject(bitmap);}
 DeleteDC(dc);ReleaseDC(h,screen);
}
static BOOL CALLBACK onSettings(HWND h,LPARAM p){wchar_t cls[40];(void)p;GetClassNameW(h,cls,40);if(!wcscmp(cls,L"BCSettings")){prefsWindow=h;return FALSE;}return TRUE;}
static void prefsPage(HWND h,int page){HWND tab=GetDlgItem(h,550);SendMessageW(tab,TCM_SETCURSEL,page,0);NMHDR nm={tab,550,TCN_SELCHANGE};SendMessageW(h,WM_NOTIFY,550,(LPARAM)&nm);}
static DWORD WINAPI prefsDriver(void*arg){
 int phase=(int)(intptr_t)arg;HWND h=NULL;wchar_t t[80];
 for(int i=0;i<200;i++){prefsWindow=NULL;EnumThreadWindows(mainTid,onSettings,0);h=prefsWindow;if(h&&GetDlgItem(h,1)&&GetWindowTextW(GetDlgItem(h,572),t,80)>0)break;Sleep(25);}
 check(h&&GetDlgItem(h,550),"preferences button opens the application preferences dialog");if(!h)return 0;
 if(phase==0)prefsSnapshot(h,L"preferences-general.bmp");
 check(SendMessageW(GetDlgItem(h,550),TCM_GETITEMCOUNT,0,0)==3,"preferences groups general/compare/history, appearance/keys and external tools");
 check((GetWindowLongPtrW(GetDlgItem(h,560),GWL_STYLE)&WS_VISIBLE)&&!(GetWindowLongPtrW(GetDlgItem(h,501),GWL_STYLE)&WS_VISIBLE),"general page is initially visible and appearance page is hidden");
 prefsPage(h,1);if(phase==0)prefsSnapshot(h,L"preferences-appearance.bmp");check((GetWindowLongPtrW(GetDlgItem(h,501),GWL_STYLE)&WS_VISIBLE)&&!(GetWindowLongPtrW(GetDlgItem(h,560),GWL_STYLE)&WS_VISIBLE),"appearance tab exposes the existing font and shortcut fields");
 prefsPage(h,2);if(phase==0)prefsSnapshot(h,L"preferences-external.bmp");check((GetWindowLongPtrW(GetDlgItem(h,540),GWL_STYLE)&WS_VISIBLE)&&!(GetWindowLongPtrW(GetDlgItem(h,501),GWL_STYLE)&WS_VISIBLE),"external-tools tab exposes its command editor");prefsPage(h,0);
 if(phase==0){SetWindowTextW(GetDlgItem(h,572),L"7");SendMessageW(h,WM_COMMAND,2,0);return 0;}
 if(phase==2){
  GetWindowTextW(GetDlgItem(h,572),t,80);check(!wcscmp(t,L"3"),"saved history limit appears when preferences reopen");
  SendMessageW(h,WM_COMMAND,3,0);GetWindowTextW(GetDlgItem(h,572),t,80);check(!wcscmp(t,L"30")&&SendMessageW(GetDlgItem(h,570),CB_GETCURSEL,0,0)==0,"defaults reset controls to documented values");
  SendMessageW(h,WM_COMMAND,2,0);return 0;
 }
 int before=shownBoxes;SetWindowTextW(GetDlgItem(h,572),L"0");prefsPage(h,1);PostMessageW(h,WM_COMMAND,1,0);
 for(int i=0;i<100&&shownBoxes==before;i++)Sleep(10);
 check(shownBoxes==before+1&&IsWindow(h)&&SendMessageW(GetDlgItem(h,550),TCM_GETCURSEL,0,0)==0,"invalid history limits show an error and return to the field's tab without closing preferences");
 SetWindowTextW(GetDlgItem(h,572),L"3");SetWindowTextW(GetDlgItem(h,573),L"1");
 SendMessageW(GetDlgItem(h,560),BM_SETCHECK,BST_CHECKED,0);SendMessageW(GetDlgItem(h,563),BM_SETCHECK,BST_CHECKED,0);
 SendMessageW(GetDlgItem(h,565),BM_SETCHECK,BST_UNCHECKED,0);SendMessageW(GetDlgItem(h,567),BM_SETCHECK,BST_UNCHECKED,0);
 SendMessageW(GetDlgItem(h,569),BM_SETCHECK,BST_CHECKED,0);SendMessageW(GetDlgItem(h,570),CB_SETCURSEL,1,0);SendMessageW(GetDlgItem(h,571),CB_SETCURSEL,4,0);
 SendMessageW(h,WM_COMMAND,1,0);return 0;
}
static int historyCount(const wchar_t*path){
 wchar_t normalized[1200],dir[1600];wcscpy(normalized,path);CharLowerBuffW(normalized,(DWORD)wcslen(normalized));uint64_t hash=0xcbf29ce484222325ull;
 for(const wchar_t*p=normalized;*p;p++){hash^=*p;hash*=0x100000001b3ull;}
 wcscpy(dir,g_ini);wchar_t*slash=wcsrchr(dir,L'\\');if(!slash)return 0;*slash=0;
 size_t n=wcslen(dir);swprintf(dir+n,1600-n,L"\\history\\%016llX\\*.bak",(unsigned long long)hash);
 WIN32_FIND_DATAW data;HANDLE f=FindFirstFileW(dir,&data);int count=0;if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(!(data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY))count++;}while(FindNextFileW(f,&data));FindClose(f);return count;
}
static void preferencesChecks(void){
 int original=(int)GetPrivateProfileIntW(L"History",L"MaxVersions",30,g_ini);expectBoxes=1;
 for(int phase=0;phase<3;phase++){
  HANDLE driver=CreateThread(NULL,0,prefsDriver,(void*)(intptr_t)phase,0,NULL);
  SendMessageW(GetDlgItem(frame,IDM_SETTINGS),BM_CLICK,0,0);check(WaitForSingleObject(driver,5000)==WAIT_OBJECT_0,"preferences closes through its own buttons");CloseHandle(driver);
  if(phase==0)check(GetPrivateProfileIntW(L"History",L"MaxVersions",30,g_ini)==(UINT)original,"Cancel does not persist changed controls");
 }
 expectBoxes=0;
 check(GetPrivateProfileIntW(L"History",L"MaxVersions",0,g_ini)==3&&GetPrivateProfileIntW(L"History",L"MaxFileMB",0,g_ini)==1,"history limits persist and cancelling Defaults preserves them");
 check(g_dark==1&&GetPrivateProfileIntW(L"UI",L"Dark",0,g_ini)==1,"theme preference persists and applies immediately");
 uint64_t before=g_nsess;SendMessageW(frame,WM_COMMAND,1002,0);for(int i=0;i<100&&g_nsess==before;i++)Sleep(10);
 HWND text=FindWindowExW(frame,NULL,L"BCText",NULL);
 check(text&&SendMessageW(GetDlgItem(text,217),BM_GETCHECK,0,0)==BST_CHECKED&&SendMessageW(GetDlgItem(text,226),BM_GETCHECK,0,0)==BST_CHECKED,"new text views consume the saved folding and character-highlight defaults");
 check(text&&SendMessageW(GetDlgItem(text,222),CB_GETCURSEL,0,0)==4,"new text views consume the saved context-line default");
 wchar_t probe[1200];GetFullPathNameW(L"preferences-history.txt",1200,probe,NULL);
 for(int i=0;i<5;i++){putfile(probe,"recorded version");HANDLE f=CreateFileW(probe,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);ULONGLONG value=133000000000000000ull+(ULONGLONG)i*10000000;FILETIME time={(DWORD)value,(DWORD)(value>>32)};SetFileTime(f,NULL,NULL,&time);CloseHandle(f);lh_keep(probe);}
 check(historyCount(probe)==3,"local history actually enforces the configured version count");
 WritePrivateProfileStringW(L"History",L"Enabled",L"0",g_ini);putfile(probe,"a later version");lh_keep(probe);check(historyCount(probe)==3,"turning history off leaves existing versions intact");
 WritePrivateProfileStringW(L"History",L"Enabled",L"1",g_ini);
 GetFullPathNameW(L"preferences-large.txt",1200,probe,NULL);FILE*f=_wfopen(probe,L"wb");if(f){fseek(f,2*1024*1024,SEEK_SET);fputc('x',f);fclose(f);}lh_keep(probe);
 check(historyCount(probe)==0,"local history actually skips files over the configured size limit");
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
 wchar_t out[520]={0};int r=__real_browse_folder(frame,out);
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
static int rows(HWND home){return (int)SendMessageW(GetDlgItem(home,ID_FIND_LIST),LVM_GETITEMCOUNT,0,0);}
/* name, kind (row of the combo box), extensions, folder text, drive (row of its combo box), cloud choice -> rows */
static int filtered2(HWND home,const wchar_t*text,int kind,const wchar_t*ext,const wchar_t*folder,int drive,int cloud){
 SendMessageW(GetDlgItem(home,ID_FIND_KIND),CB_SETCURSEL,kind,0);
 SendMessageW(GetDlgItem(home,ID_FIND_DRIVE),CB_SETCURSEL,drive,0);
 SendMessageW(GetDlgItem(home,ID_FIND_CLOUD),CB_SETCURSEL,cloud,0);
 SetWindowTextW(GetDlgItem(home,ID_FIND_EXT),ext);SetWindowTextW(GetDlgItem(home,ID_FIND_PATH),folder);
 SetWindowTextW(GetDlgItem(home,ID_FIND_EDIT),text);
 SendMessageW(GetDlgItem(home,ID_FIND_BTN),BM_CLICK,0,0);
 return rows(home);
}
/* the checks written for one tree keep to the first tree */
static int filtered(HWND home,const wchar_t*text,int kind,const wchar_t*ext,const wchar_t*folder){return filtered2(home,text,kind,ext,folder,1,0);}
static int search(HWND home,const wchar_t*text){        /* every filter off: type, press the button, count the rows */
 return filtered2(home,text,0,L"",L"",0,0);
}
static void cell(HWND home,int row,int col,wchar_t*out,int cap){
 LVITEMW it;memset(&it,0,sizeof it);it.iSubItem=col;it.pszText=out;it.cchTextMax=cap;out[0]=0;
 SendMessageW(GetDlgItem(home,ID_FIND_LIST),LVM_GETITEMTEXTW,row,(LPARAM)&it);
}
static void rowPath(HWND home,int row,wchar_t*out){       /* folder column + name column */
 wchar_t n[1100];cell(home,row,4,out,1100);cell(home,row,0,n,1100);wcscat(out,L"\\");wcscat(out,n);
}
static int rowIs(HWND home,int i,const wchar_t*want){
 wchar_t t[2300];rowPath(home,i,t);
 if(_wcsicmp(t,want))printf("row %d is another path (%d characters)\n",i,(int)wcslen(t));
 return !_wcsicmp(t,want);
}
static int hasRow(HWND home,const wchar_t*want){
 int n=rows(home);wchar_t t[2300];
 for(int i=0;i<n;i++){rowPath(home,i,t);if(!_wcsicmp(t,want))return 1;}
 return 0;
}
/* the names of the rows from the top, joined with '|' */
static int order(HWND home,const wchar_t*want){
 wchar_t got[3000]=L"",n[1100];int c=rows(home);
 for(int i=0;i<c&&i<20;i++){cell(home,i,0,n,1100);if(i)wcscat(got,L"|");wcscat(got,n);}
 if(wcscmp(got,want)){
  char a[600];WideCharToMultiByte(CP_UTF8,0,got,-1,a,600,NULL,NULL);printf("order is %s\n",a);
 }
 return !wcscmp(got,want);
}
static void selectRow(HWND list,int row){LVITEMW it;memset(&it,0,sizeof it);it.state=it.stateMask=LVIS_SELECTED|LVIS_FOCUSED;SendMessageW(list,LVM_SETITEMSTATE,row,(LPARAM)&it);}
static void clearSelection(HWND list){LVITEMW it;memset(&it,0,sizeof it);it.stateMask=LVIS_SELECTED;SendMessageW(list,LVM_SETITEMSTATE,-1,(LPARAM)&it);}
static void clickColumn(HWND home,int col){
 NMLISTVIEW nm;memset(&nm,0,sizeof nm);nm.hdr.hwndFrom=GetDlgItem(home,ID_FIND_LIST);nm.hdr.idFrom=ID_FIND_LIST;nm.hdr.code=LVN_COLUMNCLICK;nm.iItem=-1;nm.iSubItem=col;
 SendMessageW(home,WM_NOTIFY,ID_FIND_LIST,(LPARAM)&nm);
}
static void doubleClick(HWND home,int row){
 NMITEMACTIVATE nm;memset(&nm,0,sizeof nm);nm.hdr.hwndFrom=GetDlgItem(home,ID_FIND_LIST);nm.hdr.idFrom=ID_FIND_LIST;nm.hdr.code=NM_DBLCLK;nm.iItem=row;
 SendMessageW(home,WM_NOTIFY,ID_FIND_LIST,(LPARAM)&nm);
}
static int arrow(HWND home,int col){                    /* 1 up, 2 down, 0 none */
 HWND h=(HWND)SendMessageW(GetDlgItem(home,ID_FIND_LIST),LVM_GETHEADER,0,0);HDITEMW hd;memset(&hd,0,sizeof hd);hd.mask=HDI_FORMAT;
 SendMessageW(h,HDM_GETITEMW,col,(LPARAM)&hd);return (hd.fmt&HDF_SORTUP)?1:(hd.fmt&HDF_SORTDOWN)?2:0;
}
#define KO_DOC L"\xBCF4\xACE0\xC11C \xCD5C\xC885.docx"
static wchar_t findRoot2[600];
/* two trees. The first:  Alpha\Report_2026.TXT (10 bytes, 3 days old), Alpha\Deep\(Korean).docx (20 bytes, 4 days),
                          Beta\report-draft.md (3000 bytes, 1 day), Beta\other.bin (500 bytes, 2 days)
              the second: Gamma\zeta_note.log (5 days). Beta counts as cloud storage. */
static void fileOf(const wchar_t*path,int bytes,int daysOld){
 FILE*f=_wfopen(path,L"wb");if(!f)return;for(int i=0;i<bytes;i++)fputc('x',f);fclose(f);stamp(path,-daysOld*24*60);
}
static void findFixture(void){
 wchar_t both[1300],cloud[700];
 CreateDirectoryW(L"ff",NULL);CreateDirectoryW(L"ff\\Alpha",NULL);CreateDirectoryW(L"ff\\Alpha\\Deep",NULL);CreateDirectoryW(L"ff\\Beta",NULL);
 CreateDirectoryW(L"ff2",NULL);CreateDirectoryW(L"ff2\\Gamma",NULL);
 fileOf(L"ff\\Alpha\\Report_2026.TXT",10,3);fileOf(L"ff\\Alpha\\Deep\\" KO_DOC,20,4);
 fileOf(L"ff\\Beta\\report-draft.md",3000,1);fileOf(L"ff\\Beta\\other.bin",500,2);
 fileOf(L"ff2\\Gamma\\zeta_note.log",7,5);
 GetFullPathNameW(L"ff",600,findRoot,NULL);GetFullPathNameW(L"ff2",600,findRoot2,NULL);
 swprintf(both,1300,L"%ls;%ls",findRoot,findRoot2);SetEnvironmentVariableW(L"BC_FIND_ROOT",both);
 swprintf(cloud,700,L"%ls\\Beta",findRoot);SetEnvironmentVariableW(L"BC_FIND_CLOUD",cloud);
}
static void findChecks(void){
 HWND home=homeView();wchar_t want[900],label[200],t[1200];uint32_t ids[10];
 for(int i=0;i<100&&!(home&&GetDlgItem(home,ID_FIND_LIST));i++){Sleep(50);home=homeView();}   /* the view is still being built */
 check(home&&GetDlgItem(home,ID_FIND_EDIT)&&GetDlgItem(home,ID_FIND_BTN)&&GetDlgItem(home,ID_FIND_LIST),"Home has the file search field, button and list");
 if(!home)return;
 HWND list=GetDlgItem(home,ID_FIND_LIST),recent=GetDlgItem(home,ID_RECENT);
 check(!shown(list)&&shown(recent)&&fi_state==0,"before a search the recent sessions are shown and nothing is listed");
 search(home,L"report");
 for(int i=0;i<200&&fi_state!=2;i++)Sleep(50);
 check(fi_state==2,"the folder trees are listed");
 {int n=0;for(int i=0;i<60;i++){n=rows(home);if(n==2)break;Sleep(50);}
  check(n==2,"the first search fills in by itself once the listing is done");}
 {RECT re,rb2,rl,rk;GetWindowRect(GetDlgItem(home,ID_FIND_EDIT),&re);GetWindowRect(GetDlgItem(home,ID_FIND_BTN),&rb2);GetWindowRect(list,&rl);GetWindowRect(GetDlgItem(home,ID_FIND_KIND),&rk);
  check(re.bottom-re.top>=16&&re.right-re.left>=200&&rb2.bottom-rb2.top>=16&&rb2.right-rb2.left>=40,"the search field and button have a usable size");
  check(rb2.left>=re.right&&re.bottom<=rk.top&&rk.top<rl.top,"the field is left of the button; the filters are below it, above the list");}
 check(fi_mode==2,"they were listed by walking the folders");
 check(search(home,L"report")==2,"two names contain the text, whatever their case");
 swprintf(want,900,L"%ls\\Alpha\\Report_2026.TXT",findRoot);check(hasRow(home,want),"a result is the full path of the file");
 swprintf(want,900,L"%ls\\Beta\\report-draft.md",findRoot);check(hasRow(home,want),"the other result too");
 check(shown(list)&&!shown(recent),"the results take the place of the recent sessions");
 GetWindowTextW(GetDlgItem(home,ID_RLABEL),label,200);check(wcsstr(label,L"\xACB0\xACFC 2\xAC1C")!=NULL,"the heading shows the number of results");
 check(search(home,L"\xCD5C\xC885")==1,"a Korean name is found");
 swprintf(want,900,L"%ls\\Alpha\\Deep\\" KO_DOC,findRoot);check(rowIs(home,0,want),"with its full path");
 check(search(home,L"DEEP")==1,"folders are found too");
 check(search(home,L"no-such-name-anywhere")==0,"a text that no name contains gives no rows");
 check(find_query(L"REPORT",ids,10)==2&&find_query(L".",ids,10)==5&&find_query(L"",ids,10)==0,"searching the index directly agrees");
 /* the columns of a row */
 check(filtered(home,L"draft",0,L"",L"")==1,"one row to read");
 cell(home,0,0,t,1200);check(!wcscmp(t,L"report-draft.md"),"column 1 is the name");
 cell(home,0,1,t,1200);check(wcslen(t)==16&&t[4]==L'-'&&t[10]==L' '&&t[13]==L':',"column 2 is the modification date and time");
 cell(home,0,2,t,1200);check(!wcsncmp(t,L"md ",3),"column 3 is the type");
 cell(home,0,3,t,1200);check(!wcscmp(t,L"3KB"),"column 4 is the size, rounded up to KB");
 cell(home,0,4,t,1200);swprintf(want,900,L"%ls\\Beta",findRoot);check(!_wcsicmp(t,want),"column 5 is the folder");
 check(filtered(home,L"deep",0,L"",L"")==1,"a folder row");
 cell(home,0,3,t,1200);check(!t[0],"a folder shows no size");
 cell(home,0,2,t,1200);check(t[0]!=0&&wcsncmp(t,L"deep",4),"and its type says it is a folder");
 /* sorting: newest first to begin with; a click on a header sorts by that column, a second click turns it */
 check(filtered(home,L".",1,L"",L"")==4,"the four files of the first tree");
 check(order(home,L"report-draft.md|other.bin|Report_2026.TXT|" KO_DOC)&&arrow(home,1)==2,"newest first by default, the date header shows a down arrow");
 clickColumn(home,1);check(order(home,KO_DOC L"|Report_2026.TXT|other.bin|report-draft.md")&&arrow(home,1)==1,"date again: oldest first");
 clickColumn(home,0);check(order(home,L"other.bin|report-draft.md|Report_2026.TXT|" KO_DOC)&&arrow(home,0)==1&&arrow(home,1)==0,"name: A first, case ignored; the arrow moves to that header");
 clickColumn(home,0);check(order(home,KO_DOC L"|Report_2026.TXT|report-draft.md|other.bin")&&arrow(home,0)==2,"name again: reversed");
 clickColumn(home,3);check(order(home,L"report-draft.md|other.bin|" KO_DOC L"|Report_2026.TXT"),"size: largest first");
 clickColumn(home,3);check(order(home,L"Report_2026.TXT|" KO_DOC L"|other.bin|report-draft.md"),"size again: smallest first");
 clickColumn(home,2);check(order(home,L"other.bin|" KO_DOC L"|report-draft.md|Report_2026.TXT"),"type: by extension");
 clickColumn(home,4);check(order(home,L"Report_2026.TXT|" KO_DOC L"|other.bin|report-draft.md"),"folder: by the path of the folder, then by name");
 check(filtered(home,L"e",0,L"",L"")==5&&order(home,L"Beta|Deep|Report_2026.TXT|other.bin|report-draft.md"),"a new search keeps the chosen order");
 clickColumn(home,1);check(arrow(home,1)==2&&filtered(home,L".",1,L"",L"")==4&&order(home,L"report-draft.md|other.bin|Report_2026.TXT|" KO_DOC),"back to newest first");
 /* filters. The first tree: Alpha\Report_2026.TXT, Alpha\Deep\(Korean).docx, Beta\report-draft.md, Beta\other.bin */
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
 {uint32_t f[8]={1,0,0,0,0,0,0,0};check(find_query2(L"",ids,10,f)==5,"searching the index directly with a filter agrees");}
 /* drives: each listed tree is a row of the drive filter */
 {HWND d=GetDlgItem(home,ID_FIND_DRIVE);wchar_t n1[700]={0},n2[700]={0};
  check(SendMessageW(d,CB_GETCOUNT,0,0)==3,"the drive filter lists all drives and the two trees");
  SendMessageW(d,CB_GETLBTEXT,1,(LPARAM)n1);SendMessageW(d,CB_GETLBTEXT,2,(LPARAM)n2);
  check(!_wcsicmp(n1,findRoot)&&!_wcsicmp(n2,findRoot2),"named after the trees");}
 check(filtered2(home,L".",1,L"",L"",0,0)==5,"all drives: the files of both trees");
 check(filtered2(home,L".",1,L"",L"",1,0)==4,"the first drive only");
 check(filtered2(home,L".",1,L"",L"",2,0)==1,"the second drive only");
 swprintf(want,900,L"%ls\\Gamma\\zeta_note.log",findRoot2);check(rowIs(home,0,want),"which holds the log file");
 check(filtered2(home,L"",0,L"",L"",2,0)==2,"a drive alone lists everything on it: the folder and the file");
 check(filtered2(home,L"report",0,L"",L"",2,0)==0,"a name that is on the other drive only");
 /* cloud storage: Beta of the first tree */
 check(fi_ncloud==1,"one cloud storage folder is known");
 check(filtered2(home,L".",1,L"",L"",0,0)==5,"cloud included: every file");
 check(filtered2(home,L".",1,L"",L"",0,1)==3&&order(home,L"Report_2026.TXT|" KO_DOC L"|zeta_note.log"),"cloud left out: not the two files in Beta");
 check(filtered2(home,L".",1,L"",L"",0,2)==2&&order(home,L"report-draft.md|other.bin"),"cloud only: the two files in Beta");
 check(filtered2(home,L"",2,L"",L"",0,2)==1,"the cloud folder itself counts as cloud");
 check(filtered2(home,L"",0,L"",L"",2,2)==0,"the second drive has nothing in the cloud");
 /* the x in the name field, the reset button, the folder button */
 {HWND e=GetDlgItem(home,ID_FIND_EDIT),x=GetDlgItem(home,ID_FIND_CLR),rs=GetDlgItem(home,ID_FIND_RESET),pb=GetDlgItem(home,ID_FIND_PBROWSE),pe=GetDlgItem(home,ID_FIND_PATH);
  RECT re,rx,rr,rp,rpe;wchar_t v[700];
  check(x&&rs&&pb,"Home has the clear, reset and folder buttons");
  GetWindowRect(e,&re);GetWindowRect(x,&rx);GetWindowRect(rs,&rr);GetWindowRect(pb,&rp);GetWindowRect(pe,&rpe);
  check(rx.left>re.left&&rx.right<=re.right&&rx.top>=re.top&&rx.bottom<=re.bottom&&rx.right-rx.left>=12,"the clear button is inside the right end of the name field");
  check(rp.left>=rpe.right&&rr.left>=rp.right&&rp.right-rp.left>=16&&rr.right-rr.left>=40,"folder field, folder button, reset button from left to right");
  search(home,L"");check(!shown(x),"nothing typed: no clear button");
  search(home,L"report");check(shown(x)&&rows(home)==2,"something typed: the clear button shows");
  /* the click goes to whatever is on top at the centre of the x, as a mouse click would: the field used to
     cover the button and take the click */
  {POINT c={(rx.left+rx.right)/2,(rx.top+rx.bottom)/2};ScreenToClient(home,&c);
   HWND hit=ChildWindowFromPointEx(home,c,CWP_SKIPINVISIBLE);
   int above=0;for(HWND w=GetWindow(home,GW_CHILD);w;w=GetWindow(w,GW_HWNDNEXT)){if(w==x){above=1;break;}if(w==e)break;}
   check(above,"the clear button is above the name field");
   check(hit==x,"the control on top at its centre is the clear button, not the field");
   POINT in={(rx.right-rx.left)/2,(rx.bottom-rx.top)/2};LPARAM l=MAKELPARAM(in.x,in.y);
   SendMessageW(hit,WM_LBUTTONDOWN,MK_LBUTTON,l);SendMessageW(hit,WM_LBUTTONUP,0,l);}
  GetWindowTextW(e,v,700);check(!v[0]&&!shown(x)&&shown(recent)&&!shown(list),"a click on it empties the field and the search");
  /* reset: every filter back, the name stays */
  check(filtered2(home,L"report",1,L"md",L"beta",1,2)==1,"a search with every filter set");
  SendMessageW(rs,BM_CLICK,0,0);
  GetWindowTextW(e,v,700);
  check(!wcscmp(v,L"report")&&SendMessageW(GetDlgItem(home,ID_FIND_DRIVE),CB_GETCURSEL,0,0)==0&&SendMessageW(GetDlgItem(home,ID_FIND_KIND),CB_GETCURSEL,0,0)==0
        &&SendMessageW(GetDlgItem(home,ID_FIND_CLOUD),CB_GETCURSEL,0,0)==0&&!GetWindowTextLengthW(GetDlgItem(home,ID_FIND_EXT))&&!GetWindowTextLengthW(pe),"reset: all drives, every kind, cloud included, no extension, no folder; the name stays");
  check(rows(home)==2,"and the search runs again without the filters");
  /* folder button: the chosen folder becomes the folder filter */
  search(home,L"");
  swprintf(fakeFolder,700,L"%ls\\Beta",findRoot);browseCalls=0;
  SendMessageW(pb,BM_CLICK,0,0);
  GetWindowTextW(pe,v,700);check(browseCalls==1&&!_wcsicmp(v,fakeFolder),"the folder button puts the chosen folder into the folder filter");
  check(rows(home)==2&&order(home,L"report-draft.md|other.bin"),"and lists what is inside that folder");
  fakeFolder[0]=0;cancelFolder=1;browseCalls=0;SendMessageW(pb,BM_CLICK,0,0);cancelFolder=0;
  GetWindowTextW(pe,v,700);check(browseCalls==1&&wcsstr(v,L"Beta")!=NULL&&rows(home)==2,"a cancelled folder dialog changes nothing");
  SendMessageW(rs,BM_CLICK,0,0);check(!GetWindowTextLengthW(pe)&&shown(recent),"reset with no name left: back to the recent sessions");}
 /* opening what was found */
 check(filtered(home,L"other",0,L"",L"")==1,"one row to open");
 swprintf(want,900,L"%ls\\Beta\\other.bin",findRoot);
 selectRow(list,0);shCalls=0;doubleClick(home,0);
 check(shCalls==1&&!wcscmp(shVerb,L"open")&&!_wcsicmp(shFile,want)&&!shArgs[0],"double click opens the file with its registered program");
 menuAnswer=0;menuShown=0;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 check(menuShown==13&&shCalls==0,"the menu has ten actions and three separators; leaving it does nothing");
 {HWND bSelAll=GetDlgItem(home,ID_FIND_SELALL),bCopy=GetDlgItem(home,ID_FIND_COPY),bShare=GetDlgItem(home,ID_FIND_SHARE),bEmail=GetDlgItem(home,ID_FIND_EMAIL),bDel=GetDlgItem(home,ID_FIND_DEL),bProps=GetDlgItem(home,ID_FIND_PROPS);
  check(bSelAll&&bCopy&&bShare&&bEmail&&bDel&&bProps,"action buttons (select all, copy, share, email, del, props) exist");
  check(shown(bSelAll)&&shown(bCopy)&&shown(bDel)&&shown(bProps),"action buttons are shown with search results");
  clearSelection(list);
  GetWindowTextW(bSelAll,label,200);
  check(!wcscmp(label,L"\xC804\xCCB4 \xC120\xD0DD"),"unselected results show Select All on the button");
  SendMessageW(bSelAll,BM_CLICK,0,0);
  GetWindowTextW(bSelAll,label,200);
  check(!wcscmp(label,L"\xC804\xCCB4 \xD574\xC81C"),"selecting all changes the button caption to Clear All");
  check(SendMessageW(list,LVM_GETSELECTEDCOUNT,0,0)==rows(home),"select all highlights every result row");
  GetWindowTextW(GetDlgItem(home,ID_RLABEL),label,200);
  check(wcsstr(label,L"\xC120\xD0DD\xB428")!=NULL,"select all checks items and updates count label");
  clipText[0]=0;SendMessageW(bCopy,BM_CLICK,0,0);
  check(wcsstr(clipText,L"other.bin")!=NULL,"copy copies checked items to clipboard");
  SendMessageW(bSelAll,BM_CLICK,0,0);
  GetWindowTextW(bSelAll,label,200);
  check(!wcscmp(label,L"\xC804\xCCB4 \xC120\xD0DD"),"clearing all restores the Select All caption");
  GetWindowTextW(GetDlgItem(home,ID_RLABEL),label,200);
  check(wcsstr(label,L"\xC120\xD0DD\xB428")==NULL,"select all again unchecks items");
  check(SendMessageW(list,LVM_GETSELECTEDCOUNT,0,0)==0,"select all again clears row highlighting");}
 selectRow(list,0);
 menuAnswer=2;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 check(shCalls==1&&!wcscmp(shVerb,L"openas")&&!_wcsicmp(shFile,want),"menu: choose the program");
 menuAnswer=3;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 {wchar_t arg[1000];swprintf(arg,1000,L"/select,\"%ls\"",want);
  check(shCalls==1&&!_wcsicmp(shFile,L"explorer.exe")&&!_wcsicmp(shArgs,arg),"menu: show in its folder, selected");}
 menuAnswer=5;shCalls=0;clipText[0]=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
 check(shCalls==0&&!_wcsicmp(clipText,want),"menu: copy the path");
 check(filtered(home,L"Report_2026",0,L"",L"")==1,"a text file to open in the application");
 selectRow(list,0);
 {uint64_t before=g_nsess;menuAnswer=4;shCalls=0;SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
  for(int i=0;i<40&&g_nsess==before;i++)Sleep(50);
  check(shCalls==0&&g_nsess==before+1,"menu: open in a session of the application");}
 menuAnswer=0;
 /* Deletion: decline, failure, shell cancellation, success, partial batch, folder descendants. */
 filtered(home,L"other.bin",0,L"",L"");selectRow(list,0);
 expectBoxes=1;shownBoxes=0;boxAnswer=IDNO;deleteCalls=0;
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(shownBoxes==1&&deleteCalls==0&&rows(home)==1,"declining delete keeps the result and never calls the shell");
 check((lastBoxFlags&MB_DEFBUTTON2)&&wcsstr(lastBox,L"1"),"confirmation shows target count and defaults to No");
 boxAnswer=IDYES;deleteMode=1;
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(deleteCalls==1&&rows(home)==1,"failed deletion keeps the file in search results");
 selectRow(list,0);deleteMode=3;
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(rows(home)==1&&find_query(L"other.bin",ids,10)==1,"shell cancellation keeps the indexed file");
 selectRow(list,0);deleteMode=0;
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(rows(home)==0&&find_query(L"other.bin",ids,10)==0,"confirmed deletion removes the result and index entry");
 check((deleteFlags&(FOF_ALLOWUNDO|FOF_NOCONFIRMATION))==(FOF_ALLOWUNDO|FOF_NOCONFIRMATION),"confirmed deletion recycles without a duplicate shell prompt");
 check(search(home,L"other.bin")==0,"deleted file stays absent on another search");
 check(filtered(home,L"report",0,L"",L"")==2,"two files for a partial batch deletion");
 /* A single checked row must not mask a highlighted group. */
 selectRow(list,0);
 {NMLVKEYDOWN key;memset(&key,0,sizeof key);key.hdr.hwndFrom=list;key.hdr.idFrom=ID_FIND_LIST;key.hdr.code=LVN_KEYDOWN;key.wVKey=VK_SPACE;
  SendMessageW(home,WM_NOTIFY,ID_FIND_LIST,(LPARAM)&key);}
 selectRow(list,1);
 check(SendMessageW(list,LVM_GETSELECTEDCOUNT,0,0)==2,"two highlighted rows with one checkbox checked");
 {int before=deleteCalls;boxAnswer=IDNO;
  SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
  check(wcsstr(lastBox,L"2")&&deleteCalls==before&&rows(home)==2,"multi-selection confirmation counts both rows; declining keeps both");}
 boxAnswer=IDYES;deleteMode=1;
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(wcsstr(lastBox,L"2")&&rows(home)==2,"failed deletion of a highlighted group keeps both results");
 filtered(home,L"report",0,L"",L"");
 clearSelection(list);
 SendMessageW(GetDlgItem(home,ID_FIND_SELALL),BM_CLICK,0,0);deleteMode=2;
 check(SendMessageW(list,LVM_GETSELECTEDCOUNT,0,0)==2,"select all highlights both rows of a multiple-result search");
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(rows(home)==1&&find_query(L"report",ids,10)==1,"partial deletion removes only the file actually deleted");
 check(wcsstr(lastBox,L"2")!=NULL,"batch confirmation shows both checked targets");
 check(filtered(home,L"Alpha",2,L"",L"")==1,"folder to delete with indexed descendants");
 selectRow(list,0);deleteMode=0;
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(rows(home)==0&&find_query(L"Deep",ids,10)==0&&find_query(KO_DOC,ids,10)==0,"deleted folder and its descendants disappear from searches");
 check(filtered2(home,L"",2,L"",L"",0,0)==2,"two remaining folders for highlighted multi-delete without checks");
 selectRow(list,0);selectRow(list,1);
 GetWindowTextW(GetDlgItem(home,ID_FIND_SELALL),label,200);
 check(!wcscmp(label,L"\xC804\xCCB4 \xD574\xC81C"),"manually selecting every row also shows Clear All");
 {int before=deleteCalls;boxAnswer=IDNO;menuAnswer=8; /* FA_DEL */
  SendMessageW(home,WM_CONTEXTMENU,(WPARAM)list,(LPARAM)-1);
  check(wcsstr(lastBox,L"2")&&deleteCalls==before&&rows(home)==2,"context menu preserves the highlighted group when deletion is declined");}
 menuAnswer=0;boxAnswer=IDYES;
 SendMessageW(GetDlgItem(home,ID_FIND_DEL),BM_CLICK,0,0);
 check(wcsstr(lastBox,L"2")&&rows(home)==0,"confirming a highlighted group deletes both items");
 GetWindowTextW(GetDlgItem(home,ID_FIND_SELALL),label,200);
 check(!wcscmp(label,L"\xC804\xCCB4 \xC120\xD0DD"),"empty results reset the button to Select All");
 check(filtered2(home,L"",1,L"",L"",0,0)==0,"files inside both deleted folders stay absent on re-search");
 expectBoxes=0;boxAnswer=0;
 search(home,L"");
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
 SetEnvironmentVariableW(L"BC_FIND_CLOUD",L"?");       /* no cloud folders: nothing of the machine is looked up */
 if(usnMode)SetEnvironmentVariableW(L"BC_FIND_MODE",L"usn");
 swprintf(probeName,80,L"bc_find_probe_%X_%X.tmp",(unsigned)GetCurrentProcessId(),(unsigned)GetTickCount());
 swprintf(probePath,700,L"%ls\\%ls",cwd,probeName);fileOf(probePath,5000,2);
 return 1;
}
static void mftChecks(void){
 HWND home=homeView();uint32_t ids[4];wchar_t path[1200],t[1200];
 for(int i=0;i<100&&!(home&&GetDlgItem(home,ID_FIND_LIST));i++){Sleep(50);home=homeView();}
 if(!home){check(0,"Home view exists");return;}
 DWORD t0=GetTickCount();
 search(home,probeName);
 for(int i=0;i<2000&&fi_state!=2;i++)Sleep(50);
 printf("drive listed in %lu ms: %llu entries, mode %llu\n",(unsigned long)(GetTickCount()-t0),(unsigned long long)fi_vis,(unsigned long long)fi_mode);
 check(fi_state==2,"the drive is listed");
 if(usnMode)check(fi_mode==4,"it was enumerated from the file table of the volume");
 else check(fi_mode==1,"it was parsed from the file table of the volume");
 check(fi_vis>10,"the index holds the names of the drive");      /* a build machine's work drive can be nearly empty */
 t0=GetTickCount();int n=find_query(probeName,ids,4);DWORD dt=GetTickCount()-t0;
 printf("one search over the index: %lu ms\n",(unsigned long)dt);
 check(n==1,"the file created before the search is found, once");
 check(n==1&&find_path(ids[0],path,1200)>0&&!_wcsicmp(path,probePath),"its path is rebuilt from the folder records");
 check(search(home,probeName)==1&&rowIs(home,0,probePath),"the Home list shows it");
 if(n==1&&!usnMode){
  WIN32_FILE_ATTRIBUTE_DATA a;GetFileAttributesExW(probePath,GetFileExInfoStandard,&a);
  uint64_t real=((uint64_t)a.ftLastWriteTime.dwHighDateTime<<32)|a.ftLastWriteTime.dwLowDateTime;
  check(find_time(ids[0])==real,"the index has its modification time");
  check(find_size(ids[0])==5000,"and its size");
  cell(home,0,3,t,1200);check(!wcscmp(t,L"5KB"),"the size column shows it");
  /* everything on the drive, sorted: how long the worst case takes */
  t0=GetTickCount();int all=filtered2(home,L"",1,L"",L"",0,0);
  printf("every file of the drive (%d rows) found and sorted by date in %lu ms\n",all,(unsigned long)(GetTickCount()-t0));
  check(all>10,"every file of the drive can be listed");
 }
 if(n==1&&usnMode){check(find_time(ids[0])==0,"this way of listing has no times");cell(home,0,1,t,1200);check(!t[0],"so the date column stays empty");}
 DeleteFileW(probePath);
}
/* ---- folder compare */
#define FID_LIST 150
#define FID_CHK_SIZE 111
#define FID_CHK_MTIME 112
#define FID_CHK_CONT 113
#define FID_CHK_CRC 115
#define FID_CHK_ATTR 116
#define FID_SAME 121
#define FID_DIFF 122
#define FID_LONLY 123
#define FID_RONLY 124
#define FID_STOP 106
#define FID_BROWSE_A 103
#define FID_BROWSE_B 104
#define FID_EDIT_A 101
#define FID_OPTS 178
#define FID_CRIT 180
#define FID_FILTER 182
#define FID_EXCL 131
#define FID_MASK 176
static HWND fv;                                    /* the folder view */
static int frows(void){return (int)SendMessageW(GetDlgItem(fv,FID_LIST),LVM_GETITEMCOUNT,0,0);}
static void fcell(int row,int col,wchar_t*out){
 LVITEMW it;memset(&it,0,sizeof it);it.iSubItem=col;it.pszText=out;it.cchTextMax=300;out[0]=0;
 SendMessageW(GetDlgItem(fv,FID_LIST),LVM_GETITEMTEXTW,row,(LPARAM)&it);
}
/* wait for a row count (the compare runs on a worker thread) */
static int waitRows(int want){for(int i=0;i<120;i++){if(frows()==want)return 1;Sleep(50);}return 0;}
static int checked(int id){return (int)SendMessageW(GetDlgItem(fv,id),BM_GETCHECK,0,0)==BST_CHECKED;}
static void setChecked(int id,int on){SendMessageW(GetDlgItem(fv,id),BM_SETCHECK,on?BST_CHECKED:BST_UNCHECKED,0);SendMessageW(fv,WM_COMMAND,MAKEWPARAM(id,BN_CLICKED),(LPARAM)GetDlgItem(fv,id));}
static void toggle(int id){SendMessageW(GetDlgItem(fv,id),BM_CLICK,0,0);}
static int crit(void){return (int)SendMessageW(GetDlgItem(fv,FID_CRIT),CB_GETCURSEL,0,0);}
static void pickCrit(int n){SendMessageW(GetDlgItem(fv,FID_CRIT),CB_SETCURSEL,n,0);SendMessageW(fv,WM_COMMAND,MAKEWPARAM(FID_CRIT,CBN_SELCHANGE),(LPARAM)GetDlgItem(fv,FID_CRIT));}
static void nameFilter(const wchar_t*t){SetWindowTextW(GetDlgItem(fv,FID_FILTER),t);}
static int rowNamed(const wchar_t*n){wchar_t t[400];for(int i=0;i<frows();i++){for(int c=0;c<8;c+=7){fcell(i,c,t);wchar_t*p=t;while(*p==L' ')p++;if(*p==L'\x25BC'||*p==L'\x25B6')p+=2;if(!wcscmp(p,n))return i;}}return -1;}
/* what the middle column shows: -> <- not-equal equal skip */
static const wchar_t*statusOf(const wchar_t*n){static wchar_t t[400];int r=rowNamed(n);if(r<0)return L"(none)";fcell(r,3,t);return t;}
#define G_R L"\x2192"
#define G_L L"\x2190"
#define G_NE L"\x2260"
#define G_EQ L"="
#define G_SKIP L"\x2013"
static void hdrText(int col,wchar_t*out){HWND h=(HWND)SendMessageW(GetDlgItem(fv,FID_LIST),LVM_GETHEADER,0,0);HDITEMW hd;memset(&hd,0,sizeof hd);hd.mask=HDI_TEXT;hd.pszText=out;hd.cchTextMax=100;out[0]=0;SendMessageW(h,HDM_GETITEMW,col,(LPARAM)&hd);}
/* a mouse click in a cell of the list, as the mouse does it */
static void clickCell(int row,int col){
 HWND l=GetDlgItem(fv,FID_LIST);RECT r={col==0?LVIR_LABEL:LVIR_BOUNDS,col,0,0};SendMessageW(l,LVM_GETSUBITEMRECT,row,(LPARAM)&r);   /* the bounds of column 0 are the whole row */
 LPARAM p=MAKELPARAM((r.left+r.right)/2,(r.top+r.bottom)/2);PostMessageW(l,WM_LBUTTONDOWN,MK_LBUTTON,p);PostMessageW(l,WM_LBUTTONUP,0,p);Sleep(150);   /* posted: the list tracks the mouse in a loop of its own until the button is up */
}
static void selectF(int row){LVITEMW it;memset(&it,0,sizeof it);it.state=it.stateMask=LVIS_SELECTED|LVIS_FOCUSED;SendMessageW(GetDlgItem(fv,FID_LIST),LVM_SETITEMSTATE,row,(LPARAM)&it);}
static HWND syncDialog(void){return FindWindowW(L"BCSyncPreview",NULL);}
static void fixedTime(const wchar_t*path,int hours){
 HANDLE h=CreateFileW(path,FILE_WRITE_ATTRIBUTES,7,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);if(h==INVALID_HANDLE_VALUE)return;
 ULARGE_INTEGER u;u.QuadPart=132000000000000000ULL+(ULONGLONG)hours*36000000000ULL;FILETIME ft={u.LowPart,u.HighPart};SetFileTime(h,NULL,NULL,&ft);CloseHandle(h);
}
/* left: same.txt diff.txt only_left.cs sub\in.md (Korean name too)   right: same.txt diff.txt only_right.json sub\in.md
   times are equal except for diff.txt; sizes differ for diff.txt; in.md has equal sizes and times but other content */
static void folderFixture(void){
 CreateDirectoryW(L"fc",NULL);CreateDirectoryW(L"fc\\a",NULL);CreateDirectoryW(L"fc\\b",NULL);CreateDirectoryW(L"fc\\a\\sub",NULL);CreateDirectoryW(L"fc\\b\\sub",NULL);
 putfile(L"fc\\a\\same.txt","one\r\ntwo\r\n");putfile(L"fc\\b\\same.txt","one\r\ntwo\r\n");
 putfile(L"fc\\a\\diff.txt","left\r\n");putfile(L"fc\\b\\diff.txt","right!\r\n");
 putfile(L"fc\\a\\only_left.cs","x");putfile(L"fc\\b\\only_right.json","y");
 putfile(L"fc\\a\\sub\\in.md","q");putfile(L"fc\\b\\sub\\in.md","w");
 putfile(L"fc\\a\\\xBCF4\xACE0\xC11C.txt","k");
 const wchar_t*same[]={L"same.txt",L"only_left.cs",L"sub\\in.md",L"\xBCF4\xACE0\xC11C.txt",L"only_right.json"};
 for(int i=0;i<5;i++){wchar_t a[300],b[300];swprintf(a,300,L"fc\\a\\%ls",same[i]);swprintf(b,300,L"fc\\b\\%ls",same[i]);fixedTime(a,10);fixedTime(b,10);}
 fixedTime(L"fc\\a\\diff.txt",5);fixedTime(L"fc\\b\\diff.txt",20);fixedTime(L"fc\\a\\sub",10);fixedTime(L"fc\\b\\sub",10);
}
static void folderChecks(void){
 wchar_t t[400],u[400];
 for(int i=0;i<100&&!fv;i++){fv=FindWindowExW(frame,NULL,L"BCFolder",NULL);if(!fv)Sleep(50);}
 check(fv!=NULL,"folder compare session is open");
 if(!fv)return;
 check(waitRows(7),"the compare ran by itself: seven rows");
 /* the icons: both sides draw one in front of the name (left: the list does, right: drawn over its own column) */
 {HWND l=GetDlgItem(fv,FID_LIST);RECT rr;GetClientRect(l,&rr);HDC dc=GetDC(NULL),mdc=CreateCompatibleDC(dc);HBITMAP bm=CreateCompatibleBitmap(dc,rr.right,rr.bottom);SelectObject(mdc,bm);
  HBRUSH wb=CreateSolidBrush(RGB(255,0,255));FillRect(mdc,&rr,wb);DeleteObject(wb);       /* a colour no row uses */
  SendMessageW(l,WM_PRINTCLIENT,(WPARAM)mdc,PRF_CLIENT|PRF_ERASEBKGND);
  RECT c0={LVIR_BOUNDS,0,0,0},c6={LVIR_BOUNDS,6,0,0},c3={LVIR_BOUNDS,3,0,0};
  SendMessageW(l,LVM_GETSUBITEMRECT,0,(LPARAM)&c0);SendMessageW(l,LVM_GETSUBITEMRECT,0,(LPARAM)&c6);
  int own6=0,own0=0;
  for(int y=c0.top;y<c0.bottom;y++){for(int x=c0.left;x<c0.left+22;x++){COLORREF px=GetPixel(mdc,x,y);int dark=(GetRValue(px)+GetGValue(px)+GetBValue(px))<600;if(dark)own0++;}}
  for(int y=c6.top;y<c6.bottom;y++){for(int x=c6.left;x<c6.right;x++){COLORREF px=GetPixel(mdc,x,y);int dark=(GetRValue(px)+GetGValue(px)+GetBValue(px))<600;if(dark)own6++;}}
  printf("icon pixels: left %d right %d (cells %ld,%ld-%ld,%ld and %ld,%ld-%ld,%ld)\n",own0,own6,c0.left,c0.top,c0.right,c0.bottom,c6.left,c6.top,c6.right,c6.bottom);
  check(own0>20,"the left name has an icon in front of it");
  check(own6>20,"the right name has one too");
  DeleteObject(bm);DeleteDC(mdc);ReleaseDC(NULL,dc);(void)c3;}
 /* the mirrored list */
 {wchar_t h[100],n[400];int ok=1;
  static const wchar_t*want[8]={L"\xC774\xB984",L"\xD06C\xAE30",L"\xB0A0\xC9DC",L"*",L"\xB0A0\xC9DC",L"\xD06C\xAE30",L"",L"\xC774\xB984"};
  for(int c=0;c<8;c++){hdrText(c,h);if(wcscmp(h,want[c])){ok=0;printf("column %d is called another name\n",c);}}
  check(ok,"columns: name, size, date | action | date, size, icon, name");
  int r=rowNamed(L"only_left.cs");
  fcell(r,0,n);check(wcsstr(n,L"only_left.cs")!=NULL,"only on the left: the left name is there");
  fcell(r,7,n);check(!n[0],"and the right name is empty");fcell(r,4,n);check(!n[0],"so is the right date");fcell(r,5,n);check(!n[0],"and the right size");
  fcell(r,1,n);check(!wcscmp(n,L"1"),"the left size is there");
  r=rowNamed(L"only_right.json");fcell(r,0,n);check(!n[0],"only on the right: the left name is empty");fcell(r,7,n);check(wcsstr(n,L"only_right.json")!=NULL,"the right name is there");
  fcell(r,2,n);check(!n[0],"no left date");
  check(!wcscmp(statusOf(L"only_left.cs"),G_R)&&!wcscmp(statusOf(L"only_right.json"),G_L),"a new item goes over to the other side: -> and <-");
  check(!wcscmp(statusOf(L"same.txt"),G_EQ)&&!wcscmp(statusOf(L"diff.txt"),G_L),"equal: =, different: the newer one is copied");
  /* a click on the middle cell turns through the actions; a click anywhere else does not */
  r=rowNamed(L"only_left.cs");
  clickCell(r,0);check(!wcscmp(statusOf(L"only_left.cs"),G_R),"a click on the name changes nothing");
  clickCell(r,3);check(!wcscmp(statusOf(L"only_left.cs"),G_SKIP),"a click on the middle cell: skip");
  clickCell(r,3);check(!wcscmp(statusOf(L"only_left.cs"),G_R),"again: copy");
  r=rowNamed(L"diff.txt");
  clickCell(r,3);check(!wcscmp(statusOf(L"diff.txt"),G_SKIP),"two different files: <- becomes skip");
  clickCell(r,3);check(!wcscmp(statusOf(L"diff.txt"),G_R),"then ->");
  clickCell(r,3);check(!wcscmp(statusOf(L"diff.txt"),G_L),"then <- again");
  r=rowNamed(L"same.txt");clickCell(r,3);check(!wcscmp(statusOf(L"same.txt"),G_EQ),"equal files have no action to change");
  r=rowNamed(L"only_right.json");selectF(r);
  SendMessageW(GetDlgItem(fv,FID_LIST),WM_KEYDOWN,VK_SPACE,0);check(!wcscmp(statusOf(L"only_right.json"),G_SKIP),"Space: the next action of the selected item");
  SendMessageW(GetDlgItem(fv,FID_LIST),WM_KEYDOWN,VK_SPACE,0);check(!wcscmp(statusOf(L"only_right.json"),G_L),"and again");
  /* the hint under the list */
  {RECT rl,rh;GetWindowRect(GetDlgItem(fv,FID_LIST),&rl);GetWindowRect(GetDlgItem(fv,183),&rh);
   check(rh.top>=rl.bottom&&rh.bottom<=rl.bottom+60,"a hint line under the list");}}
 /* toolbar */
 {RECT rp,rc,rs,r1,r2,r3,r4,rcb,rg,rf,rl,rv;HWND p=GetDlgItem(fv,FID_EDIT_A);
  GetWindowRect(p,&rp);GetWindowRect(GetDlgItem(fv,105),&rc);GetWindowRect(GetDlgItem(fv,FID_STOP),&rs);
  GetWindowRect(GetDlgItem(fv,FID_LONLY),&r1);GetWindowRect(GetDlgItem(fv,FID_DIFF),&r2);GetWindowRect(GetDlgItem(fv,FID_SAME),&r3);GetWindowRect(GetDlgItem(fv,FID_RONLY),&r4);
  GetWindowRect(GetDlgItem(fv,FID_CRIT),&rcb);GetWindowRect(GetDlgItem(fv,FID_OPTS),&rg);GetWindowRect(GetDlgItem(fv,FID_FILTER),&rf);GetWindowRect(GetDlgItem(fv,FID_LIST),&rl);GetWindowRect(fv,&rv);
  check(rc.top>=rp.bottom&&rc.left<rs.left,"the toolbar is below the path row: compare, then stop");
  check(rs.right<=r1.left&&r1.right<=r2.left&&r2.right<=r3.left&&r3.right<=r4.left,"then the four status toggles: left only, different, same, right only");
  check(r4.right<=rcb.left&&rcb.right<=rg.left&&rg.right<=rf.left&&rf.right<=rv.right,"then the compare-by box, the gear and the name filter, inside the window");
  check(rc.top==rs.top&&rs.top==r1.top&&r1.top==rcb.top&&rcb.top==rf.top||(rf.top>rc.top),"they share a row (the filter wraps only in a narrow window)");
  GetWindowRect(GetDlgItem(fv,141),&rs);check(rs.top>rc.bottom&&rl.top>rs.bottom,"the copy, move, delete and sync buttons are on the next row, above the list");
  GetWindowTextW(GetDlgItem(fv,FID_BROWSE_A),t,10);GetWindowTextW(GetDlgItem(fv,FID_BROWSE_B),u,10);
  check(!wcscmp(t,L"\xE838")&&!wcscmp(u,L"\xE838"),"the browse buttons are folder icons");
  GetWindowTextW(GetDlgItem(fv,FID_SAME),t,10);GetWindowTextW(GetDlgItem(fv,FID_DIFF),u,10);
  check(!wcscmp(t,L"=")&&!wcscmp(u,L"\x2260"),"the status toggles are symbols");}
 /* the options rows */
 {RECT l0,l1;HWND opt=GetDlgItem(fv,FID_OPTS),ex=GetDlgItem(fv,FID_EXCL);
  check(!shown(ex)&&!shown(GetDlgItem(fv,FID_CHK_SIZE))&&!shown(GetDlgItem(fv,FID_MASK)),"the options rows are closed to begin with");
  GetWindowRect(GetDlgItem(fv,FID_LIST),&l0);
  toggle(FID_OPTS);
  check(shown(ex)&&shown(GetDlgItem(fv,FID_CHK_SIZE))&&shown(GetDlgItem(fv,FID_MASK)),"the gear opens them");
  GetWindowRect(GetDlgItem(fv,FID_LIST),&l1);check(l1.top>l0.top+40,"the list moves down to make room");
  {RECT re,rm,rl2;GetWindowRect(ex,&re);GetWindowRect(GetDlgItem(fv,FID_MASK),&rm);GetWindowRect(GetDlgItem(fv,FID_LIST),&rl2);
   check(re.bottom<=rm.top&&rm.bottom<=rl2.top,"two rows, both above the list");}
  toggle(FID_OPTS);
  check(!shown(ex)&&!shown(GetDlgItem(fv,FID_MASK)),"the gear closes them again");
  GetWindowRect(GetDlgItem(fv,FID_LIST),&l1);check(l1.top==l0.top,"and the list goes back up");
  (void)opt;}
 /* compare by */
 check(crit()==0&&checked(FID_CHK_SIZE)&&checked(FID_CHK_MTIME)&&!checked(FID_CHK_CONT)&&!checked(FID_CHK_CRC),"compare by starts as size + time");
 check(!wcscmp(statusOf(L"in.md"),G_EQ)&&!wcscmp(statusOf(L"diff.txt"),G_L),"in.md is the same, diff.txt differs (the right one is newer: copy to the left)");
 pickCrit(3);Sleep(100);waitRows(7);Sleep(400);
 check(crit()==3&&checked(FID_CHK_SIZE)&&!checked(FID_CHK_MTIME)&&checked(FID_CHK_CONT),"choosing 'content' sets the check boxes");
 for(int i=0;i<60&&wcscmp(statusOf(L"in.md"),G_NE);i++)Sleep(50);
 check(!wcscmp(statusOf(L"in.md"),G_NE),"and compares again: in.md differs by its content (equal times: no direction)");
 pickCrit(4);for(int i=0;i<60&&!checked(FID_CHK_CRC);i++)Sleep(50);Sleep(400);
 check(crit()==4&&checked(FID_CHK_CRC)&&!checked(FID_CHK_CONT),"CRC32");
 pickCrit(1);Sleep(500);
 check(crit()==1&&checked(FID_CHK_SIZE)&&!checked(FID_CHK_MTIME)&&!wcscmp(statusOf(L"in.md"),G_EQ),"size only: in.md is the same again");
 pickCrit(2);Sleep(500);
 check(crit()==2&&!checked(FID_CHK_SIZE)&&checked(FID_CHK_MTIME),"time only");
 pickCrit(0);Sleep(500);
 /* the check boxes of the options rows move the box */
 setChecked(FID_CHK_ATTR,1);check(crit()==5,"size + time + attributes is a preset");
 setChecked(FID_CHK_ATTR,0);check(crit()==0,"and back");
 setChecked(FID_CHK_CRC,1);check(crit()==6,"a mix that is no preset reads 'custom'");
 setChecked(FID_CHK_CRC,0);Sleep(500);check(crit()==0,"size + time again");
 pickCrit(6);check(crit()==6||crit()==0,"'custom' cannot be chosen: nothing changes");
 for(int i=0;i<60&&!waitRows(7);i++);
 /* status filters */
 check(frows()==7,"seven rows");
 toggle(FID_SAME);for(int i=0;i<60&&frows()!=4;i++)Sleep(50);
 check(frows()==4&&rowNamed(L"same.txt")<0&&rowNamed(L"sub")<0,"same off: the identical files and the folder of them go");
 toggle(FID_SAME);for(int i=0;i<60&&frows()!=7;i++)Sleep(50);
 check(frows()==7,"same on");
 toggle(FID_LONLY);check(rowNamed(L"only_left.cs")<0&&rowNamed(L"only_right.json")>=0,"left only off");
 toggle(FID_LONLY);
 /* the name filter */
 nameFilter(L"only");check(frows()==2&&rowNamed(L"only_left.cs")>=0&&rowNamed(L"only_right.json")>=0,"filter 'only': the two files");
 nameFilter(L"ONLY_L");check(frows()==1&&rowNamed(L"only_left.cs")==0,"upper case matches too");
 nameFilter(L"IN.M");check(frows()==2&&rowNamed(L"sub")==0&&rowNamed(L"in.md")==1,"a folder stays for the file inside it that matches");
 nameFilter(L"sub");check(frows()==1&&rowNamed(L"sub")==0,"a folder that matches by its own name: shown without its files");
 nameFilter(L"\xACE0\xC11C");check(frows()==1&&rowNamed(L"\xBCF4\xACE0\xC11C.txt")==0,"a Korean name");
 nameFilter(L"zzz");check(frows()==0,"nothing matches");
 nameFilter(L"txt");toggle(FID_SAME);check(frows()==2,"the name filter works together with the status filters");
 toggle(FID_SAME);nameFilter(L"");check(frows()==7,"an empty filter shows everything");
 /* a collapsed folder is listed open while a name filter is set */
 nameFilter(L"");
 /* a sync from the middle column: only what is shown goes over */
 {pickCrit(0);Sleep(500);waitRows(7);
  int r=rowNamed(L"only_left.cs");clickCell(r,3);                      /* skip it */
  check(!wcscmp(statusOf(L"only_left.cs"),G_SKIP),"only_left.cs is set to skip");
  menuAnswer=5;PostMessageW(fv,WM_COMMAND,MAKEWPARAM(144,BN_CLICKED),(LPARAM)GetDlgItem(fv,144));
  HWND dlg=NULL;for(int i=0;i<100&&!dlg;i++){dlg=syncDialog();if(!dlg)Sleep(50);}
  check(dlg!=NULL,"the plan is shown first");
  check(GetFileAttributesW(L"fc\\b\\\xBCF4\xACE0\xC11C.txt")==INVALID_FILE_ATTRIBUTES,"nothing has been copied yet");
  if(dlg)PostMessageW(dlg,WM_COMMAND,1,0);                              /* its OK button */
  for(int i=0;i<100&&GetFileAttributesW(L"fc\\b\\\xBCF4\xACE0\xC11C.txt")==INVALID_FILE_ATTRIBUTES;i++)Sleep(50);
  Sleep(300);
  check(GetFileAttributesW(L"fc\\b\\\xBCF4\xACE0\xC11C.txt")!=INVALID_FILE_ATTRIBUTES,"a new item on the left was copied to the right");
  check(GetFileAttributesW(L"fc\\a\\only_right.json")!=INVALID_FILE_ATTRIBUTES,"a new item on the right was copied to the left");
  check(GetFileAttributesW(L"fc\\b\\only_left.cs")==INVALID_FILE_ATTRIBUTES,"the item set to skip was not copied");
  check(holds(L"fc\\a\\diff.txt","right!\r\n"),"of two different files the newer one went over");
  /* Ctrl+Z takes the sync back (the test answers its question with Yes), Ctrl+Y does it again */
  expectBoxes=1;shownBoxes=0;SendMessageW(fv,WM_APP+36,0,0);Sleep(300);
  check(shownBoxes==1,"undo asks before it changes files");
  check(GetFileAttributesW(L"fc\\b\\\xBCF4\xACE0\xC11C.txt")==INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(L"fc\\a\\only_right.json")==INVALID_FILE_ATTRIBUTES,"undo: the files the sync copied are gone again");
  check(holds(L"fc\\a\\diff.txt","left\r\n"),"undo: the overwritten file has its old content back");
  for(int i=0;i<60&&!holds(L"fc\\a\\diff.txt","right!\r\n");i++){SendMessageW(fv,WM_APP+37,0,0);Sleep(100);}   /* not while the list is read again */
  check(holds(L"fc\\a\\diff.txt","right!\r\n")&&GetFileAttributesW(L"fc\\b\\\xBCF4\xACE0\xC11C.txt")!=INVALID_FILE_ATTRIBUTES&&GetFileAttributesW(L"fc\\a\\only_right.json")!=INVALID_FILE_ATTRIBUTES,"redo: the sync is done again");
  expectBoxes=0;
  menuAnswer=0;}
}
/* ---- folder compare: the preview under the list */
#define PID_PREV 190
#define PID_NEXT 191
#define PID_SRC 192
#define PID_PF 193
#define PID_NF 194
#define PID_CLR 195
#define PID_CRL 196
#define PID_SAVE 197
#define PID_FOLD 198
#define PID_LBL 199
#define PID_SIDE 200
#define PID_UNI 201
#define PID_SET 202
#define PID_TOG 203
#define PID_MSG 184
#define PID_HINT 183
#define BC_TVQ_ (0x8000+60)
static HWND pvView(void){return FindWindowExW(fv,NULL,L"BCText",NULL);}
static HWND pvPane(int id){return GetDlgItem(pvView(),id);}
static void paneText(int id,wchar_t*out,int cap){out[0]=0;SendMessageW(pvPane(id),WM_GETTEXT,cap,(LPARAM)out);}
static int paneIs(int id,const wchar_t*want){wchar_t t[400];paneText(id,t,400);wchar_t o[400];int n=0;for(wchar_t*p=t;*p;p++){if(*p==L'\r'){o[n++]=L'|';if(p[1]==L'\n')p++;}else if(*p==L'\n')o[n++]=L'|';else o[n++]=*p;}o[n]=0;return !wcscmp(o,want);}
static int waitPane(int id,const wchar_t*want){for(int i=0;i<60;i++){if(paneIs(id,want))return 1;Sleep(50);}return 0;}
static int blocks(void){return (int)SendMessageW(pvView(),BC_TVQ_,0,0);}
static int curBlock(void){return (int)SendMessageW(pvView(),BC_TVQ_,2,0);}
static int pvShown(void){HWND v=pvView();return v&&(GetWindowLongPtrW(v,GWL_STYLE)&WS_VISIBLE);}
static void pvClick(int id){SendMessageW(GetDlgItem(fv,id),BM_CLICK,0,0);}
static void typeInto(int id,const wchar_t*t){HWND h=pvPane(id);SendMessageW(h,EM_SETSEL,0,0);SendMessageW(h,EM_REPLACESEL,TRUE,(LPARAM)t);}
static int dirtyPv(void){return (int)SendMessageW(fv,0x8000+14,0,0);}    /* BC_ISDIRTY */
static int selectedRow(void){return (int)SendMessageW(GetDlgItem(fv,FID_LIST),LVM_GETNEXTITEM,-1,LVNI_SELECTED);}
static void pvFixture(void){
 CreateDirectoryW(L"fp",NULL);CreateDirectoryW(L"fp\\a",NULL);CreateDirectoryW(L"fp\\b",NULL);
 putfile(L"fp\\a\\a_multi.txt","one\r\ntwo\r\nthree\r\nfour\r\nfive\r\nsix\r\nseven\r\neight\r\nnine\r\n");
 putfile(L"fp\\b\\a_multi.txt","ONE\r\ntwo\r\nthree\r\nFOUR\r\nfive\r\nsix\r\nSEVEN\r\neight\r\nnine\r\n");
 putfile(L"fp\\a\\b_mod.txt","left\r\n");putfile(L"fp\\b\\b_mod.txt","right\r\n");
 putfile(L"fp\\a\\c_other.txt","alpha\r\n");putfile(L"fp\\b\\c_other.txt","omega\r\n");
 putfile(L"fp\\a\\d_same.txt","same\r\n");putfile(L"fp\\b\\d_same.txt","same\r\n");
 putfile(L"fp\\a\\e.bin","\x01\x02\x03");putfile(L"fp\\b\\e.bin","\x01\x02\x04!");
 putfile(L"fp\\a\\f_left.txt","only left\r\n");
 const wchar_t*f[]={L"a_multi.txt",L"b_mod.txt",L"c_other.txt",L"d_same.txt",L"e.bin"};
 for(int i=0;i<5;i++){wchar_t a[300],b[300];swprintf(a,300,L"fp\\a\\%ls",f[i]);swprintf(b,300,L"fp\\b\\%ls",f[i]);fixedTime(a,10);fixedTime(b,10);}
 fixedTime(L"fp\\b\\a_multi.txt",12);fixedTime(L"fp\\b\\b_mod.txt",12);fixedTime(L"fp\\b\\c_other.txt",12);fixedTime(L"fp\\b\\e.bin",12);
}
static void pvChecks(void){
 wchar_t t[400];
 for(int i=0;i<100&&!fv;i++){fv=FindWindowExW(frame,NULL,L"BCFolder",NULL);if(!fv)Sleep(50);}
 check(fv!=NULL,"folder compare session is open");
 if(!fv)return;
 check(waitRows(6),"six rows: four differing files, one equal, one only on the left");
 /* nothing chosen yet */
 check(pvView()!=NULL&&!pvShown(),"the preview view exists and shows nothing yet");
 check(GetWindowLongPtrW(GetDlgItem(fv,PID_MSG),GWL_STYLE)&WS_VISIBLE,"a message says what the preview needs");
 {RECT rl,rh,rt,rv,rb;GetWindowRect(GetDlgItem(fv,FID_LIST),&rl);GetWindowRect(GetDlgItem(fv,PID_HINT),&rh);GetWindowRect(GetDlgItem(fv,PID_PREV),&rt);GetWindowRect(GetDlgItem(fv,PID_MSG),&rv);GetWindowRect(fv,&rb);
  check(rl.bottom<=rh.top+1&&rh.bottom<=rt.top&&rt.bottom<=rv.top&&rv.bottom<=rb.bottom,"list, hint line, preview toolbar and preview view are stacked in this order");
  check(rt.bottom-rt.top>=16&&rv.bottom-rv.top>=60,"the preview has room");}
 /* a differing text file: the pair is shown */
 int r=rowNamed(L"a_multi.txt");selectF(r);
 Sleep(1500);{wchar_t d[400];paneText(201,d,400);printf("DBG sel=%d shown=%d tog=%d len=%d\n",selectedRow(),pvShown(),(int)SendMessageW(GetDlgItem(fv,203),BM_GETCHECK,0,0),(int)wcslen(d));}
 check(waitPane(201,L"one|two|three|four|five|six|seven|eight|nine"),"selecting a file shows its left side in the preview");
 check(paneIs(202,L"ONE|two|three|FOUR|five|six|SEVEN|eight|nine"),"and its right side");
 check(pvShown()&&!(GetWindowLongPtrW(GetDlgItem(fv,PID_MSG),GWL_STYLE)&WS_VISIBLE),"the view takes the place of the message");
 for(int i=0;i<30&&blocks()!=3;i++)Sleep(50);
 check(blocks()==3,"it found three differences");
 GetWindowTextW(GetDlgItem(fv,PID_LBL),t,400);check(wcsstr(t,L"3")!=NULL,"the label over the view says so");
 /* going through the differences */
 check(curBlock()==0,"the first difference is current");
 pvClick(PID_NEXT);check(curBlock()==1,"next difference");
 pvClick(PID_NEXT);check(curBlock()==2,"next difference again");
 pvClick(PID_PREV);check(curBlock()==1,"previous difference");
 /* the buttons that change the view */
 pvClick(PID_FOLD);check((int)SendMessageW(pvPane(220+0)?GetDlgItem(pvView(),217):NULL,BM_GETCHECK,0,0)==BST_CHECKED,"fold unchanged fragments is on");
 pvClick(PID_FOLD);check((int)SendMessageW(GetDlgItem(pvView(),217),BM_GETCHECK,0,0)!=BST_CHECKED,"and off again");
 pvClick(PID_UNI);check((int)SendMessageW(GetDlgItem(pvView(),218),BM_GETCHECK,0,0)==BST_CHECKED&&!(GetWindowLongPtrW(pvPane(202),GWL_STYLE)&WS_VISIBLE),"unified view: one pane");
 check((int)SendMessageW(GetDlgItem(fv,PID_UNI),BM_GETCHECK,0,0)==BST_CHECKED&&(int)SendMessageW(GetDlgItem(fv,PID_SIDE),BM_GETCHECK,0,0)!=BST_CHECKED,"the two mode buttons show which one is on");
 pvClick(PID_SIDE);check((int)SendMessageW(GetDlgItem(pvView(),218),BM_GETCHECK,0,0)!=BST_CHECKED&&(GetWindowLongPtrW(pvPane(202),GWL_STYLE)&WS_VISIBLE),"side by side: two panes again");
 /* the file that was in the preview opens in the editor */
 {uint64_t before=g_nsess;pvClick(PID_SRC);for(int i=0;i<40&&g_nsess==before;i++)Sleep(50);check(g_nsess==before+1,"the source button opens the file in the text editor");}
 /* the next and the previous file */
 pvClick(PID_NF);for(int i=0;i<40&&!waitPane(201,L"left");i++);
 check(waitPane(201,L"left")&&paneIs(202,L"right"),"next file: b_mod.txt");
 check(selectedRow()==rowNamed(L"b_mod.txt"),"and the list follows");
 pvClick(PID_NF);check(waitPane(201,L"alpha"),"next file: c_other.txt");
 pvClick(PID_PF);check(waitPane(201,L"left"),"previous file: b_mod.txt");
 pvClick(PID_NF);pvClick(PID_NF);Sleep(300);
 check(selectedRow()==rowNamed(L"e.bin")&&!pvShown(),"a file that is no text is selected but not previewed");
 pvClick(PID_NF);check(selectedRow()==rowNamed(L"e.bin"),"no further file: the selection stays");
 /* merging and saving */
 selectF(rowNamed(L"b_mod.txt"));check(waitPane(202,L"right"),"b_mod.txt again");
 check(!dirtyPv(),"nothing changed yet");
 pvClick(PID_CLR);check(waitPane(202,L"left")&&dirtyPv(),"copy the difference from the left to the right: the right pane changes and counts as changed");
 pvClick(PID_SAVE);Sleep(300);check(!dirtyPv()&&holds(L"fp\\b\\b_mod.txt","left\r\n"),"save writes the right file");
 check(!wcscmp(statusOf(L"b_mod.txt"),G_EQ),"saving the merged pair in the preview immediately updates its folder list row to equal");
 /* changes that are not saved stop a switch to another file */
 selectF(rowNamed(L"c_other.txt"));check(waitPane(201,L"alpha"),"c_other.txt");
 typeInto(201,L"edited ");check(dirtyPv(),"an edit makes the view dirty");
 expectBoxes=1;shownBoxes=0;selectF(rowNamed(L"a_multi.txt"));
 for(int i=0;i<40&&!shownBoxes;i++)Sleep(50);
 Sleep(200);expectBoxes=0;
 check(shownBoxes==1,"choosing another file asks about the unsaved changes");
 check(selectedRow()==rowNamed(L"c_other.txt")&&paneIs(201,L"edited alpha"),"cancelled: the selection and the view stay");
 expectBoxes=1;shownBoxes=0;SendMessageW(fv,WM_COMMAND,MAKEWPARAM(105,BN_CLICKED),(LPARAM)GetDlgItem(fv,105));   /* a new compare asks as well */
 expectBoxes=0;check(shownBoxes==1&&paneIs(201,L"edited alpha"),"a new compare asks about the unsaved changes too");
 /* the settings popup */
 {
#define OPT(id) ((int)SendMessageW(GetDlgItem(pvView(),id),BM_GETCHECK,0,0)==BST_CHECKED)
  menuAnswer=12;pvClick(PID_SET);check(menuShown>=10&&OPT(212)&&!OPT(213),"settings: ignore whitespace");
  menuAnswer=11;pvClick(PID_SET);check(!OPT(212)&&OPT(213),"settings: trim whitespace");
  menuAnswer=10;pvClick(PID_SET);check(!OPT(212)&&!OPT(213),"settings: ignore nothing");
  menuAnswer=13;pvClick(PID_SET);check(OPT(211),"settings: ignore case on");
  menuAnswer=13;pvClick(PID_SET);check(!OPT(211),"settings: ignore case off");
  menuAnswer=21;pvClick(PID_SET);check(!OPT(219),"settings: highlight lines only");
  menuAnswer=20;pvClick(PID_SET);check(OPT(219),"settings: highlight words");
  menuAnswer=31;pvClick(PID_SET);check(OPT(221),"settings: show blanks");
  menuAnswer=31;pvClick(PID_SET);check(!OPT(221),"settings: hide blanks");
  menuAnswer=30;pvClick(PID_SET);check(!OPT(215),"settings: syntax colours off");
  menuAnswer=30;pvClick(PID_SET);check(OPT(215),"settings: syntax colours on");
  {int sc=OPT(220);menuAnswer=1;pvClick(PID_SET);check(OPT(220)!=sc,"settings: scroll together toggles");menuAnswer=1;pvClick(PID_SET);}
  {uint64_t before=g_nsess;menuAnswer=2;pvClick(PID_SET);for(int i=0;i<40&&g_nsess==before;i++)Sleep(50);check(g_nsess==before+1,"settings: show the pair in a window of its own");}
  menuAnswer=0;
 }
 /* the strip between list and preview */
 {RECT rl0,rl1,rh,rp;POINT p;GetWindowRect(GetDlgItem(fv,FID_LIST),&rl0);GetWindowRect(GetDlgItem(fv,PID_HINT),&rh);GetWindowRect(GetDlgItem(fv,PID_PREV),&rp);
  p.x=200;p.y=(rh.bottom+rp.top)/2;ScreenToClient(fv,&p);
  PostMessageW(fv,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(p.x,p.y));PostMessageW(fv,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(p.x,p.y-120));PostMessageW(fv,WM_LBUTTONUP,0,MAKELPARAM(p.x,p.y-120));
  Sleep(400);GetWindowRect(GetDlgItem(fv,FID_LIST),&rl1);
  check(rl1.bottom<rl0.bottom-60,"dragging the strip up makes the list shorter");}
 /* hiding the preview */
 {RECT rl0,rl1;GetWindowRect(GetDlgItem(fv,FID_LIST),&rl0);pvClick(PID_TOG);Sleep(200);GetWindowRect(GetDlgItem(fv,FID_LIST),&rl1);
  check(rl1.bottom>rl0.bottom+60&&!pvShown()&&!(GetWindowLongPtrW(GetDlgItem(fv,PID_PREV),GWL_STYLE)&WS_VISIBLE),"the preview button hides it and gives the list the room");
  pvClick(PID_TOG);Sleep(300);check((GetWindowLongPtrW(GetDlgItem(fv,PID_PREV),GWL_STYLE)&WS_VISIBLE)!=0,"and shows it again");}
 SendMessageW(pvPane(201),EM_SETMODIFY,FALSE,0);SendMessageW(pvPane(202),EM_SETMODIFY,FALSE,0);
}
static DWORD WINAPI watcher(void*arg){
 (void)arg;
 for(int i=0;i<200&&!frame;i++){EnumThreadWindows(mainTid,onTop,0);if(!frame)Sleep(50);}
 check(frame!=NULL,"frame window created");
 if(!frame){PostThreadMessageW(mainTid,WM_QUIT,0,0);return 0;}
 /* the frame exists before its views are built and laid out: wait for the Home view and the tab-row buttons */
 for(int i=0;i<100;i++){
  HWND hv=FindWindowExW(frame,NULL,L"BCHome",NULL),bx=GetDlgItem(frame,IDM_EXIT),bc=GetDlgItem(frame,IDM_CLOSE_TAB);RECT r={0,0,0,0},c={0,0,0,0};
  if(bx)GetWindowRect(bx,&r);
  if(bc)GetWindowRect(bc,&c);
  if(hv&&GetDlgItem(hv,ID_FIND_LIST)&&r.right-r.left>=16&&r.bottom-r.top>=12&&c.right-c.left>=16&&c.bottom-c.top>=12)break;
  Sleep(50);
 }
 if(browseMode){tabRowChecks();tooltipChecks();browseChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(settingsMode){tabRowChecks();preferencesChecks();tooltipChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(findMode){findChecks();tooltipChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(folderMode){folderChecks();tooltipChecks();PostMessageW(frame,WM_CLOSE,0,0);return 0;}
 if(pvMode){pvChecks();tooltipChecks();SendMessageW(fv,0x8000+14,0,0);PostMessageW(frame,WM_CLOSE,0,0);return 0;}
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
 /* Ctrl+D in an editor pane duplicates the line of the caret (rich_keys.inc); one undo takes it back */
 {int n0=(int)SendMessageW(ra,EM_GETLINECOUNT,0,0),m0=(int)SendMessageW(ra,EM_GETMODIFY,0,0);
  ctrlDown=1;PostMessageW(ra,WM_KEYDOWN,'D',0);Sleep(300);ctrlDown=0;
  check((int)SendMessageW(ra,EM_GETLINECOUNT,0,0)==n0+1,"Ctrl+D duplicates the line of the caret");
  SendMessageW(ra,EM_UNDO,0,0);SendMessageW(ra,EM_SETMODIFY,m0,0);
  check((int)SendMessageW(ra,EM_GETLINECOUNT,0,0)==n0,"one undo takes the duplicated line back");
  caretTo(1);}
 /* Enter in an editor pane keeps the indent of the line (rich_keys.inc) */
 {int n0=(int)SendMessageW(ra,EM_GETLINECOUNT,0,0),m0=(int)SendMessageW(ra,EM_GETMODIFY,0,0),l0=GetWindowTextLengthW(ra);
  wchar_t c[4]={0};CHARRANGE r;TEXTRANGEW t;
  SendMessageW(ra,EM_REPLACESEL,1,(LPARAM)L"	zz");
  PostMessageW(ra,WM_KEYDOWN,VK_RETURN,0);Sleep(300);
  SendMessageW(ra,EM_EXGETSEL,0,(LPARAM)&r);t.chrg.cpMin=r.cpMin-2;t.chrg.cpMax=r.cpMin;t.lpstrText=c;
  if(r.cpMin>=2)SendMessageW(ra,EM_GETTEXTRANGE,0,(LPARAM)&t);
  check((int)SendMessageW(ra,EM_GETLINECOUNT,0,0)==n0+1&&c[0]==13&&c[1]==9,"Enter starts the new line with the indent of the line");
  SendMessageW(ra,EM_UNDO,0,0);SendMessageW(ra,EM_UNDO,0,0);SendMessageW(ra,EM_SETMODIFY,m0,0);
  check((int)SendMessageW(ra,EM_GETLINECOUNT,0,0)==n0&&GetWindowTextLengthW(ra)==l0,"two undos take the new line and the typed text back");
  caretTo(1);}
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
 /* regex / pattern substitution rule (\w+) in ID_EDIT_RULES (232) */
 SetWindowTextW(GetDlgItem(view,232),L"count=total;// \\w+=// sum");
 SendMessageW(view,WM_COMMAND,105,0);
 check(info(L"차이 3개"),"pattern substitution rule (\\w+) matches and ignores the comment difference");
 SetWindowTextW(GetDlgItem(view,232),L"");
 SendMessageW(view,WM_COMMAND,105,0);
 check(info(L"차이 4개"),"clearing the substitution rule restores all 4 differences");

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
 settingsMode=argc>1&&!strcmp(argv[1],"settings");
 findMode=argc>1&&!strcmp(argv[1],"find");
 folderMode=argc>1&&!strcmp(argv[1],"folder");
 if(folderMode)folderFixture();
 pvMode=argc>1&&!strcmp(argv[1],"folderpv");
 if(pvMode)pvFixture();
 usnMode=argc>1&&!strcmp(argv[1],"findusn");
 mftMode=usnMode||(argc>1&&!strcmp(argv[1],"findmft"));
 if(findMode)findFixture();
 if(mftMode&&!mftFixture()){printf("SKIP the volume cannot be opened (no administrator rights)\n0 checks, 0 failures\n");return 0;}
 tempFixture();
 check(GetFileAttributesW(ownTemp)!=INVALID_FILE_ATTRIBUTES,"temporary folder of this process exists before the run");
 fixture();
 GetFullPathNameW(L"ui_a.c",400,a,NULL);GetFullPathNameW(L"ui_b.c",400,b,NULL);
 swprintf(cmdline,1200,L"bcgui.exe %ls \"%ls\" \"%ls\"",mergeMode?L"merge":L"text",a,b);
 if(browseMode||findMode||mftMode||settingsMode)wcscpy(cmdline,L"bcgui.exe");
 if(pvMode){wchar_t fa[400],fb[400];GetFullPathNameW(L"fp\\a",400,fa,NULL);GetFullPathNameW(L"fp\\b",400,fb,NULL);swprintf(cmdline,1200,L"bcgui.exe folder \"%ls\" \"%ls\"",fa,fb);}
 if(folderMode){wchar_t fa[400],fb[400];GetFullPathNameW(L"fc\\a",400,fa,NULL);GetFullPathNameW(L"fc\\b",400,fb,NULL);swprintf(cmdline,1200,L"bcgui.exe folder \"%ls\" \"%ls\"",fa,fb);}
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
