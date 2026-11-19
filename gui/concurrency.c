/* Cross-process coordination. The UI owns its baseline; another tab cannot refresh it. */
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <shellapi.h>
#include <shlobj.h>

extern int text_save(const wchar_t*,const wchar_t*,uint64_t,uint64_t);
extern int safe_write2(const wchar_t*,const void*,uint64_t,const void*,uint64_t);
void co_changes_publish(const wchar_t*path);
typedef struct { DWORD kind,attrs; uint64_t size,time,id,hash; } Version;
typedef struct Watch { struct Watch *next; const void *owner; wchar_t *path; Version version; } Watch;
static Watch *watches;
static SRWLOCK watch_lock=SRWLOCK_INIT;
typedef struct {HANDLE mutex;wchar_t *path,*stage;unsigned depth;int failed,dirty;Version base;} ConfigTx;
typedef struct {Watch *saving;HANDLE save_mutex,journal_mutex;int had_conflict,staged_valid;Version staged;ConfigTx tx;} ThreadData;
static INIT_ONCE thread_once=INIT_ONCE_STATIC_INIT;
static DWORD thread_slot=FLS_OUT_OF_INDEXES;
static void NTAPI thread_free(void*p){free(p);}
static BOOL CALLBACK thread_init(PINIT_ONCE once,void*arg,void**context){
    (void)once;(void)arg;(void)context;thread_slot=FlsAlloc(thread_free);return thread_slot!=FLS_OUT_OF_INDEXES;
}
static ThreadData *thread_data(void){
    if(!InitOnceExecuteOnce(&thread_once,thread_init,NULL,NULL))ExitProcess(2);
    ThreadData *p=FlsGetValue(thread_slot);
    if(!p){p=calloc(1,sizeof *p);if(!p||!FlsSetValue(thread_slot,p))ExitProcess(2);}
    return p;
}
/* Native Windows thread storage keeps deployment independent of compiler TLS DLLs. */
#define saving (thread_data()->saving)
#define save_mutex (thread_data()->save_mutex)
#define journal_mutex (thread_data()->journal_mutex)
#define had_conflict (thread_data()->had_conflict)
#define tx (thread_data()->tx)
static uint64_t hash_bytes(uint64_t h,const void *p,size_t n){const unsigned char*b=p;while(n--){h^=*b++;h*=1099511628211ull;}return h;}
static wchar_t *absolute(const wchar_t *path){
    if(!path||!*path)return NULL;
    DWORD n=GetFullPathNameW(path,0,NULL,NULL);if(!n)return NULL;
    wchar_t *p=calloc(n+1,sizeof *p);if(!p)return NULL;
    if(!GetFullPathNameW(path,n+1,p,NULL)){free(p);return NULL;}
    return p;  /* Preserve the actual path's spelling for case-sensitive directories. */
}
static uint64_t hash_path_key(const wchar_t*path){
    wchar_t *key=_wcsdup(path);if(!key)return 0;
    CharLowerBuffW(key,(DWORD)wcslen(key));
    uint64_t h=hash_bytes(14695981039346656037ull,key,wcslen(key)*sizeof *key);free(key);return h;
}
static HANDLE named_lock(const wchar_t *kind,const wchar_t *path){
    uint64_t h=hash_path_key(path);
    wchar_t name[120];swprintf(name,120,L"Local\\BeCompare.%ls.%016llX",kind,(unsigned long long)h);
    HANDLE m=CreateMutexW(NULL,FALSE,name);if(!m)return NULL;
    DWORD r=WaitForSingleObject(m,10000);
    if(r!=WAIT_OBJECT_0&&r!=WAIT_ABANDONED){CloseHandle(m);SetLastError(ERROR_LOCK_VIOLATION);return NULL;}return m;
}
static void unlock(HANDLE m){if(m){ReleaseMutex(m);CloseHandle(m);}}
static HANDLE file_lock(void){
    wchar_t path[32768];DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
    if(!n||n>=32768)wcscpy(path,L"BeCompare-current-session");
    CharLowerBuffW(path,(DWORD)wcslen(path));return named_lock(L"Files",path);
}
HANDLE co_file_lock(void){return file_lock();}
void co_file_unlock(HANDLE m){unlock(m);}
/* Normal copy/move/delete operations cooperate with save and undo preflights too. */
#define CO_BOOL(expr) HANDLE m=file_lock();if(!m)return FALSE;BOOL r=(expr);DWORD e=GetLastError();unlock(m);SetLastError(e);return r
BOOL WINAPI co_CopyFileW(LPCWSTR a,LPCWSTR b,BOOL fail){CO_BOOL(CopyFileW(a,b,fail));}
BOOL WINAPI co_MoveFileW(LPCWSTR a,LPCWSTR b){CO_BOOL(MoveFileW(a,b));}
BOOL WINAPI co_MoveFileExW(LPCWSTR a,LPCWSTR b,DWORD flags){CO_BOOL(MoveFileExW(a,b,flags));}
BOOL WINAPI co_DeleteFileW(LPCWSTR p){CO_BOOL(DeleteFileW(p));}
BOOL WINAPI co_RemoveDirectoryW(LPCWSTR p){CO_BOOL(RemoveDirectoryW(p));}
BOOL WINAPI co_CreateDirectoryW(LPCWSTR p,LPSECURITY_ATTRIBUTES attrs){CO_BOOL(CreateDirectoryW(p,attrs));}
int WINAPI co_SHCreateDirectoryExW(HWND hwnd,LPCWSTR p,const SECURITY_ATTRIBUTES *attrs){HANDLE m=file_lock();if(!m)return ERROR_LOCK_VIOLATION;int r=SHCreateDirectoryExW(hwnd,p,attrs);unlock(m);return r;}
int WINAPI co_SHFileOperationW(LPSHFILEOPSTRUCTW op){
    HANDLE m=file_lock();if(!m)return ERROR_LOCK_VIOLATION;
    int r=SHFileOperationW(op);
    if(op&&(op->wFunc==FO_DELETE||op->wFunc==FO_MOVE||op->wFunc==FO_RENAME)&&op->pFrom){
        for(const wchar_t*p=op->pFrom;*p;p+=wcslen(p)+1){
            if(GetFileAttributesW(p)==INVALID_FILE_ATTRIBUTES){DWORD e=GetLastError();if(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)co_changes_publish(p);}
        }
    }
    unlock(m);return r;
}
/* Hash the content as well as identity/time/length, including same-size, same-time edits.
   Directory signatures include descendants so a renamed tree cannot hide edited children. */
static int version_at(const wchar_t *path,Version *v,unsigned depth){
    memset(v,0,sizeof *v);if(depth>128){SetLastError(ERROR_DIRECTORY);return 0;}
    HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS|FILE_FLAG_OPEN_REPARSE_POINT,NULL);
    if(f==INVALID_HANDLE_VALUE){DWORD e=GetLastError();if(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND)return 1;return 0;}
    BY_HANDLE_FILE_INFORMATION a,b;int ok=GetFileInformationByHandle(f,&a);
    if(!ok){CloseHandle(f);return 0;}
    v->kind=(a.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?2:1;v->attrs=a.dwFileAttributes;
    v->size=((uint64_t)a.nFileSizeHigh<<32)|a.nFileSizeLow;
    v->time=((uint64_t)a.ftLastWriteTime.dwHighDateTime<<32)|a.ftLastWriteTime.dwLowDateTime;
    v->id=((uint64_t)a.nFileIndexHigh<<32)|a.nFileIndexLow;
    v->hash=hash_bytes(14695981039346656037ull,&a.dwVolumeSerialNumber,sizeof(DWORD));
    if(v->kind==1){
        unsigned char buf[65536];DWORD got;
        for(;;){if(!ReadFile(f,buf,sizeof buf,&got,NULL)){ok=0;break;}if(!got)break;v->hash=hash_bytes(v->hash,buf,got);}
    }
    if(!GetFileInformationByHandle(f,&b)||memcmp(&a.ftLastWriteTime,&b.ftLastWriteTime,sizeof(FILETIME))||a.nFileSizeHigh!=b.nFileSizeHigh||a.nFileSizeLow!=b.nFileSizeLow)ok=0;
    CloseHandle(f);if(!ok)return 0;
    if(v->kind==2&&!(v->attrs&FILE_ATTRIBUTE_REPARSE_POINT)){
        size_t n=wcslen(path);wchar_t *child=calloc(n+MAX_PATH+4,sizeof *child);if(!child)return 0;
        swprintf(child,n+MAX_PATH+4,L"%ls\\*",path);WIN32_FIND_DATAW d;
        HANDLE search=FindFirstFileW(child,&d);
        if(search==INVALID_HANDLE_VALUE){free(child);return GetLastError()==ERROR_FILE_NOT_FOUND;}
        uint64_t sum=0,xor=0;DWORD e;
        do{if(!wcscmp(d.cFileName,L".")||!wcscmp(d.cFileName,L".."))continue;
            swprintf(child,n+MAX_PATH+4,L"%ls\\%ls",path,d.cFileName);Version cv;
            if(!version_at(child,&cv,depth+1)){ok=0;break;}
            uint64_t h=hash_bytes(hash_bytes(14695981039346656037ull,d.cFileName,wcslen(d.cFileName)*2),&cv,sizeof cv);sum+=h;xor^=h;
        }while(FindNextFileW(search,&d));
        e=GetLastError();if(ok&&e!=ERROR_NO_MORE_FILES)ok=0;
        FindClose(search);free(child);v->hash=hash_bytes(hash_bytes(v->hash,&sum,8),&xor,8);
    }return ok;
}
static int same(const Version*a,const Version*b){return !memcmp(a,b,sizeof *a);}
void fs_forget(const void *owner){
    AcquireSRWLockExclusive(&watch_lock);Watch **p=&watches;
    while(*p){Watch*w=*p;if(w->owner==owner){*p=w->next;free(w->path);free(w);}else p=&w->next;}
    ReleaseSRWLockExclusive(&watch_lock);
}
int fs_watch(const void *owner,const wchar_t *path){
    wchar_t *p=absolute(path);if(!p)return 0;Version v;int ok=version_at(p,&v,0);
    if(!ok){memset(&v,0,sizeof v);v.kind=~0u;}
    AcquireSRWLockExclusive(&watch_lock);Watch*w;
    for(w=watches;w;w=w->next)if(w->owner==owner&&!_wcsicmp(w->path,p))break;
    if(!w){w=calloc(1,sizeof *w);if(w){w->owner=owner;w->path=p;p=NULL;w->next=watches;watches=w;}}
    if(w)w->version=v;else ok=0;ReleaseSRWLockExclusive(&watch_lock);free(p);return ok;
}
static int conflict(void){had_conflict=1;SetLastError(ERROR_REVISION_MISMATCH);return 0;}
int fs_take_conflict(void){int r=had_conflict;had_conflict=0;return r;}
int fs_save_begin(const void *owner,const wchar_t *path){
    had_conflict=0;
    thread_data()->staged_valid=0;
    if(saving)return conflict();
    HANDLE m=file_lock();if(!m)return 0;
    wchar_t*p=absolute(path);Version now;
    if(!p||!version_at(p,&now,0)){free(p);unlock(m);return conflict();}
    AcquireSRWLockExclusive(&watch_lock);Watch*w;
    for(w=watches;w;w=w->next)if(w->owner==owner&&!_wcsicmp(w->path,p))break;
    if(!w&&!now.kind){w=calloc(1,sizeof *w);if(w){w->owner=owner;w->path=_wcsdup(p);w->version=now;w->next=watches;watches=w;}}
    int ok=w&&same(&w->version,&now);if(ok){saving=w;save_mutex=m;}
    ReleaseSRWLockExclusive(&watch_lock);free(p);if(!ok){unlock(m);return conflict();}return 1;
}
int fs_before_commit(const wchar_t *path,const wchar_t *stage){
    (void)path;
    if(!saving)return 1;
    if(!version_at(stage,&thread_data()->staged,0))return 0;
    thread_data()->staged_valid=1;
    Version now;return version_at(saving->path,&now,0)&&same(&saving->version,&now)?1:conflict();
}
int fs_save_end(int success){
    if(saving&&success){
        Version now;ThreadData*t=thread_data();
        int ok=t->staged_valid&&version_at(saving->path,&now,0)&&now.kind==1&&now.size==t->staged.size&&now.hash==t->staged.hash;
        AcquireSRWLockExclusive(&watch_lock);
        if(ok)saving->version=now;else{saving->version.kind=~0u;success=conflict();}
        ReleaseSRWLockExclusive(&watch_lock);
    }
    saving=NULL;unlock(save_mutex);save_mutex=NULL;
    thread_data()->staged_valid=0;return success;
}
int guarded_text_save(const void*owner,const wchar_t*path,const wchar_t*text,uint64_t n,uint64_t opts){
    if(!fs_save_begin(owner,path))return 0;
    int ok=text_save(path,text,n,opts);DWORD e=GetLastError();ok=fs_save_end(ok);SetLastError(had_conflict?ERROR_REVISION_MISMATCH:e);return ok;
}
int guarded_binary_save(const void*owner,const wchar_t*path,const void*bytes,uint64_t n){
    if(!fs_save_begin(owner,path))return 0;
    int ok=safe_write2(path,bytes,n,NULL,0);DWORD e=GetLastError();ok=fs_save_end(ok);SetLastError(had_conflict?ERROR_REVISION_MISMATCH:e);return ok;
}

/* Profile transactions stage every read/write in a private sibling file. The named
   mutex covers the whole read/modify/write, and a failed write never commits. */
static int is_tx(const wchar_t *p){return tx.path&&!_wcsicmp(p,tx.path);}
int cfg_begin(const wchar_t *path){
    wchar_t*p=absolute(path);if(!p)return 0;
    if(tx.path){int ok=is_tx(p);free(p);if(ok)tx.depth++;return ok;}
    HANDLE m=named_lock(L"Config",p);if(!m){free(p);return 0;}
    Version base;
    if(!version_at(p,&base,0)){free(p);unlock(m);return 0;}
    wchar_t *stage=calloc(32768,sizeof *stage),*dir=_wcsdup(p);wchar_t*slash=dir?wcsrchr(dir,L'\\'):NULL;
    int ok=0;if(stage&&slash){slash[1]=0;ok=GetTempFileNameW(dir,L"BCC",0,stage)!=0;}
    free(dir);
    if(ok){DWORD attrs=GetFileAttributesW(p);if(attrs!=INVALID_FILE_ATTRIBUTES)ok=CopyFileW(p,stage,FALSE);else{DWORD e=GetLastError();ok=e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND;
        if(ok){HANDLE f=CreateFileW(stage,GENERIC_WRITE,0,NULL,TRUNCATE_EXISTING,0,NULL);wchar_t bom=0xfeff;DWORD n;ok=f!=INVALID_HANDLE_VALUE&&WriteFile(f,&bom,2,&n,NULL)&&n==2;if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);}}}
    if(!ok){if(stage&&*stage)DeleteFileW(stage);free(stage);free(p);unlock(m);return 0;}
    memset(&tx,0,sizeof tx);tx.mutex=m;tx.path=p;tx.stage=stage;tx.depth=1;tx.base=base;return 1;
}
int cfg_end(void){
    if(!tx.path)return 0;
    if(--tx.depth)return !tx.failed;
    int ok=!tx.failed;
    if(!tx.dirty){SetFileAttributesW(tx.stage,FILE_ATTRIBUTE_NORMAL);DeleteFileW(tx.stage);free(tx.stage);free(tx.path);unlock(tx.mutex);memset(&tx,0,sizeof tx);return ok;}
    /* The documented cache-flush call returns zero even on success. */
    if(ok)WritePrivateProfileStringW(NULL,NULL,NULL,tx.stage);
    if(ok){HANDLE f=CreateFileW(tx.stage,GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);ok=f!=INVALID_HANDLE_VALUE&&FlushFileBuffers(f);if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);}
    Version now;
    if(ok)ok=version_at(tx.path,&now,0)&&same(&now,&tx.base);
    if(ok)ok=tx.base.kind?ReplaceFileW(tx.path,tx.stage,NULL,0,NULL,NULL):MoveFileExW(tx.stage,tx.path,MOVEFILE_WRITE_THROUGH);
    WritePrivateProfileStringW(NULL,NULL,NULL,tx.path);
    SetFileAttributesW(tx.stage,FILE_ATTRIBUTE_NORMAL);DeleteFileW(tx.stage);free(tx.stage);free(tx.path);unlock(tx.mutex);memset(&tx,0,sizeof tx);return ok;
}
void cfg_cancel(void){if(tx.path){tx.depth=1;tx.failed=1;cfg_end();}}
DWORD WINAPI settings_read(const wchar_t*s,const wchar_t*k,const wchar_t*d,wchar_t*out,DWORD n,const wchar_t*path){
    if(out&&n){wcsncpy(out,d?d:L"",n-1);out[n-1]=0;}
    DWORD fallback=out&&n?(DWORD)wcslen(out):0;
    wchar_t*p=absolute(path);if(!p)return fallback;if(is_tx(p)){free(p);return GetPrivateProfileStringW(s,k,d,out,n,tx.stage);}
    HANDLE m=named_lock(L"Config",p);DWORD r=m?GetPrivateProfileStringW(s,k,d,out,n,p):fallback;unlock(m);free(p);return r;
}
UINT WINAPI settings_read_int(const wchar_t*s,const wchar_t*k,INT d,const wchar_t*path){
    wchar_t*p=absolute(path);if(!p)return d;if(is_tx(p)){free(p);return GetPrivateProfileIntW(s,k,d,tx.stage);}
    HANDLE m=named_lock(L"Config",p);UINT r=m?GetPrivateProfileIntW(s,k,d,p):(UINT)d;unlock(m);free(p);return r;
}
DWORD WINAPI settings_read_section(const wchar_t*s,wchar_t*out,DWORD n,const wchar_t*path){
    if(out&&n)out[0]=0;
    if(out&&n>1)out[1]=0;
    wchar_t*p=absolute(path);if(!p)return 0;if(is_tx(p)){free(p);return GetPrivateProfileSectionW(s,out,n,tx.stage);}
    HANDLE m=named_lock(L"Config",p);DWORD r=m?GetPrivateProfileSectionW(s,out,n,p):0;unlock(m);free(p);return r;
}
BOOL WINAPI settings_write(const wchar_t*s,const wchar_t*k,const wchar_t*v,const wchar_t*path){
    wchar_t*p=absolute(path);if(!p)return FALSE;
    if(is_tx(p)){free(p);tx.dirty=1;int ok=WritePrivateProfileStringW(s,k,v,tx.stage);if(!ok)tx.failed=1;return ok;}
    free(p);if(!cfg_begin(path))return FALSE;tx.dirty=1;int ok=WritePrivateProfileStringW(s,k,v,tx.stage);if(!ok)tx.failed=1;return cfg_end();
}
/* A stale settings dialog must not replace newer settings with its old controls. */
int cfg_edit_watch(const void*owner,const wchar_t*path){
    /* Called while the profile transaction is held. Never acquire the file
       operation mutex here: text saves acquire file -> profile in that order. */
    return fs_watch(owner,path);
}
int cfg_edit_begin(const void*owner,const wchar_t*path,HWND hwnd){
    (void)hwnd;
    if(!cfg_begin(path))return 0;
    wchar_t*p=absolute(path);if(!p){cfg_cancel();return 0;}Version now;int ok=version_at(p,&now,0);AcquireSRWLockShared(&watch_lock);Watch*w;
    for(w=watches;w;w=w->next)if(w->owner==owner&&!_wcsicmp(w->path,p))break;
    ok=ok&&w&&same(&w->version,&now);ReleaseSRWLockShared(&watch_lock);free(p);
    if(!ok){cfg_cancel();return conflict();}return 1;
}

/* Existing-file creation used to truncate settings.ini if two starts raced. */
void cfg_prepare(const wchar_t *path){
    wchar_t*p=absolute(path);if(!p)return;HANDLE m=named_lock(L"Config",p);if(!m){free(p);return;}
    HANDLE f=CreateFileW(p,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(f!=INVALID_HANDLE_VALUE){wchar_t bom=0xfeff;DWORD n;WriteFile(f,&bom,2,&n,NULL);FlushFileBuffers(f);CloseHandle(f);}unlock(m);free(p);
}
/* Reserve a real unique name, rather than predicting one from a timer tick. */
int conversion_temp(wchar_t*out){
    wchar_t dir[400];DWORD n=GetTempPathW(400,dir);if(!n||n>=400){*out=0;return 0;}
    if(n+60>=400){*out=0;return 0;}
    wcscat(dir,L"BeCompareAsm\\");CreateDirectoryW(dir,NULL);
    size_t used=wcslen(dir);swprintf(dir+used,400-used,L"%X_x\\",(unsigned)GetCurrentProcessId());
    CreateDirectoryW(dir,NULL);
    return GetTempFileNameW(dir,L"BCV",0,out)!=0;
}

/* Journal preflight is all-or-nothing: never partially undo a step with a stale path. */
int fs_journal_begin(const void*owner){
    HANDLE m=file_lock();if(!m)return 0;int ok=1,found=0;
    AcquireSRWLockShared(&watch_lock);
    for(Watch*w=watches;w;w=w->next)if(w->owner==owner){Version now;found=1;if(!version_at(w->path,&now,0)||!same(&w->version,&now)){ok=0;break;}}
    ReleaseSRWLockShared(&watch_lock);if(!ok||!found){unlock(m);return conflict();}journal_mutex=m;return 1;
}
void fs_journal_end(const void*owner){
    AcquireSRWLockExclusive(&watch_lock);
    for(Watch*w=watches;w;w=w->next)if(w->owner==owner){Version now;if(version_at(w->path,&now,0)){w->version=now;if(!now.kind)co_changes_publish(w->path);}else w->version.kind=~0u;}
    ReleaseSRWLockExclusive(&watch_lock);unlock(journal_mutex);journal_mutex=NULL;
}

/* A bounded, per-settings shared change journal plus a wake-up message. Slow
   consumers reconcile the whole index after overflow instead of dropping changes. */
#define CHANGE_COUNT 512
#define CHANGE_PATH 1100
typedef struct {uint64_t seq;wchar_t paths[CHANGE_COUNT][CHANGE_PATH];} Changes;
static Changes *changes;
static HANDLE changes_file;
static wchar_t *changes_scope;
static UINT changes_message;
static uint64_t changes_seen;
void co_changes_init(const wchar_t*scope){
    if(changes)return;
    changes_scope=absolute(scope);if(!changes_scope)return;
    HANDLE m=named_lock(L"Changes",changes_scope);if(!m)return;
    uint64_t h=hash_path_key(changes_scope);
    wchar_t name[100];swprintf(name,100,L"Local\\BeCompare.ChangeData.%016llX",(unsigned long long)h);
    changes_file=CreateFileMappingW(INVALID_HANDLE_VALUE,NULL,PAGE_READWRITE,0,sizeof(Changes),name);
    if(changes_file)changes=MapViewOfFile(changes_file,FILE_MAP_ALL_ACCESS,0,0,sizeof(Changes));
    if(changes)changes_seen=changes->seq;
    unlock(m);
    changes_message=RegisterWindowMessageW(L"BeCompare.FilesChanged.v1");
}
int co_changes_message(UINT message){return changes_message&&message==changes_message;}
void co_changes_publish(const wchar_t*path){
    if(!changes)return;
    wchar_t*p=absolute(path);if(!p)return;
    if(wcslen(p)>=CHANGE_PATH){free(p);return;}
    HANDLE m=named_lock(L"Changes",changes_scope);if(!m){free(p);return;}
    uint64_t n=++changes->seq;wcscpy(changes->paths[(n-1)%CHANGE_COUNT],p);unlock(m);free(p);
    PostMessageW(HWND_BROADCAST,changes_message,0,0);
}
void co_changes_poll(void (*forget_path)(const wchar_t*),uint64_t ready){
    if(!changes||ready!=2)return;
    HANDLE m=named_lock(L"Changes",changes_scope);if(!m)return;
    uint64_t end=changes->seq,n=end-changes_seen;
    if(n>CHANGE_COUNT){unlock(m);forget_path(NULL);changes_seen=end;return;}
    wchar_t (*copy)[CHANGE_PATH]=n?calloc((size_t)n,sizeof *copy):NULL;
    if(n&&!copy){unlock(m);return;}
    for(uint64_t i=0;i<n;i++)wcscpy(copy[i],changes->paths[(changes_seen+i)%CHANGE_COUNT]);
    unlock(m);
    for(uint64_t i=0;i<n;i++)forget_path(copy[i]);
    free(copy);changes_seen=end;
}
