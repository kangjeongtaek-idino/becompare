/* In-process tests: no desktop windows or input automation. Real assembly
   handlers run against synthetic RichEdit messages and real event handles. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "test_layout.h"
extern int view_can_close(HWND);
extern LRESULT frame_proc(HWND,UINT,WPARAM,LPARAM);
extern uint64_t g_nsess;extern void* g_sess[];
extern int fs_watch(const void*,const wchar_t*);
static int workerBusy,destroyCalls,timerCalls;
BOOL WINAPI __wrap_DestroyWindow(HWND h){destroyCalls++;return TRUE;}
BOOL WINAPI __wrap_EnableWindow(HWND h,BOOL e){return TRUE;}
UINT_PTR WINAPI __wrap_SetTimer(HWND h,UINT_PTR id,UINT ms,TIMERPROC p){timerCalls++;return id;}
BOOL WINAPI __wrap_KillTimer(HWND h,UINT_PTR id){return TRUE;}
extern int text_compare(HWND,void*,void*);
extern DWORD WINAPI sync_thread(void*);
extern LRESULT tv_proc(HWND,UINT,WPARAM,LPARAM),ed_proc(HWND,UINT,WPARAM,LPARAM),mg_proc(HWND,UINT,WPARAM,LPARAM),hx_proc(HWND,UINT,WPARAM,LPARAM),fold_proc(HWND,UINT,WPARAM,LPARAM),fm_proc(HWND,UINT,WPARAM,LPARAM);
static unsigned char sess[SS_SIZE],tv[TV_SIZE];
static wchar_t pane[2][256];static int dirty[2],answer=IDCANCEL,checks,failed,prompts,closeMode,saveWorks;
static void check(int ok,const char*n){checks++;printf("%s %s\n",ok?"PASS":"FAIL",n);if(!ok)failed++;}
static void q(void*p,size_t o,uint64_t n){memcpy((char*)p+o,&n,8);}static void d(void*p,size_t o,uint32_t n){memcpy((char*)p+o,&n,4);}
static uint64_t get(void*p,size_t o){uint64_t n;memcpy(&n,(char*)p+o,8);return n;}
LONG_PTR WINAPI __wrap_GetWindowLongPtrW(HWND h,int n){return (LONG_PTR)sess;}
HWND WINAPI __wrap_GetDlgItem(HWND h,int id){return (HWND)(intptr_t)id;}
int WINAPI __wrap_GetWindowTextW(HWND h,LPWSTR out,int cap){const wchar_t*s=(intptr_t)h==101?L"left.txt":(intptr_t)h==102?L"right.txt":L"";wcsncpy(out,s,cap);out[cap-1]=0;return wcslen(out);}
BOOL WINAPI __wrap_SetWindowTextW(HWND h,LPCWSTR s){return TRUE;}
BOOL WINAPI __wrap_InvalidateRect(HWND h,const RECT*r,BOOL e){return TRUE;}
int __wrap_msgbox(HWND h,const wchar_t*t,int flags){prompts++;return answer;}
LRESULT WINAPI __wrap_SendMessageW(HWND h,UINT m,WPARAM w,LPARAM l){
 int i=(intptr_t)h==202?1:0;
 if(m==BC_STOPPING&&closeMode==2)return workerBusy;
 if(m==BC_ISDIRTY)return dirty[0]||dirty[1];
 if(m==BC_SAVE){if(closeMode){if(saveWorks)dirty[0]=dirty[1]=0;}else tv_proc(h,m,w,l);return 0;}
 if(m==EM_GETMODIFY)return dirty[i];
 if(m==EM_SETMODIFY){dirty[i]=(int)w;return 0;}
 if(m==WM_GETTEXTLENGTH)return wcslen(pane[i]);
 if(m==0x045e){wcscpy((wchar_t*)l,pane[i]);return wcslen(pane[i]);}
 if(m==0x0461){wcsncpy(pane[i],(wchar_t*)l,255);pane[i][255]=0;dirty[i]=0;return wcslen(pane[i]);}
 if(m==0x0434){memset((void*)l,0,8);return 0;}
 return 0;
}
static void initText(void){memset(sess,0,sizeof sess);memset(tv,0,sizeof tv);q(sess,SS_DATA,(uintptr_t)tv);q(tv,TV_HA,201);q(tv,TV_HB,202);d(tv,TV_LDA,1);d(tv,TV_LDB,1);wcscpy((wchar_t*)(tv+TV_PATHA),L"left.txt");wcscpy((wchar_t*)(tv+TV_PATHB),L"right.txt");q(tv,TV_TXA,(uintptr_t)calloc(16,1));q(tv,TV_TXB,(uintptr_t)calloc(16,1));d(tv,TV_TXA+16,1);d(tv,TV_TXB+16,1);wcscpy(pane[0],L"updated");wcscpy(pane[1],L"updated");}
int main(void){
 closeMode=1;dirty[0]=0;dirty[1]=0;prompts=0;check(view_can_close((HWND)999)&&!prompts,"clean view closes without prompt");
 dirty[0]=1;answer=IDCANCEL;check(!view_can_close((HWND)999)&&dirty[0],"cancel preserves unsaved view");
 answer=IDNO;check(view_can_close((HWND)999)&&dirty[0],"discard allows close");
 answer=IDYES;saveWorks=0;check(!view_can_close((HWND)999)&&dirty[0],"failed save vetoes close");
 saveWorks=1;check(view_can_close((HWND)999)&&!dirty[0],"successful save allows close");closeMode=0;
 initText();dirty[0]=1;dirty[1]=0;check(text_compare((HWND)999,tv,sess)==1&&dirty[0]&&!dirty[1],"recompare preserves per-pane dirty state");
 check(tv_proc((HWND)999,BC_ISDIRTY,0,0)!=0,"text view reports dirty");
 check(text_compare((HWND)999,tv,sess)==1&&dirty[0]&&!dirty[1],"repeated redraw preserves dirty state");
 FILE*f=fopen("left.txt","wb");fwrite("original",1,8,f);fclose(f);
 fs_watch(tv+TV_TXA,L"left.txt"); /* this synthetic view represents a file that was read */
 tv_proc((HWND)999,BC_SAVE,0,0);char buf[64]={0};f=fopen("left.txt","rb");size_t n=fread(buf,1,64,f);fclose(f);if(dirty[0]||n!=7||memcmp(buf,"updated",7))printf("save diagnostic: modified=%d, bytes=%llu, text=%.*s\n",dirty[0],(unsigned long long)n,(int)n,buf);check(!dirty[0]&&n==7&&!memcmp(buf,"updated",7),"Ctrl+S after recompare saves edited content");
 dirty[0]=1;wcscpy(pane[0],L"local edits");f=fopen("left.txt","wb");fwrite("external",1,8,f);fclose(f);
 tv_proc((HWND)999,BC_SAVE,0,0);memset(buf,0,sizeof buf);f=fopen("left.txt","rb");n=fread(buf,1,64,f);fclose(f);
 check(dirty[0]&&n==8&&!memcmp(buf,"external",8),"UI save conflict preserves external content and keeps local edits dirty");
 unsigned char ed[E_SIZE]={0},mg[MG_SIZE]={0},hx[HX_SIZE]={0},fold[F_SIZE]={0};
 dirty[0]=1;q(ed,E_HR,201);q(sess,SS_DATA,(uintptr_t)ed);check(ed_proc((HWND)999,BC_ISDIRTY,0,0)!=0,"editor reports dirty");
 q(mg,MG_HM,201);q(sess,SS_DATA,(uintptr_t)mg);check(mg_proc((HWND)999,BC_ISDIRTY,0,0)!=0,"merge reports dirty");
 q(hx,HX_MODB,1);q(sess,SS_DATA,(uintptr_t)hx);check(hx_proc((HWND)999,BC_ISDIRTY,0,0)!=0,"hex reports right-pane dirty");
 HANDLE scan=CreateEventW(NULL,TRUE,FALSE,NULL),sync=CreateEventW(NULL,TRUE,FALSE,NULL);q(fold,F_THREAD,(uintptr_t)scan);q(fold,F_SYNCTHR,(uintptr_t)sync);q(fold,F_PAUSED,1);q(sess,SS_DATA,(uintptr_t)fold);
 check(fold_proc((HWND)999,BC_STOPPING,0,0)==1&&fold[F_CANCEL]&&!get(fold,F_PAUSED)&&get(fold,F_CLOSING),"close cancels and unpauses before waiting");
 SetEvent(scan);check(fold_proc((HWND)999,BC_STOPPING,0,0)==1,"close waits for sync after scan finishes");SetEvent(sync);check(!fold_proc((HWND)999,BC_STOPPING,0,0),"close allowed only after both workers finish");CloseHandle(scan);CloseHandle(sync);
 memset(fold,0,sizeof fold);uint32_t plan[2]={5,0};unsigned char entry[64]={0};q(entry,0,(uintptr_t)L"must-not-create");q(fold,F_ENTS,(uintptr_t)entry);q(fold,F_PLAN,(uintptr_t)plan);q(fold,F_NPLAN,1);q(fold,F_PAUSED,1);q(fold,F_SYNCRUN,1);wcscpy((wchar_t*)(fold+F_ROOTA),L".");wcscpy((wchar_t*)(fold+F_ROOTB),L".");q(sess,SS_DATA,(uintptr_t)fold);
 HANDLE worker=CreateThread(NULL,0,sync_thread,fold,0,NULL);q(fold,F_SYNCTHR,(uintptr_t)worker);check(worker&&WaitForSingleObject(worker,120)==WAIT_TIMEOUT,"paused worker cooperatively waits");fold_proc((HWND)999,BC_STOPPING,0,0);check(WaitForSingleObject(worker,2000)==WAIT_OBJECT_0&&GetFileAttributesW(L"must-not-create")==INVALID_FILE_ATTRIBUTES,"cancel releases paused worker without executing operation");CloseHandle(worker);
 unsigned char fm[FM_SIZE]={0};HANDLE fmscan=CreateEventW(NULL,TRUE,FALSE,NULL);q(fm,FM_THREAD,(uintptr_t)fmscan);q(sess,SS_DATA,(uintptr_t)fm);
 check(fm_proc((HWND)999,BC_STOPPING,0,0)==1&&fm[FM_CANCEL]&&get(fm,FM_CLOSING),"folder merge close cancels scan and waits while worker is active");
 SetEvent(fmscan);check(!fm_proc((HWND)999,BC_STOPPING,0,0),"folder merge close allowed after scan worker finishes");CloseHandle(fmscan);
 closeMode=2;dirty[0]=dirty[1]=0;g_nsess=1;g_sess[0]=sess;q(sess,SS_HVIEW,999);workerBusy=1;frame_proc((HWND)999,WM_CLOSE,0,0);check(!destroyCalls&&timerCalls==1,"frame defers destruction while worker is busy");workerBusy=0;frame_proc((HWND)999,WM_TIMER,91,0);check(destroyCalls==1,"frame timer closes after worker completion");
 printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
