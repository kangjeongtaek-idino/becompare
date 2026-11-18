; hex_repl.inc - HX-01 replace: find the pattern (hex tokens or text) and overwrite it with bytes of the same length

; hx_parse(hwnd, edit id, out) -> eax pattern length (out holds up to 64 bytes)
hx_parse:
    PROLOG 512
    mov rsi, rcx
    mov ebx, edx
    mov r12, r8
    xor r13d, r13d                   ; pattern length
    invoke GetDlgItem, rsi, rbx
    invoke GetWindowTextW, rax, &rsp+96, 100
    ; first try hex tokens
    lea rdi, [rsp+96]
    xor r14d, r14d                   ; hex mode ok
    mov r14d, 1
.tok:
.skipsp:
    movzx eax, word [rdi]
    cmp eax, ' '
    jne .have
    add rdi, 2
    jmp .skipsp
.have:
    test eax, eax
    jz .tokdone
    xor ecx, ecx                     ; value
    xor edx, edx                     ; digits
.hd:
    movzx eax, word [rdi]
    mov r8d, eax
    cmp eax, '0'
    jb .hend
    cmp eax, '9'
    jbe .h09
    or eax, 0x20
    cmp eax, 'a'
    jb .hend
    cmp eax, 'f'
    ja .hend
    sub eax, 'a'-10
    jmp .hacc
.h09:
    sub eax, '0'
.hacc:
    shl ecx, 4
    or ecx, eax
    inc edx
    add rdi, 2
    jmp .hd
.hend:
    ; a token must be 1-2 hex digits followed by a space or the end
    test edx, edx
    jz .nothex
    cmp edx, 2
    ja .nothex
    test r8d, r8d
    jz .store
    cmp r8d, ' '
    jne .nothex
.store:
    cmp r13d, 64
    jae .tokdone
    mov [r12+r13], cl
    inc r13d
    jmp .tok
.nothex:
    xor r14d, r14d
.tokdone:
    test r14d, r14d
    jnz .search
    ; plain text: low bytes of the characters
    lea rdi, [rsp+96]
    xor r13d, r13d
.tx:
    movzx eax, word [rdi]
    test eax, eax
    jz .search
    cmp r13d, 64
    jae .search
    mov [r12+r13], al
    inc r13d
    add rdi, 2
    jmp .tx
.search:
    mov eax, r13d
    EPILOG 512

; hx_match(rcx = bytes, rdx = pattern, r8 = length) -> eax 1 when equal
hx_match:
    xor eax, eax
.l:
    cmp rax, r8
    jae .y
    mov r9b, [rcx+rax]
    cmp r9b, [rdx+rax]
    jne .n
    inc rax
    jmp .l
.y:
    mov eax, 1
    ret
.n:
    xor eax, eax
    ret

; hx_recount(hx): recompute every row's difference count after a bulk change
hx_recount:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
    xor edi, edi
.r:
    cmp rsi, [rbx+HX_ROWS]
    jae .d
    invoke hx_diffrow, rbx, rsi
    mov rax, [rbx+HX_RDIFF]
    movzx eax, byte [rax+rsi]
    add rdi, rax
    inc rsi
    jmp .r
.d:
    mov [rbx+HX_NDIFF], rdi
    invoke hx_rebuild, rbx
    invoke hx_scrollbar, rbx
    invoke hx_status, rbx
    invoke InvalidateRect, [rbx+HX_HPANE], 0, 1
    EPILOG 64

; hx_replace(hwnd, hx, all)
hx_replace:
    PROLOG 640
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    cmp qword [rbx+HX_LOADED], 0
    je .d
    mov [rsp+280], rsi
    invoke hx_parse, rsi, ID_EDIT_FIND, &rsp+96
    mov r13d, eax
    invoke hx_parse, rsi, ID_EDIT_REPL, &rsp+192
    mov r14d, eax
    test r13d, r13d
    jz .d
    cmp r13d, r14d
    jne .badlen
    mov rdi, [rbx+HX_BUFA]
    mov r15, [rbx+HX_SZA]
    lea rbp, [rbx+HX_MODA]
    cmp qword [rbx+HX_CURP], 0
    je .own
    mov rdi, [rbx+HX_BUFB]
    mov r15, [rbx+HX_SZB]
    lea rbp, [rbx+HX_MODB]
.own:
    test r12d, r12d
    jnz .all
    mov rax, [rbx+HX_CURO]
    add rax, r13
    cmp rax, r15
    ja .findnext
    mov rcx, [rbx+HX_CURO]
    add rcx, rdi
    lea rdx, [rsp+96]
    mov r8d, r13d
    call hx_match
    test eax, eax
    jz .findnext
    mov r8, [rbx+HX_CURO]
    add r8, rdi
    xor edx, edx
.cp:
    cmp edx, r13d
    jae .copied
    mov al, [rsp+192+rdx]
    mov [r8+rdx], al
    inc edx
    jmp .cp
.copied:
    mov qword [rbp], 1
    invoke hx_recount, rbx
.findnext:
    mov rsi, [rsp+280]
    invoke hx_find, rsi, rbx
    jmp .d
.all:
    xor r14d, r14d
    xor esi, esi
.al:
    lea rax, [rsi+r13]
    cmp rax, r15
    ja .alldone
    lea rcx, [rdi+rsi]
    lea rdx, [rsp+96]
    mov r8d, r13d
    call hx_match
    test eax, eax
    jz .anext
    lea r8, [rdi+rsi]
    xor edx, edx
.ac:
    cmp edx, r13d
    jae .acd
    mov al, [rsp+192+rdx]
    mov [r8+rdx], al
    inc edx
    jmp .ac
.acd:
    inc r14d
    mov eax, r13d
    add rsi, rax                     ; the position is 64 bit: files of 4 GB and more
    jmp .al
.anext:
    inc rsi
    jmp .al
.alldone:
    test r14d, r14d
    jz .none
    mov qword [rbp], 1
    invoke hx_recount, rbx
    invoke _snwprintf, &rsp+300, 60, &f_replaced, r14
    invoke set_status, &rsp+300
    jmp .d
.none:
    invoke set_status, &s_norepl
    jmp .d
.badlen:
    mov rsi, [rsp+280]
    invoke msgbox, rsi, &s_lenmis, MB_OK|MB_ICONWARNING
.d:
    EPILOG 640
