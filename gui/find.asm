; find.asm - file name index of the fixed drives and substring search (Home tab "파일 찾기")
;
;   The first search starts a background thread that lists every file and folder name of the fixed drives
;   into memory; searches then scan that list, which takes a fraction of a second.
;   An NTFS volume is listed from its master file table (FSCTL_ENUM_USN_DATA) when the volume can be
;   opened - that needs administrator rights and takes a few seconds per drive. Otherwise, and for other
;   file systems, the folders are walked with FindFirstFileExW, which takes much longer the first time.
;   The index is not kept up to date afterwards: it shows the drives as they were when it was built.
;
;   BC_FIND_ROOT (environment): "X:" lists only that drive, a folder path lists only that folder (tests).
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern VirtualAlloc, VirtualFree, CreateFileW, CloseHandle, DeviceIoControl, CreateThread
extern FindFirstFileExW, FindNextFileW, FindClose, GetLogicalDrives, GetDriveTypeW, GetEnvironmentVariableW
extern CharLowerBuffW, memcpy, wcslen, malloc, realloc, free

global find_start, find_query, find_path, fi_state, fi_vis, fi_mode, fi_cancel

; entry (16 bytes); id 0 means "none"
%define FI_PARENT 0                  ; dd id of the folder it is in
%define FI_NAME   4                  ; dd offset of the name in the pool, in characters
%define FI_LEN    8                  ; dw name length
%define FI_FLAGS  10                 ; dw 1 = folder, 2 = top of a listed tree (its name is a whole path)
%define FI_REC    12                 ; dd parent's file record number while a volume is being read
%define FI_SIZE   16
%define FI_MAXENT  0x2000000         ; 32M entries (512MB of address space, committed as it fills)
%define FI_MAXPOOL 0x40000000        ; 1G characters
%define FI_STEP    0x400000          ; commit step
%define FI_MAXNAME 1000

section .rdata
WSTR s_env_root, "BC_FIND_ROOT"

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
fi_mode:    resq 1                   ; bit 0: a volume was read from its file table, bit 1: folders were walked
fi_cancel:  resq 1
fi_full:    resq 1                   ; ran out of room

section .text

; fi_init() -> eax 1 ok: reserve the two areas, build the lower case table
fi_init:
    PROLOG 64
    cmp qword [fi_ent], 0
    jne .ok
    invoke VirtualAlloc, 0, FI_MAXENT*FI_SIZE, 0x2000, 4            ; MEM_RESERVE, PAGE_READWRITE
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

; fi_add(parent id, name, length in characters, flags, parent record number) -> eax id, 0 when not added
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
    shl rax, 4
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
    shl rax, 4
    add rax, [fi_ent]
    mov [rax+FI_PARENT], r12d
    mov [rax+FI_NAME], edi
    mov [rax+FI_LEN], r13w
    mov [rax+FI_FLAGS], r14w
    mov [rax+FI_REC], r15d
    lea rax, [rbx+1]
    mov [fi_n], rax
    mov eax, ebx
    EPILOG 64
.full:
    mov qword [fi_full], 1
.no:
    xor eax, eax
    EPILOG 64

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
    shl rax, 4
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
    shl rax, 4
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

; find_query(text, out dd[], capacity) -> eax number of names that contain the text (case ignored);
;   the first `capacity` ids are stored. The whole index is always scanned.
find_query:
    PROLOG 2144
    mov rsi, rcx
    mov rdi, rdx
    mov r12d, r8d
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
    test r14d, r14d
    jz .zero
    movzx r15d, word [rsp+96]        ; its first character
    mov ebx, 1                       ; id
    xor ebp, ebp                     ; found
    mov r8, [fi_ent]
    mov r9, [fi_pool]
.id:
    cmp rbx, [fi_vis]
    jae .done
    mov rax, rbx
    shl rax, 4
    movzx ecx, word [r8+rax+FI_LEN]
    sub ecx, r14d                    ; last position the text can start at
    js .next
    mov edx, [r8+rax+FI_NAME]
    lea r10, [r9+rdx*2]
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
    EPILOG 2144
.zero:
    xor eax, eax
    EPILOG 2144

; ---------------------------------------------------------------- listing by walking the folders
; fi_walk(root id): every file and folder below the entry; reparse points are listed but not entered
%define FW_FD   96                   ; WIN32_FIND_DATAW (592) -> 688
%define FW_PATH 688                  ; wchar[1100] -> 2888
%define FW_STK  2888                 ; ids of the folders still to list
%define FW_SN   2896
%define FW_SC   2904
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
    xor ebp, ebp
    test r15d, 0x10
    jz .add
    mov ebp, 1
.add:
    invoke fi_add, r13, rsi, r14, rbp, 0
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

; ---------------------------------------------------------------- listing an NTFS volume from its file table
; fi_mft(drive letter, id of the drive's entry) -> eax 1 listed, 0 not possible (no rights, not NTFS)
;   Every file record gives a name and the record number of its folder; record numbers are turned into
;   entry ids once the whole volume has been read. Record 5 is the root folder.
%define FM_VOL  96                   ; "\\.\X:"
%define FM_NV   112                  ; NTFS_VOLUME_DATA_BUFFER (96) -> 208
%define FM_MED  208                  ; MFT_ENUM_DATA_V0 (24) -> 232
%define FM_RET  232
%define FM_MAP  240                  ; dd[records]: record number -> id
%define FM_NREC 248
%define FM_BASE 256                  ; first id of this volume
%define FM_BUF  264
%define FM_ROOT 272
%define FM_RLEN 280
%define FM_IOBUF 0x20000
fi_mft:
    PROLOG 288
    mov [rsp+FM_ROOT], rdx
    lea rdi, [rsp+FM_VOL]
    mov dword [rdi], 0x005C005C
    mov dword [rdi+4], 0x005C002E
    mov [rdi+8], cx
    mov word [rdi+10], ':'
    mov word [rdi+12], 0
    invoke CreateFileW, rdi, GENERIC_READ, 3, 0, OPEN_EXISTING, 0, 0
    cmp rax, -1
    je .no
    mov rbx, rax
    invoke DeviceIoControl, rbx, 0x00090064, 0, 0, &rsp+FM_NV, 96, &rsp+FM_RET, 0    ; FSCTL_GET_NTFS_VOLUME_DATA
    test eax, eax
    jz .close0
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
    invoke fi_add, 0, rsi, r14, rdi, rbp
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
    mov r12, [rsp+FM_BASE]
    mov r13, [fi_ent]
    mov r14, [rsp+FM_MAP]
.fx:
    cmp r12, [fi_n]
    jae .fxd
    mov rax, r12
    shl rax, 4
    mov ecx, [r13+rax+FI_REC]
    mov edx, [rsp+FM_ROOT]
    cmp ecx, 5
    je .set
    xor edx, edx                     ; a folder that was not listed: the entry stays without a path
    cmp rcx, [rsp+FM_NREC]
    jae .set
    mov edx, [r14+rcx*4]
.set:
    mov [r13+rax+FI_PARENT], edx
    inc r12
    jmp .fx
.fxd:
    mov rax, [fi_n]
    mov [fi_vis], rax
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

; fi_drive(drive letter): the drive's own entry, then its contents from the file table or by walking
fi_drive:
    PROLOG 64
    mov ebx, ecx
    mov [rsp+48], bx
    mov word [rsp+50], ':'
    mov word [rsp+52], 0
    invoke fi_add, 0, &rsp+48, 2, 3, 0
    test eax, eax
    jz .d
    mov esi, eax
    mov rcx, [fi_n]
    mov [fi_vis], rcx
    invoke fi_mft, rbx, rsi
    test eax, eax
    jz .walk
    or qword [fi_mode], 1
    jmp .d
.walk:
    or qword [fi_mode], 2
    invoke fi_walk, rsi
.d:
    EPILOG 64

; fi_proc: the listing thread
fi_proc:
    PROLOG 2176
    call fi_init
    test eax, eax
    jz .end
    invoke GetEnvironmentVariableW, &s_env_root, &rsp+96, 1000
    test eax, eax
    jz .drives
    cmp eax, 1000
    jae .drives
    cmp eax, 2
    jne .folder
    cmp word [rsp+98], ':'
    jne .folder
    movzx ebx, word [rsp+96]
    invoke fi_drive, rbx
    jmp .end
.folder:
    mov r12d, eax
    invoke fi_add, 0, &rsp+96, r12, 3, 0
    test eax, eax
    jz .end
    mov ebx, eax
    mov rcx, [fi_n]
    mov [fi_vis], rcx
    or qword [fi_mode], 2
    invoke fi_walk, rbx
    jmp .end
.drives:
    call GetLogicalDrives
    mov r13d, eax
    xor r14d, r14d
.dl:
    cmp r14d, 26
    jae .end
    bt r13d, r14d
    jnc .dn
    lea ebx, [r14+'A']
    mov [rsp+96], bx
    mov dword [rsp+98], 0x005C003A   ; ":\"
    mov word [rsp+102], 0
    invoke GetDriveTypeW, &rsp+96
    cmp eax, 3                       ; DRIVE_FIXED
    jne .dn
    invoke fi_drive, rbx
.dn:
    inc r14d
    jmp .dl
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
