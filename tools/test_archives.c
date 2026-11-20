/* Production ZIP/snapshot readers and writer, isolated by the Python driver. */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
typedef struct {wchar_t *name;uint32_t attr,crc;uint64_t size,time,offset,compressed;uint32_t method,implied;} Entry;
typedef struct {Entry *entries;uint64_t count,capacity;void *data;uint64_t size,kind;} Table;
extern int zip_open(const wchar_t*,Table*),snap_open(const wchar_t*,Table*),zip_extract(Table*,Entry*,const wchar_t*);
extern void vt_free(Table*);
int tl_convert(const wchar_t*p,wchar_t*o){(void)p;(void)o;return 0;}
int tl_isconv(const wchar_t*p){(void)p;return 0;}
/* util.asm's UI-independent string duplication contract. */
wchar_t *str_dup(const wchar_t*p){return _wcsdup(p);}
static int fault;
BOOL WINAPI __real_WriteFile(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
BOOL WINAPI __wrap_WriteFile(HANDLE h,LPCVOID p,DWORD n,LPDWORD got,LPOVERLAPPED ov){
    if(fault==1){*got=0;SetLastError(ERROR_DISK_FULL);return FALSE;}
    if(fault==2){*got=0;return TRUE;}
    if(fault==3&&n>3)n=3;
    return __real_WriteFile(h,p,n,got,ov);
}
BOOL WINAPI __real_FlushFileBuffers(HANDLE);
BOOL WINAPI __wrap_FlushFileBuffers(HANDLE h){if(fault==4){SetLastError(ERROR_WRITE_FAULT);return FALSE;}return __real_FlushFileBuffers(h);}
int wmain(int argc,wchar_t **argv){
    if(argc<3)return 20;
    Table t={0};ULONGLONG start=GetTickCount64();
    int ok=!wcscmp(argv[1],L"snapshot")?snap_open(argv[2],&t):zip_open(argv[2],&t);
    printf("open=%d count=%llu capacity=%llu ms=%llu\n",ok,(unsigned long long)t.count,(unsigned long long)t.capacity,(unsigned long long)(GetTickCount64()-start));
    if(ok&&argc>=4){
        fault=argc>4?_wtoi(argv[4]):0;
        Entry *e=NULL;for(uint64_t i=0;i<t.count;i++)if(!(t.entries[i].attr&FILE_ATTRIBUTE_DIRECTORY)){e=&t.entries[i];break;}
        ok=e&&zip_extract(&t,e,argv[3]);printf("extract=%d\n",ok);
    }
    vt_free(&t);return ok?0:2;
}
