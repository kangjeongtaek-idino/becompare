; folder_pair.inc - FC-06 name pairing: unicode normalisation, wildcard masks and differing extensions
;   Entries whose right-hand name differs are renamed to the left-hand name for the merge walk; the original
;   right-hand relative path is kept in the F_ALT table {key, real B path} and the entry gets EF_ALT.

extern NormalizeString

; alt_find(fold, name) -> rax real B relative path or 0
alt_find:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    xor edi, edi
.l:
    cmp rdi, [rbx+F_ALT+8]
    jae .no
    mov rax, rdi
    shl rax, 4
    add rax, [rbx+F_ALT]
    mov r12, rax
    invoke _wcsicmp, [r12], rsi
    test eax, eax
    jz .hit
    inc rdi
    jmp .l
.hit:
    mov rax, [r12+8]
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; alt_bpath(fold, name) -> real B relative path (or `name` itself)
alt_bpath:
    PROLOG 64
    mov rsi, rdx
    cmp qword [rcx+F_ALT+8], 0
    je .same
    invoke alt_find, rcx, rsi
    test rax, rax
    jnz .d
.same:
    mov rax, rsi
.d:
    EPILOG 64

; alt_clear(fold)
alt_clear:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
.l:
    cmp rsi, [rbx+F_ALT+8]
    jae .d
    mov rax, rsi
    shl rax, 4
    add rax, [rbx+F_ALT]
    mov rdi, rax
    invoke free, [rdi]
    invoke free, [rdi+8]
    inc rsi
    jmp .l
.d:
    invoke free, [rbx+F_ALT]
    xor eax, eax
    mov [rbx+F_ALT], rax
    mov [rbx+F_ALT+8], rax
    mov [rbx+F_ALT+16], rax
    EPILOG 64

; pr_norm(a, b) -> eax 1 when both names are equal after NFC normalisation (and case folding)
pr_norm:
    PROLOG 2240
    mov rsi, rcx
    mov rdi, rdx
    invoke NormalizeString, 1, rsi, -1, &rsp+96, 520
    test eax, eax
    jle .no
    invoke NormalizeString, 1, rdi, -1, &rsp+1140, 520
    test eax, eax
    jle .no
    invoke _wcsicmp, &rsp+96, &rsp+1140
    test eax, eax
    jnz .no
    mov eax, 1
    EPILOG 2240
.no:
    xor eax, eax
    EPILOG 2240

; pr_ext(a, b) -> eax 1 when the names share the stem but not the extension
pr_ext:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    invoke wcsrchr, rsi, '.'
    mov r12, rax
    test rax, rax
    jnz .a
    xor eax, eax
    EPILOG 64
.a:
    invoke wcsrchr, rdi, '.'
    mov r13, rax
    test rax, rax
    jz .no
    sub r12, rsi
    sub r13, rdi
    cmp r12, r13
    jne .no
    shr r12, 1
    xor ecx, ecx
.c:
    cmp rcx, r12
    jae .yes
    movzx eax, word [rsi+rcx*2]
    movzx edx, word [rdi+rcx*2]
    or eax, 0x20
    or edx, 0x20
    cmp eax, edx
    jne .no
    inc rcx
    jmp .c
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; pr_mask(fold, a, b) -> eax 1 when a rule "L=R" of F_MASKS maps the left name onto the right name
;   L and R may contain one '*'; the text matched by it is carried over
pr_mask:
    PROLOG 2240
    mov r12, rcx
    mov rsi, rdx
    mov rdi, r8
    lea r13, [r12+F_MASKS]
.rule:
    cmp word [r13], 0
    je .no
    ; locate '=' and ';'
    mov r14, r13
.eq:
    movzx eax, word [r14]
    test eax, eax
    jz .no
    cmp eax, '='
    je .gote
    add r14, 2
    jmp .eq
.gote:
    lea r15, [r14+2]                  ; right pattern
    mov rbx, r15
.sc:
    movzx eax, word [rbx]
    test eax, eax
    jz .gots
    cmp eax, ';'
    je .gots
    add rbx, 2
    jmp .sc
.gots:
    ; left pattern [r13, r14), right pattern [r15, rbx)
    ; split the left pattern at '*': prefix len / suffix start
    mov rcx, r13
.ls:
    cmp rcx, r14
    jae .lnostar
    cmp word [rcx], '*'
    je .lstar
    add rcx, 2
    jmp .ls
.lnostar:
    ; literal left name: compare whole name
    mov rax, r14
    sub rax, r13
    shr rax, 1
    mov [rsp+2200], rax
    xor ecx, ecx
.lc:
    cmp rcx, [rsp+2200]
    jae .lcend
    movzx eax, word [r13+rcx*2]
    movzx edx, word [rsi+rcx*2]
    or eax, 0x20
    or edx, 0x20
    cmp eax, edx
    jne .next
    inc rcx
    jmp .lc
.lcend:
    cmp word [rsi+rcx*2], 0
    jne .next
    xor r9d, r9d                      ; capture length 0
    mov [rsp+2208], rsi
    jmp .build
.lstar:
    mov [rsp+2216], rcx               ; star position in the left pattern
    mov rax, rcx
    sub rax, r13
    shr rax, 1
    mov [rsp+2200], rax               ; prefix length
    mov rax, r14
    sub rax, rcx
    sub rax, 2
    shr rax, 1
    mov [rsp+2208], rax               ; suffix length
    invoke wcslen, rsi
    mov r9, rax                       ; name length
    mov rax, [rsp+2200]
    add rax, [rsp+2208]
    cmp r9, rax
    jb .next
    ; prefix
    xor ecx, ecx
.pc:
    cmp rcx, [rsp+2200]
    jae .pcd
    movzx eax, word [r13+rcx*2]
    movzx edx, word [rsi+rcx*2]
    or eax, 0x20
    or edx, 0x20
    cmp eax, edx
    jne .next
    inc rcx
    jmp .pc
.pcd:
    ; suffix
    mov r10, [rsp+2216]
    add r10, 2                        ; suffix pattern start
    mov r11, r9
    sub r11, [rsp+2208]               ; suffix start in the name
    xor ecx, ecx
.sf:
    cmp rcx, [rsp+2208]
    jae .sfd
    lea rax, [r11+rcx]
    movzx edx, word [rsi+rax*2]
    movzx eax, word [r10+rcx*2]
    or eax, 0x20
    or edx, 0x20
    cmp eax, edx
    jne .next
    inc rcx
    jmp .sf
.sfd:
    ; capture = name[prefix .. suffix start)
    mov rax, [rsp+2200]
    lea rcx, [rsi+rax*2]
    mov [rsp+2208], rcx               ; capture start
    mov r9, r11
    sub r9, rax                       ; capture length
.build:
    ; expected = right prefix + capture + right suffix  (into &rsp+96)
    lea r10, [rsp+96]
    mov r11, r15
.rb:
    cmp r11, rbx
    jae .rd
    movzx eax, word [r11]
    cmp eax, '*'
    jne .rput
    xor ecx, ecx
    mov rdx, [rsp+2208]
.cap:
    cmp rcx, r9
    jae .capd
    movzx eax, word [rdx+rcx*2]
    mov [r10], ax
    add r10, 2
    inc rcx
    jmp .cap
.capd:
    add r11, 2
    jmp .rb
.rput:
    mov [r10], ax
    add r10, 2
    add r11, 2
    jmp .rb
.rd:
    mov word [r10], 0
    invoke _wcsicmp, &rsp+96, rdi
    test eax, eax
    jnz .next
    mov eax, 1
    EPILOG 2240
.next:
    cmp word [rbx], 0
    je .no
    lea r13, [rbx+2]
    jmp .rule
.no:
    xor eax, eax
    EPILOG 2240

; pair_names(fold, rel, VA, VB)
pair_names:
    PROLOG 3440
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    test dword [r12+F_OPT], 0x80
    jnz .on
    cmp word [r12+F_MASKS], 0
    je .ret
.on:
    mov rax, [r14+8]
    mov [rsp+96], rax
    mov rax, [r15+8]
    mov [rsp+104], rax
    cmp qword [rsp+96], 0
    je .ret
    cmp qword [rsp+104], 0
    je .ret
    mov rcx, [rsp+96]
    mov edx, 1
    call calloc
    mov rbx, rax
    mov rcx, [rsp+104]
    mov edx, 1
    call calloc
    mov rbp, rax
    mov qword [rsp+112], 0
    ; mark names present on both sides
    xor esi, esi
    xor edi, edi
.mk:
    cmp rsi, [rsp+96]
    jae .mkd
    cmp rdi, [rsp+104]
    jae .mkd
    mov rax, rsi
    shl rax, 5
    add rax, [r14]
    mov r8, [rax]
    mov rax, rdi
    shl rax, 5
    add rax, [r15]
    mov r9, [rax]
    invoke _wcsicmp, r8, r9
    test eax, eax
    js .ia
    jg .ib
    mov byte [rbx+rsi], 1
    mov byte [rbp+rdi], 1
    inc rsi
    inc rdi
    jmp .mk
.ia:
    inc rsi
    jmp .mk
.ib:
    inc rdi
    jmp .mk
.mkd:
    xor esi, esi
.A:
    cmp rsi, [rsp+96]
    jae .pd
    cmp byte [rbx+rsi], 0
    jne .An
    mov rax, rsi
    shl rax, 5
    add rax, [r14]
    mov [rsp+120], rax                ; A entry
    mov qword [rsp+128], 1            ; pass
.pass:
    cmp qword [rsp+128], 3
    ja .An
    mov qword [rsp+136], -1           ; candidate
    mov qword [rsp+144], 0            ; candidate count
    xor edi, edi
.B:
    cmp rdi, [rsp+104]
    jae .bd
    cmp byte [rbp+rdi], 0
    jne .Bn
    mov rax, rdi
    shl rax, 5
    add rax, [r15]
    mov [rsp+152], rax                ; B entry
    mov rcx, [rsp+120]
    mov r10, [rcx]                    ; A name
    mov r11, [rax]                    ; B name
    mov r8, [rsp+128]
    cmp r8, 1
    jne .p2
    test dword [r12+F_OPT], 0x80
    jz .Bn
    invoke pr_norm, r10, r11
    jmp .tst
.p2:
    cmp r8, 2
    jne .p3
    invoke pr_mask, r12, r10, r11
    jmp .tst
.p3:
    test dword [r12+F_OPT], 0x80
    jz .Bn
    ; extensions only for files on both sides
    mov rax, [rsp+120]
    test dword [rax+8], FILE_ATTRIBUTE_DIRECTORY
    jnz .Bn
    mov rax, [rsp+152]
    test dword [rax+8], FILE_ATTRIBUTE_DIRECTORY
    jnz .Bn
    invoke pr_ext, r10, r11
.tst:
    test eax, eax
    jz .Bn
    inc qword [rsp+144]
    cmp qword [rsp+136], -1
    jne .Bn
    mov [rsp+136], rdi
.Bn:
    inc rdi
    jmp .B
.bd:
    cmp qword [rsp+144], 0
    je .nextpass
    ; passes 1 and 2 take the first candidate, pass 3 only a unique one
    cmp qword [rsp+128], 3
    jne .dopair
    cmp qword [rsp+144], 1
    jne .nextpass
.dopair:
    mov rdi, [rsp+136]
    mov rax, rdi
    shl rax, 5
    add rax, [r15]
    mov [rsp+152], rax
    ; key = <rel\>A name ; real = <rel\>B name
    mov rcx, [rsp+120]
    mov r10, [rcx]
    cmp word [r13], 0
    je .norel1
    invoke _snwprintf, &rsp+200, 529, &f_path, r13, r10
    jmp .k1
.norel1:
    invoke _snwprintf, &rsp+200, 529, &f_str, r10
.k1:
    mov rcx, [rsp+152]
    mov r10, [rcx]
    cmp word [r13], 0
    je .norel2
    invoke _snwprintf, &rsp+1260, 1030, &f_path, r13, r10
    jmp .k2
.norel2:
    invoke _snwprintf, &rsp+1260, 1030, &f_str, r10
.k2:
    invoke vec_push, &r12+F_ALT, 16
    mov [rsp+160], rax
    invoke str_dup, &rsp+200
    mov rcx, [rsp+160]
    mov [rcx], rax
    invoke str_dup, &rsp+1260
    mov rcx, [rsp+160]
    mov [rcx+8], rax
    ; the B entry takes the A name for the merge walk
    mov rcx, [rsp+152]
    invoke free, [rcx]
    mov rax, [rsp+120]
    invoke str_dup, [rax]
    mov rcx, [rsp+152]
    mov [rcx], rax
    mov byte [rbx+rsi], 1
    mov byte [rbp+rdi], 1
    mov qword [rsp+112], 1
    jmp .An
.nextpass:
    inc qword [rsp+128]
    jmp .pass
.An:
    inc rsi
    jmp .A
.pd:
    cmp qword [rsp+112], 0
    je .fr
    mov rax, [r15+8]
    invoke qsort, [r15], rax, 32, &cmp_te
.fr:
    invoke free, rbx
    invoke free, rbp
.ret:
    EPILOG 3440
