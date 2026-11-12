; text_word.inc - word level inline change highlighting (IntelliJ style)
;   Both lines of a modified row are split into tokens (a run of letters / digits / '_', a run of blanks,
;   or one other character); an LCS over the tokens finds what is shared and only the other tokens are
;   painted with th_tc_modin, so two separate edits in one line give two separate spans.
;   Lines with more than WH_MAXT tokens are left to the caller's prefix / suffix highlighting.

%define WH_MAXT 256
%define WH_TAS  0                    ; dword[256] token starts, line A
%define WH_TAE  1024                 ; token ends, line A
%define WH_TBS  2048
%define WH_TBE  3072
%define WH_FA   4096                 ; byte[256]: 1 = token not shared
%define WH_FB   4352
%define WH_DP   4608                 ; word[(na+1)*(nb+1)] LCS lengths

section .bss
alignb 8
wh_buf: resb 140000

section .text

; wh_class(ecx = character) -> eax: 1 word character, 2 blank, 0 anything else
wh_class:
    cmp ecx, 0x80
    jae .w
    cmp ecx, '0'
    jb .n1
    cmp ecx, '9'
    jbe .w
.n1:
    cmp ecx, 'A'
    jb .n2
    cmp ecx, 'Z'
    jbe .w
.n2:
    cmp ecx, '_'
    je .w
    cmp ecx, 'a'
    jb .n3
    cmp ecx, 'z'
    jbe .w
.n3:
    cmp ecx, ' '
    je .s
    cmp ecx, 9
    je .s
    xor eax, eax
    ret
.w:
    mov eax, 1
    ret
.s:
    mov eax, 2
    ret

; wh_tok(text, len, starts, ends) -> eax token count, -1 when there are more than WH_MAXT
wh_tok:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov r13, r9
    xor ebx, ebx                     ; tokens
    xor ebp, ebp                     ; position
.l:
    cmp rbp, rdi
    jae .done
    cmp ebx, WH_MAXT
    jae .over
    mov [r12+rbx*4], ebp
    movzx ecx, word [rsi+rbp*2]
    call wh_class
    mov r14d, eax
    inc rbp
    test r14d, r14d
    jz .emit                         ; punctuation: one character per token
.ext:
    cmp rbp, rdi
    jae .emit
    movzx ecx, word [rsi+rbp*2]
    call wh_class
    cmp eax, r14d
    jne .emit
    inc rbp
    jmp .ext
.emit:
    mov [r13+rbx*4], ebp
    inc ebx
    jmp .l
.done:
    mov eax, ebx
    EPILOG 64
.over:
    mov eax, -1
    EPILOG 64

; wh_eq(quad*, i, j) -> eax 1 when token i of line A equals token j of line B   (quad: A, lenA, B, lenB)
wh_eq:
    PROLOG 32
    lea rbx, [wh_buf]
    mov esi, [rbx+WH_TAS+rdx*4]
    mov edi, [rbx+WH_TAE+rdx*4]
    sub edi, esi                     ; length A
    mov r10d, [rbx+WH_TBS+r8*4]
    mov r11d, [rbx+WH_TBE+r8*4]
    sub r11d, r10d                   ; length B
    cmp edi, r11d
    jne .no
    mov rax, [rcx]
    lea rax, [rax+rsi*2]
    mov rdx, [rcx+16]
    lea rdx, [rdx+r10*2]
    xor r8d, r8d
.c:
    cmp r8d, edi
    jae .yes
    movzx r9d, word [rax+r8*2]
    cmp r9w, [rdx+r8*2]
    jne .no
    inc r8d
    jmp .c
.yes:
    mov eax, 1
    EPILOG 32
.no:
    xor eax, eax
    EPILOG 32

; tv_wordhl(tv, quad*, baseA, baseB) -> eax 1 when painted, 0 to use the prefix / suffix fallback
;   quad = {A text, A length, B text, B length}; baseA / baseB = offset of the line in its pane
%define WW_TV   96
%define WW_Q    104
%define WW_BA   112
%define WW_BB   120
%define WW_STR  128                  ; row stride of the LCS table (nb + 1)
tv_wordhl:
    PROLOG 160
    mov [rsp+WW_TV], rcx
    mov [rsp+WW_Q], rdx
    mov [rsp+WW_BA], r8
    mov [rsp+WW_BB], r9
    lea rbx, [wh_buf]
    mov rax, [rsp+WW_Q]
    invoke wh_tok, [rax], [rax+8], &rbx+WH_TAS, &rbx+WH_TAE
    cmp eax, -1
    je .fallback
    mov r12d, eax                    ; tokens in A
    mov rax, [rsp+WW_Q]
    invoke wh_tok, [rax+16], [rax+24], &rbx+WH_TBS, &rbx+WH_TBE
    cmp eax, -1
    je .fallback
    mov r13d, eax                    ; tokens in B
    mov eax, r12d
    imul eax, r13d
    cmp eax, 16384                   ; keep the LCS table small: big changed lines use the plain span
    ja .fallback
    invoke memset, &rbx+WH_FA, 1, 512
    lea rax, [r13+1]
    mov [rsp+WW_STR], rax
    lea r8, [rax*2]
    invoke memset, &rbx+WH_DP, 0, r8
    mov r14d, 1
.i:
    cmp r14d, r12d
    ja .backtrack
    mov rax, r14
    imul rax, [rsp+WW_STR]
    mov word [rbx+WH_DP+rax*2], 0
    mov r15d, 1
.j:
    cmp r15d, r13d
    ja .inext
    mov rcx, [rsp+WW_Q]
    lea edx, [r14-1]
    lea r8d, [r15-1]
    call wh_eq
    mov rdi, r14
    imul rdi, [rsp+WW_STR]
    add rdi, r15                     ; index of [i][j]
    mov rsi, [rsp+WW_STR]
    test eax, eax
    jz .ne
    mov rax, rdi
    sub rax, rsi
    dec rax
    movzx eax, word [rbx+WH_DP+rax*2]
    inc eax
    mov [rbx+WH_DP+rdi*2], ax
    jmp .jn
.ne:
    mov rax, rdi
    sub rax, rsi
    movzx eax, word [rbx+WH_DP+rax*2]        ; [i-1][j]
    movzx ecx, word [rbx+WH_DP+rdi*2-2]      ; [i][j-1]
    cmp eax, ecx
    jae .st
    mov eax, ecx
.st:
    mov [rbx+WH_DP+rdi*2], ax
.jn:
    inc r15d
    jmp .j
.inext:
    inc r14d
    jmp .i
.backtrack:
    mov r14d, r12d
    mov r15d, r13d
.bt:
    test r14d, r14d
    jz .paint
    test r15d, r15d
    jz .paint
    mov rcx, [rsp+WW_Q]
    lea edx, [r14-1]
    lea r8d, [r15-1]
    call wh_eq
    test eax, eax
    jz .btne
    lea eax, [r14-1]
    mov byte [rbx+WH_FA+rax], 0
    lea eax, [r15-1]
    mov byte [rbx+WH_FB+rax], 0
    dec r14d
    dec r15d
    jmp .bt
.btne:
    mov rsi, [rsp+WW_STR]
    mov rdi, r14
    imul rdi, rsi
    add rdi, r15
    mov rax, rdi
    sub rax, rsi
    movzx eax, word [rbx+WH_DP+rax*2]        ; [i-1][j]
    movzx ecx, word [rbx+WH_DP+rdi*2-2]      ; [i][j-1]
    cmp eax, ecx
    jb .left
    dec r14d                                 ; token of A is not shared
    jmp .bt
.left:
    dec r15d                                 ; token of B is not shared
    jmp .bt
.paint:
    ; ---- line A: one span per run of unshared tokens
    xor r14d, r14d
.pa:
    cmp r14d, r12d
    jae .pad
    cmp byte [rbx+WH_FA+r14], 0
    je .pan
    mov r15d, r14d
.pae:
    inc r14d
    cmp r14d, r12d
    jae .parun
    cmp byte [rbx+WH_FA+r14], 0
    jne .pae
.parun:
    mov eax, [rbx+WH_TAS+r15*4]
    lea ecx, [r14-1]
    mov edx, [rbx+WH_TAE+rcx*4]
    mov rsi, [rsp+WW_BA]
    lea r8, [rsi+rdx]
    add rsi, rax
    mov rcx, [rsp+WW_TV]
    invoke rich_bk, [rcx+TV_HA], rsi, r8, #[th_tc_modin]
    jmp .pa
.pan:
    inc r14d
    jmp .pa
.pad:
    ; ---- line B
    xor r14d, r14d
.pb:
    cmp r14d, r13d
    jae .pbd
    cmp byte [rbx+WH_FB+r14], 0
    je .pbn
    mov r15d, r14d
.pbe:
    inc r14d
    cmp r14d, r13d
    jae .pbrun
    cmp byte [rbx+WH_FB+r14], 0
    jne .pbe
.pbrun:
    mov eax, [rbx+WH_TBS+r15*4]
    lea ecx, [r14-1]
    mov edx, [rbx+WH_TBE+rcx*4]
    mov rsi, [rsp+WW_BB]
    lea r8, [rsi+rdx]
    add rsi, rax
    mov rcx, [rsp+WW_TV]
    invoke rich_bk, [rcx+TV_HB], rsi, r8, #[th_tc_modin]
    jmp .pb
.pbn:
    inc r14d
    jmp .pb
.pbd:
    mov eax, 1
    EPILOG 160
.fallback:
    xor eax, eax
    EPILOG 160
