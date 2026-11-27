/* The snapshot store of gui/recover.c, linked to the production text writer. Run in an empty folder. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef struct { wchar_t dir[600], data[700], path[520]; unsigned slot,type,bytes; uint64_t time; } RcItem;
extern int rc_enabled(void);
extern unsigned rc_interval_ms(void);
extern int rc_put_text(const void*,unsigned,unsigned,const wchar_t*,const wchar_t*,uint64_t,uint64_t);
extern int rc_put_bytes(const void*,unsigned,unsigned,const wchar_t*,const void*,uint64_t);
extern void rc_drop_slot(const void*,unsigned);
extern void rc_drop(const void*);
extern int rc_scan(RcItem*,int);
extern void rc_discard(const RcItem*);
extern int text_save(const wchar_t*,const wchar_t*,uint64_t,uint64_t);
/* what the application provides */
wchar_t g_ini[520];
uint64_t g_batch;
int boxes;
void *add_session(uint64_t t,const wchar_t*a,const wchar_t*b,const wchar_t*c){(void)t;(void)a;(void)b;(void)c;return NULL;}
void tc_tmp(wchar_t*o,const wchar_t*f,uint64_t id){(void)f;(void)id;*o=0;}
int msgbox(HWND h,const wchar_t*t,uint64_t f){(void)h;(void)t;(void)f;boxes++;return IDNO;}
int tl_convert(const wchar_t*p,wchar_t*o){(void)p;(void)o;return 0;}
int tl_isconv(const wchar_t*p){(void)p;return 0;}

static int failed,checks;
static wchar_t rootDir[600];
static void check(int ok,const char*name){checks++;printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)failed++;fflush(stdout);}
static void setting(const wchar_t*s,const wchar_t*k,const wchar_t*v){WritePrivateProfileStringW(s,k,v,g_ini);}
static int exists(const wchar_t*p){return GetFileAttributesW(p)!=INVALID_FILE_ATTRIBUTES;}
static size_t bytesOf(const wchar_t*p,unsigned char*out,size_t cap){FILE*f=_wfopen(p,L"rb");if(!f)return (size_t)-1;size_t n=fread(out,1,cap,f);fclose(f);return n;}
static uint64_t written(const wchar_t*p){WIN32_FILE_ATTRIBUTE_DATA a;if(!GetFileAttributesExW(p,GetFileExInfoStandard,&a))return 0;return ((uint64_t)a.ftLastWriteTime.dwHighDateTime<<32)|a.ftLastWriteTime.dwLowDateTime;}
/* the folders under recover\ whose name starts with the prefix; the last one found is copied to dir */
static int folders(const wchar_t*prefix,wchar_t*dir){
 wchar_t pat[700];swprintf(pat,700,L"%ls\\%ls*",rootDir,prefix);WIN32_FIND_DATAW d;int n=0;
 HANDLE f=FindFirstFileW(pat,&d);if(f==INVALID_HANDLE_VALUE)return 0;
 do{if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY){n++;if(dir)swprintf(dir,600,L"%ls\\%ls",rootDir,d.cFileName);}}while(FindNextFileW(f,&d));
 FindClose(f);return n;
}
static void mine(wchar_t*out){swprintf(out,40,L"%X_",(unsigned)GetCurrentProcessId());}
/* a snapshot folder as another process would have left it */
static void fake(const wchar_t*name,const wchar_t*started,const wchar_t*path,const char*content){
 wchar_t dir[600],p[700];swprintf(dir,600,L"%ls\\%ls",rootDir,name);CreateDirectoryW(rootDir,NULL);CreateDirectoryW(dir,NULL);
 swprintf(p,700,L"%ls\\0.dat",dir);FILE*f=_wfopen(p,L"wb");fputs(content,f);fclose(f);
 swprintf(p,700,L"%ls\\info.ini",dir);
 if(started)WritePrivateProfileStringW(L"Owner",L"Started",started,p);
 WritePrivateProfileStringW(L"0",L"Path",path,p);WritePrivateProfileStringW(L"0",L"Type",L"10",p);
 WritePrivateProfileStringW(L"0",L"Bytes",L"0",p);WritePrivateProfileStringW(L"0",L"Time",L"133000000000000000",p);
 WritePrivateProfileStringW(NULL,NULL,NULL,p);
}
int wmain(int argc,wchar_t**argv){
 static RcItem items[8];int owner1,owner2;wchar_t dir[600],p[700],pre[40];unsigned char got[64],want[64];
 GetFullPathNameW(L"rc-data",600,rootDir,NULL);CreateDirectoryW(rootDir,NULL);
 wcscat(rootDir,L"\\BeCompareAsm");CreateDirectoryW(rootDir,NULL);
 swprintf(g_ini,520,L"%ls\\settings.ini",rootDir);wcscat(rootDir,L"\\recover");
 if(argc>1&&!wcscmp(argv[1],L"scan")){             /* the other instance: takes what is there, then stays alive */
  int n=rc_scan(items,8);printf("%d\n",n);fflush(stdout);
  HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,L"Local\\BcRecoverTestReady"),go=OpenEventW(SYNCHRONIZE,FALSE,L"Local\\BcRecoverTestGo");
  SetEvent(ready);WaitForSingleObject(go,20000);return n;
 }
 mine(pre);
 /* ---- recording */
 check(rc_enabled()==1&&rc_interval_ms()==30000,"on by default, every 30 seconds");
 check(rc_put_text(&owner1,0,10,L"C:\\x\\가.txt",L"가나\r\n",4,1|1<<8)==1&&folders(pre,dir)==1,"a modified pane gets a folder of this process");
 swprintf(p,700,L"%ls\\0.dat",dir);
 text_save(L"expected.txt",L"가나\r\n",4,1|1<<8);size_t n=bytesOf(L"expected.txt",want,64);
 check(n==8&&bytesOf(p,got,64)==n&&!memcmp(got,want,n),"the snapshot holds the bytes a save would write");
 {wchar_t ini[700],v[520];swprintf(ini,700,L"%ls\\info.ini",dir);GetPrivateProfileStringW(L"0",L"Path",L"",v,520,ini);
  check(!wcscmp(v,L"C:\\x\\가.txt")&&GetPrivateProfileIntW(L"0",L"Type",0,ini)==10&&GetPrivateProfileIntW(L"0",L"Bytes",9,ini)==0,"path and type are recorded, also outside the code page");}
 {HANDLE f=CreateFileW(p,FILE_WRITE_ATTRIBUTES,7,NULL,OPEN_EXISTING,0,NULL);FILETIME old={0x10000000,0x01d00000};SetFileTime(f,NULL,NULL,&old);CloseHandle(f);
  uint64_t before=written(p);
  check(rc_put_text(&owner1,0,10,L"C:\\x\\가.txt",L"가나\r\n",4,1|1<<8)==1&&written(p)==before,"unchanged content is not written again");
  check(rc_put_text(&owner1,0,10,L"C:\\x\\가.txt",L"가나다\r\n",5,1|1<<8)==1&&written(p)!=before,"changed content is written");}
 {unsigned char raw[64];for(int i=0;i<64;i++)raw[i]=(unsigned char)(i*3);
  check(rc_put_bytes(&owner1,1,4,L"C:\\x\\b.bin",raw,64)==1,"bytes of a hex pane");
  swprintf(p,700,L"%ls\\1.dat",dir);wchar_t ini[700];swprintf(ini,700,L"%ls\\info.ini",dir);
  check(bytesOf(p,got,64)==64&&!memcmp(got,raw,64)&&GetPrivateProfileIntW(L"1",L"Bytes",0,ini)==1&&folders(pre,NULL)==1,"both panes of a view share its folder");}
 check(rc_put_text(&owner2,0,2,L"",L"x",1,1)==1&&folders(pre,NULL)==2,"a document without a path, in a folder of its own view");
 check(rc_scan(items,8)==0,"snapshots of this process are not offered");
 rc_drop_slot(&owner1,0);swprintf(p,700,L"%ls\\0.dat",dir);check(!exists(p)&&exists(dir),"dropping one pane keeps the other");
 rc_drop(&owner1);check(!exists(dir)&&folders(pre,NULL)==1,"dropping the view removes its folder");
 rc_drop(&owner2);rc_drop(&owner2);check(folders(pre,NULL)==0,"dropping twice is harmless");
 /* ---- settings */
 setting(L"Recover",L"IntervalSec",L"3");check(rc_interval_ms()==30000,"an interval below 5 seconds falls back to 30");
 setting(L"Recover",L"IntervalSec",L"601");check(rc_interval_ms()==30000,"an interval above 600 seconds falls back to 30");
 setting(L"Recover",L"IntervalSec",L"60");check(rc_interval_ms()==60000,"interval from the settings");
 g_batch=4;check(rc_enabled()==0&&rc_put_text(&owner1,0,10,L"a",L"x",1,1)==0&&folders(pre,NULL)==0,"nothing is recorded in an unattended run");g_batch=0;
 check(rc_put_text(&owner1,0,10,L"a",L"x",1,1)==1,"recording again");
 setting(L"Recover",L"Enabled",L"0");check(rc_enabled()==0&&rc_put_text(&owner1,0,10,L"a",L"y",1,1)==0&&folders(pre,NULL)==0,"turned off: not recorded, and the earlier snapshot of that pane goes");
 setting(L"Recover",L"Enabled",L"1");
 {setting(L"History",L"MaxFileMB",L"1");size_t big=2*1024*1024;wchar_t*t=malloc((big+1)*sizeof *t);for(size_t i=0;i<big;i++)t[i]=L'a';t[big]=0;
  check(rc_put_text(&owner1,0,10,L"a",t,big,1)==0&&folders(pre,NULL)==0,"content over the size limit is not kept");free(t);setting(L"History",L"MaxFileMB",NULL);}
 /* ---- what other runs left */
 fake(L"FFFFFFF0_1",L"5",L"C:\\gone\\a.txt","RECOVERED");
 check(rc_scan(items,8)==1&&!wcscmp(items[0].path,L"C:\\gone\\a.txt")&&items[0].type==10&&!items[0].bytes&&items[0].time==133000000000000000ull&&items[0].slot==0,"the snapshot of a process that is gone is offered");
 swprintf(p,700,L"%ls\\FFFFFFF0_1",rootDir);
 {wchar_t c[40];swprintf(c,40,L"%X_c",(unsigned)GetCurrentProcessId());
  check(!exists(p)&&folders(c,NULL)==1&&bytesOf(items[0].data,got,64)==9&&!memcmp(got,"RECOVERED",9),"and taken over by this process");
  check(rc_scan(items,8)==1&&folders(c,NULL)==1,"what was taken over and left for later is offered again");
  rc_discard(&items[0]);check(folders(c,NULL)==0&&rc_scan(items,8)==0,"discarding removes it");
  /* the process id of this very process, but another creation time: an earlier process had the same id */
  wchar_t name[40];swprintf(name,40,L"%X_7",(unsigned)GetCurrentProcessId());fake(name,L"1",L"C:\\gone\\reused.txt","OLD");
  check(rc_scan(items,8)==1&&!wcscmp(items[0].path,L"C:\\gone\\reused.txt"),"a reused process id does not hide a snapshot");rc_discard(&items[0]);
  /* a folder whose owner did not get to record its time: running process, left alone */
  fake(name,NULL,L"C:\\gone\\young.txt","NEW");
  check(rc_scan(items,8)==1,"without a recorded time the folder of an id that is this process is an old one");rc_discard(&items[0]);
  /* a folder without content is cleaned up, not offered */
  fake(L"FFFFFFF0_2",L"5",L"x","");swprintf(p,700,L"%ls\\FFFFFFF0_2\\0.dat",rootDir);DeleteFileW(p);
  check(rc_scan(items,8)==0&&folders(L"FFFFFFF0",NULL)==0,"an empty leftover folder is removed");
  /* two instances start together: the other one takes it first, and is still running */
  fake(L"FFFFFFF0_3",L"5",L"C:\\gone\\b.txt","B");
  HANDLE ready=CreateEventW(NULL,TRUE,FALSE,L"Local\\BcRecoverTestReady"),go=CreateEventW(NULL,TRUE,FALSE,L"Local\\BcRecoverTestGo");
  wchar_t exe[600],cmd[700];GetModuleFileNameW(NULL,exe,600);swprintf(cmd,700,L"\"%ls\" scan",exe);
  STARTUPINFOW si;PROCESS_INFORMATION pi;memset(&si,0,sizeof si);si.cb=sizeof si;
  int started=CreateProcessW(NULL,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi);
  check(started&&WaitForSingleObject(ready,15000)==WAIT_OBJECT_0&&rc_scan(items,8)==0,"a snapshot another running instance took over is not offered here");
  SetEvent(go);DWORD code=99;if(started){WaitForSingleObject(pi.hProcess,15000);GetExitCodeProcess(pi.hProcess,&code);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);}
  check(code==1&&rc_scan(items,8)==1&&!wcscmp(items[0].path,L"C:\\gone\\b.txt"),"once that instance is gone too, it is offered again");rc_discard(&items[0]);}
 /* ---- the folder cannot be made: recording gives up quietly */
 rc_drop(&owner1);RemoveDirectoryW(rootDir);
 {FILE*f=_wfopen(rootDir,L"wb");if(f)fclose(f);
  check(!(GetFileAttributesW(rootDir)&FILE_ATTRIBUTE_DIRECTORY)&&rc_put_text(&owner1,0,10,L"a",L"x",1,1)==0&&rc_put_bytes(&owner1,1,4,L"a","x",1)==0&&rc_scan(items,8)==0&&boxes==0,"a recover folder that cannot be made: nothing recorded, no message");
  rc_drop(&owner1);DeleteFileW(rootDir);}
 printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
