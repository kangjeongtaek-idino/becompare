; fjournal.asm - a journal of file operations, so that copy / move / delete / sync / folder merge can be
;   taken back with Ctrl+Z and done again with Ctrl+Y
;
;   The operations themselves are not changed. Right BEFORE one runs, the view tells the journal what is
;   about to happen:
;       fj_file(j, path)      a file is about to be written, overwritten or deleted. When it exists, a copy
;                             goes into the stash folder of the journal ("<n>.b", the state before).
;       fj_dirs(j, dir, drop) folders are about to be created: every folder of the path that is missing.
;       fj_rmdir(j, dir)      a folder is about to be removed.
;       fj_moved(j, a, b)     a was renamed to b.
;       fj_tree / fj_lists    the same for whole trees handed to the shell (copy, move, delete).
;   Undo walks the records backwards: what is at the path now moves into the stash ("<n>.a", the state after)
;   and the copy from before moves back. Redo walks forward and swaps them again. Nothing is deleted for good
;   until the journal is discarded - then its stash folder goes.
;
;   The stash is %TEMP%\BeCompareAsm\<process id>_x\undo_<id>\ (removed with the other temporary files when
;   the application ends). A journal backs up at most FJ_BUDGET bytes; files beyond that are marked "missed"
;   and are left as they are by undo.
;
;   A journal is one step on an undo stack of undo.asm (tag -1, see fj_commit / fj_step).
%include "inc/macros.inc"
%include "inc/win.inc"

extern calloc, realloc, free, wcslen, wcscpy, wcscat, wcsncpy, wcsrchr, _snwprintf
extern GetFileAttributesW, GetFileAttributesExW, SetFileAttributesW, CopyFileW, MoveFileExW, DeleteFileW
extern CreateDirectoryW, RemoveDirectoryW, SHCreateDirectoryExW, FindFirstFileW, FindNextFileW, FindClose
extern LoadCursorW, SetCursor
extern str_dup, msgbox, tc_tmp, us_push, us_clear

global fj_begin, fj_file, fj_dirs, fj_rmdir, fj_moved, fj_tree, fj_lists, fj_commit, fj_step, fj_discard

%define FJ_BUDGET 0x80000000         ; 2 GB of backups in one journal

; journal
%define J_REC    0                   ; records
%define J_N      8
%define J_CAP    16
%define J_NEXT   24                  ; next stash number
%define J_USED   32                  ; bytes backed up
%define J_MISS   40                  ; files that existed but were not backed up
%define J_DIR    48                  ; wchar[520]: stash folder -> 1088
%define J_P1     1088                ; wchar[1100]: tree walk, source path -> 3288
%define J_P2     3288                ; wchar[1100]: tree walk, destination path -> 5488
%define J_SIZE   5488

; record (32 bytes)
%define R_TYPE   0
%define R_FLAGS  4
%define R_N      8                   ; stash number
%define R_P1     16
%define R_P2     24

%define T_FILE   0                   ; the content at P1 changes (created, overwritten, deleted)
%define T_MKDIR  1                   ; folder P1 is created
%define T_RMDIR  2                   ; folder P1 is removed
%define T_MOVE   3                   ; P1 is moved to P2

%define F_HAD    1                   ; the file existed before and "<n>.b" holds it
%define F_AFTER  2                   ; "<n>.a" holds the state after (set by undo)
%define F_MISSED 4                   ; the file existed before but could not be backed up

%define M_COPY   1
%define M_MOVE   2
%define M_DELETE 3

section .rdata
WSTR f_fj_dir, "undo_%X"
WSTR f_fj_sp, "%s\%llX.%c"
WSTR f_fj_all, "%s\*"
WSTR f_fj_one, "%s\%s"
WSTR f_fj_undo, "파일 작업을 되돌립니다. 이 작업으로 바뀐 파일·폴더 %llu건을 실행 전 상태로 돌립니다."
WSTR f_fj_redo, "파일 작업을 다시 실행합니다. 파일·폴더 %llu건을 실행 후 상태로 돌립니다."
WSTR f_fj_miss, " 백업하지 못한 파일 %llu개는 그대로 둡니다."
WSTR s_fj_ask, " 계속할까요?"
WSTR f_fj_fail, "%u건은 처리하지 못했습니다(사용 중이거나 권한이 없는 항목)."

section .bss
g_fj_id: resq 1

section .text

; fj_begin() -> rax journal, 0 when it cannot be made (every fj_ function accepts 0 and does nothing)
fj_begin:
    PROLOG 64
    invoke calloc, 1, J_SIZE
    test rax, rax
    jz .d
    mov rbx, rax
    mov eax, 1
    lock xadd [g_fj_id], rax
    lea r12, [rax+1]
    invoke tc_tmp, &rbx+J_DIR, &f_fj_dir, r12
    invoke CreateDirectoryW, &rbx+J_DIR, 0
    mov rax, rbx
.d:
    EPILOG 64

; fj_add(j, type, p1, p2 or 0) -> rax record, 0 out of memory
fj_add:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    mov rsi, r8
    mov rdi, r9
    mov rax, [rbx+J_N]
    cmp rax, [rbx+J_CAP]
    jb .room
    lea r13, [rax*2+64]
    mov rdx, r13
    shl rdx, 5
    invoke realloc, [rbx+J_REC], rdx
    test rax, rax
    jz .fail
    mov [rbx+J_REC], rax
    mov [rbx+J_CAP], r13
.room:
    mov r14, [rbx+J_N]
    shl r14, 5
    add r14, [rbx+J_REC]
    mov [r14+R_TYPE], r12d
    mov dword [r14+R_FLAGS], 0
    mov qword [r14+R_N], 0
    mov qword [r14+R_P2], 0
    invoke str_dup, rsi
    mov [r14+R_P1], rax
    test rdi, rdi
    jz .np
    invoke str_dup, rdi
    mov [r14+R_P2], rax
.np:
    inc qword [rbx+J_N]
    mov rax, r14
    EPILOG 64
.fail:
    xor eax, eax
    EPILOG 64

; fj_sp(j, n, letter, out wchar[720]): path of a stash file
fj_sp:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov edi, r8d
    mov r12, r9
    lea r13, [rbx+J_DIR]
    invoke _snwprintf, r12, 700, &f_fj_sp, r13, rsi, rdi
    mov word [r12+1400], 0
    EPILOG 64

; fj_file(j, path): the file at path is about to be written, overwritten or deleted
%define FF_FAD   96                  ; WIN32_FILE_ATTRIBUTE_DATA (36) -> 136
%define FF_SP    144                 ; wchar[720] -> 1584
fj_file:
    PROLOG 1600
    test rcx, rcx
    jz .d
    mov rbx, rcx
    mov rsi, rdx
    invoke GetFileAttributesW, rsi
    mov r12d, eax
    cmp eax, -1
    je .rec
    test eax, 0x10                   ; a folder: nothing to keep
    jnz .d
.rec:
    invoke fj_add, rbx, T_FILE, rsi, 0
    test rax, rax
    jz .d
    mov r13, rax
    mov rax, [rbx+J_NEXT]
    inc qword [rbx+J_NEXT]
    mov [r13+R_N], rax
    cmp r12d, -1
    je .d                            ; a new file: nothing to back up
    invoke GetFileAttributesExW, rsi, 0, &rsp+FF_FAD
    test eax, eax
    jz .miss
    mov eax, [rsp+FF_FAD+32]
    mov edx, [rsp+FF_FAD+28]
    shl rdx, 32
    or rax, rdx
    mov r14, rax
    add rax, [rbx+J_USED]
    mov rcx, FJ_BUDGET
    cmp rax, rcx
    ja .miss
    invoke fj_sp, rbx, [r13+R_N], 'b', &rsp+FF_SP
    invoke CopyFileW, rsi, &rsp+FF_SP, 0
    test eax, eax
    jz .miss
    add [rbx+J_USED], r14
    or dword [r13+R_FLAGS], F_HAD
    jmp .d
.miss:
    or dword [r13+R_FLAGS], F_MISSED
    inc qword [rbx+J_MISS]
.d:
    EPILOG 1600

; fj_dirs(j, path, drop 0 / 1): folders are about to be created - records every folder of the path that does
;   not exist yet, outermost first. With drop = 1 the path names a file and its folder is meant.
fj_dirs:
    PROLOG 2304
    test rcx, rcx
    jz .d
    mov rbx, rcx
    mov r12d, r8d
    invoke wcsncpy, &rsp+96, rdx, 1090
    mov word [rsp+96+2180], 0
    lea rsi, [rsp+96]
    test r12d, r12d
    jz .keep
    invoke wcsrchr, rsi, 0x5C
    test rax, rax
    jz .d
    mov word [rax], 0
.keep:
    invoke wcslen, rsi
    test rax, rax
    jz .d
    cmp word [rsi+rax*2-2], 0x5C
    jne .nt
    mov word [rsi+rax*2-2], 0
.nt:
    mov edi, 3                       ; behind "C:\"
    cmp word [rsi], 0x5C
    jne .scan
    cmp word [rsi+2], 0x5C
    jne .scan
    mov edi, 2                       ; \\server\share\ : behind the share
    xor ecx, ecx
.u:
    movzx eax, word [rsi+rdi*2]
    test eax, eax
    jz .d
    inc edi
    cmp eax, 0x5C
    jne .u
    inc ecx
    cmp ecx, 2
    jb .u
.scan:
    invoke wcslen, rsi
    cmp rdi, rax
    jae .d
.c:
    movzx eax, word [rsi+rdi*2]
    test eax, eax
    jz .chk
    cmp eax, 0x5C
    je .chk
    inc rdi
    jmp .c
.chk:
    mov r14d, eax
    mov word [rsi+rdi*2], 0
    invoke GetFileAttributesW, rsi
    cmp eax, -1
    jne .ex
    invoke fj_add, rbx, T_MKDIR, rsi, 0
.ex:
    mov [rsi+rdi*2], r14w
    test r14d, r14d
    jz .d
    inc rdi
    jmp .c
.d:
    EPILOG 2304

; fj_rmdir(j, dir): the folder is about to be removed
fj_rmdir:
    PROLOG 64
    test rcx, rcx
    jz .d
    mov r8, rdx
    mov edx, T_RMDIR
    xor r9d, r9d
    call fj_add
.d:
    EPILOG 64

; fj_moved(j, from, to): from was renamed to to
fj_moved:
    PROLOG 64
    test rcx, rcx
    jz .d
    mov r9, r8
    mov r8, rdx
    mov edx, T_MOVE
    call fj_add
.d:
    EPILOG 64

; fj_walk(j, mode): records for the tree J_P1 (-> J_P2). Both buffers are extended in place.
%define FW_FD    96                  ; WIN32_FIND_DATAW (592) -> 688
fj_walk:
    PROLOG 704
    mov rbx, rcx
    mov r12d, edx
    lea rsi, [rbx+J_P1]
    lea rdi, [rbx+J_P2]
    invoke GetFileAttributesW, rsi
    cmp eax, -1
    je .d
    test eax, 0x10
    jnz .dir
    cmp r12d, M_DELETE
    jne .cf
    invoke fj_file, rbx, rsi
    jmp .d
.cf:
    invoke fj_file, rbx, rdi
    cmp r12d, M_MOVE
    jne .d
    invoke fj_add, rbx, T_MOVE, rsi, rdi
    jmp .d
.dir:
    test eax, 0x400                  ; a junction or a link to a folder: left alone
    jnz .d
    cmp r12d, M_DELETE
    je .nomk
    invoke GetFileAttributesW, rdi
    cmp eax, -1
    jne .nomk
    invoke fj_add, rbx, T_MKDIR, rdi, 0
.nomk:
    invoke wcslen, rsi
    mov r13, rax
    invoke wcslen, rdi
    mov r14, rax
    cmp r13, 1000
    jae .post
    cmp r14, 1000
    jae .post
    mov dword [rsi+r13*2], 0x002A005C     ; "\*"
    mov word [rsi+r13*2+4], 0
    invoke FindFirstFileW, rsi, &rsp+FW_FD
    mov word [rsi+r13*2], 0
    cmp rax, -1
    je .post
    mov r15, rax
.f:
    lea rbp, [rsp+FW_FD+44]
    cmp word [rbp], '.'
    jne .use
    cmp word [rbp+2], 0
    je .nx
    cmp word [rbp+2], '.'
    jne .use
    cmp word [rbp+4], 0
    je .nx
.use:
    invoke wcslen, rbp
    lea rcx, [rax+r13]
    cmp rcx, 1080
    jae .nx
    lea rcx, [rax+r14]
    cmp rcx, 1080
    jae .nx
    mov word [rsi+r13*2], 0x5C
    lea rcx, [rsi+r13*2+2]
    mov rdx, rbp
    call wcscpy
    cmp r12d, M_DELETE
    je .rec
    mov word [rdi+r14*2], 0x5C
    lea rcx, [rdi+r14*2+2]
    mov rdx, rbp
    call wcscpy
.rec:
    invoke fj_walk, rbx, r12
    mov word [rsi+r13*2], 0
    mov word [rdi+r14*2], 0
.nx:
    invoke FindNextFileW, r15, &rsp+FW_FD
    test eax, eax
    jnz .f
    invoke FindClose, r15
.post:
    cmp r12d, M_COPY
    je .d
    invoke fj_add, rbx, T_RMDIR, rsi, 0   ; after its content: redo removes it last, undo makes it first
.d:
    EPILOG 704

; fj_tree(j, src, dst or 0, mode 1 copy / 2 move / 3 delete): src (a file or a folder with everything in it)
;   is about to be copied / moved to dst, or deleted
fj_tree:
    PROLOG 64
    test rcx, rcx
    jz .d
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12d, r9d
    invoke wcsncpy, &rbx+J_P1, rsi, 1090
    mov word [rbx+J_P1+2180], 0
    mov word [rbx+J_P2], 0
    test rdi, rdi
    jz .w
    invoke wcsncpy, &rbx+J_P2, rdi, 1090
    mov word [rbx+J_P2+2180], 0
    invoke fj_dirs, rbx, rdi, 1
.w:
    invoke fj_walk, rbx, r12
.d:
    EPILOG 64

; fj_lists(j, sources, destinations or 0, mode): the double-NUL lists that are handed to SHFileOperation
fj_lists:
    PROLOG 64
    test rcx, rcx
    jz .d
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12d, r9d
.l:
    cmp word [rsi], 0
    je .d
    invoke fj_tree, rbx, rsi, rdi, r12
    invoke wcslen, rsi
    lea rsi, [rsi+rax*2+2]
    test rdi, rdi
    jz .l
    invoke wcslen, rdi
    lea rdi, [rdi+rax*2+2]
    jmp .l
.d:
    EPILOG 64

; fj_apply(j, redo 0 / 1) -> eax number of records that could not be processed
%define FA_SP    96                  ; wchar[720] -> 1536
fj_apply:
    PROLOG 1600
    mov rbx, rcx
    mov r12d, edx
    xor r15d, r15d
    mov r13, [rbx+J_N]
    xor r14d, r14d
    test r12d, r12d
    jnz .loop
    lea r14, [r13-1]
.loop:
    cmp r14, r13
    jae .done
    mov rsi, r14
    shl rsi, 5
    add rsi, [rbx+J_REC]
    mov eax, [rsi+R_TYPE]
    cmp eax, T_FILE
    je .file
    cmp eax, T_MKDIR
    je .mk
    cmp eax, T_RMDIR
    je .rm
    jmp .mv
.fail:
    inc r15d
.next:
    test r12d, r12d
    jz .dec
    inc r14
    jmp .loop
.dec:
    dec r14
    jmp .loop

.file:
    test dword [rsi+R_FLAGS], F_MISSED
    jnz .next
    test r12d, r12d
    jnz .fredo
    and dword [rsi+R_FLAGS], ~F_AFTER
    invoke GetFileAttributesW, [rsi+R_P1]
    cmp eax, -1
    je .frest
    test eax, 0x10
    jnz .frest
    invoke fj_sp, rbx, [rsi+R_N], 'a', &rsp+FA_SP
    invoke MoveFileExW, [rsi+R_P1], &rsp+FA_SP, 3    ; replace existing, copy allowed
    test eax, eax
    jz .fail
    or dword [rsi+R_FLAGS], F_AFTER
.frest:
    test dword [rsi+R_FLAGS], F_HAD
    jz .next
    invoke fj_sp, rbx, [rsi+R_N], 'b', &rsp+FA_SP
    invoke MoveFileExW, &rsp+FA_SP, [rsi+R_P1], 3
    test eax, eax
    jz .fail
    jmp .next
.fredo:
    test dword [rsi+R_FLAGS], F_HAD
    jz .fr2
    invoke fj_sp, rbx, [rsi+R_N], 'b', &rsp+FA_SP
    invoke MoveFileExW, [rsi+R_P1], &rsp+FA_SP, 3
.fr2:
    test dword [rsi+R_FLAGS], F_AFTER
    jz .next
    invoke fj_sp, rbx, [rsi+R_N], 'a', &rsp+FA_SP
    invoke MoveFileExW, &rsp+FA_SP, [rsi+R_P1], 3
    test eax, eax
    jz .fail
    and dword [rsi+R_FLAGS], ~F_AFTER
    jmp .next

.mk:
    test r12d, r12d
    jnz .mkdir
.rmdir:
    invoke RemoveDirectoryW, [rsi+R_P1]              ; fails when something else is in it: it stays
    jmp .next
.rm:
    test r12d, r12d
    jnz .rmdir
.mkdir:
    invoke SHCreateDirectoryExW, 0, [rsi+R_P1], 0
    jmp .next

.mv:
    mov rdi, [rsi+R_P1]              ; undo: P2 goes back to P1
    mov rbp, [rsi+R_P2]
    test r12d, r12d
    jz .mvgo
    xchg rdi, rbp
.mvgo:                               ; rbp -> rdi, only when it really is where the operation left it
    invoke GetFileAttributesW, rdi
    cmp eax, -1
    jne .next
    invoke GetFileAttributesW, rbp
    cmp eax, -1
    je .next
    invoke wcsncpy, &rsp+FA_SP, rdi, 700
    mov word [rsp+FA_SP+1400], 0
    invoke wcsrchr, &rsp+FA_SP, 0x5C
    test rax, rax
    jz .mvdo
    mov word [rax], 0
    invoke SHCreateDirectoryExW, 0, &rsp+FA_SP, 0
.mvdo:
    invoke MoveFileExW, rbp, rdi, 2
    test eax, eax
    jz .fail
    jmp .next
.done:
    mov eax, r15d
    EPILOG 1600

; fj_discard(j): forget the journal - its backups are deleted, the files on disk stay as they are
%define FD_FD    96                  ; WIN32_FIND_DATAW (592) -> 688
%define FD_P     704                 ; wchar[700] -> 2104
fj_discard:
    PROLOG 2112
    test rcx, rcx
    jz .d
    mov rbx, rcx
    xor esi, esi
.r:
    cmp rsi, [rbx+J_N]
    jae .recs
    mov rdi, rsi
    shl rdi, 5
    add rdi, [rbx+J_REC]
    invoke free, [rdi+R_P1]
    invoke free, [rdi+R_P2]
    inc rsi
    jmp .r
.recs:
    invoke free, [rbx+J_REC]
    lea r12, [rbx+J_DIR]
    invoke _snwprintf, &rsp+FD_P, 690, &f_fj_all, r12
    invoke FindFirstFileW, &rsp+FD_P, &rsp+FD_FD
    cmp rax, -1
    je .nodir
    mov r13, rax
.f:
    test dword [rsp+FD_FD], 0x10
    jnz .nx
    lea r14, [rsp+FD_FD+44]
    invoke _snwprintf, &rsp+FD_P, 690, &f_fj_one, r12, r14
    invoke SetFileAttributesW, &rsp+FD_P, 0x80       ; a read only backup could not be deleted
    invoke DeleteFileW, &rsp+FD_P
.nx:
    invoke FindNextFileW, r13, &rsp+FD_FD
    test eax, eax
    jnz .f
    invoke FindClose, r13
.nodir:
    invoke RemoveDirectoryW, r12
    invoke free, rbx
.d:
    EPILOG 2112

; fj_commit(slot*, j): the operation is over - the journal becomes the newest undo step of the view (a new
;   operation empties the redo stack). A journal without records is dropped.
fj_commit:
    PROLOG 96
    test rdx, rdx
    jz .d
    mov rbx, rcx
    mov rsi, rdx
    cmp qword [rsi+J_N], 0
    jne .push
    invoke fj_discard, rsi
    jmp .d
.push:
    invoke us_clear, rbx, 1
    mov qword [rsp+64], -1
    mov [rsp+72], rsi
    mov qword [rsp+80], 0
    invoke us_push, rbx, 0, &rsp+64
.d:
    EPILOG 96

; fj_step(hwnd, slot*, step*, redo 0 / 1) -> eax 1 done, 0 the user said no (the step is back on its stack)
;   step is a journal step that was just taken off the undo (redo = 0) or redo (redo = 1) stack.
%define FS_MSG   96                  ; wchar[340] -> 776
fj_step:
    PROLOG 800
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, r8
    mov r12d, r9d
    mov r13, [rdi+8]
    lea r14, [f_fj_undo]
    test r12d, r12d
    jz .fmt
    lea r14, [f_fj_redo]
.fmt:
    invoke _snwprintf, &rsp+FS_MSG, 200, r14, [r13+J_N]
    mov word [rsp+FS_MSG+400], 0
    cmp qword [r13+J_MISS], 0
    je .ask
    invoke wcslen, &rsp+FS_MSG
    lea r14, [rsp+FS_MSG+rax*2]
    invoke _snwprintf, r14, 100, &f_fj_miss, [r13+J_MISS]
    mov word [r14+200], 0
.ask:
    invoke wcscat, &rsp+FS_MSG, &s_fj_ask
    invoke msgbox, rsi, &rsp+FS_MSG, MB_YESNO|MB_ICONWARNING
    cmp eax, IDYES
    je .go
    invoke us_push, rbx, r12, rdi
    xor eax, eax
    EPILOG 800
.go:
    invoke LoadCursorW, 0, 32514     ; IDC_WAIT
    invoke SetCursor, rax
    mov r14, rax
    invoke fj_apply, r13, r12
    mov r15d, eax
    invoke SetCursor, r14
    mov r14d, r12d
    xor r14d, 1
    invoke us_push, rbx, r14, rdi
    test r15d, r15d
    jz .ok
    invoke _snwprintf, &rsp+FS_MSG, 200, &f_fj_fail, r15
    mov word [rsp+FS_MSG+400], 0
    invoke msgbox, rsi, &rsp+FS_MSG, MB_OK|MB_ICONWARNING
.ok:
    mov eax, 1
    EPILOG 800
