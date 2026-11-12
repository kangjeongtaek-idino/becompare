; engine.asm - shared algorithms: vectors, number/time formatting, wildcards, string builder, utf-8 output
%define SNW_IMPL 1
%include "inc/macros.inc"
%include "inc/win.inc"

extern realloc, malloc, free, wcslen, memcpy, _vsnwprintf
global bc_snwprintf
%define _snwprintf bc_snwprintf
extern FileTimeToLocalFileTime, FileTimeToSystemTime
extern WideCharToMultiByte, MultiByteToWideChar, CreateFileW, WriteFile, CloseHandle, ReadFile, GetFileSizeEx

global vec_push, fmt_size, fmt_time, wild_w, excl_match, excl_match2
extern GetFullPathNameW, GetTempFileNameW, GetFileAttributesW, GetLastError
extern FlushFileBuffers, ReplaceFileW, MoveFileExW, DeleteFileW
global safe_write2
global sb_init, sb_append, sb_append_esc, sb_free, write_utf8, read_file_bytes, lower_w

section .rdata
WSTR s_tmp_prefix, "BCA"
WSTR f_time, "%04d-%02d-%02d %02d:%02d:%02d"
WSTR e_amp, "&amp;"
WSTR e_lt, "&lt;"
WSTR e_gt, "&gt;"
WSTR e_quot, "&quot;"

section .text

; bc_snwprintf(rcx=buffer, rdx=count in characters, r8=format, ...) -> eax = characters written
;   The app-wide replacement for _snwprintf (see inc/macros.inc). Text that does not fit is cut to
;   count-1 characters and terminated; the result is then count-1, never negative, so callers that
;   advance a pointer by the result stay inside their buffer.
bc_snwprintf:
    mov [rsp+32], r9                  ; home the 4th argument: the variable arguments are now contiguous
    push rbx
    push rsi
    sub rsp, 40
    mov rbx, rcx
    mov rsi, rdx
    lea r9, [rsp+40+16+32]            ; va_list = address of the first variable argument
    call _vsnwprintf
    test rsi, rsi
    jz .d
    test eax, eax
    js .cut
    cmp eax, esi
    jb .d
.cut:
    mov word [rbx+rsi*2-2], 0
    lea eax, [rsi-1]
.d:
    add rsp, 40
    pop rsi
    pop rbx
    ret

; vec_push(rcx=vec{ptr,count,cap}, rdx=elemsize) -> rax = pointer to the new (uninitialised) element
vec_push:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rax, [rbx+8]
    cmp rax, [rbx+16]
    jb .have
    mov rcx, [rbx+16]
    add rcx, rcx
    jnz .nz
    mov ecx, 64
.nz:
    mov [rbx+16], rcx
    imul rcx, rsi
    mov rdx, rcx
    invoke realloc, [rbx], rdx
    mov [rbx], rax
.have:
    mov rax, [rbx+8]
    imul rax, rsi
    add rax, [rbx]
    inc qword [rbx+8]
    EPILOG 64

; lower_w(ecx = wchar) -> eax lowercase (ASCII + Latin-1/Hangul untouched)
lower_w:
    mov eax, ecx
    cmp eax, 'A'
    jb .d
    cmp eax, 'Z'
    ja .d
    add eax, 32
.d:
    ret

; fmt_size(rcx=buf, rdx=value): decimal with thousands separators
fmt_size:
    PROLOG 192
    mov r12, rcx
    mov rax, rdx
    lea rsi, [rsp+96]                ; reversed text (up to 27 wchar)
    xor ebx, ebx                     ; chars written
    xor r14d, r14d                   ; digits since last comma
    test rax, rax
    jnz .loop
    mov word [rsi], '0'
    mov ebx, 1
    jmp .rev
.loop:
    test rax, rax
    jz .rev
    cmp r14d, 3
    jne .nc
    mov word [rsi+rbx*2], ','
    inc ebx
    xor r14d, r14d
.nc:
    xor edx, edx
    mov ecx, 10
    div rcx
    add edx, '0'
    mov [rsi+rbx*2], dx
    inc ebx
    inc r14d
    jmp .loop
.rev:
    xor ecx, ecx
.cp:
    cmp ecx, ebx
    jae .end
    mov edx, ebx
    dec edx
    sub edx, ecx
    movzx eax, word [rsi+rdx*2]
    mov [r12+rcx*2], ax
    inc ecx
    jmp .cp
.end:
    mov word [r12+rbx*2], 0
    EPILOG 192

; fmt_time(rcx=buf, rdx=FILETIME value) -> "yyyy-mm-dd hh:mm:ss" (empty if 0)
fmt_time:
    PROLOG 192
    mov r12, rcx
    test rdx, rdx
    jnz .go
    mov word [r12], 0
    EPILOG 192
.go:
    mov [rsp+96], rdx
    invoke FileTimeToLocalFileTime, &rsp+96, &rsp+104
    invoke FileTimeToSystemTime, &rsp+104, &rsp+112          ; SYSTEMTIME (16 bytes)
    movzx eax, word [rsp+114]
    mov [rsp+32], rax                ; month
    movzx eax, word [rsp+118]
    mov [rsp+40], rax                ; day
    movzx eax, word [rsp+120]
    mov [rsp+48], rax                ; hour
    movzx eax, word [rsp+122]
    mov [rsp+56], rax                ; minute
    movzx eax, word [rsp+124]
    mov [rsp+64], rax                ; second
    movzx r9d, word [rsp+112]        ; year
    mov rcx, r12
    mov edx, 24
    lea r8, [f_time]
    call _snwprintf
    EPILOG 192

; wild_w(rcx=pattern start, rdx=pattern end, r8=string) -> eax 1 on match (* and ?, case-insensitive)
wild_w:
    push rbx
    push rsi
    push rdi
    xor esi, esi                     ; star pattern pos
    xor edi, edi                     ; star string pos
.loop:
    movzx eax, word [r8]             ; string char
    xor ebx, ebx
    cmp rcx, rdx
    jae .pc
    movzx ebx, word [rcx]            ; pattern char
.pc:
    cmp ebx, '*'
    jne .ns
    add rcx, 2
    cmp rcx, rdx
    jae .yes
    mov rsi, rcx
    mov rdi, r8
    jmp .loop
.ns:
    test eax, eax
    jz .strend
    cmp ebx, '?'
    je .adv
    mov r9d, eax
    cmp r9d, 'A'
    jb .l1
    cmp r9d, 'Z'
    ja .l1
    add r9d, 32
.l1:
    mov r10d, ebx
    cmp r10d, 'A'
    jb .l2
    cmp r10d, 'Z'
    ja .l2
    add r10d, 32
.l2:
    cmp r9d, r10d
    jne .mis
.adv:
    add rcx, 2
    add r8, 2
    jmp .loop
.mis:
    test rsi, rsi
    jz .no
    cmp word [rdi], 0
    je .no
    add rdi, 2
    mov r8, rdi
    mov rcx, rsi
    jmp .loop
.strend:
    cmp rcx, rdx
    jae .yes
    cmp word [rcx], '*'
    jne .no
    add rcx, 2
    jmp .strend
.yes:
    mov eax, 1
    jmp .out
.no:
    xor eax, eax
.out:
    pop rdi
    pop rsi
    pop rbx
    ret

; excl_match(rcx=pattern list "a;b,c", rdx=name) -> eax 1 if any pattern matches
excl_match:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    test rsi, rsi
    jz .no
.seg:
    movzx eax, word [rsi]
    test eax, eax
    jz .no
    cmp eax, ';'
    je .skip
    cmp eax, ','
    je .skip
    mov rbx, rsi
.find:
    movzx eax, word [rbx]
    test eax, eax
    jz .have
    cmp eax, ';'
    je .have
    cmp eax, ','
    je .have
    add rbx, 2
    jmp .find
.have:
    invoke wild_w, rsi, rbx, rdi
    test eax, eax
    jnz .yes
    mov rsi, rbx
    movzx eax, word [rsi]
    test eax, eax
    jz .no
.skip:
    add rsi, 2
    jmp .seg
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; excl_match2(rcx=pattern list, rdx=name, r8=anchored path such as <backslash>rel<backslash>name) -> eax 1 if any pattern matches
;   a pattern containing a backslash is matched against the path, every other pattern against the name
excl_match2:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    test rsi, rsi
    jz .no
.seg:
    movzx eax, word [rsi]
    test eax, eax
    jz .no
    cmp eax, ';'
    je .skip
    cmp eax, ','
    je .skip
    mov rbx, rsi
    xor r13d, r13d                   ; pattern has a backslash
.find:
    movzx eax, word [rbx]
    test eax, eax
    jz .have
    cmp eax, ';'
    je .have
    cmp eax, ','
    je .have
    cmp eax, 0x5C
    jne .nb
    mov r13d, 1
.nb:
    add rbx, 2
    jmp .find
.have:
    mov r8, rdi
    test r13d, r13d
    jz .m
    mov r8, r12
.m:
    invoke wild_w, rsi, rbx, r8
    test eax, eax
    jnz .yes
    mov rsi, rbx
    movzx eax, word [rsi]
    test eax, eax
    jz .no
.skip:
    add rsi, 2
    jmp .seg
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; ---------------------------------------------------------------- string builder (UTF-16)
; SB: { wchar* ptr; qword len; qword cap }

sb_init:
    mov qword [rcx], 0
    mov qword [rcx+8], 0
    mov qword [rcx+16], 0
    ret

; sb_append(rcx=sb, rdx=wide string)
sb_append:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    invoke wcslen, rsi
    mov r12, rax
    ; need len+n+1 chars
    lea rax, [r12+1]
    add rax, [rbx+8]
    cmp rax, [rbx+16]
    jbe .copy
    mov rcx, [rbx+16]
    add rcx, rcx
    cmp rcx, rax
    jae .c1
    mov rcx, rax
.c1:
    cmp rcx, 4096
    jae .sz
    mov ecx, 4096
.sz:
    mov [rbx+16], rcx
    lea rdx, [rcx*2]
    invoke realloc, [rbx], rdx
    mov [rbx], rax
.copy:
    mov rax, [rbx+8]
    mov rcx, [rbx]
    lea rcx, [rcx+rax*2]
    lea r8, [r12*2+2]
    invoke memcpy, rcx, rsi, r8
    add [rbx+8], r12
    EPILOG 64

; sb_append_esc(rcx=sb, rdx=wide string): HTML-escaped append
sb_append_esc:
    PROLOG 96
    mov rbx, rcx
    mov rsi, rdx
.l:
    movzx eax, word [rsi]
    test eax, eax
    jz .d
    cmp eax, '&'
    je .amp
    cmp eax, '<'
    je .lt
    cmp eax, '>'
    je .gt
    cmp eax, '"'
    je .q
    mov [rsp+96], ax
    mov word [rsp+98], 0
    invoke sb_append, rbx, &rsp+96
    jmp .n
.amp:
    invoke sb_append, rbx, &e_amp
    jmp .n
.lt:
    invoke sb_append, rbx, &e_lt
    jmp .n
.gt:
    invoke sb_append, rbx, &e_gt
    jmp .n
.q:
    invoke sb_append, rbx, &e_quot
.n:
    add rsi, 2
    jmp .l
.d:
    EPILOG 96

sb_free:
    PROLOG 64
    mov rbx, rcx
    invoke free, [rbx]
    mov qword [rbx], 0
    mov qword [rbx+8], 0
    mov qword [rbx+16], 0
    EPILOG 64

; write_utf8(rcx=path, rdx=wchar*, r8=length in chars or -1 for NUL-terminated, r9=bom 0/1) -> eax 1 ok
; Write each block completely; a short write is a failure, never a successful save.
write_all:
    PROLOG 112
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
.loop:
    test rdi, rdi
    jz .ok
    mov ebp, 1048576
    cmp rdi, rbp
    cmovb rbp, rdi
    mov dword [rsp+96], 0
    invoke WriteFile, rbx, rsi, rbp, &rsp+96, 0
    test eax, eax
    jz .fail
    cmp [rsp+96], ebp
    jne .fail
    add rsi, rbp
    sub rdi, rbp
    jmp .loop
.ok:
    mov eax, 1
    EPILOG 112
.fail:
    xor eax, eax
    EPILOG 112

; safe_write2(path, p1, n1, p2, n2): stage in the destination directory,
; flush and close, then replace. A failed save never truncates the destination.
safe_write2:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    mov rax, [rsp+272]
    mov [rsp+96], rax
    mov dword [rsp+120], 0           ; owns a successfully created temporary file
    xor ebx, ebx
    xor esi, esi
    xor edi, edi
    invoke malloc, 131072
    test rax, rax
    jz .fail
    mov rbx, rax                     ; absolute destination
    lea rsi, [rbx+65536]             ; temporary path
    mov word [rsi], 0
    invoke GetFullPathNameW, r12, 32768, rbx, &rsp+104
    test eax, eax
    jz .cleanup
    cmp eax, 32768
    jae .cleanup
    mov rbp, [rsp+104]               ; file component (directory ends before it)
    test rbp, rbp
    jz .cleanup
    movzx eax, word [rbp]
    test eax, eax
    jz .cleanup
    mov [rsp+112], eax
    mov word [rbp], 0
    invoke GetTempFileNameW, rbx, &s_tmp_prefix, 0, rsi
    mov edx, [rsp+112]
    mov [rbp], dx
    test eax, eax
    jz .cleanup
    mov dword [rsp+120], 1
    invoke CreateFileW, rsi, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .cleanup
    mov rbp, rax
    invoke write_all, rbp, r13, r14
    test eax, eax
    jz .closefail
    invoke write_all, rbp, r15, [rsp+96]
    test eax, eax
    jz .closefail
    invoke FlushFileBuffers, rbp
    test eax, eax
    jz .closefail
    invoke CloseHandle, rbp
    test eax, eax
    jz .cleanup
    invoke GetFileAttributesW, rbx
    cmp eax, -1
    je .new
    ; Preserve the existing file's ACL and metadata through ReplaceFileW.
    invoke ReplaceFileW, rbx, rsi, 0, 0, 0, 0
    jmp .result
.new:
    invoke GetLastError
    cmp eax, 2                       ; ERROR_FILE_NOT_FOUND only
    jne .cleanup
    ; No replacement flag: if another writer creates the target, fail safely.
    invoke MoveFileExW, rsi, rbx, 8   ; MOVEFILE_WRITE_THROUGH
.result:
    test eax, eax
    jz .cleanup
    mov edi, 1
    jmp .free
.closefail:
    invoke CloseHandle, rbp
.cleanup:
    cmp dword [rsp+120], 0
    je .free
    invoke DeleteFileW, rsi
.free:
    invoke free, rbx
.fail:
    mov eax, edi
    EPILOG 160

; write_utf8(path, wide text, length or -1, BOM) -> 1 only after commit
write_utf8:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    xor esi, esi
    xor ebx, ebx
    test r14, r14
    jz .write
    invoke WideCharToMultiByte, CP_UTF8, 0, r13, r14, 0, 0, 0, 0
    test eax, eax
    jle .fail
    mov ebx, eax
    lea rcx, [rbx+4]
    call malloc
    test rax, rax
    jz .fail
    mov rsi, rax
    invoke WideCharToMultiByte, CP_UTF8, 0, r13, r14, rsi, rbx, 0, 0
    test eax, eax
    jle .freefail
    mov ebx, eax
    cmp r14, -1
    jne .write
    dec ebx
.write:
    mov dword [rsp+96], 0x00BFBBEF
    xor r14d, r14d
    test r15, r15
    jz .commit
    mov r14d, 3
.commit:
    invoke safe_write2, r12, &rsp+96, r14, rsi, rbx
    mov edi, eax
    invoke free, rsi
    mov eax, edi
    EPILOG 160
.freefail:
    invoke free, rsi
.fail:
    xor eax, eax
    EPILOG 160

; read_file_bytes(rcx=path, rdx=&size) -> rax = buffer (size+4 bytes, zero padded) or 0
read_file_bytes:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    invoke CreateFileW, r12, GENERIC_READ, 3, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .fail
    mov rbx, rax
    invoke GetFileSizeEx, rbx, &rsp+96
    test eax, eax
    jz .close
    mov rdi, [rsp+96]
    mov [r13], rdi
    lea rcx, [rdi+4]
    call malloc
    test rax, rax
    jz .close
    mov rsi, rax
    mov qword [rsp+104], 0
    invoke ReadFile, rbx, rsi, rdi, &rsp+104, 0
    mov eax, [rsp+104]
    mov qword [r13], rax
    mov dword [rsi+rax], 0
    invoke CloseHandle, rbx
    mov rax, rsi
    EPILOG 160
.close:
    invoke CloseHandle, rbx
.fail:
    xor eax, eax
    EPILOG 160
