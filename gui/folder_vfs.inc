; folder_vfs.inc - Folder Compare: virtual sides (ZIP / snapshot), CRC-32, attributes, snapshot save

; ---------------------------------------------------------------- CRC of a disk file
; file_crc(fold, path) -> rax = crc (zero extended) or -1
file_crc:
    PROLOG 128
    mov rsi, rcx
    invoke CreateFileW, rdx, GENERIC_READ, 3, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .err
    mov rdi, rax
    xor ebx, ebx
.l:
    cmp byte [rsi+F_CANCEL], 0
    jne .bad
    invoke ReadFile, rdi, [rsi+F_BUFA], 65536, &rsp+96, 0
    test eax, eax
    jz .bad
    mov r12d, [rsp+96]
    test r12d, r12d
    jz .done
    invoke crc32_update, rbx, [rsi+F_BUFA], r12
    mov ebx, eax
    jmp .l
.done:
    invoke CloseHandle, rdi
    mov eax, ebx
    EPILOG 128
.bad:
    invoke CloseHandle, rdi
.err:
    mov rax, -1
    EPILOG 128

; side_crc(fold, side, te, rel) -> rax crc or -1 (unknown)
side_crc:
    PROLOG 2400
    mov r12, rcx
    mov r13d, edx
    mov r14, r8
    mov r15, r9
    mov rax, [r12+F_KIND+r13*8]
    test rax, rax
    jz .disk
    cmp rax, 2
    je .remote
    test dword [r14+8], HAS_CRC
    jz .unknown
    mov eax, [r14+12]
    EPILOG 2400
.unknown:
    mov rax, -1
    EPILOG 2400
.remote:
    invoke fold_side_path, r12, r13, r15, &rsp+96
    test eax, eax
    jz .unknown
    invoke file_crc, r12, &rsp+96
    EPILOG 2400
.disk:
    lea rbx, [r12+F_ROOTA]
    test r13d, r13d
    jz .p
    lea rbx, [r12+F_ROOTB]
.p:
    invoke _snwprintf, &rsp+96, 1100, &f_path, rbx, r15
    invoke file_crc, r12, &rsp+96
    EPILOG 2400

; ---------------------------------------------------------------- virtual side helpers
; fold_free_vt(fold)
fold_free_vt:
    PROLOG 64
    mov rbx, rcx
    invoke ftp_close, rbx
    cmp qword [rbx+F_KIND], 1
    jne .b
    invoke vt_free, &rbx+F_VTA
.b:
    cmp qword [rbx+F_KIND+8], 1
    jne .d
    invoke vt_free, &rbx+F_VTB
.d:
    xor eax, eax
    mov [rbx+F_KIND], rax
    mov [rbx+F_KIND+8], rax
    EPILOG 64

; fold_open_root(fold, side, path) -> eax 1 ok : sets the kind (0 folder, 1 archive/snapshot)
fold_open_root:
    PROLOG 128
    mov rbx, rcx
    mov r12d, edx
    mov r13, r8
    invoke ftp_is_url, r13
    test eax, eax
    jz .local
    invoke ftp_open, rbx, r12, r13
    EPILOG 128
.local:
    invoke GetFileAttributesW, r13
    cmp eax, -1
    je .bad
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jz .file
    mov qword [rbx+F_KIND+r12*8], 0
    mov eax, 1
    EPILOG 128
.file:
    lea r14, [rbx+F_VTA]
    test r12d, r12d
    jz .t
    lea r14, [rbx+F_VTB]
.t:
    invoke wcsrchr, r13, '.'
    test rax, rax
    jz .zip
    invoke _wcsicmp, rax, &s_ext_bcss
    test eax, eax
    jnz .zip
    invoke snap_open, r13, r14
    jmp .res
.zip:
    invoke zip_open, r13, r14
.res:
    test eax, eax
    jz .bad
    mov qword [rbx+F_KIND+r12*8], 1
    mov eax, 1
    EPILOG 128
.bad:
    xor eax, eax
    EPILOG 128

; vt_find(VT*, name) -> VE* or 0
vt_find:
    PROLOG 64
    mov rbx, rcx
    mov r12, rdx
    xor esi, esi
.l:
    cmp rsi, [rbx+VT_N_]
    jae .none
    mov rax, rsi
    imul rax, 56
    add rax, [rbx]
    mov rdi, rax
    invoke _wcsicmp, [rdi], r12
    test eax, eax
    jz .hit
    inc rsi
    jmp .l
.hit:
    mov rax, rdi
    EPILOG 64
.none:
    xor eax, eax
    EPILOG 64

; fold_side_path(fold, side, rel, out(520 wchar)) -> eax 1 ok : a real file path for this side
;   folders -> root\rel ; archives -> the entry is extracted to %TEMP%\BeCompareAsm\<side>\<name>
fold_side_path:
    PROLOG 1600
    mov rbx, rcx
    mov r12d, edx
    mov r13, r8
    mov r14, r9
    cmp qword [rbx+F_KIND+r12*8], 0
    jne .virt
    lea r15, [rbx+F_ROOTA]
    test r12d, r12d
    jz .a
    lea r15, [rbx+F_ROOTB]
.a:
    test r12d, r12d
    jz .noaltp
    invoke alt_bpath, rbx, r13
    mov r13, rax
.noaltp:
    invoke _snwprintf, r14, 519, &f_path, r15, r13
    mov eax, 1
    EPILOG 1600
.virt:
    invoke wcslen, r13
    invoke vt_safe_name, r13, rax
    test eax, eax
    jz .fail
    mov [rsp+1560], r13              ; the original relative name
    cmp qword [rbx+F_KIND+r12*8], 2
    je .tmp
    lea rsi, [rbx+F_VTA]
    test r12d, r12d
    jz .have
    lea rsi, [rbx+F_VTB]
.have:
    cmp qword [rsi+VT_KIND_], 0
    jne .nosnap                      ; a snapshot has no contents
    invoke vt_find, rsi, r13
    test rax, rax
    jz .fail
    mov rdi, rax
.tmp:
    invoke GetTempPathW, 360, &rsp+96
    test eax, eax
    jz .fail
    cmp eax, 360
    jae .fail
    invoke wcscat, &rsp+96, &s_tmpdir
    invoke CreateDirectoryW, &rsp+96, 0
    mov r15d, 'A'
    test r12d, r12d
    jz .side
    mov r15d, 'B'
.side:
    call GetCurrentProcessId
    mov ebp, eax
    invoke _snwprintf, r14, 519, &f_tmpuniq, &rsp+96, rbp, rbx, r15   ; <tmp>\<pid>_<session>\A
    invoke wcslen, r14
    mov [rsp+1552], rax
    invoke wcslen, r13
    add rax, [rsp+1552]
    add rax, 2                      ; separator + terminator within out wchar[520]
    cmp rax, 520
    ja .fail
    invoke SHCreateDirectoryExW, 0, r14, 0
    test eax, eax
    jz .rootready
    cmp eax, 183                    ; ERROR_ALREADY_EXISTS
    jne .fail
.rootready:
    mov rax, [rsp+1552]
    lea r15, [r14+rax*2]
    mov word [r15], 0x5C
    invoke wcscpy, &r15+2, r13       ; keep the relative path
    invoke wcsrchr, r14, 0x5C
    mov rbp, rax
    mov word [rbp], 0
    invoke SHCreateDirectoryExW, 0, r14, 0
    mov word [rbp], 0x5C
    test eax, eax
    jz .parentready
    cmp eax, 183
    jne .fail
.parentready:
    cmp qword [rbx+F_KIND+r12*8], 2
    je .dl
    invoke zip_extract, rsi, rdi, r14
    EPILOG 1600
.dl:
    invoke ftp_download, rbx, r12, [rsp+1560], r14
    EPILOG 1600
.nosnap:
    invoke set_status, &s_snap_nocontent
.fail:
    xor eax, eax
    EPILOG 1600

; ---------------------------------------------------------------- snapshot save
; fold_snapshot(hwnd, fold): write the left side as a .bcss file (CRC-32 only when a content criterion is on)
%define SN_SB   96
%define SN_CRC  136
%define SN_LINE 144
%define SN_SAVE 1400
%define SN_PATH 2448
fold_snapshot:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+F_NENT], 0
    je .norun
    lea rdi, [rsp+96]
    mov word [rdi], 0
    invoke browse_save, rsi, rdi
    test eax, eax
    jz .d
    invoke fold_snapshot_path, rbx, rdi
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &s_norun, MB_OK|MB_ICONINFO
    EPILOG 1200

; fold_snapshot_path(fold, path): worker (also used by /snapshot)
fold_snapshot_path:
    PROLOG 3504
    mov rbx, rcx
    lea rdi, [rsp+SN_SAVE]
    invoke wcscpy, rdi, rdx
    invoke wcsrchr, rdi, '.'
    test rax, rax
    jnz .ext
    invoke wcscat, rdi, &s_ext_bcss
.ext:
    invoke sb_init, &rsp+SN_SB
    invoke sb_append, &rsp+SN_SB, &s_snap_head
    xor r12d, r12d
.l:
    cmp r12, [rbx+F_NENT]
    jae .fin
    mov rax, r12
    shl rax, 6
    add rax, [rbx+F_ENTS]
    mov r13, rax
    test dword [r13+E_FLAGS], EF_EXA
    jz .n
    mov qword [rsp+SN_CRC], -1
    xor r14d, r14d                   ; attribute value to store
    test dword [r13+E_FLAGS], EF_DIRA
    jz .file
    mov r14d, FILE_ATTRIBUTE_DIRECTORY
    jmp .line
.file:
    test dword [rbx+F_OPT], 0x24
    jz .line
    cmp qword [rbx+F_KIND], 0
    jne .line
    invoke _snwprintf, &rsp+SN_PATH, 519, &f_path, &rbx+F_ROOTA, [r13+E_NAME]
    invoke file_crc, rbx, &rsp+SN_PATH
    mov [rsp+SN_CRC], rax
.line:
    mov r15, [rsp+SN_CRC]
    cmp r15, -1
    je .nocrc
    invoke _snwprintf, &rsp+SN_LINE, 600, &f_snap_crc, [r13+E_NAME], r14, [r13+E_SIZEA], [r13+E_MTA], r15
    jmp .add
.nocrc:
    invoke _snwprintf, &rsp+SN_LINE, 600, &f_snap_nocrc, [r13+E_NAME], r14, [r13+E_SIZEA], [r13+E_MTA]
.add:
    invoke sb_append, &rsp+SN_SB, &rsp+SN_LINE
.n:
    inc r12
    jmp .l
.fin:
    invoke write_utf8, rdi, [rsp+SN_SB], [rsp+SN_SB+8], 1
    mov ebp, eax
    invoke sb_free, &rsp+SN_SB
    test ebp, ebp
    jz .savefail
    invoke set_status, &s_snap_saved
    mov eax, 1
    EPILOG 3504
.savefail:
    invoke set_status, &s_report_fail
    xor eax, eax
.d:
    EPILOG 3504
