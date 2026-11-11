; ftp.inc - FTP side for Folder Compare (VF-02) through WinInet; included by folder.asm

%define FT_USER  96              ; wchar[260]
%define FT_PASS  616             ; wchar[260]
%define FT_HOST  1136            ; wchar[260]
%define FT_PATH  1656            ; wchar[520]
%define FT_PORT  2700

; ftp_is_url(path) -> eax 1 if it starts with ftp:// (case-insensitive)
ftp_is_url:
    PROLOG 64
    mov rsi, rcx
    invoke _wcsnicmp, rsi, &s_ftp_scheme, 6
    test eax, eax
    setz al
    movzx eax, al
    EPILOG 64

; ftp_open(fold, side, url) -> eax 1 ok  (ftp://[user[:pass]@]host[:port]/path)
ftp_open:
    PROLOG 2800
    test qword [g_policy], 4
    jz .polok
    xor eax, eax
    EPILOG 2800
.polok:
    mov rbx, rcx
    mov r12d, edx
    lea rsi, [r8+12]                 ; after "ftp://"
    lea r13, [rsp+FT_USER]
    lea r14, [rsp+FT_PASS]
    lea r15, [rsp+FT_HOST]
    mov word [r13], 0
    mov word [r14], 0
    mov word [r15], 0
    ; end of the authority (first '/')
    mov rdi, rsi
.au:
    movzx eax, word [rdi]
    test eax, eax
    jz .aud
    cmp eax, '/'
    je .aud
    add rdi, 2
    jmp .au
.aud:
    ; last '@' inside the authority
    xor ebp, ebp
    mov rcx, rsi
.at:
    cmp rcx, rdi
    jae .atd
    cmp word [rcx], '@'
    jne .atn
    mov rbp, rcx
.atn:
    add rcx, 2
    jmp .at
.atd:
    test rbp, rbp
    jz .nocred
    ; user[:pass] = [rsi, rbp)
    mov rdx, r13
    mov rcx, rsi
.us:
    cmp rcx, rbp
    jae .usd
    cmp word [rcx], ':'
    je .pw
    movzx eax, word [rcx]
    mov [rdx], ax
    add rdx, 2
    add rcx, 2
    jmp .us
.pw:
    mov word [rdx], 0
    add rcx, 2
    mov rdx, r14
.pwl:
    cmp rcx, rbp
    jae .usd
    movzx eax, word [rcx]
    mov [rdx], ax
    add rdx, 2
    add rcx, 2
    jmp .pwl
.usd:
    mov word [rdx], 0
    lea rsi, [rbp+2]                 ; host starts after '@'
    jmp .host
.nocred:
    invoke wcscpy, r13, &s_anon
    invoke wcscpy, r14, &s_anonpw
.host:
    cmp word [r13], 0
    jne .hh
    invoke wcscpy, r13, &s_anon
.hh:
    mov dword [rsp+FT_PORT], 21    ; port
    mov rdx, r15
    mov rcx, rsi
.hl:
    cmp rcx, rdi
    jae .hld
    cmp word [rcx], ':'
    je .port
    movzx eax, word [rcx]
    mov [rdx], ax
    add rdx, 2
    add rcx, 2
    jmp .hl
.port:
    mov word [rdx], 0
    add rcx, 2
    xor eax, eax
.pl:
    cmp rcx, rdi
    jae .pld
    movzx edx, word [rcx]
    sub edx, '0'
    imul eax, eax, 10
    add eax, edx
    add rcx, 2
    jmp .pl
.pld:
    mov [rsp+FT_PORT], eax
    jmp .path
.hld:
    mov word [rdx], 0
.path:
    ; remote path (default "/"), without a trailing slash
    lea r13, [rsp+FT_PATH]
    mov word [r13], 0
    cmp word [rdi], 0
    je .pathd
    invoke wcscpy, r13, rdi
.pathd:
    invoke wcslen, r13
    test rax, rax
    jz .conn
    cmp word [r13+rax*2-2], '/'
    jne .conn
    mov word [r13+rax*2-2], 0
.conn:
    cmp qword [rbx+F_INET], 0
    jne .have
    invoke InternetOpenW, &s_agent, 0, 0, 0, 0
    mov [rbx+F_INET], rax
    test rax, rax
    jz .fail
.have:
    mov r14d, [rsp+FT_PORT]
    invoke InternetConnectW, [rbx+F_INET], r15, r14, &rsp+FT_USER, &rsp+FT_PASS, 1, 0x08000000, 0
    test rax, rax
    jz .fail
    mov [rbx+F_FTPC+r12*8], rax
    mov qword [rbx+F_KIND+r12*8], 2
    ; keep the remote base path
    lea rcx, [rbx+F_FTPP]
    test r12d, r12d
    jz .sp
    lea rcx, [rbx+F_FTPP+1040]
.sp:
    invoke wcscpy, rcx, &rsp+FT_PATH
    mov eax, 1
    EPILOG 2800
.fail:
    xor eax, eax
    EPILOG 2800

; ftp_remote(fold, side, rel, out) : out = base + "/" + rel with forward slashes
ftp_remote:
    PROLOG 64
    mov rbx, rcx
    mov rsi, r8
    mov rdi, r9
    lea rbp, [rbx+F_FTPP]
    test edx, edx
    jz .b
    lea rbp, [rbx+F_FTPP+1040]
.b:
    cmp word [rsi], 0
    jne .rel
    invoke _snwprintf, rdi, 1099, &f_ftp_root, rbp
    jmp .conv
.rel:
    invoke _snwprintf, rdi, 1099, &f_ftp_rel, rbp, rsi
.conv:
    xor ecx, ecx
.l:
    movzx eax, word [rdi+rcx*2]
    test eax, eax
    jz .d
    cmp eax, 0x5C
    jne .n
    mov word [rdi+rcx*2], '/'
.n:
    inc rcx
    jmp .l
.d:
    EPILOG 64

; ftp_enum(fold, side, rel, vec): children of a remote directory as TE entries
%define FE_PAT 96
%define FE_FD  2304
ftp_enum:
    PROLOG 2896
    mov r12, rcx
    mov r13d, edx
    mov r14, r8
    mov r15, r9
    invoke ftp_remote, r12, r13, r14, &rsp+FE_PAT
    invoke wcslen, &rsp+FE_PAT
    cmp rax, 1000
    ja .d
    mov word [rsp+FE_PAT+rax*2], '/'
    mov word [rsp+FE_PAT+rax*2+2], '*'
    mov word [rsp+FE_PAT+rax*2+4], 0
    invoke FtpFindFirstFileW, [r12+F_FTPC+r13*8], &rsp+FE_PAT, &rsp+FE_FD, 0x80000000, 0
    test rax, rax
    jz .d
    mov rbx, rax
.loop:
    lea rsi, [rsp+FE_FD+FD_NAME]
    cmp word [rsi], '.'
    jne .nd
    cmp word [rsi+2], 0
    je .next
    cmp word [rsi+2], '.'
    jne .nd
    cmp word [rsi+4], 0
    je .next
.nd:
    invoke fold_excluded, r12, r14, rsi
    test eax, eax
    jnz .next
    invoke vec_push, r15, 32
    mov rdi, rax
    invoke str_dup, rsi
    mov [rdi], rax
    mov eax, [rsp+FE_FD+FD_ATTR]
    mov [rdi+8], eax
    mov dword [rdi+12], 0
    mov edx, [rsp+FE_FD+FD_SZHI]
    shl rdx, 32
    mov eax, [rsp+FE_FD+FD_SZLO]
    or rdx, rax
    mov [rdi+16], rdx
    mov rax, [rsp+FE_FD+FD_MTIME]
    mov [rdi+24], rax
.next:
    invoke InternetFindNextFileW, rbx, &rsp+FE_FD
    test eax, eax
    jnz .loop
    invoke InternetCloseHandle, rbx
    mov rax, [r15+8]
    cmp rax, 1
    jbe .d
    invoke qsort, [r15], rax, 32, &cmp_te
.d:
    EPILOG 2896

; ftp_download(fold, side, rel, local path) -> eax 1 ok
ftp_download:
    PROLOG 2400
    mov rbx, rcx
    mov r12d, edx
    mov r13, r9
    invoke ftp_remote, rbx, r12, r8, &rsp+96
    invoke FtpGetFileW, [rbx+F_FTPC+r12*8], &rsp+96, r13, 0, FILE_ATTRIBUTE_NORMAL, 2, 0
    EPILOG 2400

; ftp_close(fold): connections and the session handle
ftp_close:
    PROLOG 64
    mov rbx, rcx
    mov rcx, [rbx+F_FTPC]
    test rcx, rcx
    jz .b
    call InternetCloseHandle
    mov qword [rbx+F_FTPC], 0
.b:
    mov rcx, [rbx+F_FTPC+8]
    test rcx, rcx
    jz .i
    call InternetCloseHandle
    mov qword [rbx+F_FTPC+8], 0
.i:
    mov rcx, [rbx+F_INET]
    test rcx, rcx
    jz .d
    call InternetCloseHandle
    mov qword [rbx+F_INET], 0
.d:
    EPILOG 64
