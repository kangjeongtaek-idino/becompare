; find.asm - file name index of the fixed drives, search, sort (Home tab "파일 찾기")
;
;   The first search starts a background thread that lists every file and folder of the fixed drives into
;   memory: name, folder, modification time and size. Searches and sorts then work on that list.
;   An NTFS volume is listed in one of three ways, tried in this order:
;     1. its master file table is read from the volume and parsed (needs administrator rights; a few
;        seconds per drive; gives names, times and sizes)
;     2. the file table is enumerated with FSCTL_ENUM_USN_DATA (also administrator; names only: time and
;        size stay 0)
;     3. the folders are walked with FindFirstFileExW (no special rights; much slower the first time)
;   Other file systems are always walked.
;   The index is not kept up to date afterwards: it shows the drives as they were when it was built.
;
;   Environment (tests and trouble shooting):
;     BC_FIND_ROOT   what to list instead of all fixed drives: entries separated by ';', each "X:" (that
;                    drive) or a folder path (walked)
;     BC_FIND_MODE   "usn" skips way 1, "walk" skips ways 1 and 2
;     BC_FIND_CLOUD  folders that count as cloud storage (separated by ';') instead of the detected ones
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern VirtualAlloc, VirtualFree, CreateFileW, CloseHandle, DeviceIoControl, CreateThread
extern FindFirstFileExW, FindNextFileW, FindClose, GetLogicalDrives, GetDriveTypeW, GetEnvironmentVariableW
extern SetFilePointerEx, ReadFile, RegOpenKeyExW, RegEnumKeyExW, RegEnumValueW, RegCloseKey
extern CharLowerBuffW, memcpy, wcslen, malloc, realloc, free, qsort, wcscat, wcscpy

global find_start, find_query, find_query2, find_path, find_sort
global find_forget
global find_forget_path
extern _wcsicmp, wcsrchr, GetFileAttributesW, GetLastError
global find_time, find_size, find_flags, find_name, find_parent, find_volcount, find_volname
global fi_state, fi_vis, fi_mode, fi_cancel, fi_ncloud

; entry (32 bytes); id 0 means "none"
%define FI_PARENT 0                  ; dd id of the folder it is in
%define FI_NAME   4                  ; dd offset of the name in the pool, in characters
%define FI_LEN    8                  ; dw name length
%define FI_FLAGS  10                 ; dw 1 = folder, 2 = top of a listed tree (its name is a whole path)
%define FI_REC    12                 ; dd parent's file record number while a volume is being read
%define FI_TIME   16                 ; dq last modification (FILETIME), 0 = unknown
%define FI_BYTES  24                 ; dq size in bytes
%define FI_SHIFT  5
%define FI_DELETED 4                 ; removed through file search; keep ids/parents stable
%define FI_ESIZE  32
%define FI_MAXENT  0x1000000         ; 16M entries (512MB of address space, committed as it fills)
%define FI_MAXPOOL 0x40000000        ; 1G characters
%define FI_STEP    0x400000          ; commit step
%define FI_MAXNAME 1000
%define FI_MAXVOL  32
%define FI_MAXCLOUD 32
; listed tree ("volume"): { dd id of its top entry, dd first id of its contents, dd end id (0 = still growing), dd way }
%define FV_ROOT  0
%define FV_FIRST 4
%define FV_END   8
%define FV_WAY   12
%define FV_SIZE  16

section .rdata
WSTR s_env_root, "BC_FIND_ROOT"
WSTR s_env_mode, "BC_FIND_MODE"
WSTR s_env_cloud, "BC_FIND_CLOUD"
WSTR s_env_od1, "OneDrive"
WSTR s_env_od2, "OneDriveConsumer"
WSTR s_env_od3, "OneDriveCommercial"
WSTR s_reg_sync, "SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer\SyncRootManager"
WSTR s_reg_user, "\UserSyncRoots"

section .bss
alignb 8
fi_ent:     resq 1                   ; entries
fi_n:       resq 1                   ; entries written
fi_ent_c:   resq 1                   ; bytes committed
fi_pool:    resq 1                   ; names
fi_pool_n:  resq 1                   ; characters used
fi_pool_c:  resq 1                   ; bytes committed
fi_lower:   resq 1                   ; word[65536]: lower case of every character
fi_state:   resq 1                   ; 0 not started, 1 listing, 2 ready
fi_vis:     resq 1                   ; entries a search may look at (their parents are final)
fi_mode:    resq 1                   ; bit 0 file table parsed, bit 1 folders walked, bit 2 file table enumerated
fi_cancel:  resq 1
fi_deleted: resq 1                   ; ancestor checks are needed only after a deletion
fi_full:    resq 1                   ; ran out of room
fi_force:   resq 1                   ; 0 try every way, 1 no parsing, 2 walk only
fi_nvol:    resq 1
fi_ncloud:  resq 1
fi_vol:     resb FI_MAXVOL*FV_SIZE
fi_cloud:   resd FI_MAXCLOUD         ; ids of the cloud storage folders
fs_key:     resq 1                   ; sort in progress
fs_desc:    resq 1
fq_pat:     resw 260                 ; folder filter of the running search, lower case
fq_path:    resw 1100                ; folder path of the entry being checked
fs_bufa:    resw 1100                ; folder paths of the two entries being compared
fs_bufb:    resw 1100

section .text

; fi_init() -> eax 1 ok: reserve the two areas, build the lower case table
fi_init:
    PROLOG 64
    cmp qword [fi_ent], 0
    jne .ok
    invoke VirtualAlloc, 0, FI_MAXENT*FI_ESIZE, 0x2000, 4            ; MEM_RESERVE, PAGE_READWRITE
    test rax, rax
    jz .no
    mov rbx, rax
    mov rsi, FI_MAXPOOL*2
    invoke VirtualAlloc, 0, rsi, 0x2000, 4
    test rax, rax
    jz .no
    mov rdi, rax
    invoke VirtualAlloc, 0, 131072, 0x3000, 4                       ; committed
    test rax, rax
    jz .no
    mov r12, rax
    xor ecx, ecx
.t:
    mov [r12+rcx*2], cx
    inc ecx
    cmp ecx, 65536
    jb .t
    invoke CharLowerBuffW, &r12+2, 65535
    invoke VirtualAlloc, rbx, FI_STEP, 0x1000, 4                    ; MEM_COMMIT
    test rax, rax
    jz .no
    invoke VirtualAlloc, rdi, FI_STEP, 0x1000, 4
    test rax, rax
    jz .no
    mov qword [fi_ent_c], FI_STEP
    mov qword [fi_pool_c], FI_STEP
    mov qword [fi_n], 1              ; entry 0 and pool offset 0 are "none"
    mov qword [fi_pool_n], 1
    mov [fi_lower], r12
    mov [fi_pool], rdi
    mov [fi_ent], rbx
.ok:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; fi_add(parent id, name, length, flags, parent record number, time, size) -> eax id, 0 when not added
fi_add:
    PROLOG 64
    mov r12d, ecx
    mov rsi, rdx
    mov r13d, r8d
    mov r14d, r9d
    mov r15d, [rsp+64+112]
    test r13d, r13d
    jz .no
    cmp r13d, FI_MAXNAME
    ja .no
    mov rbx, [fi_n]
    cmp rbx, FI_MAXENT-1
    jae .full
    lea rax, [rbx+1]
    shl rax, FI_SHIFT
    cmp rax, [fi_ent_c]
    jbe .e_ok
    mov rdi, [fi_ent]
    add rdi, [fi_ent_c]
    invoke VirtualAlloc, rdi, FI_STEP, 0x1000, 4
    test rax, rax
    jz .full
    add qword [fi_ent_c], FI_STEP
.e_ok:
    mov rdi, [fi_pool_n]
    lea rax, [rdi+r13+1]
    cmp rax, FI_MAXPOOL-1
    jae .full
    shl rax, 1
    cmp rax, [fi_pool_c]
    jbe .p_ok
    mov rbp, [fi_pool]
    add rbp, [fi_pool_c]
    invoke VirtualAlloc, rbp, FI_STEP, 0x1000, 4
    test rax, rax
    jz .full
    add qword [fi_pool_c], FI_STEP
.p_ok:
    mov rbp, [fi_pool]
    lea rbp, [rbp+rdi*2]
    lea rax, [r13*2]
    invoke memcpy, rbp, rsi, rax
    mov word [rbp+r13*2], 0
    lea rax, [rdi+r13+1]
    mov [fi_pool_n], rax
    mov rax, rbx
    shl rax, FI_SHIFT
    add rax, [fi_ent]
    mov [rax+FI_PARENT], r12d
    mov [rax+FI_NAME], edi
    mov [rax+FI_LEN], r13w
    mov [rax+FI_FLAGS], r14w
    mov [rax+FI_REC], r15d
    mov rcx, [rsp+64+120]
    mov [rax+FI_TIME], rcx
    mov rcx, [rsp+64+128]
    mov [rax+FI_BYTES], rcx
    lea rax, [rbx+1]
    mov [fi_n], rax
    mov eax, ebx
    EPILOG 64
.full:
    mov qword [fi_full], 1
.no:
    xor eax, eax
    EPILOG 64

; ---------------------------------------------------------------- reading the index
; fi_e(ecx id) -> rax entry or 0  (keeps every other register)
fi_e:
    mov eax, ecx
    test eax, eax
    jz .no
    cmp rax, [fi_n]
    jae .no
    shl rax, FI_SHIFT
    add rax, [fi_ent]
    ret
.no:
    xor eax, eax
    ret

; find_time(id) -> rax FILETIME (0 unknown) / find_size(id) -> rax bytes / find_flags(id) -> eax
; find_parent(id) -> eax id of its folder
find_time:
    call fi_e
    test rax, rax
    jz .d
    mov rax, [rax+FI_TIME]
.d: ret
find_size:
    call fi_e
    test rax, rax
    jz .d
    mov rax, [rax+FI_BYTES]
.d: ret
find_flags:
    call fi_e
    test rax, rax
    jz .d
    movzx eax, word [rax+FI_FLAGS]
.d: ret
find_parent:
    call fi_e
    test rax, rax
    jz .d
    mov eax, [rax+FI_PARENT]
.d: ret

; find_name(id, out, capacity in characters) -> eax length (0 and an empty text when it does not fit)
find_name:
    call fi_e
    mov word [rdx], 0
    test rax, rax
    jz .no
    movzx r9d, word [rax+FI_LEN]
    cmp r9d, r8d
    jae .no
    mov r10d, [rax+FI_NAME]
    mov r11, [fi_pool]
    lea r11, [r11+r10*2]
    xor ecx, ecx
.c:
    cmp ecx, r9d
    jae .cd
    mov ax, [r11+rcx*2]
    mov [rdx+rcx*2], ax
    inc ecx
    jmp .c
.cd:
    mov word [rdx+rcx*2], 0
    mov eax, r9d
    ret
.no:
    xor eax, eax
    ret

; find_volcount() -> eax listed trees (drives); find_volname(index, out, capacity) -> eax length
find_volcount:
    mov rax, [fi_nvol]
    ret
find_volname:
    mov word [rdx], 0
    xor eax, eax
    cmp rcx, [fi_nvol]
    jae .d
    shl rcx, 4
    lea rax, [fi_vol]
    mov ecx, [rax+rcx+FV_ROOT]
    jmp find_name
.d: ret

; find_path(id, out, capacity in characters) -> eax length, 0 when the entry has no complete path
find_path:
    PROLOG 608
    mov ebx, ecx
    mov rdi, rdx
    mov r12d, r8d
    xor esi, esi                     ; depth
    mov r13, [fi_ent]
    test r13, r13
    jz .bad
.up:
    test ebx, ebx
    jz .bad
    cmp rbx, [fi_n]
    jae .bad
    cmp esi, 128
    jae .bad
    mov [rsp+96+rsi*4], ebx
    inc esi
    mov rax, rbx
    shl rax, FI_SHIFT
    test word [r13+rax+FI_FLAGS], 2
    jnz .top
    mov ebx, [r13+rax+FI_PARENT]
    jmp .up
.top:
    xor r14d, r14d                   ; characters written
.em:
    dec esi
    js .done
    mov ebx, [rsp+96+rsi*4]
    mov rax, rbx
    shl rax, FI_SHIFT
    movzx ecx, word [r13+rax+FI_LEN]
    mov edx, [r13+rax+FI_NAME]
    lea eax, [r14+rcx+2]
    cmp eax, r12d
    jae .bad
    test r14d, r14d
    jz .nosep
    mov word [rdi+r14*2], 0x5C
    inc r14d
.nosep:
    mov r8, [fi_pool]
    lea r8, [r8+rdx*2]
    xor r9d, r9d
.cp:
    cmp r9d, ecx
    jae .cpd
    mov ax, [r8+r9*2]
    lea r10, [r14+r9]
    mov [rdi+r10*2], ax
    inc r9d
    jmp .cp
.cpd:
    add r14d, ecx
    jmp .em
.done:
    mov word [rdi+r14*2], 0
    mov eax, r14d
    EPILOG 608
.bad:
    mov word [rdi], 0
    xor eax, eax
    EPILOG 608

; ---------------------------------------------------------------- search
; find_query(text, out dd[], capacity) -> eax number of names that contain the text (case ignored)
; find_query2(text, out dd[], capacity, filter) -> the same, narrowed by a filter:
;   filter + 0  dd  kind: 0 files and folders, 1 files only, 2 folders only
;   filter + 8  dq  extensions: lower case strings one after the other, ended by an empty one (or 0)
;   filter + 16 dq  text that the path of the item's folder must contain, any case (or 0)
;   filter + 24 dd  drive: 0 all, n = the n-th listed tree only (find_volname(n-1))
;   filter + 28 dd  cloud storage: 0 included, 1 left out, 2 only
;   The first `capacity` ids are stored; the whole index (or drive) is always scanned. With a filter the
;   text may be empty (every name that passes the filter); with nothing to look for the result is 0.
;   Searches run on the user interface thread only: the folder filter uses two static buffers.
%define FQ_FLT   2104
%define FQ_KIND  2112
%define FQ_EXT   2120
%define FQ_PLEN  2128                ; length of the folder filter
%define FQ_END   2136                ; id where the scan stops
%define FQ_CLOUD 2144
find_query:
    xor r9d, r9d
find_query2:
    PROLOG 2160
    mov rsi, rcx
    mov rdi, rdx
    mov r12d, r8d
    mov [rsp+FQ_FLT], r9
    mov r13, [fi_lower]
    test r13, r13
    jz .zero
    xor ecx, ecx
.lp:
    movzx eax, word [rsi+rcx*2]
    test eax, eax
    jz .lpd
    cmp ecx, 1000
    jae .lpd
    movzx eax, word [r13+rax*2]
    mov [rsp+96+rcx*2], ax
    inc ecx
    jmp .lp
.lpd:
    mov r14d, ecx                    ; length of the text
    mov qword [rsp+FQ_KIND], 0
    mov qword [rsp+FQ_EXT], 0
    mov qword [rsp+FQ_PLEN], 0
    mov qword [rsp+FQ_CLOUD], 0
    mov rax, [fi_vis]
    mov [rsp+FQ_END], rax
    mov ebx, 1                       ; first id
    xor ebp, ebp                     ; "a drive or the cloud choice is set"
    mov rsi, [rsp+FQ_FLT]
    test rsi, rsi
    jz .nof
    mov eax, [rsi]
    mov [rsp+FQ_KIND], rax
    mov eax, [rsi+28]
    mov [rsp+FQ_CLOUD], rax
    or ebp, eax
    mov eax, [rsi+24]                ; drive
    test eax, eax
    jz .alld
    cmp rax, [fi_nvol]
    ja .zero
    mov ebp, 1
    dec eax
    shl rax, 4
    lea rcx, [fi_vol]
    mov ebx, [rcx+rax+FV_FIRST]
    mov edx, [rcx+rax+FV_END]
    test edx, edx
    jz .alld                         ; still growing: up to what is visible
    cmp rdx, [rsp+FQ_END]
    jae .alld
    mov [rsp+FQ_END], rdx
.alld:
    mov rax, [rsi+8]
    test rax, rax
    jz .noext
    cmp word [rax], 0
    je .noext
    mov [rsp+FQ_EXT], rax
.noext:
    mov rax, [rsi+16]
    test rax, rax
    jz .nof
    lea r8, [fq_pat]
    xor ecx, ecx
.pl:
    movzx edx, word [rax+rcx*2]
    test edx, edx
    jz .pld
    cmp ecx, 250
    jae .pld
    movzx edx, word [r13+rdx*2]
    mov [r8+rcx*2], dx
    inc ecx
    jmp .pl
.pld:
    mov [rsp+FQ_PLEN], rcx
.nof:
    mov eax, r14d
    or eax, ebp
    or rax, [rsp+FQ_KIND]
    or rax, [rsp+FQ_EXT]
    or rax, [rsp+FQ_PLEN]
    jz .zero                         ; nothing to look for
    movzx r15d, word [rsp+96]        ; first character of the text
    xor ebp, ebp                     ; found
.id:
    cmp rbx, [rsp+FQ_END]
    jae .done
    mov r8, [fi_ent]
    mov r9, [fi_pool]
    mov rax, rbx
    shl rax, FI_SHIFT
    movzx edx, word [r8+rax+FI_FLAGS]
    test edx, 2
    jnz .next                        ; the top of a listed tree (a drive, a listed folder) is not a result
    mov rcx, [rsp+FQ_KIND]
    cmp ecx, 1
    jne .k2
    test edx, 1
    jnz .next                        ; files only
    jmp .kok
.k2:
    cmp ecx, 2
    jne .kok
    test edx, 1
    jz .next                         ; folders only
.kok:
    movzx ecx, word [r8+rax+FI_LEN]
    mov esi, ecx                     ; name length
    mov edx, [r8+rax+FI_NAME]
    lea r10, [r9+rdx*2]              ; name
    test r14d, r14d
    jz .hit
    sub ecx, r14d                    ; last position the text can start at
    js .next
    xor r11d, r11d
.st:
    cmp r11d, ecx
    ja .next
    movzx eax, word [r10+r11*2]
    movzx eax, word [r13+rax*2]
    cmp eax, r15d
    jne .sn
    mov edx, 1
.cm:
    cmp edx, r14d
    jae .hit
    lea rax, [r11+rdx]
    movzx eax, word [r10+rax*2]
    movzx eax, word [r13+rax*2]
    cmp ax, [rsp+96+rdx*2]
    jne .sn
    inc edx
    jmp .cm
.sn:
    inc r11d
    jmp .st
.hit:
    ; ---- extension: what follows the last dot of the name is one of the list
    mov rax, [rsp+FQ_EXT]
    test rax, rax
    jz .cloud
    mov ecx, esi
.dot:
    test ecx, ecx
    jz .next
    dec ecx
    cmp word [r10+rcx*2], '.'
    jne .dot
    lea r11, [r10+rcx*2+2]           ; extension
    mov edx, esi
    sub edx, ecx
    dec edx                          ; its length
    jz .next
.el:
    cmp word [rax], 0
    je .next                         ; end of the list
    xor ecx, ecx
.ec:
    movzx r8d, word [rax+rcx*2]
    test r8d, r8d
    jz .eend
    cmp ecx, edx
    jae .eskip
    movzx r9d, word [r11+rcx*2]
    movzx r9d, word [r13+r9*2]
    cmp r9d, r8d
    jne .eskip
    inc ecx
    jmp .ec
.eend:
    cmp ecx, edx
    je .cloud
.eskip:
    cmp word [rax], 0
    je .esd
    add rax, 2
    jmp .eskip
.esd:
    add rax, 2
    jmp .el
.cloud:
    ; ---- cloud storage: the item is, or is inside, one of the cloud folders
    cmp qword [rsp+FQ_CLOUD], 0
    je .path
    mov r8, [fi_ent]
    mov ecx, ebx
    xor r9d, r9d                     ; depth
.cw:
    test ecx, ecx
    jz .cno
    xor edx, edx
    lea r11, [fi_cloud]
.cl:
    cmp rdx, [fi_ncloud]
    jae .cup
    cmp [r11+rdx*4], ecx
    je .cyes
    inc edx
    jmp .cl
.cup:
    mov rax, rcx
    shl rax, FI_SHIFT
    test word [r8+rax+FI_FLAGS], 2
    jnz .cno
    mov ecx, [r8+rax+FI_PARENT]
    inc r9d
    cmp r9d, 128
    jb .cw
.cno:
    cmp qword [rsp+FQ_CLOUD], 2      ; only cloud: not this one
    je .next
    jmp .path
.cyes:
    cmp qword [rsp+FQ_CLOUD], 1      ; cloud left out: not this one
    je .next
.path:
    ; ---- folder: the path of the folder the item is in contains the filter text
    cmp qword [rsp+FQ_PLEN], 0
    je .store
    mov r8, [fi_ent]
    mov rax, rbx
    shl rax, FI_SHIFT
    mov esi, [r8+rax+FI_PARENT]
    test esi, esi
    jz .next
    invoke find_path, rsi, &fq_path, 1090
    test eax, eax
    jz .next
    mov ecx, eax
    sub ecx, [rsp+FQ_PLEN]           ; last start
    js .next
    lea r10, [fq_path]
    lea r11, [fq_pat]
    xor r8d, r8d
.ps:
    cmp r8d, ecx
    ja .next
    xor r9d, r9d
.pc:
    cmp r9, [rsp+FQ_PLEN]
    jae .store
    lea rax, [r8+r9]
    movzx eax, word [r10+rax*2]
    movzx eax, word [r13+rax*2]
    cmp ax, [r11+r9*2]
    jne .pn
    inc r9d
    jmp .pc
.pn:
    inc r8d
    jmp .ps
.store:
    ; A removed folder also hides its descendants, regardless of entry order.
    cmp qword [fi_deleted], 0
    je .alive_done
    mov r8, [fi_ent]
    mov ecx, ebx
    mov edx, 128
.alive:
    test ecx, ecx
    jz .alive_done
    mov rax, rcx
    shl rax, FI_SHIFT
    test word [r8+rax+FI_FLAGS], FI_DELETED
    jnz .next
    mov ecx, [r8+rax+FI_PARENT]
    dec edx
    jnz .alive
.alive_done:
    cmp ebp, r12d
    jae .nostore
    mov [rdi+rbp*4], ebx
.nostore:
    inc ebp
.next:
    inc rbx
    jmp .id
.done:
    mov eax, ebp
    EPILOG 2160
.zero:
    xor eax, eax
    EPILOG 2160

; find_forget(id): invalidate a confirmed missing item without moving indexed entries.
find_forget:
    test ecx, ecx
    jz .d
    cmp rcx, [fi_vis]
    jae .d
    shl rcx, FI_SHIFT
    mov rax, [fi_ent]
    or word [rax+rcx+FI_FLAGS], FI_DELETED
    mov qword [fi_deleted], 1
.d:
    ret

; find_forget_path(path or 0): invalidate another instance's deletion. A ring
; overflow (0) verifies every visible path, so no missed notification revives files.
find_forget_path:
    PROLOG 2304
    mov r12, rcx
    xor r13d, r13d
    test r12, r12
    jz .scan
    invoke wcsrchr, r12, 0x5C
    test rax, rax
    jz .done
    lea r13, [rax+2]                 ; compare names before rebuilding full paths
.scan:
    mov ebx, 1
.entry:
    cmp rbx, [fi_vis]
    jae .done
    mov rax, rbx
    shl rax, FI_SHIFT
    mov r14, [fi_ent]
    test word [r14+rax+FI_FLAGS], FI_DELETED
    jnz .next
    test r13, r13
    jz .path
    mov edx, [r14+rax+FI_NAME]
    mov rax, [fi_pool]
    lea rdx, [rax+rdx*2]
    invoke _wcsicmp, r13, rdx
    test eax, eax
    jnz .next
.path:
    invoke find_path, rbx, &rsp+96, 1090
    test eax, eax
    jz .next
    test r12, r12
    jz .verify
    invoke _wcsicmp, r12, &rsp+96
    test eax, eax
    jnz .next
.verify:
    invoke GetFileAttributesW, &rsp+96
    cmp eax, -1
    jne .next
    call GetLastError
    cmp eax, 2
    je .forget
    cmp eax, 3
    jne .next
.forget:
    invoke find_forget, rbx
.next:
    inc rbx
    jmp .entry
.done:
    EPILOG 2304

; ---------------------------------------------------------------- sort
; find_sort(ids, count, key, descending): key 0 name, 1 modification time, 2 type (extension), 3 size,
;   4 folder path. Equal items are ordered by name, then by id. Folder paths are built for every
;   comparison: the caller keeps that key to small lists.
find_sort:
    PROLOG 64
    mov [fs_key], r8
    mov [fs_desc], r9
    cmp rdx, 2
    jb .d
    invoke qsort, rcx, rdx, 4, &fs_cmp
.d:
    EPILOG 64

; fs_cmpw: r8 / r9d text A and length, r10 / r11d text B, r14 lower table -> eax -1, 0, 1 (case ignored)
fs_cmpw:
    xor ecx, ecx
.l:
    cmp ecx, r9d
    jae .aend
    cmp ecx, r11d
    jae .gt
    movzx eax, word [r8+rcx*2]
    movzx eax, word [r14+rax*2]
    movzx edx, word [r10+rcx*2]
    movzx edx, word [r14+rdx*2]
    cmp eax, edx
    ja .gt
    jb .lt
    inc ecx
    jmp .l
.aend:
    cmp ecx, r11d
    jb .lt
    xor eax, eax
    ret
.gt:
    mov eax, 1
    ret
.lt:
    mov eax, -1
    ret

; fs_ext: r8 / r9d name and length -> r8 / r9d its extension (length 0 when it has none)
fs_ext:
    mov ecx, r9d
.l:
    test ecx, ecx
    jz .none
    dec ecx
    cmp word [r8+rcx*2], '.'
    jne .l
    lea r8, [r8+rcx*2+2]
    sub r9d, ecx
    dec r9d
    ret
.none:
    xor r9d, r9d
    ret

; fs_cmp(a*, b*): comparison for qsort
fs_cmp:
    PROLOG 64
    mov esi, [rcx]
    mov edi, [rdx]
    mov r12, [fi_ent]
    mov r13, [fi_pool]
    mov r14, [fi_lower]
    mov rax, rsi
    shl rax, FI_SHIFT
    lea rbx, [r12+rax]               ; entry A
    mov rax, rdi
    shl rax, FI_SHIFT
    lea rbp, [r12+rax]               ; entry B
    mov rax, [fs_key]
    cmp eax, 1
    je .date
    cmp eax, 2
    je .type
    cmp eax, 3
    je .size
    cmp eax, 4
    je .dir
.name:
    mov eax, [rbx+FI_NAME]
    lea r8, [r13+rax*2]
    movzx r9d, word [rbx+FI_LEN]
    mov eax, [rbp+FI_NAME]
    lea r10, [r13+rax*2]
    movzx r11d, word [rbp+FI_LEN]
    call fs_cmpw
    test eax, eax
    jnz .out
    cmp esi, edi
    mov eax, 1
    ja .out
    mov eax, -1
    jb .out
    xor eax, eax
.out:
    cmp qword [fs_desc], 0
    je .ret
    neg eax
.ret:
    EPILOG 64
.date:
    mov rax, [rbx+FI_TIME]
    cmp rax, [rbp+FI_TIME]
    je .name
    mov eax, 1
    ja .out
    mov eax, -1
    jmp .out
.size:
    mov rax, [rbx+FI_BYTES]
    cmp rax, [rbp+FI_BYTES]
    je .name
    mov eax, 1
    ja .out
    mov eax, -1
    jmp .out
.type:
    ; folders have no type: they come first
    mov eax, [rbp+FI_NAME]
    lea r8, [r13+rax*2]
    movzx r9d, word [rbp+FI_LEN]
    test word [rbp+FI_FLAGS], 1
    jz .tb
    xor r9d, r9d
    jmp .tbd
.tb:
    call fs_ext
.tbd:
    mov r10, r8
    mov r11d, r9d
    mov eax, [rbx+FI_NAME]
    lea r8, [r13+rax*2]
    movzx r9d, word [rbx+FI_LEN]
    test word [rbx+FI_FLAGS], 1
    jz .ta
    xor r9d, r9d
    jmp .tad
.ta:
    call fs_ext
.tad:
    call fs_cmpw
    test eax, eax
    jnz .out
    jmp .name
.dir:
    mov r15d, [rbx+FI_PARENT]
    invoke find_path, r15, &fs_bufa, 1090
    mov [rsp+48], rax
    mov r15d, [rbp+FI_PARENT]
    invoke find_path, r15, &fs_bufb, 1090
    mov r11d, eax
    mov r9d, [rsp+48]
    lea r8, [fs_bufa]
    lea r10, [fs_bufb]
    call fs_cmpw
    test eax, eax
    jnz .out
    jmp .name

; ---------------------------------------------------------------- listing by walking the folders
; fi_walk(root id): every file and folder below the entry; reparse points are listed but not entered
%define FW_FD   96                   ; WIN32_FIND_DATAW (592) -> 688
%define FW_PATH 688                  ; wchar[1100] -> 2888
%define FW_STK  2888                 ; ids of the folders still to list
%define FW_SN   2896
%define FW_SC   2904
%define FW_TIME 2912
%define FW_SIZE 2920
fi_walk:
    PROLOG 2944
    mov r12d, ecx
    invoke malloc, 16384
    test rax, rax
    jz .ret
    mov [rsp+FW_STK], rax
    mov qword [rsp+FW_SC], 4096
    mov qword [rsp+FW_SN], 1
    mov [rax], r12d
.pop:
    cmp qword [fi_cancel], 0
    jne .end
    mov rax, [rsp+FW_SN]
    test rax, rax
    jz .end
    dec rax
    mov [rsp+FW_SN], rax
    mov rcx, [rsp+FW_STK]
    mov r13d, [rcx+rax*4]            ; folder id
    lea rdi, [rsp+FW_PATH]           ; "\\?\" + path + "\*": no length limit of 260
    mov dword [rdi], 0x005C005C
    mov dword [rdi+4], 0x005C003F
    invoke find_path, r13, &rdi+8, 1080
    test eax, eax
    jz .pop
    lea rax, [rdi+rax*2+8]
    mov dword [rax], 0x002A005C
    mov word [rax+4], 0
    invoke FindFirstFileExW, rdi, 1, &rsp+FW_FD, 0, 0, 2          ; basic info, large fetch
    cmp rax, -1
    je .pop
    mov rbx, rax
.ent:
    lea rsi, [rsp+FW_FD+44]          ; cFileName
    cmp word [rsi], '.'
    jne .use
    cmp word [rsi+2], 0
    je .nx
    cmp word [rsi+2], '.'
    jne .use
    cmp word [rsi+4], 0
    je .nx
.use:
    invoke wcslen, rsi
    mov r14d, eax
    mov r15d, [rsp+FW_FD]            ; attributes
    mov rax, [rsp+FW_FD+20]          ; ftLastWriteTime
    mov [rsp+FW_TIME], rax
    mov eax, [rsp+FW_FD+28]          ; nFileSizeHigh
    shl rax, 32
    mov ecx, [rsp+FW_FD+32]          ; nFileSizeLow
    or rax, rcx
    mov [rsp+FW_SIZE], rax
    xor ebp, ebp
    test r15d, 0x10
    jz .add
    mov ebp, 1
    mov qword [rsp+FW_SIZE], 0
.add:
    invoke fi_add, r13, rsi, r14, rbp, 0, [rsp+FW_TIME], [rsp+FW_SIZE]
    test eax, eax
    jz .nx
    mov rcx, [fi_n]
    mov [fi_vis], rcx
    test r15d, 0x10
    jz .nx
    test r15d, 0x400                 ; reparse point (junction, link): listed, not entered
    jnz .nx
    mov r14d, eax                    ; the new folder
    mov rcx, [rsp+FW_SN]
    cmp rcx, [rsp+FW_SC]
    jb .push
    mov rax, [rsp+FW_SC]
    shl rax, 1
    mov [rsp+FW_SC], rax
    shl rax, 2
    invoke realloc, [rsp+FW_STK], rax
    test rax, rax
    jz .closeend
    mov [rsp+FW_STK], rax
.push:
    mov rcx, [rsp+FW_SN]
    mov rdx, [rsp+FW_STK]
    mov [rdx+rcx*4], r14d
    inc qword [rsp+FW_SN]
.nx:
    invoke FindNextFileW, rbx, &rsp+FW_FD
    test eax, eax
    jnz .ent
    invoke FindClose, rbx
    jmp .pop
.closeend:
    invoke FindClose, rbx
    jmp .ret
.end:
    invoke free, [rsp+FW_STK]
.ret:
    EPILOG 2944

; ---------------------------------------------------------------- NTFS: shared pieces
; fi_fix(first id, map, records, root id): turn the parent record numbers of a volume's entries into ids.
;   Record 5 is the root folder; an entry whose folder was not listed stays without a path.
fi_fix:
    PROLOG 64
    mov r12, rcx
    mov r14, rdx
    mov r15, r8
    mov esi, r9d
    mov r13, [fi_ent]
.fx:
    cmp r12, [fi_n]
    jae .fxd
    mov rax, r12
    shl rax, FI_SHIFT
    mov ecx, [r13+rax+FI_REC]
    mov edx, esi
    cmp ecx, 5
    je .set
    xor edx, edx
    cmp rcx, r15
    jae .set
    mov edx, [r14+rcx*4]
.set:
    mov [r13+rax+FI_PARENT], edx
    inc r12
    jmp .fx
.fxd:
    mov rax, [fi_n]
    mov [fi_vis], rax
    EPILOG 64

; fi_openvol(drive letter, out NTFS_VOLUME_DATA_BUFFER[96]) -> rax handle, 0 when not possible
fi_openvol:
    PROLOG 96
    mov rsi, rdx
    lea rdi, [rsp+64]                ; "\\.\X:"
    mov dword [rdi], 0x005C005C
    mov dword [rdi+4], 0x005C002E
    mov [rdi+8], cx
    mov word [rdi+10], ':'
    mov word [rdi+12], 0
    invoke CreateFileW, rdi, GENERIC_READ, 3, 0, OPEN_EXISTING, 0, 0
    cmp rax, -1
    je .no
    mov rbx, rax
    invoke DeviceIoControl, rbx, 0x00090064, 0, 0, rsi, 96, &rsp+80, 0     ; FSCTL_GET_NTFS_VOLUME_DATA
    test eax, eax
    jz .close
    mov rax, rbx
    EPILOG 96
.close:
    invoke CloseHandle, rbx
.no:
    xor eax, eax
    EPILOG 96

; ---------------------------------------------------------------- NTFS: parsing the master file table
; fi_mftraw(drive letter, id of the drive's entry) -> eax 1 listed, 0 not possible
;   The table is a file of fixed size records, one per file or folder. Its own record (number 0) says
;   where on the volume it lies; those runs are read in large pieces. A record in use holds, as
;   attributes: 0x10 standard information (times), 0x30 file name (name and the record of its folder),
;   0x80 data (size). Each record ends its sectors with a check value that has to be put back first.
%define FR_NV    96                  ; NTFS_VOLUME_DATA_BUFFER (96) -> 192
%define FR_RET   192
%define FR_MAP   200                 ; dd[records]: record number -> id
%define FR_NREC  208
%define FR_BASE  216                 ; first id of this volume
%define FR_BUF   224
%define FR_ROOT  232
%define FR_BPC   240                 ; bytes per cluster
%define FR_RSZ   248                 ; bytes per record
%define FR_BPS   256                 ; bytes per sector
%define FR_EXT   264                 ; runs: pairs { first cluster, clusters }
%define FR_NEXT  272
%define FR_REC   280                 ; number of the record being parsed
%define FR_LEFT  288                 ; bytes left in the run
%define FR_POS   296                 ; byte offset on the volume
%define FR_TIME  304
%define FR_BYTES 312
%define FR_NAME  320
%define FR_NLEN  328
%define FR_PREC  336
%define FR_HAVE  344                 ; 0 no name yet, 1 a short (DOS) name, 2 a long name
%define FR_TOTAL 352                 ; clusters in all runs
%define FR_IEXT  360                 ; run being read
%define FR_GOT   368
%define FR_XBASE 376                 ; record the name belongs to (a continuation record names its base record)
%define FR_CHUNK 0x100000
fi_mftraw:
    PROLOG 384
    mov [rsp+FR_ROOT], rdx
    invoke fi_openvol, rcx, &rsp+FR_NV
    test rax, rax
    jz .no
    mov rbx, rax
    xor eax, eax
    mov [rsp+FR_MAP], rax
    mov [rsp+FR_BUF], rax
    mov [rsp+FR_EXT], rax
    mov eax, [rsp+FR_NV+40]
    mov [rsp+FR_BPS], rax
    mov eax, [rsp+FR_NV+44]
    mov [rsp+FR_BPC], rax
    mov eax, [rsp+FR_NV+48]
    mov [rsp+FR_RSZ], rax
    ; sizes this code can read: records of 1K .. 4K made of whole sectors
    cmp qword [rsp+FR_BPS], 512
    jb .fail
    cmp qword [rsp+FR_RSZ], 1024
    jb .fail
    cmp qword [rsp+FR_RSZ], 4096
    ja .fail
    mov rax, [rsp+FR_RSZ]
    xor edx, edx
    div qword [rsp+FR_BPS]
    test rdx, rdx
    jnz .fail
    mov rax, FR_CHUNK
    xor edx, edx
    div qword [rsp+FR_RSZ]
    test rdx, rdx
    jnz .fail
    mov rax, [rsp+FR_NV+56]          ; valid length of the table
    xor edx, edx
    div qword [rsp+FR_RSZ]
    test rax, rax
    jz .fail
    cmp rax, 0x40000000
    ja .fail
    mov [rsp+FR_NREC], rax
    lea rsi, [rax*4+16]
    invoke VirtualAlloc, 0, rsi, 0x3000, 4
    test rax, rax
    jz .fail
    mov [rsp+FR_MAP], rax
    invoke VirtualAlloc, 0, FR_CHUNK+4096, 0x3000, 4
    test rax, rax
    jz .fail
    mov [rsp+FR_BUF], rax
    invoke VirtualAlloc, 0, 8192, 0x3000, 4
    test rax, rax
    jz .fail
    mov [rsp+FR_EXT], rax
    ; ---- record 0: where the table lies
    mov rax, [rsp+FR_NV+64]          ; first cluster of the table
    imul rax, [rsp+FR_BPC]
    mov [rsp+FR_POS], rax
    invoke SetFilePointerEx, rbx, [rsp+FR_POS], 0, 0
    test eax, eax
    jz .fail
    mov rdi, [rsp+FR_RSZ]
    invoke ReadFile, rbx, [rsp+FR_BUF], rdi, &rsp+FR_RET, 0
    test eax, eax
    jz .fail
    mov eax, [rsp+FR_RET]
    cmp rax, [rsp+FR_RSZ]
    jne .fail
    mov r12, [rsp+FR_BUF]
    call .fixup
    test eax, eax
    jz .fail
    mov r13, r12
    add r13, [rsp+FR_RSZ]            ; end of the record
    movzx eax, word [r12+0x14]
    lea r14, [r12+rax]
.a0:
    lea rax, [r14+0x40]
    cmp rax, r13
    ja .fail                         ; no data attribute here
    mov eax, [r14]
    cmp eax, -1
    je .fail
    mov ecx, [r14+4]
    test ecx, ecx
    jz .fail
    cmp eax, 0x80
    jne .a0n
    cmp byte [r14+9], 0              ; unnamed
    jne .a0n
    cmp byte [r14+8], 0              ; not resident
    jne .runs
.a0n:
    add r14, rcx
    jmp .a0
.runs:
    movzx eax, word [r14+0x20]
    lea rsi, [r14+rax]               ; run list
    mov ecx, [r14+4]
    lea r13, [r14+rcx]               ; its end
    xor r14d, r14d                   ; cluster of the current run
    xor r15d, r15d                   ; runs
    mov qword [rsp+FR_TOTAL], 0
.run:
    cmp rsi, r13
    jae .rund
    movzx eax, byte [rsi]
    test eax, eax
    jz .rund
    mov r8d, eax
    and r8d, 15                      ; bytes of the length
    mov r9d, eax
    shr r9d, 4                       ; bytes of the offset
    inc rsi
    test r8d, r8d
    jz .fail
    cmp r8d, 8
    ja .fail
    test r9d, r9d
    jz .fail                         ; a hole: the table has none
    cmp r9d, 8
    ja .fail
    xor edx, edx
    mov ecx, r8d
.rl:
    dec ecx
    js .rld
    shl rdx, 8
    movzx r10d, byte [rsi+rcx]
    or rdx, r10
    jmp .rl
.rld:
    add rsi, r8
    mov ecx, r9d
    movsx r11, byte [rsi+rcx-1]      ; the offset is signed
    dec ecx
.ol:
    dec ecx
    js .old
    shl r11, 8
    movzx r10d, byte [rsi+rcx]
    or r11, r10
    jmp .ol
.old:
    add rsi, r9
    add r14, r11
    cmp r15, 500
    jae .fail
    mov rax, [rsp+FR_EXT]
    mov rcx, r15
    shl rcx, 4
    mov [rax+rcx], r14
    mov [rax+rcx+8], rdx
    inc r15
    add [rsp+FR_TOTAL], rdx
    jmp .run
.rund:
    test r15, r15
    jz .fail
    mov [rsp+FR_NEXT], r15
    mov rax, [rsp+FR_TOTAL]
    imul rax, [rsp+FR_BPC]
    cmp rax, [rsp+FR_NV+56]
    jb .fail                         ; the run list continues in another record: not handled here
    ; ---- the records
    mov rax, [fi_n]
    mov [rsp+FR_BASE], rax
    mov qword [rsp+FR_REC], 0
    mov qword [rsp+FR_IEXT], 0
.ext:
    mov rax, [rsp+FR_IEXT]
    cmp rax, [rsp+FR_NEXT]
    jae .fix
    shl rax, 4
    add rax, [rsp+FR_EXT]
    mov rcx, [rax]
    imul rcx, [rsp+FR_BPC]
    mov [rsp+FR_POS], rcx
    mov rcx, [rax+8]
    imul rcx, [rsp+FR_BPC]
    mov [rsp+FR_LEFT], rcx
    inc qword [rsp+FR_IEXT]
.chunk:
    cmp qword [fi_cancel], 0
    jne .fix
    mov rdi, [rsp+FR_LEFT]
    test rdi, rdi
    jz .ext
    mov rax, [rsp+FR_REC]
    cmp rax, [rsp+FR_NREC]
    jae .fix
    cmp rdi, FR_CHUNK
    jbe .rd
    mov edi, FR_CHUNK
.rd:
    invoke SetFilePointerEx, rbx, [rsp+FR_POS], 0, 0
    test eax, eax
    jz .fix
    invoke ReadFile, rbx, [rsp+FR_BUF], rdi, &rsp+FR_RET, 0
    test eax, eax
    jz .fix
    mov eax, [rsp+FR_RET]
    test eax, eax
    jz .fix
    mov [rsp+FR_GOT], rax
    add [rsp+FR_POS], rdi
    sub [rsp+FR_LEFT], rdi
    xor r15d, r15d                   ; offset in the piece
.rec:
    mov rax, r15
    add rax, [rsp+FR_RSZ]
    cmp rax, [rsp+FR_GOT]
    ja .chunk
    mov rax, [rsp+FR_REC]
    cmp rax, [rsp+FR_NREC]
    jae .fix
    mov r12, [rsp+FR_BUF]
    add r12, r15
    call .parse
    inc qword [rsp+FR_REC]
    add r15, [rsp+FR_RSZ]
    jmp .rec
.fix:
    invoke fi_fix, [rsp+FR_BASE], [rsp+FR_MAP], [rsp+FR_NREC], [rsp+FR_ROOT]
    call .release
    mov eax, 1
    EPILOG 384
.fail:
    call .release
.no:
    xor eax, eax
    EPILOG 384

.release:                            ; local: frees what was allocated (rsp is 8 lower here)
    sub rsp, 40
    mov rcx, [rsp+48+FR_BUF]
    test rcx, rcx
    jz .r1
    invoke VirtualFree, rcx, 0, 0x8000
.r1:
    mov rcx, [rsp+48+FR_MAP]
    test rcx, rcx
    jz .r2
    invoke VirtualFree, rcx, 0, 0x8000
.r2:
    mov rcx, [rsp+48+FR_EXT]
    test rcx, rcx
    jz .r3
    invoke VirtualFree, rcx, 0, 0x8000
.r3:
    invoke CloseHandle, rbx
    add rsp, 40
    ret

; local: put the last two bytes of every sector of the record at r12 back -> eax 1 when it is a sound record
.fixup:
    xor eax, eax
    cmp dword [r12], 0x454C4946      ; "FILE"
    jne .fud
    movzx r8d, word [r12+4]          ; where the saved values are
    movzx ecx, word [r12+6]          ; one check value + one saved value per sector
    cmp ecx, 2
    jb .fud
    lea edx, [r8+rcx*2]
    cmp rdx, [rsp+8+FR_RSZ]
    ja .fud
    movzx r9d, word [r12+r8]         ; the check value
    mov r10d, 1
.fu:
    cmp r10d, ecx
    jae .fuok
    mov rax, [rsp+8+FR_RSZ]                ; the saved values belong to blocks of record size / (count - 1)
    xor edx, edx                     ; bytes - 512 on NTFS whatever the sector size of the volume is:
    lea r11d, [rcx-1]                ; with 4096 byte sectors only one of the eight places was put back
    div r11
    imul rax, r10
    lea r11, [rax-2]
    cmp r11, [rsp+8+FR_RSZ]
    jae .fuok
    cmp [r12+r11], r9w
    jne .fubad                       ; a record written half way
    lea rdx, [r8+r10*2]
    movzx edx, word [r12+rdx]
    mov [r12+r11], dx
    inc r10d
    jmp .fu
.fuok:
    mov eax, 1
    ret
.fubad:
    xor eax, eax
.fud:
    ret

; local: one record at r12 -> an entry when it is a file or folder in use (rsp is 8 lower here)
.parse:
    sub rsp, 72                      ; frame slots are at [rsp+80+...] in here
%define PF 80
    call .fixup2
    test eax, eax
    jz .pd
    test word [r12+0x16], 1          ; in use
    jz .pd
    mov rax, [r12+0x20]              ; not 0: a continuation of that record
    mov rcx, 0x0000FFFFFFFFFFFF
    and rax, rcx
    mov [rsp+PF+FR_XBASE], rax
    xor eax, eax
    mov [rsp+PF+FR_HAVE], rax
    mov [rsp+PF+FR_TIME], rax
    mov [rsp+PF+FR_BYTES], rax
    mov r13, r12
    add r13, [rsp+PF+FR_RSZ]
    movzx eax, word [r12+0x14]
    lea r14, [r12+rax]
.at:
    lea rax, [r14+16]
    cmp rax, r13
    ja .atd
    mov eax, [r14]
    cmp eax, -1
    je .atd
    mov ecx, [r14+4]
    test ecx, ecx
    jz .atd
    lea rdx, [r14+rcx]
    cmp rdx, r13
    ja .atd
    cmp eax, 0x10
    je .si
    cmp eax, 0x30
    je .fn
    cmp eax, 0x80
    je .data
.an:
    mov ecx, [r14+4]
    add r14, rcx
    jmp .at
.si:
    cmp byte [r14+8], 0
    jne .an
    movzx eax, word [r14+0x14]
    lea rdx, [r14+rax+16]
    cmp rdx, r13
    ja .an
    mov rdx, [r14+rax+8]             ; last modification
    mov [rsp+PF+FR_TIME], rdx
    jmp .an
.fn:
    cmp byte [r14+8], 0
    jne .an
    movzx eax, word [r14+0x14]
    lea r10, [r14+rax]               ; the attribute's content
    lea rdx, [r10+0x42]
    cmp rdx, r13
    ja .an
    movzx ecx, byte [r10+0x40]       ; characters
    lea rdx, [r10+rcx*2+0x42]
    cmp rdx, r13
    ja .an
    mov r11d, 2
    cmp byte [r10+0x41], 2           ; a short name only: used when there is no other
    jne .fnset
    mov r11d, 1
.fnset:
    cmp r11, [rsp+PF+FR_HAVE]
    jbe .an
    mov [rsp+PF+FR_HAVE], r11
    lea rdx, [r10+0x42]
    mov [rsp+PF+FR_NAME], rdx
    mov [rsp+PF+FR_NLEN], rcx
    mov rdx, [r10]
    mov rax, 0x0000FFFFFFFFFFFF
    and rdx, rax
    mov [rsp+PF+FR_PREC], rdx
    jmp .an
.data:
    cmp byte [r14+9], 0              ; a named stream
    jne .an
    cmp byte [r14+8], 0
    jne .nr
    mov eax, [r14+0x10]              ; resident: length of the content
    mov [rsp+PF+FR_BYTES], rax
    jmp .an
.nr:
    lea rax, [r14+0x38]
    cmp rax, r13
    ja .an
    cmp qword [r14+0x10], 0          ; a later piece of the same data
    jne .an
    mov rax, [r14+0x30]
    mov [rsp+PF+FR_BYTES], rax
    jmp .an
.atd:
    cmp qword [rsp+PF+FR_HAVE], 0
    je .pd
    mov rax, [rsp+PF+FR_XBASE]
    test rax, rax
    jnz .cont
    mov rax, [rsp+PF+FR_REC]
    mov [rsp+PF+FR_XBASE], rax       ; an ordinary record names itself
    ; named before by one of its continuation records: only the time, size and kind are added
    mov rdx, [rsp+PF+FR_MAP]
    mov ecx, [rdx+rax*4]
    test ecx, ecx
    jz .chk
    call fi_e
    test rax, rax
    jz .pd
    mov rdx, [rsp+PF+FR_TIME]
    mov [rax+FI_TIME], rdx
    mov rdx, [rsp+PF+FR_BYTES]
    mov [rax+FI_BYTES], rdx
    test word [r12+0x16], 2
    jz .pd
    or word [rax+FI_FLAGS], 1
    mov qword [rax+FI_BYTES], 0
    jmp .pd
.cont:
    ; a file with many names keeps some of them in continuation records: the first one found names it
    cmp rax, [rsp+PF+FR_NREC]
    jae .pd
    mov rdx, [rsp+PF+FR_MAP]
    cmp dword [rdx+rax*4], 0
    jne .pd
    mov qword [rsp+PF+FR_TIME], 0
    mov qword [rsp+PF+FR_BYTES], 0
.chk:
    mov rax, [rsp+PF+FR_XBASE]
    cmp rax, 5
    je .pd                           ; the root folder itself is the drive's entry
    mov rax, [rsp+PF+FR_PREC]
    cmp rax, [rsp+PF+FR_NREC]
    jae .pd
    xor ebp, ebp
    test word [r12+0x16], 2          ; folder
    jz .pa
    mov ebp, 1
    mov qword [rsp+PF+FR_BYTES], 0
.pa:
    mov rsi, [rsp+PF+FR_NAME]
    mov rdi, [rsp+PF+FR_NLEN]
    invoke fi_add, 0, rsi, rdi, rbp, [rsp+PF+FR_PREC], [rsp+PF+FR_TIME], [rsp+PF+FR_BYTES]
    test eax, eax
    jz .pd
    mov rcx, [rsp+PF+FR_XBASE]
    mov rdx, [rsp+PF+FR_MAP]
    mov [rdx+rcx*4], eax
.pd:
    add rsp, 72
    ret
.fixup2:                             ; .fixup with the frame 80 bytes further away
    xor eax, eax
    cmp dword [r12], 0x454C4946
    jne .f2d
    movzx r8d, word [r12+4]
    movzx ecx, word [r12+6]
    cmp ecx, 2
    jb .f2d
    lea edx, [r8+rcx*2]
    cmp rdx, [rsp+8+PF+FR_RSZ]
    ja .f2d
    movzx r9d, word [r12+r8]
    mov r10d, 1
.f2:
    cmp r10d, ecx
    jae .f2ok
    mov rax, [rsp+8+PF+FR_RSZ]                ; the saved values belong to blocks of record size / (count - 1)
    xor edx, edx                     ; bytes - 512 on NTFS whatever the sector size of the volume is:
    lea r11d, [rcx-1]                ; with 4096 byte sectors only one of the eight places was put back
    div r11
    imul rax, r10
    lea r11, [rax-2]
    cmp r11, [rsp+8+PF+FR_RSZ]
    jae .f2ok
    cmp [r12+r11], r9w
    jne .f2bad
    lea rdx, [r8+r10*2]
    movzx edx, word [r12+rdx]
    mov [r12+r11], dx
    inc r10d
    jmp .f2
.f2ok:
    mov eax, 1
    ret
.f2bad:
    xor eax, eax
.f2d:
    ret

; ---------------------------------------------------------------- NTFS: enumerating the file table
; fi_mft(drive letter, id of the drive's entry) -> eax 1 listed, 0 not possible (no rights, not NTFS)
;   Every file record gives a name and the record number of its folder, but no time and no size.
%define FM_NV   96                   ; NTFS_VOLUME_DATA_BUFFER (96) -> 192
%define FM_MED  192                  ; MFT_ENUM_DATA_V0 (24) -> 216
%define FM_RET  216
%define FM_MAP  224                  ; dd[records]: record number -> id
%define FM_NREC 232
%define FM_BASE 240                  ; first id of this volume
%define FM_BUF  248
%define FM_ROOT 256
%define FM_RLEN 264
%define FM_IOBUF 0x20000
fi_mft:
    PROLOG 288
    mov [rsp+FM_ROOT], rdx
    invoke fi_openvol, rcx, &rsp+FM_NV
    test rax, rax
    jz .no
    mov rbx, rax
    mov ecx, [rsp+FM_NV+48]          ; bytes per file record
    test ecx, ecx
    jz .close0
    mov rax, [rsp+FM_NV+56]          ; valid length of the file table
    xor edx, edx
    div rcx
    test rax, rax
    jz .close0
    cmp rax, 0x40000000
    ja .close0
    mov [rsp+FM_NREC], rax
    lea rsi, [rax*4+16]
    invoke VirtualAlloc, 0, rsi, 0x3000, 4
    test rax, rax
    jz .close0
    mov [rsp+FM_MAP], rax
    invoke VirtualAlloc, 0, FM_IOBUF, 0x3000, 4
    test rax, rax
    jz .freemap0
    mov [rsp+FM_BUF], rax
    mov rax, [fi_n]
    mov [rsp+FM_BASE], rax
    mov qword [rsp+FM_MED], 0
    mov qword [rsp+FM_MED+8], 0
    mov rax, 0x7FFFFFFFFFFFFFFF
    mov [rsp+FM_MED+16], rax
.io:
    cmp qword [fi_cancel], 0
    jne .fix
    invoke DeviceIoControl, rbx, 0x000900B3, &rsp+FM_MED, 24, [rsp+FM_BUF], FM_IOBUF, &rsp+FM_RET, 0   ; FSCTL_ENUM_USN_DATA
    test eax, eax
    jz .fix                          ; end of the table
    mov r12, [rsp+FM_BUF]
    mov eax, [rsp+FM_RET]
    cmp eax, 8
    jbe .fix
    lea r13, [r12+rax]               ; end of the data
    mov rax, [r12]
    mov [rsp+FM_MED], rax            ; where the next call starts
    add r12, 8
.rec:
    lea rax, [r12+60]
    cmp rax, r13
    ja .io
    mov r14d, [r12]                  ; record length
    test r14d, r14d
    jz .io
    cmp word [r12+4], 2              ; USN_RECORD_V2
    jne .rn
    mov rcx, 0x0000FFFFFFFFFFFF
    mov r15, [r12+8]
    and r15, rcx                     ; record number
    cmp r15, 5
    je .rn                           ; the root folder itself is the drive's entry
    mov rbp, [r12+16]
    and rbp, rcx                     ; record number of its folder
    cmp rbp, [rsp+FM_NREC]
    jae .rn
    movzx edx, word [r12+58]
    lea rsi, [r12+rdx]               ; name
    xor edi, edi
    test dword [r12+52], 0x10
    jz .a
    mov edi, 1
.a:
    mov [rsp+FM_RLEN], r14
    movzx r14d, word [r12+56]
    shr r14d, 1                      ; name length in characters
    invoke fi_add, 0, rsi, r14, rdi, rbp, 0, 0
    test eax, eax
    jz .radv
    cmp r15, [rsp+FM_NREC]
    jae .radv
    mov rcx, [rsp+FM_MAP]
    mov [rcx+r15*4], eax
.radv:
    mov r14, [rsp+FM_RLEN]
.rn:
    add r12, r14
    jmp .rec
.fix:
    invoke fi_fix, [rsp+FM_BASE], [rsp+FM_MAP], [rsp+FM_NREC], [rsp+FM_ROOT]
    invoke VirtualFree, [rsp+FM_BUF], 0, 0x8000
    invoke VirtualFree, [rsp+FM_MAP], 0, 0x8000
    invoke CloseHandle, rbx
    mov eax, 1
    EPILOG 288
.freemap0:
    invoke VirtualFree, [rsp+FM_MAP], 0, 0x8000
.close0:
    invoke CloseHandle, rbx
.no:
    xor eax, eax
    EPILOG 288

; ---------------------------------------------------------------- what to list
; fi_tree(name, length, drive letter or 0): one listed tree. Its own entry, then its contents - a drive
;   from its file table when possible, anything else by walking.
fi_tree:
    PROLOG 64
    mov rsi, rcx
    mov edi, edx
    mov ebx, r8d
    cmp qword [fi_nvol], FI_MAXVOL
    jae .d
    invoke fi_add, 0, rsi, rdi, 3, 0, 0, 0
    test eax, eax
    jz .d
    mov r12d, eax                    ; id of the top entry
    mov rcx, [fi_n]
    mov [fi_vis], rcx
    mov r13, [fi_nvol]
    shl r13, 4
    lea rax, [fi_vol]
    add r13, rax
    mov [r13+FV_ROOT], r12d
    mov rcx, [fi_n]
    mov [r13+FV_FIRST], ecx
    mov dword [r13+FV_END], 0
    mov dword [r13+FV_WAY], 0
    inc qword [fi_nvol]
    test ebx, ebx
    jz .walk
    cmp qword [fi_force], 0
    jne .usn
    invoke fi_mftraw, rbx, r12
    test eax, eax
    jz .usn
    or qword [fi_mode], 1
    mov dword [r13+FV_WAY], 1
    jmp .end
.usn:
    cmp qword [fi_force], 2
    jae .walk
    invoke fi_mft, rbx, r12
    test eax, eax
    jz .walk
    or qword [fi_mode], 4
    mov dword [r13+FV_WAY], 4
    jmp .end
.walk:
    or qword [fi_mode], 2
    mov dword [r13+FV_WAY], 2
    invoke fi_walk, r12
.end:
    mov rcx, [fi_n]
    mov [r13+FV_END], ecx
.d:
    EPILOG 64

; ---------------------------------------------------------------- cloud storage folders
; fi_resolve(path) -> eax id of the folder with that path, 0 when it is not in the index
fi_resolve:
    PROLOG 96
    mov rsi, rcx
    mov r14, [fi_lower]
    xor r12d, r12d                   ; tree
.vol:
    cmp r12, [fi_nvol]
    jae .no
    mov rax, r12
    shl rax, 4
    lea r13, [fi_vol]
    add r13, rax
    mov ecx, [r13+FV_ROOT]
    call fi_e
    test rax, rax
    jz .vn
    mov r10d, [rax+FI_NAME]
    mov r11, [fi_pool]
    lea r10, [r11+r10*2]             ; name of the tree's top
    movzx r11d, word [rax+FI_LEN]
    xor ecx, ecx
.pre:
    cmp ecx, r11d
    jae .pred
    movzx eax, word [rsi+rcx*2]
    test eax, eax
    jz .vn
    movzx eax, word [r14+rax*2]
    movzx edx, word [r10+rcx*2]
    movzx edx, word [r14+rdx*2]
    cmp eax, edx
    jne .vn
    inc ecx
    jmp .pre
.pred:
    movzx eax, word [rsi+rcx*2]
    test eax, eax
    jz .top
    cmp eax, 0x5C
    jne .vn
    ; ---- below the top: one name at a time
    lea rdi, [rsi+rcx*2]
    mov ebx, [r13+FV_ROOT]           ; current folder
.comp:
    cmp word [rdi], 0x5C
    jne .cs
    add rdi, 2
    jmp .comp
.cs:
    cmp word [rdi], 0
    je .found
    xor r15d, r15d                   ; length of this name
.cl:
    movzx eax, word [rdi+r15*2]
    test eax, eax
    jz .look
    cmp eax, 0x5C
    je .look
    inc r15d
    jmp .cl
.look:
    mov ebp, [r13+FV_FIRST]          ; scan the tree for the child with that name
    mov eax, [r13+FV_END]
    test eax, eax
    jnz .le
    mov rax, [fi_n]
.le:
    mov [rsp+80], rax
.ln:
    cmp rbp, [rsp+80]
    jae .no
    mov rax, rbp
    shl rax, FI_SHIFT
    add rax, [fi_ent]
    cmp [rax+FI_PARENT], ebx
    jne .lnx
    movzx r11d, word [rax+FI_LEN]
    cmp r11d, r15d
    jne .lnx
    mov r10d, [rax+FI_NAME]
    mov r8, [fi_pool]
    lea r10, [r8+r10*2]
    mov r8, rdi
    mov r9d, r15d
    call fs_cmpw
    test eax, eax
    jz .lhit
.lnx:
    inc rbp
    jmp .ln
.lhit:
    mov ebx, ebp
    lea rdi, [rdi+r15*2]
    jmp .comp
.found:
    mov eax, ebx
    EPILOG 96
.top:
    mov eax, [r13+FV_ROOT]
    EPILOG 96
.vn:
    inc r12
    jmp .vol
.no:
    xor eax, eax
    EPILOG 96

; fi_cloud_add(path): remember the folder as cloud storage when it is in the index
fi_cloud_add:
    PROLOG 64
    mov rsi, rcx
    cmp word [rsi], 0
    je .d
    invoke wcslen, rsi               ; without a trailing separator
    test eax, eax
    jz .d
    lea rcx, [rsi+rax*2-2]
    cmp word [rcx], 0x5C
    jne .r
    mov word [rcx], 0
.r:
    invoke fi_resolve, rsi
    test eax, eax
    jz .d
    mov rcx, [fi_ncloud]
    cmp rcx, FI_MAXCLOUD
    jae .d
    lea rdx, [fi_cloud]
    xor r8d, r8d
.dup:
    cmp r8, rcx
    jae .new
    cmp [rdx+r8*4], eax
    je .d
    inc r8
    jmp .dup
.new:
    mov [rdx+rcx*4], eax
    inc qword [fi_ncloud]
.d:
    EPILOG 64

; fi_cloud_list(text): folders separated by ';'
fi_cloud_list:
    PROLOG 64
    mov rsi, rcx
.n:
    cmp word [rsi], 0
    je .d
    mov rdi, rsi
.s:
    movzx eax, word [rdi]
    test eax, eax
    jz .last
    cmp eax, ';'
    je .cut
    add rdi, 2
    jmp .s
.cut:
    mov word [rdi], 0
    invoke fi_cloud_add, rsi
    lea rsi, [rdi+2]
    jmp .n
.last:
    invoke fi_cloud_add, rsi
.d:
    EPILOG 64

; fi_cloud_init(): find the cloud storage folders - BC_FIND_CLOUD when set; otherwise the OneDrive
;   variables of the environment and every sync root registered with Windows (OneDrive, Dropbox,
;   iCloud Drive, Google Drive in mirror mode, ...)
%define FC_BUF  96                   ; wchar[1024] -> 2144
%define FC_SUB  2144                 ; wchar[400] -> 2944
%define FC_KEY  2944
%define FC_KEY2 2952
%define FC_LEN  2960
%define FC_TYPE 2968
%define FC_CB   2976
%define FC_VN   2984                 ; value name wchar[130] -> 3244
%define FC_VL   3248
fi_cloud_init:
    PROLOG 3264
    invoke GetEnvironmentVariableW, &s_env_cloud, &rsp+FC_BUF, 1000
    test eax, eax
    jz .detect
    cmp eax, 1000
    jae .d
    invoke fi_cloud_list, &rsp+FC_BUF
    jmp .d
.detect:
    lea r12, [s_env_od1]
    call .env
    lea r12, [s_env_od2]
    call .env
    lea r12, [s_env_od3]
    call .env
    mov qword [rsp+FC_KEY], 0
    mov rbx, 0xFFFFFFFF80000002      ; HKEY_LOCAL_MACHINE
    invoke RegOpenKeyExW, rbx, &s_reg_sync, 0, 0x20019, &rsp+FC_KEY
    test eax, eax
    jnz .d
    xor r13d, r13d                   ; sync root
.k:
    mov dword [rsp+FC_LEN], 300
    invoke RegEnumKeyExW, [rsp+FC_KEY], r13, &rsp+FC_SUB, &rsp+FC_LEN, 0, 0, 0, 0
    test eax, eax
    jnz .kd
    invoke wcscat, &rsp+FC_SUB, &s_reg_user
    mov qword [rsp+FC_KEY2], 0
    invoke RegOpenKeyExW, [rsp+FC_KEY], &rsp+FC_SUB, 0, 0x20019, &rsp+FC_KEY2
    test eax, eax
    jnz .kn
    xor r14d, r14d                   ; user
.v:
    mov dword [rsp+FC_VL], 128
    mov dword [rsp+FC_CB], 2000
    invoke RegEnumValueW, [rsp+FC_KEY2], r14, &rsp+FC_VN, &rsp+FC_VL, 0, &rsp+FC_TYPE, &rsp+FC_BUF, &rsp+FC_CB
    test eax, eax
    jnz .vd
    mov eax, [rsp+FC_TYPE]
    cmp eax, 1                       ; REG_SZ
    je .vs
    cmp eax, 2                       ; REG_EXPAND_SZ
    jne .vn
.vs:
    mov eax, [rsp+FC_CB]
    cmp eax, 2000
    jae .vn
    mov word [rsp+FC_BUF+rax], 0     ; the data may come without a terminator
    invoke fi_cloud_add, &rsp+FC_BUF
.vn:
    inc r14d
    cmp r14d, 64
    jb .v
.vd:
    invoke RegCloseKey, [rsp+FC_KEY2]
.kn:
    inc r13d
    cmp r13d, 64
    jb .k
.kd:
    invoke RegCloseKey, [rsp+FC_KEY]
.d:
    EPILOG 3264
.env:                                ; local: the folder an environment variable names (rsp is 8 lower)
    sub rsp, 40
    lea rdx, [rsp+48+FC_BUF]
    invoke GetEnvironmentVariableW, r12, rdx, 1000
    test eax, eax
    jz .ed
    cmp eax, 1000
    jae .ed
    lea rcx, [rsp+48+FC_BUF]
    call fi_cloud_add
.ed:
    add rsp, 40
    ret

; ---------------------------------------------------------------- the listing thread
; fi_item(text, length): one entry of BC_FIND_ROOT - "X:" or a folder
fi_item:
    PROLOG 64
    mov rsi, rcx
    mov edi, edx
    test edi, edi
    jz .d
    xor ebx, ebx
    cmp edi, 2
    jne .go
    cmp word [rsi+2], ':'
    jne .go
    movzx ebx, word [rsi]            ; a drive
.go:
    invoke fi_tree, rsi, rdi, rbx
.d:
    EPILOG 64

fi_proc:
    PROLOG 2176
    call fi_init
    test eax, eax
    jz .end
    invoke GetEnvironmentVariableW, &s_env_mode, &rsp+96, 20
    test eax, eax
    jz .root
    movzx eax, word [rsp+96]
    or eax, 0x20
    cmp eax, 'u'
    jne .m2
    mov qword [fi_force], 1
.m2:
    cmp eax, 'w'
    jne .root
    mov qword [fi_force], 2
.root:
    invoke GetEnvironmentVariableW, &s_env_root, &rsp+96, 1000
    test eax, eax
    jz .drives
    cmp eax, 1000
    jae .drives
    lea rsi, [rsp+96]
.it:
    xor ecx, ecx                     ; length of this item
.il:
    movzx eax, word [rsi+rcx*2]
    test eax, eax
    jz .ilast
    cmp eax, ';'
    je .icut
    inc ecx
    jmp .il
.icut:
    mov r12d, ecx
    invoke fi_item, rsi, r12
    lea rsi, [rsi+r12*2+2]
    jmp .it
.ilast:
    mov r12d, ecx
    invoke fi_item, rsi, r12
    jmp .cloud
.drives:
    call GetLogicalDrives
    mov r13d, eax
    xor r14d, r14d
.dl:
    cmp r14d, 26
    jae .cloud
    bt r13d, r14d
    jnc .dn
    lea ebx, [r14+'A']
    mov [rsp+96], bx
    mov dword [rsp+98], 0x005C003A   ; ":\"
    mov word [rsp+102], 0
    invoke GetDriveTypeW, &rsp+96
    cmp eax, 3                       ; DRIVE_FIXED
    jne .dn
    mov word [rsp+100], 0            ; "X:"
    invoke fi_tree, &rsp+96, 2, rbx
.dn:
    inc r14d
    jmp .dl
.cloud:
    call fi_cloud_init
.end:
    mov qword [fi_state], 2
    xor eax, eax
    EPILOG 2176

; find_start(): start listing the drives once; returns at once
find_start:
    PROLOG 64
    cmp qword [fi_state], 0
    jne .d
    mov qword [fi_state], 1
    invoke CreateThread, 0, 0, &fi_proc, 0, 0, 0
    test rax, rax
    jz .fail
    invoke CloseHandle, rax
.d:
    EPILOG 64
.fail:
    mov qword [fi_state], 2
    EPILOG 64
