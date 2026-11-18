; vtab.asm - virtual directory tables: ZIP archives (AR-01) and folder snapshots (SN-01)
%include "inc/macros.inc"
%include "inc/win.inc"

extern malloc, free, memcpy, memset, wcslen, wcscpy, _wcsicmp, _wcsnicmp, _wcstoui64, qsort, realloc
extern MultiByteToWideChar, DosDateTimeToFileTime, LocalFileTimeToFileTime, CreateFileW, WriteFile, CloseHandle
extern read_file_bytes, vec_push, str_dup, text_load, lines_split
extern inflate_buf, crc32_update

global zip_open, zip_extract, snap_open, vt_enum, vt_free

; VT { ve*, count, data*, datasize, kind }
%define VT_VE    0
%define VT_N     8
%define VT_CAP   16
%define VT_DATA  24
%define VT_SIZE  32
%define VT_KIND  40
%define VT_STRUCT 48

; VE (56 bytes)
%define VE_NAME  0
%define VE_ATTR  8
%define VE_CRC   12
%define VE_SIZE  16
%define VE_MTIME 24
%define VE_LHO   32
%define VE_CSIZE 40
%define VE_METH  48
%define VE_IMPL  52
%define VE_BYTES 56

%define HAS_CRC 0x80000000

section .rdata
WSTR s_hdr, "BCSS1"

section .text

; cmp_ve(a, b): by name (case-insensitive); real entries before implied ones
cmp_ve:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    invoke _wcsicmp, [rsi], [rdi]
    test eax, eax
    jnz .d
    mov eax, [rsi+VE_IMPL]
    sub eax, [rdi+VE_IMPL]
.d:
    EPILOG 64

; ve_push(vt-vec: VT*, name (owned), attr, size...) helper: ve_new(vt) -> zeroed entry pointer
ve_new:
    PROLOG 64
    mov rbx, rcx
    lea rcx, [rbx+VT_VE]
    mov edx, VE_BYTES
    call vec_push
    mov rdi, rax
    invoke memset, rdi, 0, VE_BYTES
    mov rax, rdi
    EPILOG 64

; vt_sort(VT*): sort and drop duplicate names (keeping the first = real entry)
vt_sort:
    PROLOG 64
    mov rbx, rcx
    mov rax, [rbx+VT_N]
    cmp rax, 1
    jbe .d
    invoke qsort, [rbx+VT_VE], rax, VE_BYTES, &cmp_ve
    xor esi, esi                     ; read
    xor edi, edi                     ; write
.l:
    cmp rsi, [rbx+VT_N]
    jae .fin
    mov rax, rsi
    imul rax, VE_BYTES
    add rax, [rbx+VT_VE]
    test rdi, rdi
    jz .keep
    mov rcx, rdi
    dec rcx
    imul rcx, VE_BYTES
    add rcx, [rbx+VT_VE]
    mov r12, rax
    mov r13, rcx
    invoke _wcsicmp, [r12], [r13]
    test eax, eax
    jnz .keep2
    invoke free, [r12]               ; duplicate name: drop
    inc rsi
    jmp .l
.keep2:
    mov rax, r12
.keep:
    mov rcx, rdi
    imul rcx, VE_BYTES
    add rcx, [rbx+VT_VE]
    cmp rcx, rax
    je .same
    invoke memcpy, rcx, rax, VE_BYTES
.same:
    inc rdi
    inc rsi
    jmp .l
.fin:
    mov [rbx+VT_N], rdi
.d:
    EPILOG 64

; vt_implied(VT*): add the missing parent directories of every entry
vt_implied:
    PROLOG 2400
    mov rbx, rcx
    mov r12, [rbx+VT_N]              ; only the original entries
    xor esi, esi
.e:
    cmp rsi, r12
    jae .d
    mov rax, rsi
    imul rax, VE_BYTES
    add rax, [rbx+VT_VE]
    mov rdi, [rax+VE_NAME]
    xor ecx, ecx
.c:
    movzx eax, word [rdi+rcx*2]
    test eax, eax
    jz .next
    cmp eax, 0x5C
    jne .cn
    cmp ecx, 1000
    ja .cn
    ; prefix [0, ecx) is a directory
    mov r13, rcx
    lea rdx, [rsp+96]
    mov r8, rcx
    shl r8, 1
    invoke memcpy, rdx, rdi, r8
    mov word [rsp+96+r13*2], 0
    invoke ve_new, rbx
    mov r14, rax
    invoke str_dup, &rsp+96
    mov [r14+VE_NAME], rax
    mov dword [r14+VE_ATTR], FILE_ATTRIBUTE_DIRECTORY
    mov dword [r14+VE_IMPL], 1
    mov rax, rsi
    imul rax, VE_BYTES
    add rax, [rbx+VT_VE]
    mov rdi, [rax+VE_NAME]           ; the vector may have moved
    mov rcx, r13
.cn:
    inc rcx
    jmp .c
.next:
    inc rsi
    jmp .e
.d:
    EPILOG 2400

; ---------------------------------------------------------------- ZIP
; zip_open(path, VT*) -> eax 1 ok
zip_open:
    PROLOG 256
    mov r12, rcx
    mov rbx, rdx
    invoke memset, rbx, 0, VT_STRUCT
    invoke read_file_bytes, r12, &rbx+VT_SIZE
    mov [rbx+VT_DATA], rax
    test rax, rax
    jz .fail
    mov rsi, rax
    mov rdi, [rbx+VT_SIZE]
    cmp rdi, 22
    jb .fail
    ; end of central directory
    lea rcx, [rdi-22]
    mov rax, rdi
    sub rax, 65557
    jns .lim
    xor eax, eax
.lim:
    mov r13, rax
.eocd:
    cmp dword [rsi+rcx], 0x06054b50
    je .found
    cmp rcx, r13
    jbe .fail
    dec rcx
    jmp .eocd
.found:
    movzx r14d, word [rsi+rcx+10]    ; total entries
    mov r15d, [rsi+rcx+16]           ; central directory offset
    lea rbp, [rsi+r15]               ; cursor
    mov [rsp+96], r14
.entry:
    mov rax, [rsp+96]
    test rax, rax
    jz .built
    dec qword [rsp+96]
    ; bounds / signature
    mov rax, rbp
    sub rax, rsi
    add rax, 46
    cmp rax, rdi
    ja .built
    cmp dword [rbp], 0x02014b50
    jne .built
    movzx r8d, word [rbp+8]          ; flags
    movzx r9d, word [rbp+10]         ; method
    movzx r10d, word [rbp+28]        ; name length
    movzx r11d, word [rbp+30]
    movzx eax, word [rbp+32]
    mov [rsp+104], r8
    mov [rsp+112], r9
    mov [rsp+120], r10
    lea rcx, [r11+rax]
    lea rcx, [rcx+r10+46]
    mov [rsp+128], rcx               ; total record size
    mov rax, rbp                     ; the whole record (its name) must lie inside the file
    sub rax, rsi
    add rax, rcx
    cmp rax, rdi
    ja .built
    ; name -> wide
    mov ecx, CP_UTF8
    test r8d, 0x800
    jnz .cp
    mov ecx, 1                       ; CP_OEMCP
.cp:
    lea rdx, [rbp+46]
    mov [rsp+136], rdx
    mov [rsp+144], rcx
    invoke MultiByteToWideChar, [rsp+144], 0, [rsp+136], [rsp+120], 0, 0
    mov r13, rax
    lea rcx, [rax*2+8]
    call malloc
    mov r14, rax
    invoke MultiByteToWideChar, [rsp+144], 0, [rsp+136], [rsp+120], r14, r13
    mov word [r14+r13*2], 0
    ; '/' -> '\'
    xor ecx, ecx
.sl:
    cmp rcx, r13
    jae .sld
    cmp word [r14+rcx*2], 0x2F
    jne .sln
    mov word [r14+rcx*2], 0x5C
.sln:
    inc rcx
    jmp .sl
.sld:
    xor r15d, r15d                   ; attr
    test r13, r13
    jz .skip
    cmp word [r14+r13*2-2], 0x5C
    jne .nodir
    mov word [r14+r13*2-2], 0
    mov r15d, FILE_ATTRIBUTE_DIRECTORY
.nodir:
    invoke ve_new, rbx
    mov rdi, rax
    mov [rdi+VE_NAME], r14
    or r15d, HAS_CRC
    mov [rdi+VE_ATTR], r15d
    mov eax, [rbp+16]
    mov [rdi+VE_CRC], eax
    mov eax, [rbp+24]
    mov [rdi+VE_SIZE], rax
    mov eax, [rbp+20]
    mov [rdi+VE_CSIZE], rax
    mov eax, [rbp+42]
    mov [rdi+VE_LHO], rax
    mov rax, [rsp+112]
    mov [rdi+VE_METH], eax
    movzx ecx, word [rbp+14]         ; DOS date
    movzx edx, word [rbp+12]         ; DOS time
    invoke DosDateTimeToFileTime, rcx, rdx, &rdi+VE_MTIME
    invoke LocalFileTimeToFileTime, &rdi+VE_MTIME, &rdi+VE_MTIME
    mov rsi, [rbx+VT_DATA]
    mov rdi, [rbx+VT_SIZE]
    jmp .adv
.skip:
    invoke free, r14
.adv:
    mov rsi, [rbx+VT_DATA]
    mov rdi, [rbx+VT_SIZE]
    add rbp, [rsp+128]
    jmp .entry
.built:
    invoke vt_implied, rbx
    invoke vt_sort, rbx
    mov qword [rbx+VT_KIND], 0
    mov eax, 1
    EPILOG 256
.fail:
    invoke vt_free, rbx
    xor eax, eax
    EPILOG 256

; zip_extract(VT*, VE*, dest path) -> eax 1 ok
zip_extract:
    PROLOG 256
    mov rbx, rcx
    mov rsi, rdx
    mov r12, r8
    mov rdi, [rbx+VT_DATA]
    mov rax, [rsi+VE_LHO]
    lea rcx, [rax+30]
    cmp rcx, [rbx+VT_SIZE]
    ja .fail
    cmp dword [rdi+rax], 0x04034b50
    jne .fail
    movzx edx, word [rdi+rax+26]
    movzx r8d, word [rdi+rax+28]
    lea r13, [rcx+rdx]
    add r13, r8                      ; data offset
    mov r14, [rsi+VE_CSIZE]
    lea rax, [r13+r14]
    cmp rax, [rbx+VT_SIZE]
    ja .fail
    lea r15, [rdi+r13]               ; compressed data
    mov rbp, [rsi+VE_SIZE]
    cmp dword [rsi+VE_METH], 0
    je .stored
    cmp dword [rsi+VE_METH], 8
    jne .fail
    lea rcx, [rbp+16]
    call malloc
    mov [rsp+96], rax
    test rax, rax
    jz .fail
    invoke inflate_buf, r15, r14, rax, rbp
    test eax, eax
    jz .fail2
    mov rbp, rdx                     ; what was produced, not the size the directory claims
    mov r15, [rsp+96]
    jmp .write
.stored:
    mov rbp, r14
.write:
    invoke CreateFileW, r12, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .fail2
    mov r13, rax
    test rbp, rbp
    jz .close
    invoke WriteFile, r13, r15, rbp, &rsp+104, 0
.close:
    invoke CloseHandle, r13
    cmp dword [rsi+VE_METH], 8
    jne .ok
    invoke free, [rsp+96]
.ok:
    mov eax, 1
    EPILOG 256
.fail2:
    cmp dword [rsi+VE_METH], 8
    jne .fail
    invoke free, [rsp+96]
.fail:
    xor eax, eax
    EPILOG 256

; ---------------------------------------------------------------- snapshot (text): BCSS1 / path TAB attr TAB size TAB mtime TAB crc
; snap_open(path, VT*) -> eax 1 ok
%define N_TXT 96                     ; TXT (32 bytes) at [rsp+96]
%define N_LNS 128                    ; LINES (16)
snap_open:
    PROLOG 256
    mov rbx, rdx
    mov rsi, rcx
    invoke memset, rbx, 0, VT_STRUCT
    invoke text_load, rsi, &rsp+N_TXT
    test eax, eax
    jz .fail
    mov rax, [rsp+N_TXT]
    mov [rbx+VT_DATA], rax           ; kept so the strings stay valid
    invoke lines_split, [rsp+N_TXT], [rsp+N_TXT+8], &rsp+N_LNS
    mov rcx, [rsp+N_LNS+8]
    test rcx, rcx
    jz .fail
    mov rax, [rsp+N_LNS]
    mov rsi, [rax]
    invoke _wcsicmp, rsi, &s_hdr
    test eax, eax
    jnz .fail
    mov r12d, 1                      ; line index
.l:
    cmp r12, [rsp+N_LNS+8]
    jae .built
    mov rax, [rsp+N_LNS]
    mov rsi, [rax+r12*8]             ; line
    ; fields split on TAB (in place)
    mov rdi, rsi
    xor r13d, r13d                   ; field number
    lea r14, [rsp+160]               ; field pointers [5]
    mov [r14], rsi
    mov r13d, 1
.f:
    movzx eax, word [rdi]
    test eax, eax
    jz .fd
    cmp eax, 9
    jne .fn
    mov word [rdi], 0
    cmp r13d, 5
    jae .fn
    lea rcx, [rdi+2]
    mov [r14+r13*8], rcx
    inc r13d
.fn:
    add rdi, 2
    jmp .f
.fd:
    cmp r13d, 5
    jb .skip
    invoke ve_new, rbx
    mov r15, rax
    mov rsi, [r14]
    invoke str_dup, rsi
    mov [r15+VE_NAME], rax
    invoke _wcstoui64, [r14+8], 0, 16
    mov [r15+VE_ATTR], eax
    invoke _wcstoui64, [r14+16], 0, 10
    mov [r15+VE_SIZE], rax
    invoke _wcstoui64, [r14+24], 0, 10
    mov [r15+VE_MTIME], rax
    mov rcx, [r14+32]
    cmp word [rcx], 0
    je .skip
    invoke _wcstoui64, rcx, 0, 16
    mov [r15+VE_CRC], eax
    or dword [r15+VE_ATTR], HAS_CRC
.skip:
    inc r12
    jmp .l
.built:
    invoke vt_implied, rbx
    invoke vt_sort, rbx
    mov qword [rbx+VT_KIND], 1
    invoke free, [rsp+N_LNS]
    mov eax, 1
    EPILOG 256
.fail:
    invoke vt_free, rbx
    xor eax, eax
    EPILOG 256

; ---------------------------------------------------------------- enumeration
; vt_enum(VT*, rel, vec*): direct children of a directory as TE {name*, attr dd, crc dd, size, mtime}
vt_enum:
    PROLOG 2304
    mov rbx, rcx
    mov r13, rdx
    mov r14, r8
    ; prefix = rel + "\"
    mov word [rsp+96], 0
    xor r15d, r15d                   ; prefix length
    cmp word [r13], 0
    je .have
    invoke wcslen, r13
    mov r15, rax
    cmp rax, 1000
    ja .d
    lea r8, [rax*2+2]
    invoke memcpy, &rsp+96, r13, r8
    mov word [rsp+96+r15*2], 0x5C
    inc r15
    mov word [rsp+96+r15*2], 0
.have:
    ; lower bound
    xor esi, esi
    mov rdi, [rbx+VT_N]
.lb:
    cmp rsi, rdi
    jae .scan
    lea rax, [rsi+rdi]
    shr rax, 1
    mov r12, rax
    imul rax, VE_BYTES
    add rax, [rbx+VT_VE]
    invoke _wcsicmp, [rax], &rsp+96
    test eax, eax
    js .right
    mov rdi, r12
    jmp .lb
.right:
    lea rsi, [r12+1]
    jmp .lb
.scan:
    cmp rsi, [rbx+VT_N]
    jae .d
    mov rax, rsi
    imul rax, VE_BYTES
    add rax, [rbx+VT_VE]
    mov r12, rax                     ; entry
    test r15, r15
    jz .nopre
    invoke _wcsnicmp, [r12], &rsp+96, r15
    test eax, eax
    jnz .d                           ; left the subtree
.nopre:
    mov rdi, [r12]
    lea rdi, [rdi+r15*2]             ; remainder
    cmp word [rdi], 0
    je .next                         ; the directory itself
    mov rcx, rdi
.chk:
    movzx eax, word [rcx]
    test eax, eax
    jz .child
    cmp eax, 0x5C
    je .next                         ; deeper descendant
    add rcx, 2
    jmp .chk
.child:
    invoke vec_push, r14, 32
    mov rbp, rax
    invoke str_dup, rdi
    mov [rbp], rax
    mov eax, [r12+VE_ATTR]
    mov [rbp+8], eax
    mov eax, [r12+VE_CRC]
    mov [rbp+12], eax
    mov rax, [r12+VE_SIZE]
    mov [rbp+16], rax
    mov rax, [r12+VE_MTIME]
    mov [rbp+24], rax
.next:
    inc rsi
    jmp .scan
.d:
    EPILOG 2304

; vt_free(VT*)
vt_free:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
.l:
    cmp rsi, [rbx+VT_N]
    jae .d
    mov rax, rsi
    imul rax, VE_BYTES
    add rax, [rbx+VT_VE]
    invoke free, [rax]
    inc rsi
    jmp .l
.d:
    invoke free, [rbx+VT_VE]
    invoke free, [rbx+VT_DATA]
    invoke memset, rbx, 0, VT_STRUCT
    EPILOG 64
