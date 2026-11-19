/* Real production functions, isolated fixtures, and child processes. No user files. */
#define UNICODE
#include <windows.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
extern int fs_watch(const void*,const wchar_t*),fs_save_begin(const void*,const wchar_t*);
extern int fs_save_end(int);extern void fs_forget(const void*);
extern int guarded_binary_save(const void*,const wchar_t*,const void*,uint64_t);
extern int cfg_begin(const wchar_t*),cfg_end(void),cfg_edit_watch(const void*,const wchar_t*),cfg_edit_begin(const void*,const wchar_t*,HWND);
extern void cfg_cancel(void),cfg_prepare(const wchar_t*);
extern BOOL WINAPI settings_write(const wchar_t*,const wchar_t*,const wchar_t*,const wchar_t*);
extern DWORD WINAPI settings_read(const wchar_t*,const wchar_t*,const wchar_t*,wchar_t*,DWORD,const wchar_t*);
extern int conversion_temp(wchar_t*);
extern void recent_add(const wchar_t*);extern wchar_t g_ini[];
extern void *fj_begin(void);extern void fj_file(void*,const wchar_t*),fj_commit(void**,void*),us_free(void**);
extern int fj_apply(void*,int);
extern void co_changes_init(const wchar_t*),co_changes_publish(const wchar_t*),co_changes_poll(void(*)(const wchar_t*),uint64_t);
extern int find_start(void),find_query(const wchar_t*,uint32_t*,uint32_t);
extern void find_forget_path(const wchar_t*);extern uint64_t fi_state;
static int checks,failed,inject;
static int inject_after_replace;
static int recent_worker,recent_reads;
static const wchar_t *recent_ready,*recent_go;
static void barrier(const wchar_t*,const wchar_t*);
static wchar_t inject_path[1200];
static void check(int ok,const char*n){checks++;printf("%s %s\n",ok?"PASS":"FAIL",n);fflush(stdout);if(!ok)failed++;}
static void put(const wchar_t*p,const char*s){FILE*f=_wfopen(p,L"wb");if(!f)ExitProcess(20);fwrite(s,1,strlen(s),f);fclose(f);}
static int content(const wchar_t*p,const char*s){char b[256]={0};FILE*f=_wfopen(p,L"rb");if(!f)return 0;size_t n=fread(b,1,sizeof b,f);fclose(f);return n==strlen(s)&&!memcmp(b,s,n);}
BOOL WINAPI __real_WriteFile(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
BOOL WINAPI __wrap_WriteFile(HANDLE h,LPCVOID b,DWORD n,LPDWORD got,LPOVERLAPPED ov){
 if(inject){inject=0;put(inject_path,"EXTERNAL");}
 return __real_WriteFile(h,b,n,got,ov);
}
BOOL WINAPI __real_ReplaceFileW(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPVOID,LPVOID);
BOOL WINAPI __wrap_ReplaceFileW(LPCWSTR target,LPCWSTR stage,LPCWSTR backup,DWORD flags,LPVOID p,LPVOID q){
 BOOL ok=__real_ReplaceFileW(target,stage,backup,flags,p,q);
 if(ok&&inject_after_replace){inject_after_replace=0;put(target,"EXTERNAL-AFTER");}return ok;
}
DWORD WINAPI __wrap_GetTickCount(void){return 0x12345678;} /* unique temp names cannot rely on the clock */
int __wrap_msgbox(HWND h,const wchar_t*t,int flags){(void)h;(void)t;return flags&MB_YESNO?IDYES:IDOK;}
DWORD WINAPI __real_settings_read(const wchar_t*,const wchar_t*,const wchar_t*,wchar_t*,DWORD,const wchar_t*);
DWORD WINAPI __wrap_settings_read(const wchar_t*s,const wchar_t*k,const wchar_t*d,wchar_t*out,DWORD n,const wchar_t*p){
 DWORD r=__real_settings_read(s,k,d,out,n,p);
 if(recent_worker&&s&&k&&!wcscmp(s,L"Recent")&&!wcscmp(k,L"R0")&&++recent_reads==2)barrier(recent_ready,recent_go);
 return r;
}
static void barrier(const wchar_t*ready,const wchar_t*go){
 put(ready,"ready");for(int i=0;i<2000;i++){if(GetFileAttributesW(go)!=INVALID_FILE_ATTRIBUTES)return;Sleep(5);}ExitProcess(21);
}
static int child(int argc,wchar_t**argv){
 if(argc==2&&!wcscmp(argv[1],L"temp")){wchar_t out[520];if(!conversion_temp(out))return 22;wprintf(L"%ls\n",out);return 0;}
 if(argc>=4&&!wcscmp(argv[1],L"publish")){
  co_changes_init(argv[2]);DeleteFileW(argv[3]);co_changes_publish(argv[3]);
  if(argc>4)for(int i=0;i<600;i++){wchar_t path[100];swprintf(path,100,L"C:\\unused-notification-%d",i);co_changes_publish(path);}return 0;
 }
 if(argc!=7)return 23;
 if(!wcscmp(argv[1],L"lock-settings")){
  static int owner;if(!cfg_begin(argv[2]))return 26;barrier(argv[4],argv[5]);
  int ok=cfg_edit_watch(&owner,argv[2]);cfg_cancel();return ok?0:27;
 }
 if(!wcscmp(argv[1],L"lock-save")){
  static int owner;fs_watch(&owner,argv[3]);if(!fs_save_begin(&owner,argv[3]))return 28;
  barrier(argv[4],argv[5]);wchar_t value[20];settings_read(L"UI",L"a",L"",value,20,argv[2]);fs_save_end(0);return 0;
 }
 if(!wcscmp(argv[1],L"save")){
  static int owner;fs_watch(&owner,argv[2]);barrier(argv[4],argv[5]);char c=(char)argv[3][0];
  int ok=guarded_binary_save(&owner,argv[2],&c,1);printf("saved=%d\n",ok);return ok==_wtoi(argv[6])?0:24;
 }
 if(!wcscmp(argv[1],L"recent")){
  wcscpy(g_ini,argv[2]);recent_worker=1;recent_ready=argv[4];recent_go=argv[5];recent_add(argv[3]);return 0;
 }
 return 25;
}
static int publish_child(const wchar_t*scope,const wchar_t*path,int overflow){
 wchar_t exe[1200],cmd[4000];GetModuleFileNameW(NULL,exe,1200);
 swprintf(cmd,4000,L"\"%ls\" publish \"%ls\" \"%ls\"%ls",exe,scope,path,overflow?L" overflow":L"");
 STARTUPINFOW si={sizeof si};PROCESS_INFORMATION pi;
 if(!CreateProcessW(NULL,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi))return 0;
 DWORD exit=99;WaitForSingleObject(pi.hProcess,10000);GetExitCodeProcess(pi.hProcess,&exit);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);return exit==0;
}
int main(void){
 int argc;wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(argc>1)return child(argc,argv);
 int a=0,b=0;const wchar_t *p=L"guarded.bin";
 put(p,"ORIGINAL");check(fs_watch(&a,p)&&fs_watch(&b,p),"two views retain separate file baselines");
 check(guarded_binary_save(&a,p,"FIRST",5),"first view can save its unchanged baseline");
 check(!guarded_binary_save(&b,p,"STALE",5)&&content(p,"FIRST"),"second stale view cannot overwrite first view's save");
 check(guarded_binary_save(&a,p,"AGAIN",5)&&content(p,"AGAIN"),"successful save refreshes only the saving view's baseline");
 fs_watch(&a,p);HANDLE f=CreateFileW(p,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);FILETIME ft;GetFileTime(f,NULL,NULL,&ft);CloseHandle(f);
 put(p,"OTHER");f=CreateFileW(p,FILE_WRITE_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,0,NULL);SetFileTime(f,NULL,NULL,&ft);CloseHandle(f);
 check(!guarded_binary_save(&a,p,"STALE",5)&&content(p,"OTHER"),"content hash detects same-size changes with restored modification time");
 DeleteFileW(L"missing.bin");fs_watch(&a,L"missing.bin");put(L"missing.bin","CREATED");
 check(!guarded_binary_save(&a,L"missing.bin","STALE",5)&&content(L"missing.bin","CREATED"),"file created after a missing-file baseline is protected");
 fs_watch(&a,p);DeleteFileW(p);check(!guarded_binary_save(&a,p,"STALE",5)&&GetFileAttributesW(p)==INVALID_FILE_ATTRIBUTES,"external deletion cannot be silently reversed by saving");
 put(p,"BASE");fs_watch(&a,p);GetFullPathNameW(p,1200,inject_path,NULL);inject=1;
 check(!guarded_binary_save(&a,p,"LOCAL",5)&&content(p,"EXTERNAL"),"external edit during staged writing is caught before replacement");
 fs_watch(&a,p);inject_after_replace=1;
 check(!guarded_binary_save(&a,p,"LOCAL",5)&&content(p,"EXTERNAL-AFTER"),"external edit immediately after replacement is reported without accepting its content as our baseline");
 check(!guarded_binary_save(&a,p,"RETRY",5)&&content(p,"EXTERNAL-AFTER"),"retry after a post-replacement conflict remains blocked until reload");
 put(L"unwatched.bin","KEEP");check(!guarded_binary_save(&a,L"unwatched.bin","STALE",5)&&content(L"unwatched.bin","KEEP"),"unread existing save destination is never overwritten implicitly");
 DeleteFileW(L"new.bin");check(guarded_binary_save(&a,L"new.bin","NEW",3)&&content(L"new.bin","NEW"),"new destination can be created and tracked");
 fs_watch(&a,L"unwatched.bin");check(guarded_binary_save(&a,L"unwatched.bin","CONFIRMED",9),"explicitly watched Save As destination can be saved");
 wchar_t ini[1200];GetFullPathNameW(L"transaction.ini",1200,ini,NULL);cfg_prepare(ini);settings_write(L"UI",L"a",L"old",ini);
 wchar_t text[100];check(cfg_begin(ini),"configuration transaction begins");settings_write(L"UI",L"a",L"staged",ini);settings_write(L"UI",L"b",L"staged",ini);
 GetPrivateProfileStringW(L"UI",L"a",L"",text,100,ini);check(!wcscmp(text,L"old"),"staged configuration does not change the original before commit");
 settings_read(L"UI",L"a",L"",text,100,ini);check(!wcscmp(text,L"staged"),"transaction reads its own staged writes");
 cfg_cancel();settings_read(L"UI",L"a",L"",text,100,ini);check(!wcscmp(text,L"old"),"cancelled transaction preserves original settings");
 cfg_begin(ini);settings_write(L"UI",L"a",L"new",ini);settings_write(L"UI",L"b",L"new",ini);check(cfg_end(),"multi-key configuration commits as one replacement");
 settings_read(L"UI",L"b",L"",text,100,ini);check(!wcscmp(text,L"new"),"committed multi-key settings are readable");
 cfg_edit_watch(&a,ini);settings_write(L"UI",L"a",L"elsewhere",ini);
 check(!cfg_edit_begin(&a,ini,NULL),"stale settings dialog cannot overwrite a concurrent change");
 cfg_edit_watch(&a,ini);check(cfg_edit_begin(&a,ini,NULL),"fresh settings dialog can begin its save");settings_write(L"UI",L"a",L"fresh",ini);check(cfg_end(),"fresh settings dialog commits successfully");
 cfg_begin(ini);settings_write(L"UI",L"a",L"must-not-commit",ini);SetFileAttributesW(ini,FILE_ATTRIBUTE_READONLY);
 check(!cfg_end(),"failed configuration replacement reports failure");SetFileAttributesW(ini,FILE_ATTRIBUTE_NORMAL);
 settings_read(L"UI",L"a",L"",text,100,ini);check(!wcscmp(text,L"fresh"),"failed configuration replacement preserves original values");
 void*slot=NULL;put(L"undo.bin","BEFORE");void*j=fj_begin();fj_file(j,L"undo.bin");put(L"undo.bin","AFTER");fj_commit(&slot,j);
 put(L"undo.bin","EXTERNAL");check(fj_apply(j,0)==-1&&content(L"undo.bin","EXTERNAL"),"undo preflight protects another process's later edit");us_free(&slot);
 slot=NULL;put(L"undo.bin","BEFORE");j=fj_begin();fj_file(j,L"undo.bin");put(L"undo.bin","AFTER");fj_commit(&slot,j);
 check(fj_apply(j,0)==0&&content(L"undo.bin","BEFORE"),"unchanged journal can undo");
 check(fj_apply(j,1)==0&&content(L"undo.bin","AFTER"),"unchanged journal can redo with its refreshed baseline");
 check(fj_apply(j,0)==0,"journal can undo again");put(L"undo.bin","EXTERNAL");
 check(fj_apply(j,1)==-1&&content(L"undo.bin","EXTERNAL"),"redo protects an edit made after undo");us_free(&slot);
 CreateDirectoryW(L"search",NULL);put(L"search\\delete-me.txt","one");put(L"search\\keep-me.txt","two");put(L"search\\overflow.txt","three");
 wchar_t root[1200],scope[1200],doomed[1200];GetFullPathNameW(L"search",1200,root,NULL);GetFullPathNameW(L"shared-change-scope.ini",1200,scope,NULL);
 SetEnvironmentVariableW(L"BC_FIND_ROOT",root);SetEnvironmentVariableW(L"BC_FIND_MODE",L"walk");SetEnvironmentVariableW(L"BC_FIND_CLOUD",L"?");
 co_changes_init(scope);find_start();for(int i=0;i<1000&&fi_state!=2;i++)Sleep(5);uint32_t ids[10];
 check(fi_state==2&&find_query(L".txt",ids,10)==3,"independent search index starts with three files");
 GetFullPathNameW(L"search\\delete-me.txt",1200,doomed,NULL);check(publish_child(scope,doomed,0),"another process deletes and publishes a path");
 co_changes_poll(find_forget_path,1);check(find_query(L"delete-me",ids,10)==1,"index notifications remain pending while indexing");
 co_changes_poll(find_forget_path,2);check(find_query(L"delete-me",ids,10)==0&&find_query(L"keep-me",ids,10)==1,"other process deletion is removed from the index without hiding surviving files");
 GetFullPathNameW(L"search\\overflow.txt",1200,doomed,NULL);check(publish_child(scope,doomed,1),"another process can overflow the bounded notification journal");
 co_changes_poll(find_forget_path,2);check(find_query(L"overflow",ids,10)==0&&find_query(L"keep-me",ids,10)==1,"notification overflow reconciles the index instead of losing deletions");
 printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
