; lhist.asm - local history (IntelliJ "Local History"): what a file held before this program saved over it
;
;   Before a text view writes a file (text compare, the editor, the merge output), the file as it is on disk
;   is copied to  <settings folder>\history\<hash of the path>\<last write time>.bak . The name is the time
;   the content was written, so the same content is kept once. The newest LH_MAX versions of a file stay,
;   older ones are deleted; a file of more than LH_BIG bytes is not kept. Nothing is recorded in an
;   unattended run.
;   lh_show lists the versions of a file in a menu (newest first); the chosen one is copied to the temporary
;   folder of this process and opened in a text compare against the file as it is now.
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern g_ini, g_batch, msgbox, add_session, tc_tmp
extern malloc, free, wcscpy, wcsrchr, wcslen, _snwprintf, _wcstoui64
extern CreateDirectoryW, GetFileAttributesExW, CopyFileW, DeleteFileW, FindFirstFileW, FindNextFileW, FindClose
extern CharLowerBuffW, CreatePopupMenu, AppendMenuW, TrackPopupMenu, DestroyMenu, GetCursorPos
extern FileTimeToLocalFileTime, FileTimeToSystemTime, GetTickCount

global lh_keep, lh_show

%define LH_MAX  30                   ; versions kept per file
%define LH_CAP  256                  ; versions looked at
%define LH_BIG  0x1000000            ; 16 MB: larger files are not recorded

; work area of lh_show (one allocation)
%define LS_DIR  0                    ; wchar[600]
%define LS_LIST 1200                 ; qword[LH_CAP]
%define LS_SRC  3248                 ; wchar[650]
%define LS_TMP  4548                 ; wchar[650]
%define LS_TXT  5848                 ; wchar[100]
%define LS_FAD  6048                 ; WIN32_FILE_ATTRIBUTE_DATA (36)
%define LS_FT   6096                 ; FILETIME, local FILETIME
%define LS_LFT  6104
%define LS_ST   6112                 ; SYSTEMTIME (16)
%define LS_PT   6128                 ; POINT
%define LS_SIZE 8192

section .rdata
WSTR s_lh_hist, "history"
WSTR f_lh_s, "%s"
WSTR f_lh_sub, "\%016llX"
WSTR f_lh_file, "%s\%016llX.bak"
WSTR f_lh_pat, "%s\*.bak"
WSTR f_lh_item, "%04u-%02u-%02u %02u:%02u:%02u    %u KB"
WSTR f_lh_tmp, "기록%X_"
WSTR s_lh_none, "이 파일의 로컬 기록이 없습니다. 이 프로그램에서 파일을 저장할 때마다 저장 직전의 내용이 기록됩니다."
WSTR s_lh_fail, "기록된 내용을 임시 파일로 꺼낼 수 없습니다."

section .text

; lh_dir(path, out wchar[600]) -> eax 1: the folder of the versions of that file (created when missing)
lh_dir:
    PROLOG 1136
    mov rsi, rcx
    mov rdi, rdx
    invoke _snwprintf, &rsp+64, 519, &f_lh_s, rsi
    mov word [rsp+64+1038], 0
    invoke wcslen, &rsp+64
    mov r12, rax
    invoke CharLowerBuffW, &rsp+64, r12             ; the same file whatever the case it was typed in
    mov rax, 0xCBF29CE484222325                     ; FNV-1a
    mov r8, 0x100000001B3
    xor ecx, ecx
.h:
    cmp rcx, r12
    jae .hd
    movzx edx, word [rsp+64+rcx*2]
    xor rax, rdx
    imul rax, r8
    inc rcx
    jmp .h
.hd:
    mov r13, rax
    invoke wcscpy, rdi, &g_ini
    invoke wcsrchr, rdi, 0x5C
    test rax, rax
    jz .no
    lea rcx, [rax+2]
    invoke _snwprintf, rcx, 20, &f_lh_s, &s_lh_hist
    invoke CreateDirectoryW, rdi, 0
    invoke wcslen, rdi
    lea rcx, [rdi+rax*2]
    invoke _snwprintf, rcx, 20, &f_lh_sub, r13
    invoke CreateDirectoryW, rdi, 0
    mov eax, 1
    EPILOG 1136
.no:
    xor eax, eax
    EPILOG 1136

; lh_list(folder, out qword[LH_CAP]) -> eax count: the write times of the versions, newest first
lh_list:
    PROLOG 1968
    mov rsi, rcx
    mov rdi, rdx
    xor ebx, ebx
    invoke _snwprintf, &rsp+64, 640, &f_lh_pat, rsi
    mov word [rsp+64+1280], 0
    invoke FindFirstFileW, &rsp+64, &rsp+1368
    cmp rax, -1
    je .d
    mov r12, rax
.l:
    test dword [rsp+1368], 0x10
    jnz .n
    invoke _wcstoui64, &rsp+1368+44, 0, 16          ; the name is the time, in hexadecimal
    test rax, rax
    jz .n
    cmp ebx, LH_CAP
    jae .n
    mov ecx, ebx                     ; kept in order: newest first
.ins:
    test ecx, ecx
    jz .put
    cmp [rdi+rcx*8-8], rax
    jae .put
    mov rdx, [rdi+rcx*8-8]
    mov [rdi+rcx*8], rdx
    dec ecx
    jmp .ins
.put:
    mov [rdi+rcx*8], rax
    inc ebx
.n:
    invoke FindNextFileW, r12, &rsp+1368
    test eax, eax
    jnz .l
    invoke FindClose, r12
.d:
    mov eax, ebx
    EPILOG 1968

; lh_prune(folder): only the newest LH_MAX versions stay
lh_prune:
    PROLOG 3424
    mov rsi, rcx
    invoke lh_list, rsi, &rsp+64
    mov r12d, eax
    mov ebx, LH_MAX
.l:
    cmp ebx, r12d
    jae .d
    mov r13, [rsp+64+rbx*8]
    invoke _snwprintf, &rsp+2112, 640, &f_lh_file, rsi, r13
    mov word [rsp+2112+1280], 0
    invoke DeleteFileW, &rsp+2112
    inc ebx
    jmp .l
.d:
    EPILOG 3424

; lh_keep(path): the file is about to be written - keep what it holds now
lh_keep:
    PROLOG 2640
    mov rsi, rcx
    cmp qword [g_batch], 0
    jne .d
    cmp word [rsi], 0
    je .d
    invoke GetFileAttributesExW, rsi, 0, &rsp+64
    test eax, eax
    jz .d                            ; a new file: nothing to keep
    test dword [rsp+64], 0x10
    jnz .d
    cmp dword [rsp+64+28], 0
    jne .d
    cmp dword [rsp+64+32], LH_BIG
    ja .d
    invoke lh_dir, rsi, &rsp+112
    test eax, eax
    jz .d
    mov r12, [rsp+64+20]             ; last write time
    invoke _snwprintf, &rsp+1312, 640, &f_lh_file, &rsp+112, r12
    mov word [rsp+1312+1280], 0
    invoke CopyFileW, rsi, &rsp+1312, 1             ; that version is there already: fine
    invoke lh_prune, &rsp+112
.d:
    EPILOG 2640

; lh_show(hwnd, path): the menu of the versions; the chosen one is compared with the file
lh_show:
    PROLOG 192
    mov rsi, rcx
    mov rdi, rdx
    xor r15d, r15d
    cmp word [rdi], 0
    je .none
    invoke malloc, LS_SIZE
    mov r15, rax
    invoke lh_dir, rdi, r15
    test eax, eax
    jz .none
    invoke lh_list, r15, &r15+LS_LIST
    mov r12d, eax
    test eax, eax
    jz .none
    cmp r12d, LH_MAX
    jbe .menu
    mov r12d, LH_MAX
.menu:
    call CreatePopupMenu
    mov r13, rax
    xor ebx, ebx
.m:
    cmp ebx, r12d
    jae .track
    mov r14, [r15+LS_LIST+rbx*8]
    mov [r15+LS_FT], r14
    invoke FileTimeToLocalFileTime, &r15+LS_FT, &r15+LS_LFT
    invoke FileTimeToSystemTime, &r15+LS_LFT, &r15+LS_ST
    invoke _snwprintf, &r15+LS_SRC, 640, &f_lh_file, r15, r14
    mov word [r15+LS_SRC+1280], 0
    mov dword [r15+LS_FAD+32], 0
    invoke GetFileAttributesExW, &r15+LS_SRC, 0, &r15+LS_FAD
    mov eax, [r15+LS_FAD+32]
    add eax, 1023
    shr eax, 10
    mov [rsp+120], eax               ; size in KB
    movzx eax, word [r15+LS_ST]
    mov [rsp+96], eax
    movzx eax, word [r15+LS_ST+2]
    mov [rsp+100], eax
    movzx eax, word [r15+LS_ST+6]
    mov [rsp+104], eax
    movzx eax, word [r15+LS_ST+8]
    mov [rsp+108], eax
    movzx eax, word [r15+LS_ST+10]
    mov [rsp+112], eax
    movzx eax, word [r15+LS_ST+12]
    mov [rsp+116], eax
    invoke _snwprintf, &r15+LS_TXT, 90, &f_lh_item, #[rsp+96], #[rsp+100], #[rsp+104], #[rsp+108], #[rsp+112], #[rsp+116], #[rsp+120]
    mov word [r15+LS_TXT+180], 0
    lea r14, [rbx+1]
    invoke AppendMenuW, r13, MF_STRING, r14, &r15+LS_TXT
    inc ebx
    jmp .m
.track:
    invoke GetCursorPos, &r15+LS_PT
    invoke TrackPopupMenu, r13, TPM_RETURNCMD, #[r15+LS_PT], #[r15+LS_PT+4], 0, rsi, 0
    mov ebx, eax
    invoke DestroyMenu, r13
    test ebx, ebx
    jz .d
    cmp ebx, r12d
    ja .d
    ; a copy of that version in the temporary folder (the history itself is not opened for editing)
    mov r14, [r15+LS_LIST+rbx*8-8]
    invoke _snwprintf, &r15+LS_SRC, 640, &f_lh_file, r15, r14
    mov word [r15+LS_SRC+1280], 0
    call GetTickCount
    invoke tc_tmp, &r15+LS_TMP, &f_lh_tmp, rax
    invoke wcsrchr, rdi, 0x5C        ; ... under the name of the file, so that its type is recognised
    lea r14, [rax+2]
    test rax, rax
    jnz .base
    mov r14, rdi
.base:
    invoke wcslen, &r15+LS_TMP
    lea rbx, [r15+LS_TMP+rax*2]
    invoke _snwprintf, rbx, 100, &f_lh_s, r14
    mov word [rbx+200], 0
    invoke CopyFileW, &r15+LS_SRC, &r15+LS_TMP, 0
    test eax, eax
    jz .fail
    invoke add_session, T_TEXT, &r15+LS_TMP, rdi, 0
    jmp .d
.fail:
    invoke msgbox, rsi, &s_lh_fail, MB_OK|MB_ICONWARNING
    jmp .d
.none:
    invoke msgbox, rsi, &s_lh_none, MB_OK|MB_ICONINFO
.d:
    invoke free, r15
    EPILOG 192
