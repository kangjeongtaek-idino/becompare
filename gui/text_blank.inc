; text_blank.inc - Text Compare: "ignore blank lines" (IntelliJ: ignore whitespaces and empty lines)
;
;   A difference that is made of nothing but blank lines (empty, or only spaces and tabs) on both sides is not
;   a difference: it gets no colour, it is not counted and the navigation steps over it.
;   tv_blank_ignore runs right after tv_build. The rows of such a block become equal rows (kind 0) - a row
;   may then have a line on one side only, which the panes, gutters and exports already cope with - and the
;   block leaves TV_BLK. The kind a row had is kept in its block field as -2 - kind (-3, -4, -5; a real equal
;   row has -1), so that the patch, which has to list every changed line, can put the kinds back for a
;   moment (tv_blank_kinds).

section .rdata
WSTR s_c_blank, "빈 줄 무시"
section .text

; tb_blank(rdx = line) -> eax 1 when the line is empty or holds only spaces and tabs (leaf: rax, rdx)
tb_blank:
.l:
    movzx eax, word [rdx]
    test eax, eax
    jz .yes
    cmp eax, ' '
    je .n
    cmp eax, 9
    jne .no
.n:
    add rdx, 2
    jmp .l
.yes:
    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

; tv_blank_ignore(tv, LA, LB): take the blocks of blank lines out of the differences
tv_blank_ignore:
    PROLOG 64
    mov rbx, rcx
    mov r12, [rdx]                   ; line starts of the left text
    mov r13, [r8]                    ; and of the right text
    mov r14, [rbx+TV_ROWS]
    mov r15, [rbx+TV_BLK]
    xor esi, esi                     ; block
    xor edi, edi                     ; blocks that stay
    mov qword [rbx+TV_NIGN], 0
.b:
    cmp rsi, [rbx+TV_NBLK]
    jae .done
    mov r8d, [r15+rsi*8]             ; first row
    mov r9d, [r15+rsi*8+4]           ; end row (exclusive)
    mov r10, r8
.chk:
    cmp r10, r9
    jae .blank
    mov rbp, r10
    shl rbp, 4
    add rbp, r14
    mov ecx, [rbp+4]
    cmp ecx, -1
    je .cb
    mov rdx, [r12+rcx*8]
    call tb_blank
    test eax, eax
    jz .keep
.cb:
    mov ecx, [rbp+8]
    cmp ecx, -1
    je .cn
    mov rdx, [r13+rcx*8]
    call tb_blank
    test eax, eax
    jz .keep
.cn:
    inc r10
    jmp .chk
.blank:
    mov r10, r8
.bl:
    cmp r10, r9
    jae .bd
    mov rbp, r10
    shl rbp, 4
    add rbp, r14
    mov eax, -2
    sub eax, [rbp]
    mov [rbp+12], eax                ; remembers the kind
    mov dword [rbp], 0
    inc r10
    jmp .bl
.bd:
    inc qword [rbx+TV_NIGN]
    jmp .nb
.keep:
    mov r10, r8
.kl:
    cmp r10, r9
    jae .kd
    mov rbp, r10
    shl rbp, 4
    add rbp, r14
    mov [rbp+12], edi                ; its new block number
    inc r10
    jmp .kl
.kd:
    mov [r15+rdi*8], r8d
    mov [r15+rdi*8+4], r9d
    inc rdi
.nb:
    inc rsi
    jmp .b
.done:
    mov [rbx+TV_NBLK], rdi
    EPILOG 64

; tv_blank_kinds(tv, real 0 / 1): the rows of ignored blocks get their real kind (1) or are equal rows
;   again (0). Only for rows as tv_blank_ignore left them (not folded, not unified). (leaf)
tv_blank_kinds:
    cmp qword [rcx+TV_NIGN], 0
    je .d
    mov r8, [rcx+TV_ROWS]
    mov r9, [rcx+TV_NROWS]
.l:
    test r9, r9
    jz .d
    mov eax, [r8+12]
    cmp eax, -3
    jg .n
    neg eax
    sub eax, 2                       ; -2 - block field
    test edx, edx
    jnz .set
    xor eax, eax
.set:
    mov [r8], eax
.n:
    add r8, 16
    dec r9
    jmp .l
.d:
    ret
