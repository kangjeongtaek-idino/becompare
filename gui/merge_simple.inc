; merge_simple.inc - Text Merge: "resolve simple conflicts" (IntelliJ "Resolve Simple Conflicts")
;
;   The 3-way merge works on whole lines: when both sides changed the same line it is a conflict, even if one
;   side renamed a word at the start of the line and the other added an argument at its end. Such a conflict
;   is merged here the way the lines are, one level down: base, left and right are cut into tokens (a word, a
;   run of blanks, or one other character - the line end is one of those) and merged token by token. Where
;   only one side changed the tokens between two that all three share, that side is taken; where both made
;   the same change, it is taken once; where both changed the same place differently, the conflict is not a
;   simple one and stays as it is.
;
;   The base text of a conflict is not in the output pane (the marker block holds the two sides), so mg_merge
;   keeps it per conflict (MG_BASES: left, right and base text). A conflict of the output pane is looked up
;   there by its two sides; one that was edited by hand is not found and stays.

extern wcscmp, memcmp, us_pop, set_status

%define MS_MAXT 1500                 ; tokens of one side of a conflict

section .rdata
WSTR s_b_simple, "단순 충돌 해결"
WSTR s_ms_none, "자동으로 해결할 수 있는 단순 충돌이 없습니다."
WSTR f_ms_done, "단순 충돌 %u개를 자동으로 해결했습니다 (남은 충돌 %llu개)."
section .text

; ms_cls(ecx = character) -> eax 1 word character, 2 blank, 0 anything else (changes eax only)
ms_cls:
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

; ms_tok(text, out {dword start, dword length}[], max) -> eax token count, -1 when there are more than max
ms_tok:
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    mov rsi, rcx
    mov rdi, rdx
    mov r12d, r8d
    xor ebx, ebx
    xor r13d, r13d
.l:
    movzx ecx, word [rsi+r13*2]
    test ecx, ecx
    jz .d
    cmp ebx, r12d
    jae .over
    mov [rdi+rbx*8], r13d
    call ms_cls
    mov edx, eax
    inc r13d
    test edx, edx
    jz .end
.run:
    movzx ecx, word [rsi+r13*2]
    test ecx, ecx
    jz .end
    call ms_cls
    cmp eax, edx
    jne .end
    inc r13d
    jmp .run
.end:
    mov eax, r13d
    sub eax, [rdi+rbx*8]
    mov [rdi+rbx*8+4], eax
    inc ebx
    jmp .l
.over:
    mov ebx, -1
.d:
    mov eax, ebx
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; a tokenised text: { wchar* text; tokens*; dword count }
; ms_teq: token ebx of the text at r12 against token ebp of the text at r13 -> eax 1 equal
;   (changes rcx, rdx, rsi, rdi)
ms_teq:
    mov rsi, [r12+8]
    mov ecx, [rsi+rbx*8+4]
    mov edx, [rsi+rbx*8]
    mov rdi, [r13+8]
    cmp ecx, [rdi+rbp*8+4]
    jne .no
    mov eax, [rdi+rbp*8]
    mov rsi, [r12]
    lea rsi, [rsi+rdx*2]
    mov rdi, [r13]
    lea rdi, [rdi+rax*2]
    repe cmpsw
    jne .no
    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

; ms_lcs(a*, b*, match dword[count a]): match[i] = the token of b that token i of a is paired with, -1 none
ms_lcs:
    PROLOG 96
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r8d, [r12+16]
    shl r8, 2
    invoke memset, r14, 0xFF, r8
    mov eax, [r13+16]
    inc eax
    mov [rsp+64], rax                ; width of a row
    mov ecx, [r12+16]
    inc ecx
    imul rcx, rax
    mov edx, 2
    call calloc                      ; the last row and column stay 0
    mov r15, rax
    mov ebx, [r12+16]
.i:
    dec ebx
    js .walk
    mov ebp, [r13+16]
.j:
    dec ebp
    js .i
    call ms_teq
    mov rdx, rbx
    imul rdx, [rsp+64]
    add rdx, rbp
    lea rdx, [r15+rdx*2]
    mov rcx, [rsp+64]
    test eax, eax
    jz .ne
    movzx eax, word [rdx+rcx*2+2]
    inc eax
    jmp .st
.ne:
    movzx eax, word [rdx+rcx*2]
    movzx r8d, word [rdx+2]
    cmp eax, r8d
    jae .st
    mov eax, r8d
.st:
    mov [rdx], ax
    jmp .j
.walk:
    xor ebx, ebx
    xor ebp, ebp
.w:
    cmp ebx, [r12+16]
    jae .done
    cmp ebp, [r13+16]
    jae .done
    call ms_teq
    test eax, eax
    jz .wn
    mov [r14+rbx*4], ebp
    inc ebx
    inc ebp
    jmp .w
.wn:
    mov rdx, rbx
    imul rdx, [rsp+64]
    add rdx, rbp
    lea rdx, [r15+rdx*2]
    mov rcx, [rsp+64]
    movzx eax, word [rdx+rcx*2]
    movzx r8d, word [rdx+2]
    cmp eax, r8d
    jb .wb
    inc ebx
    jmp .w
.wb:
    inc ebp
    jmp .w
.done:
    invoke free, r15
    EPILOG 96

; ms_off(tokenised text*, token index) -> eax character offset where that token starts (the end of the
;   text for the index one past the last) (leaf)
ms_off:
    mov r8, [rcx+8]
    cmp edx, [rcx+16]
    jae .end
    mov eax, [r8+rdx*8]
    ret
.end:
    xor eax, eax
    mov edx, [rcx+16]
    test edx, edx
    jz .d
    mov eax, [r8+rdx*8-8]
    add eax, [r8+rdx*8-4]
.d:
    ret

; ms_seq(text x, length x, text y, length y) -> eax 1 when the two pieces are the same
ms_seq:
    sub rsp, 40
    xor eax, eax
    cmp edx, r9d
    jne .d
    mov eax, 1
    test edx, edx
    jz .d
    mov rdx, r8
    mov r8d, r9d
    shl r8, 1
    call memcmp
    test eax, eax
    setz al
    movzx eax, al
.d:
    add rsp, 40
    ret

; ms_emit(sb, text, start, end): append the characters start..end-1
ms_emit:
    PROLOG 64
    mov r12, rcx
    mov rsi, rdx
    mov r13d, r8d
    mov r14d, r9d
    cmp r13d, r14d
    jae .d
    lea rdi, [rsi+r14*2]
    movzx ebx, word [rdi]
    mov word [rdi], 0
    lea rdx, [rsi+r13*2]
    invoke sb_append, r12, rdx
    mov [rdi], bx
.d:
    EPILOG 64

%define MS_O    96                   ; tokenised base {text, tokens, count}
%define MS_L    120
%define MS_R    144
%define MS_ML   168                  ; match base -> left
%define MS_MR   176
%define MS_SB   184                  ; the merged text so far (ptr, length, capacity)
%define MS_OUT  208
%define MS_LJ   216                  ; dword: the left token paired with the next stable base token
%define MS_RJ   220
%define MS_RES  224
%define MS_SO   232                  ; dword x 6: start / end of the piece of base, left, right
%define MS_EO   236
%define MS_SL   240
%define MS_EL   244
%define MS_SR   248
%define MS_ER   252

; ms_merge(base, left, right, out sb) -> eax 1 when the three texts merge without a conflict; the result is
;   appended to the string builder then (nothing is appended otherwise)
ms_merge:
    PROLOG 256
    mov [rsp+MS_O], rcx
    mov [rsp+MS_L], rdx
    mov [rsp+MS_R], r8
    mov [rsp+MS_OUT], r9
    xor eax, eax
    mov [rsp+MS_RES], rax
    mov [rsp+MS_ML], rax
    mov [rsp+MS_MR], rax
    mov [rsp+MS_O+8], rax
    mov [rsp+MS_L+8], rax
    mov [rsp+MS_R+8], rax
    invoke sb_init, &rsp+MS_SB
    lea rbx, [rsp+MS_O]              ; the three texts in turn
    xor esi, esi
.tk:
    mov ecx, MS_MAXT*8+16
    call malloc
    mov [rbx+8], rax
    invoke ms_tok, [rbx], rax, MS_MAXT
    mov [rbx+16], rax
    cmp eax, -1
    je .out                          ; too long to look at
    add rbx, 24
    inc esi
    cmp esi, 3
    jb .tk
    mov ecx, [rsp+MS_O+16]
    lea rcx, [rcx*4+8]
    call malloc
    mov [rsp+MS_ML], rax
    mov ecx, [rsp+MS_O+16]
    lea rcx, [rcx*4+8]
    call malloc
    mov [rsp+MS_MR], rax
    invoke ms_lcs, &rsp+MS_O, &rsp+MS_L, [rsp+MS_ML]
    invoke ms_lcs, &rsp+MS_O, &rsp+MS_R, [rsp+MS_MR]
    xor r13d, r13d                   ; base token
    xor r14d, r14d                   ; left token
    xor r15d, r15d                   ; right token
.loop:
    mov ecx, r13d                    ; the next base token that both sides kept
.find:
    cmp ecx, [rsp+MS_O+16]
    jae .end
    mov rax, [rsp+MS_ML]
    mov edx, [rax+rcx*4]
    cmp edx, -1
    je .nj
    mov rax, [rsp+MS_MR]
    mov r8d, [rax+rcx*4]
    cmp r8d, -1
    jne .have
.nj:
    inc ecx
    jmp .find
.end:
    mov ecx, [rsp+MS_O+16]
    mov edx, [rsp+MS_L+16]
    mov r8d, [rsp+MS_R+16]
.have:
    mov ebp, ecx
    mov [rsp+MS_LJ], edx
    mov [rsp+MS_RJ], r8d
    ; the pieces in front of it
    lea rcx, [rsp+MS_O]
    mov edx, r13d
    call ms_off
    mov [rsp+MS_SO], eax
    lea rcx, [rsp+MS_O]
    mov edx, ebp
    call ms_off
    mov [rsp+MS_EO], eax
    lea rcx, [rsp+MS_L]
    mov edx, r14d
    call ms_off
    mov [rsp+MS_SL], eax
    lea rcx, [rsp+MS_L]
    mov edx, [rsp+MS_LJ]
    call ms_off
    mov [rsp+MS_EL], eax
    lea rcx, [rsp+MS_R]
    mov edx, r15d
    call ms_off
    mov [rsp+MS_SR], eax
    lea rcx, [rsp+MS_R]
    mov edx, [rsp+MS_RJ]
    call ms_off
    mov [rsp+MS_ER], eax
    ; base = left: the right piece
    mov rcx, [rsp+MS_O]
    mov eax, [rsp+MS_SO]
    lea rcx, [rcx+rax*2]
    mov edx, [rsp+MS_EO]
    sub edx, eax
    mov r8, [rsp+MS_L]
    mov eax, [rsp+MS_SL]
    lea r8, [r8+rax*2]
    mov r9d, [rsp+MS_EL]
    sub r9d, eax
    call ms_seq
    test eax, eax
    jnz .right
    ; base = right: the left piece
    mov rcx, [rsp+MS_O]
    mov eax, [rsp+MS_SO]
    lea rcx, [rcx+rax*2]
    mov edx, [rsp+MS_EO]
    sub edx, eax
    mov r8, [rsp+MS_R]
    mov eax, [rsp+MS_SR]
    lea r8, [r8+rax*2]
    mov r9d, [rsp+MS_ER]
    sub r9d, eax
    call ms_seq
    test eax, eax
    jnz .left
    ; both changed it: the same way, or it is a real conflict
    mov rcx, [rsp+MS_L]
    mov eax, [rsp+MS_SL]
    lea rcx, [rcx+rax*2]
    mov edx, [rsp+MS_EL]
    sub edx, eax
    mov r8, [rsp+MS_R]
    mov eax, [rsp+MS_SR]
    lea r8, [r8+rax*2]
    mov r9d, [rsp+MS_ER]
    sub r9d, eax
    call ms_seq
    test eax, eax
    jz .out
.left:
    invoke ms_emit, &rsp+MS_SB, [rsp+MS_L], #[rsp+MS_SL], #[rsp+MS_EL]
    jmp .stable
.right:
    invoke ms_emit, &rsp+MS_SB, [rsp+MS_R], #[rsp+MS_SR], #[rsp+MS_ER]
.stable:
    cmp ebp, [rsp+MS_O+16]
    jae .merged
    mov edx, [rsp+MS_LJ]             ; the shared token itself
    inc edx
    lea rcx, [rsp+MS_L]
    call ms_off
    mov r9d, eax
    invoke ms_emit, &rsp+MS_SB, [rsp+MS_L], #[rsp+MS_EL], r9
    lea r13d, [rbp+1]
    mov r14d, [rsp+MS_LJ]
    inc r14d
    mov r15d, [rsp+MS_RJ]
    inc r15d
    jmp .loop
.merged:
    mov rax, [rsp+MS_SB+8]
    test rax, rax
    jz .ok                           ; nothing is left of the lines
    mov rcx, [rsp+MS_SB]
    cmp word [rcx+rax*2-2], 13       ; whole lines only
    je .app
    invoke sb_append, &rsp+MS_SB, &s_cr
.app:
    invoke sb_append, [rsp+MS_OUT], [rsp+MS_SB]
.ok:
    mov qword [rsp+MS_RES], 1
.out:
    invoke sb_free, &rsp+MS_SB
    invoke free, [rsp+MS_O+8]
    invoke free, [rsp+MS_L+8]
    invoke free, [rsp+MS_R+8]
    invoke free, [rsp+MS_ML]
    invoke free, [rsp+MS_MR]
    mov rax, [rsp+MS_RES]
    EPILOG 256

; ---------------------------------------------------------------- the texts of the conflicts
; ms_dup_range(LINES*, s, e) -> rax the lines s..e-1, each followed by a CR, as a string of its own
ms_dup_range:
    PROLOG 96
    mov rsi, rcx
    mov r12, rdx
    mov r13, r8
    invoke sb_init, &rsp+64
    invoke emit_range, &rsp+64, rsi, r12, r13
    mov rax, [rsp+64]
    test rax, rax
    jnz .d
    invoke calloc, 1, 8              ; no lines: an empty string
.d:
    EPILOG 96

; mg_base_add(mg, block*, LINES* base, s, e): remember a conflict of the merge - block as for conflict()
;   (+8 left lines, +16 / +24 range, +32 right lines, +40 / +48 range), with its base lines s..e-1
mg_base_add:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12, r9
    mov r13, [rsp+64+112]
    invoke vec_push, &rbx+MG_BASES, 24
    mov r14, rax
    invoke ms_dup_range, [rsi+8], [rsi+16], [rsi+24]
    mov [r14], rax
    invoke ms_dup_range, [rsi+32], [rsi+40], [rsi+48]
    mov [r14+8], rax
    invoke ms_dup_range, rdi, r12, r13
    mov [r14+16], rax
    EPILOG 64

; mg_bases_free(mg)
mg_bases_free:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
.l:
    cmp rsi, [rbx+MG_BASES+8]
    jae .d
    lea rdi, [rsi+rsi*2]
    shl rdi, 3
    add rdi, [rbx+MG_BASES]
    invoke free, [rdi]
    invoke free, [rdi+8]
    invoke free, [rdi+16]
    inc rsi
    jmp .l
.d:
    invoke free, [rbx+MG_BASES]
    xor eax, eax
    mov [rbx+MG_BASES], rax
    mov [rbx+MG_BASES+8], rax
    mov [rbx+MG_BASES+16], rax
    EPILOG 64

; mg_simple_one(mg, conflict {start, mid, end}*, out sb) -> eax 1 when the conflict was merged (its lines are
;   appended to the string builder), 0 when it is not simple, was edited, or has no base
mg_simple_one:
    PROLOG 96
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    xor r15d, r15d                   ; result
    mov edx, [rsi]
    inc edx
    mov r8d, [rsi+4]
    invoke ms_dup_range, &rbx+MG_SCL, rdx, r8
    mov r12, rax                     ; left side
    mov edx, [rsi+4]
    inc edx
    mov r8d, [rsi+8]
    invoke ms_dup_range, &rbx+MG_SCL, rdx, r8
    mov r13, rax                     ; right side
    xor r14d, r14d
.f:
    cmp r14, [rbx+MG_BASES+8]
    jae .d
    lea rbp, [r14+r14*2]
    shl rbp, 3
    add rbp, [rbx+MG_BASES]
    invoke wcscmp, [rbp], r12
    test eax, eax
    jnz .n
    invoke wcscmp, [rbp+8], r13
    test eax, eax
    jnz .n
    invoke ms_merge, [rbp+16], r12, r13, rdi
    mov r15d, eax
    jmp .d
.n:
    inc r14
    jmp .f
.d:
    invoke free, r12
    invoke free, r13
    mov eax, r15d
    EPILOG 96
