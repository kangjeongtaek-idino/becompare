/* Links to the production assembly. Failure injection affects only its Win32 calls. */
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
extern int text_save(const wchar_t*,const wchar_t*,uint64_t,uint64_t);
extern int safe_write2(const wchar_t*,const void*,uint64_t,const void*,uint64_t);
extern int bc_snwprintf(wchar_t*,uint64_t,const wchar_t*,...);
/* the converter hooks of textlib.asm are not part of this unit test */
int tl_convert(const wchar_t*p,wchar_t*o){(void)p;(void)o;return 0;}
int tl_isconv(const wchar_t*p){(void)p;return 0;}
static int fault, failed, checks;
BOOL WINAPI __real_WriteFile(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED);
BOOL WINAPI __real_FlushFileBuffers(HANDLE);
BOOL WINAPI __real_ReplaceFileW(LPCWSTR,LPCWSTR,LPCWSTR,DWORD,LPVOID,LPVOID);
BOOL WINAPI __wrap_WriteFile(HANDLE h,LPCVOID p,DWORD n,LPDWORD written,LPOVERLAPPED ov){
 if(fault==1){*written=0;SetLastError(ERROR_DISK_FULL);return FALSE;}
 if(fault==2){*written=n? n-1:0;return TRUE;}
 return __real_WriteFile(h,p,n,written,ov);
}
BOOL WINAPI __wrap_FlushFileBuffers(HANDLE h){if(fault==3){SetLastError(ERROR_WRITE_FAULT);return FALSE;}return __real_FlushFileBuffers(h);}
BOOL WINAPI __wrap_ReplaceFileW(LPCWSTR a,LPCWSTR b,LPCWSTR c,DWORD d,LPVOID e,LPVOID f){if(fault==4){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}return __real_ReplaceFileW(a,b,c,d,e,f);}
static void check(int ok,const char*name){checks++;printf("%s %s\n",ok?"PASS":"FAIL",name);if(!ok)failed++;}
static void seed(){FILE*f=fopen("target.txt","wb");fwrite("ORIGINAL",1,8,f);fclose(f);}
static int content(const unsigned char *want,size_t n){unsigned char b[64];FILE*f=fopen("target.txt","rb");if(!f)return 0;size_t k=fread(b,1,sizeof b,f);fclose(f);return k==n&&!memcmp(b,want,n);}
int main(void){
 int encs[]={0,1,2,3};
 for(int e=0;e<4;e++){
   unsigned char want[16]={0};size_t n=0;if(e==2){want[n++]=0xef;want[n++]=0xbb;want[n++]=0xbf;}if(e==3){want[n++]=0xff;want[n++]=0xfe;}
   seed();int r=text_save(L"target.txt",L"",0,encs[e]);char label[100];sprintf(label,"empty encoding %d",encs[e]);check(r==1&&content(want,n),label);
   seed();r=text_save(L"target.txt",L"abc",3,encs[e]);for(int i=0;i<3;i++){want[n++]='a'+i;if(e==3)want[n++]=0;}sprintf(label,"normal encoding %d",encs[e]);check(r==1&&content(want,n),label);
   for(int fail=1;fail<=4;fail++){seed();fault=fail;r=text_save(L"target.txt",L"NEW",3,encs[e]);fault=0;sprintf(label,"encoding %d fault %d preserves original",encs[e],fail);check(!r&&content((const unsigned char*)"ORIGINAL",8),label);}
 }
 unsigned char lines[]="a\r\nb\r\n";check(text_save(L"target.txt",L"a\nb\n",4,257)&&content(lines,6),"CRLF conversion");
 DeleteFileW(L"new-empty.txt");check(text_save(L"new-empty.txt",L"",0,1)==1,"create empty new file");
 DeleteFileW(L"new-failed.txt");fault=1;int r=text_save(L"new-failed.txt",L"x",1,1);fault=0;check(!r&&GetFileAttributesW(L"new-failed.txt")==INVALID_FILE_ATTRIBUTES,"failed new save leaves no target");
 seed();SetFileAttributesW(L"target.txt",FILE_ATTRIBUTE_READONLY);r=text_save(L"target.txt",L"NEW",3,1);SetFileAttributesW(L"target.txt",FILE_ATTRIBUTE_NORMAL);check(!r&&content((const unsigned char*)"ORIGINAL",8),"read-only target preserved");
 unsigned char *large=malloc(2500000);memset(large,0x5a,2500000);r=safe_write2(L"large.bin",large,1250000,large+1250000,1250000);free(large);WIN32_FILE_ATTRIBUTE_DATA a;check(r&&GetFileAttributesExW(L"large.bin",GetFileExInfoStandard,&a)&&a.nFileSizeLow==2500000,"chunked binary two-block save");
 /* bc_snwprintf: always terminated within the count, the result is the length written */
 {wchar_t b[16];int n;
  for(int i=0;i<16;i++)b[i]=L'#';n=bc_snwprintf(b,8,L"%s-%u",L"ab",7u);check(n==4&&!wcscmp(b,L"ab-7")&&b[5]==L'#',"format fits");
  for(int i=0;i<16;i++)b[i]=L'#';n=bc_snwprintf(b,8,L"%s",L"12345678");check(n==7&&!wcscmp(b,L"1234567")&&b[8]==L'#',"format exactly fills the count");
  for(int i=0;i<16;i++)b[i]=L'#';n=bc_snwprintf(b,8,L"%s",L"123456789012");check(n==7&&!wcscmp(b,L"1234567")&&b[8]==L'#',"format longer than the count");
  for(int i=0;i<16;i++)b[i]=L'#';n=bc_snwprintf(b,1,L"%s",L"x");check(n==0&&b[0]==0&&b[1]==L'#',"count of one");
  for(int i=0;i<16;i++)b[i]=L'#';n=bc_snwprintf(b,0,L"%s",L"x");check(b[0]==L'#',"count of zero writes nothing");
  wchar_t w[64];n=bc_snwprintf(w,64,L"%u %u %u %u %u %s %llu",1u,2u,3u,4u,5u,L"six",7ull);check(n==15&&!wcscmp(w,L"1 2 3 4 5 six 7"),"arguments passed on the stack");}
 WIN32_FIND_DATAW fd;HANDLE h=FindFirstFileW(L"BCA*.tmp",&fd);check(h==INVALID_HANDLE_VALUE,"temporary files cleaned");if(h!=INVALID_HANDLE_VALUE)FindClose(h);
 printf("%d checks, %d failures\n",checks,failed);return failed?1:0;
}
