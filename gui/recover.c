/* Crash recovery: what the modified panes hold is kept under <settings folder>\recover while it is
   unsaved, and a later start offers what a run that ended abnormally left behind.

   recover\<pid>_<n>\            snapshots of one view of a running process
       0.dat, 1.dat              the pane as a save would have written it (text) or its bytes (hex)
       info.ini                  [Owner] Started=<creation time of the process>
                                 [0] / [1] Path=, Type=, Bytes=, Time=
   recover\<pid>_c<started>_<n>\ the same folder once a later process took it over to offer it. The rename
                                 is the claim: two instances that start together cannot both offer it.

   A folder belongs to a running process when that process exists and was created at the recorded time
   (a process id alone is reused). Nothing here shows a message box while recording: the timer would
   repeat it every interval. Unattended runs neither record nor offer. */
#define UNICODE
#define _UNICODE
#define _WIN32_WINNT 0x0600
#include <windows.h>
#include <commctrl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>

extern wchar_t g_ini[];
extern uint64_t g_batch;
extern int text_save(const wchar_t*,const wchar_t*,uint64_t,uint64_t);
extern int safe_write2(const wchar_t*,const void*,uint64_t,const void*,uint64_t);
extern void *add_session(uint64_t type,const wchar_t*a,const wchar_t*b,const wchar_t*c);
extern void tc_tmp(wchar_t*out,const wchar_t*format,uint64_t id);
extern int msgbox(HWND,const wchar_t*,uint64_t);
extern UINT WINAPI settings_read_int(const wchar_t*,const wchar_t*,INT,const wchar_t*);   /* aware of a settings transaction */

enum {T_TEXT=2,T_HEX=4,T_EDIT=10};
enum {SS_HVIEW=8,SS_DATA=16,ID_EDIT_A=101,BC_RUN=WM_APP+11};
typedef struct { wchar_t dir[600], data[700], path[520]; unsigned slot,type,bytes; uint64_t time; } RcItem;
typedef struct Own { struct Own *next; const void *owner; wchar_t dir[600]; uint64_t hash[2]; int has[2]; } Own;
static Own *owns;

int rc_enabled(void){ return !g_batch&&settings_read_int(L"Recover",L"Enabled",1,g_ini)!=0; }
unsigned rc_interval_ms(void){
    UINT s=settings_read_int(L"Recover",L"IntervalSec",30,g_ini);
    if(s<5||s>600)s=30;
    return s*1000;
}
static uint64_t limit(void){
    UINT mb=settings_read_int(L"History",L"MaxFileMB",16,g_ini);
    if(mb<1||mb>1024)mb=16;
    return (uint64_t)mb<<20;
}
static uint64_t fnv(uint64_t h,const void *p,uint64_t n){ const unsigned char *b=p; while(n--){h^=*b++;h*=1099511628211ull;} return h; }
static uint64_t ft64(FILETIME t){ return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime; }
static uint64_t started_of(HANDLE process){ FILETIME c,e,k,u; return GetProcessTimes(process,&c,&e,&k,&u)?ft64(c):0; }
static uint64_t my_start(void){ static uint64_t t; if(!t)t=started_of(GetCurrentProcess()); return t; }
static uint64_t now(void){ FILETIME t; GetSystemTimeAsFileTime(&t); return ft64(t); }

static int root(wchar_t *out){                           /* out: wchar[600] */
    if(wcslen(g_ini)>=450)return 0;
    wcscpy(out,g_ini);
    wchar_t *slash=wcsrchr(out,L'\\'); if(!slash)return 0;
    wcscpy(slash+1,L"recover");
    return 1;
}
static void ini_of(const wchar_t *dir,wchar_t *out){ swprintf(out,700,L"%ls\\info.ini",dir); }
static void data_of(const wchar_t *dir,unsigned slot,wchar_t *out){ swprintf(out,700,L"%ls\\%u.dat",dir,slot); }
static void section(unsigned slot,wchar_t *out){ out[0]=(wchar_t)(L'0'+slot); out[1]=0; }
static int has_data(const wchar_t *dir){
    wchar_t p[700];
    for(unsigned s=0;s<2;s++){ data_of(dir,s,p); if(GetFileAttributesW(p)!=INVALID_FILE_ATTRIBUTES)return 1; }
    return 0;
}
/* the folder and everything in it (there are no folders inside) */
static void remove_dir(const wchar_t *dir){
    wchar_t p[700]; WIN32_FIND_DATAW d;
    ini_of(dir,p); WritePrivateProfileStringW(NULL,NULL,NULL,p);        /* the profile cache lets go of it */
    swprintf(p,700,L"%ls\\*",dir);
    HANDLE f=FindFirstFileW(p,&d);
    if(f!=INVALID_HANDLE_VALUE){
        do{
            if(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)continue;
            if(wcslen(dir)+wcslen(d.cFileName)+2>=700)continue;
            swprintf(p,700,L"%ls\\%ls",dir,d.cFileName);
            SetFileAttributesW(p,FILE_ATTRIBUTE_NORMAL); DeleteFileW(p);
        }while(FindNextFileW(f,&d));
        FindClose(f);
    }
    RemoveDirectoryW(dir);
}
static void put_number(const wchar_t *ini,const wchar_t *sec,const wchar_t *key,uint64_t v){
    wchar_t t[32]; swprintf(t,32,L"%llu",(unsigned long long)v); WritePrivateProfileStringW(sec,key,t,ini);
}
static uint64_t get_number(const wchar_t *ini,const wchar_t *sec,const wchar_t *key){
    wchar_t t[32]; GetPrivateProfileStringW(sec,key,L"0",t,32,ini); return _wcstoui64(t,NULL,10);
}

static Own *find(const void *owner){ Own *o=owns; while(o&&o->owner!=owner)o=o->next; return o; }
static void forget(Own *o){ Own **p=&owns; while(*p&&*p!=o)p=&(*p)->next; if(*p)*p=o->next; free(o); }
/* a new folder for the snapshots of that view */
static Own *create(const void *owner){
    static unsigned seq;
    Own *o=calloc(1,sizeof *o); if(!o)return NULL;
    wchar_t base[600],ini[700];
    if(!root(base)){free(o);return NULL;}
    CreateDirectoryW(base,NULL);
    for(int tries=0;;tries++){
        swprintf(o->dir,600,L"%ls\\%X_%u",base,(unsigned)GetCurrentProcessId(),++seq);
        if(CreateDirectoryW(o->dir,NULL))break;
        if(GetLastError()!=ERROR_ALREADY_EXISTS||tries>10000){free(o);return NULL;}
    }
    ini_of(o->dir,ini);
    HANDLE f=CreateFileW(ini,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);   /* UTF-16: any path fits */
    if(f!=INVALID_HANDLE_VALUE){ wchar_t bom=0xFEFF; DWORD n; WriteFile(f,&bom,2,&n,NULL); CloseHandle(f); }
    put_number(ini,L"Owner",L"Started",my_start());
    o->owner=owner; o->next=owns; owns=o;
    return o;
}
void rc_drop_slot(const void *owner,unsigned slot){
    Own *o=find(owner); if(!o||slot>1)return;
    if(o->has[slot]){
        wchar_t p[700],sec[2];
        data_of(o->dir,slot,p); DeleteFileW(p);
        ini_of(o->dir,p); section(slot,sec); WritePrivateProfileStringW(sec,NULL,NULL,p);
        o->has[slot]=0;
    }
    if(!o->has[0]&&!o->has[1]){ remove_dir(o->dir); forget(o); }
}
/* A recovered snapshot stays until the tab it was opened in saves: closing that tab without saving, or a
   second crash, must not lose it. It is then offered again by the next start. */
typedef struct Kept { struct Kept *next; const void *owner; RcItem item; } Kept;
static Kept *kept;
void rc_discard(const RcItem *it);
static void release(const void *owner,int discard){
    Kept **p=&kept;
    while(*p){
        Kept *k=*p;
        if(k->owner!=owner){ p=&k->next; continue; }
        *p=k->next;
        if(discard)rc_discard(&k->item);
        free(k);
    }
}
/* rc_saved(owner): that view wrote a file - what was recovered into it has served its purpose */
void rc_saved(const void *owner){ release(owner,1); }
/* rc_drop(owner): the view is closing in the ordinary way */
void rc_drop(const void *owner){ rc_drop_slot(owner,0); rc_drop_slot(owner,1); release(owner,0); }

/* 1: written, or the same as the last time. 0: not kept (off, too large, cannot be written). */
static int put(const void *owner,unsigned slot,unsigned type,const wchar_t *path,int bytes,
               const void *content,uint64_t n,uint64_t opts){
    if(slot>1)return 0;
    if(!path)path=L"";
    uint64_t size=bytes?n:n*2;
    if(!rc_enabled()||n>limit()||wcslen(path)>=520){ rc_drop_slot(owner,slot); return 0; }
    uint64_t h=fnv(14695981039346656037ull,content,size);
    h=fnv(h,path,wcslen(path)*2); h=fnv(h,&opts,8); h=fnv(h,&type,sizeof type);
    Own *o=find(owner);
    if(o&&o->has[slot]&&o->hash[slot]==h)return 1;
    if(!o&&!(o=create(owner)))return 0;
    wchar_t p[700],sec[2];
    data_of(o->dir,slot,p);
    int ok=bytes?safe_write2(p,content,n,NULL,0):text_save(p,content,n,opts);
    if(!ok){ if(!o->has[0]&&!o->has[1]){ remove_dir(o->dir); forget(o); } return 0; }   /* an older snapshot stays */
    ini_of(o->dir,p); section(slot,sec);
    WritePrivateProfileStringW(sec,L"Path",path,p);
    put_number(p,sec,L"Type",type); put_number(p,sec,L"Bytes",(uint64_t)bytes); put_number(p,sec,L"Time",now());
    o->has[slot]=1; o->hash[slot]=h;
    return 1;
}
int rc_put_text(const void *owner,unsigned slot,unsigned type,const wchar_t *path,const wchar_t *text,uint64_t n,uint64_t opts){
    return put(owner,slot,type,path,0,text,n,opts);
}
int rc_put_bytes(const void *owner,unsigned slot,unsigned type,const wchar_t *path,const void *bytes,uint64_t n){
    return put(owner,slot,type,path,1,bytes,n,0);
}

/* ---------------------------------------------------------------- what an earlier run left behind */
static int alive(DWORD pid,uint64_t started){
    HANDLE h=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!h)return GetLastError()==ERROR_ACCESS_DENIED;    /* it exists, and is not ours to judge */
    DWORD code=0; int running=GetExitCodeProcess(h,&code)&&code==STILL_ACTIVE;
    uint64_t t=started_of(h); CloseHandle(h);
    if(!running)return 0;
    return !started||!t||t==started;                     /* no time recorded yet: its folder is being made */
}
static int tracked(const wchar_t *dir){ for(Own *o=owns;o;o=o->next)if(!_wcsicmp(o->dir,dir))return 1; return 0; }
static int hex_run(const wchar_t **p,uint64_t *v,int most){
    int n=0; *v=0;
    for(;n<most;n++){
        wchar_t c=(*p)[n]; unsigned d;
        if(c>=L'0'&&c<=L'9')d=c-L'0'; else if((c|0x20)>=L'a'&&(c|0x20)<=L'f')d=(c|0x20)-L'a'+10; else break;
        *v=*v<<4|d;
    }
    *p+=n; return n;
}
/* rc_scan(out, cap) -> count: the snapshots of processes that are gone, taken over by this process */
int rc_scan(RcItem *out,int cap){
    wchar_t base[600],pattern[700],dir[600],ini[700]; WIN32_FIND_DATAW d; int count=0;
    if(!root(base))return 0;
    swprintf(pattern,700,L"%ls\\*",base);
    HANDLE f=FindFirstFileW(pattern,&d); if(f==INVALID_HANDLE_VALUE)return 0;
    DWORD mine=GetCurrentProcessId(); static unsigned claims;
    do{
        if(!(d.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)||wcslen(d.cFileName)>60)continue;
        const wchar_t *p=d.cFileName; uint64_t pid,started=0; int claimed=0;
        if(!hex_run(&p,&pid,8)||*p++!=L'_'||!pid)continue;
        swprintf(dir,600,L"%ls\\%ls",base,d.cFileName);
        if(*p==L'c'){ p++; claimed=1; if(hex_run(&p,&started,16)!=16||*p!=L'_')continue; }
        else{ ini_of(dir,ini); started=get_number(ini,L"Owner",L"Started"); }
        int own=pid==mine&&started==my_start();
        if(claimed?!own&&alive((DWORD)pid,started):own||tracked(dir)||(pid!=mine&&alive((DWORD)pid,started)))continue;
        if(!has_data(dir)){ remove_dir(dir); continue; }
        if(!(claimed&&own)){                             /* take it over: the rename succeeds for one process only */
            wchar_t to[600]; int moved=0;
            ini_of(dir,ini); WritePrivateProfileStringW(NULL,NULL,NULL,ini);
            for(int tries=0;tries<10000&&!moved;tries++){
                swprintf(to,600,L"%ls\\%X_c%016llX_%u",base,(unsigned)mine,(unsigned long long)my_start(),++claims);
                if(MoveFileW(dir,to))moved=1; else if(GetLastError()!=ERROR_ALREADY_EXISTS)break;
            }
            if(!moved)continue;
            wcscpy(dir,to);
        }
        ini_of(dir,ini);
        for(unsigned s=0;s<2&&count<cap;s++){
            RcItem *it=&out[count]; wchar_t sec[2]; section(s,sec);
            data_of(dir,s,it->data);
            if(GetFileAttributesW(it->data)==INVALID_FILE_ATTRIBUTES)continue;
            wcscpy(it->dir,dir); it->slot=s;
            GetPrivateProfileStringW(sec,L"Path",L"",it->path,520,ini);
            it->type=(unsigned)get_number(ini,sec,L"Type");
            it->bytes=get_number(ini,sec,L"Bytes")!=0;
            it->time=get_number(ini,sec,L"Time");
            count++;
        }
    }while(FindNextFileW(f,&d));
    FindClose(f);
    return count;
}
void rc_discard(const RcItem *it){
    wchar_t ini[700],sec[2];
    SetFileAttributesW(it->data,FILE_ATTRIBUTE_NORMAL); DeleteFileW(it->data);
    ini_of(it->dir,ini); section(it->slot,sec); WritePrivateProfileStringW(sec,NULL,NULL,ini);
    if(!has_data(it->dir))remove_dir(it->dir);
}
/* rc_restore(item) -> 1 opened. The file itself is never written: the kept content is opened next to it
   (a copy in the temporary folder on the left, the file on the right). A document that had no file, or
   whose file is gone, opens in the editor with an empty path, so that saving asks for a name.
   The snapshot is not removed here: it stays until the opened tab saves. */
int rc_restore(const RcItem *it){
    static unsigned serial;
    wchar_t tmp[700]; const wchar_t *name=wcsrchr(it->path,L'\\');
    name=name?name+1:it->path;
    if(!*name)name=it->bytes?L"recovered.bin":L"recovered.txt";
    tc_tmp(tmp,L"복구%X_",(uint64_t)(GetTickCount()+ ++serial));
    if(wcslen(tmp)+wcslen(name)>=519)name=it->bytes?L"recovered.bin":L"recovered.txt";
    if(wcslen(tmp)+wcslen(name)>=519)return 0;
    wcscat(tmp,name);
    if(!CopyFileW(it->data,tmp,FALSE))return 0;
    SetFileAttributesW(tmp,FILE_ATTRIBUTE_NORMAL);
    DWORD attrs=it->path[0]?GetFileAttributesW(it->path):INVALID_FILE_ATTRIBUTES;
    int there=attrs!=INVALID_FILE_ATTRIBUTES&&!(attrs&FILE_ATTRIBUTE_DIRECTORY);
    void *sess;
    if(there)sess=add_session(it->bytes?T_HEX:T_TEXT,tmp,it->path,NULL);
    else if(it->bytes)sess=add_session(T_HEX,tmp,NULL,NULL);
    else{
        sess=add_session(T_EDIT,tmp,NULL,NULL);
        if(sess){
            HWND view=*(HWND*)((char*)sess+SS_HVIEW);
            SendMessageW(view,BC_RUN,0,0);                          /* read the copy now ... */
            SetWindowTextW(GetDlgItem(view,ID_EDIT_A),L"");        /* ... and forget where it came from */
            HWND pane=FindWindowExW(view,NULL,L"RICHEDIT50W",NULL);
            if(pane)SendMessageW(pane,EM_SETMODIFY,1,0);
        }
    }
    if(!sess)return 0;
    const void *owner=*(void**)((char*)sess+SS_DATA);        /* the view's data: what it passes as owner */
    Kept *k=owner?calloc(1,sizeof *k):NULL;
    if(k){ k->owner=owner; k->item=*it; k->next=kept; kept=k; }   /* gone once that view saves (rc_saved) */
    return 1;
}

/* ---------------------------------------------------------------- the list shown when the application starts */
enum {RC_RESTORE=1,RC_LATER=2,RC_DISCARD=3,RC_LIST=10,RC_TEXT=11,RC_CAP=64};
static RcItem items[RC_CAP];
static int item_count,action;
static unsigned char picked[RC_CAP];

static void layout(HWND h){
    RECT r; GetClientRect(h,&r);
    MoveWindow(GetDlgItem(h,RC_TEXT),12,10,r.right-24,36,TRUE);
    MoveWindow(GetDlgItem(h,RC_LIST),12,50,r.right-24,r.bottom-104,TRUE);
    MoveWindow(GetDlgItem(h,RC_RESTORE),r.right-332,r.bottom-42,100,30,TRUE);
    MoveWindow(GetDlgItem(h,RC_DISCARD),r.right-222,r.bottom-42,100,30,TRUE);
    MoveWindow(GetDlgItem(h,RC_LATER),r.right-112,r.bottom-42,100,30,TRUE);
}
static void finish(HWND h,int what){
    HWND list=GetDlgItem(h,RC_LIST); int any=0;
    for(int i=0;i<item_count;i++){ picked[i]=(SendMessageW(list,LVM_GETITEMSTATE,i,LVIS_STATEIMAGEMASK)>>12)==2; any|=picked[i]; }
    if(what!=RC_LATER&&!any)return;
    if(what==RC_DISCARD&&msgbox(h,L"선택한 항목의 보관된 내용을 지웁니다. 지운 뒤에는 되살릴 수 없습니다. 계속할까요?",MB_YESNO|MB_ICONWARNING)!=IDYES)return;
    action=what;
}
static LRESULT CALLBACK recover_proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_SIZE: layout(h); return 0;
    case WM_COMMAND: if(LOWORD(w)>=RC_RESTORE&&LOWORD(w)<=RC_DISCARD){ finish(h,LOWORD(w)); return 0; } break;
    case WM_CLOSE: action=RC_LATER; return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
static HWND child(HWND parent,const wchar_t *cls,const wchar_t *text,DWORD style,int id){
    HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,0,0,parent,(HMENU)(INT_PTR)id,GetModuleHandleW(NULL),NULL);
    SendMessageW(c,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),TRUE);
    return c;
}
static void column(HWND list,int i,const wchar_t *title,int width){
    LVCOLUMNW c; memset(&c,0,sizeof c); c.mask=LVCF_TEXT|LVCF_WIDTH; c.pszText=(wchar_t*)title; c.cx=width;
    SendMessageW(list,LVM_INSERTCOLUMNW,i,(LPARAM)&c);
}
static void cell(HWND list,int row,int col,const wchar_t *text){
    LVITEMW it; memset(&it,0,sizeof it); it.mask=LVIF_TEXT; it.iItem=row; it.iSubItem=col; it.pszText=(wchar_t*)text;
    SendMessageW(list,col?LVM_SETITEMW:LVM_INSERTITEMW,0,(LPARAM)&it);
}
/* rc_startup(frame): offers what a run that ended abnormally left behind. Later keeps it for the next start. */
void rc_startup(HWND frame){
    if(g_batch)return;
    item_count=rc_scan(items,RC_CAP);
    if(item_count<=0)return;
    HINSTANCE instance=GetModuleHandleW(NULL);
    WNDCLASSW cls; memset(&cls,0,sizeof cls);
    cls.lpfnWndProc=recover_proc; cls.hInstance=instance; cls.hCursor=LoadCursorW(NULL,IDC_ARROW);
    cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); cls.lpszClassName=L"BCRecover";
    if(!RegisterClassW(&cls)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return;
    RECT area; if(!frame||!GetWindowRect(frame,&area))SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);
    int width=680,height=380;
    HWND h=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_DLGMODALFRAME,L"BCRecover",L"저장하지 않은 내용 복구",WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_CLIPCHILDREN,
        area.left+(area.right-area.left-width)/2,area.top+(area.bottom-area.top-height)/2,width,height,frame,NULL,instance,NULL);
    if(!h)return;
    child(h,L"STATIC",L"이전 실행이 비정상 종료되어 저장하지 못한 내용이 남아 있습니다.\n복구하면 보관된 내용을 현재 파일과 나란히 엽니다. 파일은 바뀌지 않습니다.",0,RC_TEXT);
    HWND list=child(h,L"SysListView32",L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SHOWSELALWAYS|LVS_NOSORTHEADER,RC_LIST);
    SendMessageW(list,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_CHECKBOXES|LVS_EX_FULLROWSELECT);
    column(list,0,L"파일",190); column(list,1,L"폴더",310); column(list,2,L"기록 시각",130);
    for(int i=0;i<item_count;i++){
        wchar_t folder[520],when[40]=L""; const wchar_t *name=L"(이름 없음)";
        wcscpy(folder,items[i].path);
        wchar_t *slash=wcsrchr(folder,L'\\');
        if(slash){ *slash=0; name=slash+1; } else if(folder[0]){ name=items[i].path; folder[0]=0; }
        FILETIME t={(DWORD)items[i].time,(DWORD)(items[i].time>>32)},local; SYSTEMTIME st;
        if(items[i].time&&FileTimeToLocalFileTime(&t,&local)&&FileTimeToSystemTime(&local,&st))
            swprintf(when,40,L"%04u-%02u-%02u %02u:%02u",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute);
        cell(list,i,0,name); cell(list,i,1,slash?folder:L""); cell(list,i,2,when);
        LVITEMW it; memset(&it,0,sizeof it); it.stateMask=LVIS_STATEIMAGEMASK; it.state=2<<12;
        SendMessageW(list,LVM_SETITEMSTATE,i,(LPARAM)&it);
    }
    child(h,L"BUTTON",L"복구",WS_TABSTOP|BS_DEFPUSHBUTTON,RC_RESTORE);
    child(h,L"BUTTON",L"버리기",WS_TABSTOP|BS_PUSHBUTTON,RC_DISCARD);
    child(h,L"BUTTON",L"나중에",WS_TABSTOP|BS_PUSHBUTTON,RC_LATER);
    layout(h);
    action=0;
    if(frame)EnableWindow(frame,FALSE);
    ShowWindow(h,SW_SHOW); SetFocus(list);
    MSG m; int quit=0;
    while(!action){
        int got=GetMessageW(&m,NULL,0,0);
        if(got<=0){ quit=1; action=RC_LATER; break; }
        if(!IsDialogMessageW(h,&m)){ TranslateMessage(&m); DispatchMessageW(&m); }
    }
    if(frame)EnableWindow(frame,TRUE);
    DestroyWindow(h);
    if(quit)PostQuitMessage((int)m.wParam);
    for(int i=0;i<item_count;i++){
        if(!picked[i])continue;
        if(action==RC_RESTORE)rc_restore(&items[i]);
        else if(action==RC_DISCARD)rc_discard(&items[i]);
    }
}
