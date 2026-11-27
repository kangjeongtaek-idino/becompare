; regex.inc - the regular expressions of the substitution rules ( /pattern/=text , textlib.asm rules_apply )
;
;   .            any character                    x*  x+  x?      none or more, one or more, none or one
;   \d \w \s     digit, word character, blank     x{n} x{n,} x{n,m}   that many
;   \D \W \S     the opposite                     ^    the start of the line (anywhere in the pattern)
;   \t           tab                              $    the end of the line (as the last item)
;   \x           the character x itself           [abc] [a-z] [^...]   one of / none of; \d \w \s \t \x inside
;   Quantifiers are greedy and give back what the rest of the pattern needs. There are no groups and no
;   alternatives. A pattern that is not well formed matches its characters literally where it can
;   (an unclosed [ , a { that is not a count) - it never fails the compare.

section .text

; re_esc(eax = the letter behind a backslash, edx = character) -> eax 1 matches, 0 not. Keeps edx, r8-r10.
re_esc:
    cmp eax, 't'
    jne .nt
    mov eax, 9
    jmp .lit
.nt:
    mov r11d, eax
    or eax, 0x20
    cmp eax, 'd'
    je .cls
    cmp eax, 'w'
    je .cls
    cmp eax, 's'
    je .cls
    mov eax, r11d
.lit:
    cmp eax, edx
    sete al
    movzx eax, al
    ret
.cls:
    mov ecx, eax
    or ecx, 0x10000
    call pat_char_match              ; uses eax and ecx only
    cmp r11d, 'a'                    ; \D \W \S: the opposite
    jae .r
    xor eax, 1
.r:
    ret

; re_atom_end(rcx = atom, rdx = pattern end) -> rax what follows the atom
re_atom_end:
    movzx eax, word [rcx]
    cmp eax, 0x5C
    je .esc
    cmp eax, '['
    je .cls
.one:
    lea rax, [rcx+2]
    ret
.esc:
    lea rax, [rcx+4]
    cmp rax, rdx
    jbe .r
    mov rax, rdx                     ; a backslash at the very end: the character itself
.r:
    ret
.cls:
    lea rax, [rcx+2]
.cl:
    cmp rax, rdx
    jae .one                         ; no closing bracket: a literal [
    movzx r8d, word [rax]
    cmp r8d, ']'
    je .cend
    cmp r8d, 0x5C
    jne .cn
    add rax, 2
.cn:
    add rax, 2
    jmp .cl
.cend:
    add rax, 2
    ret

; re_atom_match(rcx = atom, edx = character, r8 = what follows the atom) -> eax 1 matches, 0 not
re_atom_match:
    test edx, edx
    jz .no                           ; the end of the line matches nothing
    movzx eax, word [rcx]
    cmp eax, '.'
    je .yes
    cmp eax, 0x5C
    je .esc
    cmp eax, '['
    jne .lit
    lea r9, [rcx+2]
    cmp r9, r8
    je .lit                          ; an unclosed bracket is a literal one
    xor r10d, r10d                   ; 1: none of
    cmp word [r9], '^'
    jne .item
    mov r10d, 1
    add r9, 2
.item:
    lea rax, [r8-2]                  ; the closing bracket
    cmp r9, rax
    jae .cls_no
    movzx eax, word [r9]
    cmp eax, 0x5C
    jne .plainitem
    movzx eax, word [r9+2]
    add r9, 4
    call re_esc
    test eax, eax
    jnz .cls_hit
    jmp .item
.plainitem:
    cmp word [r9+2], '-'
    jne .single
    lea r11, [r9+4]
    lea rcx, [r8-2]
    cmp r11, rcx
    jae .single                      ; a - before the closing bracket is the character itself
    movzx ecx, word [r9+4]
    add r9, 6
    cmp edx, eax
    jb .item
    cmp edx, ecx
    ja .item
    jmp .cls_hit
.single:
    add r9, 2
    cmp edx, eax
    jne .item
.cls_hit:
    mov eax, 1
    xor eax, r10d
    ret
.cls_no:
    mov eax, r10d
    ret
.esc:
    lea r9, [rcx+2]
    cmp r9, r8
    jae .lit                         ; a backslash at the very end: the character itself
    movzx eax, word [r9]
    jmp re_esc
.lit:
    cmp eax, edx
    je .yes
.no:
    xor eax, eax
    ret
.yes:
    mov eax, 1
    ret

; re_match(rcx = pattern, rdx = pattern end, r8 = text, r9 = start of the line) -> rax end of the match in the
; text, 0 when the pattern does not match at this position
%define RE_AEND 64                   ; what follows the current atom (before its quantifier)
re_match:
    PROLOG 96
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    mov r15, r9
.loop:
    cmp rsi, rdi
    jae .matched
    movzx eax, word [rsi]
    cmp eax, '^'
    jne .notbol
    cmp rbx, r15
    jne .fail
    add rsi, 2
    jmp .loop
.notbol:
    cmp eax, '$'
    jne .atom
    lea rcx, [rsi+2]                 ; only the last item is the end of the line
    cmp rcx, rdi
    jne .atom
    cmp word [rbx], 0
    jne .fail
    add rsi, 2
    jmp .loop
.atom:
    invoke re_atom_end, rsi, rdi
    mov r12, rsi                     ; the atom
    mov rsi, rax
    mov [rsp+RE_AEND], rax
    mov r13d, 1                      ; at least
    mov r14d, 1                      ; at most
    cmp rsi, rdi
    jae .count
    movzx eax, word [rsi]
    cmp eax, '*'
    je .q_star
    cmp eax, '+'
    je .q_plus
    cmp eax, '?'
    je .q_opt
    cmp eax, '{'
    jne .count
    ; {n} {n,} {n,m}; anything else is a literal brace, taken as the next atom
    lea rcx, [rsi+2]
    xor eax, eax
    xor edx, edx                     ; digits read
.bn:
    cmp rcx, rdi
    jae .count
    movzx r8d, word [rcx]
    sub r8d, '0'
    cmp r8d, 9
    ja .bn_done
    cmp edx, 5
    jae .count                       ; a count of more than five digits is not one
    imul eax, eax, 10
    add eax, r8d
    inc edx
    add rcx, 2
    jmp .bn
.bn_done:
    test edx, edx
    jz .count
    movzx r8d, word [rcx]
    cmp r8d, '}'
    je .b_exact
    cmp r8d, ','
    jne .count
    mov r9d, eax                     ; n of {n, ...
    add rcx, 2
    xor eax, eax
    xor edx, edx
.bm:
    cmp rcx, rdi
    jae .count
    movzx r8d, word [rcx]
    sub r8d, '0'
    cmp r8d, 9
    ja .bm_done
    cmp edx, 5
    jae .count
    imul eax, eax, 10
    add eax, r8d
    inc edx
    add rcx, 2
    jmp .bm
.bm_done:
    cmp word [rcx], '}'
    jne .count
    mov r13d, r9d
    mov r14, -1
    test edx, edx
    jz .b_take
    mov r14d, eax
.b_take:
    lea rsi, [rcx+2]
    jmp .count
.b_exact:
    mov r13d, eax
    mov r14d, eax
    lea rsi, [rcx+2]
    jmp .count
.q_star:
    xor r13d, r13d
    mov r14, -1
    add rsi, 2
    jmp .count
.q_plus:
    mov r14, -1
    add rsi, 2
    jmp .count
.q_opt:
    xor r13d, r13d
    add rsi, 2
.count:                              ; as many as there are, up to the most
    xor ebp, ebp
.c:
    cmp rbp, r14
    jae .try
    movzx edx, word [rbx+rbp*2]
    mov rcx, r12
    mov r8, [rsp+RE_AEND]
    call re_atom_match
    test eax, eax
    jz .try
    inc rbp
    jmp .c
.try:
    cmp rbp, r13
    jb .fail
    cmp r13, r14
    jne .rest
    lea rbx, [rbx+rbp*2]             ; a fixed count: nothing to give back
    jmp .loop
.rest:                               ; the rest of the pattern behind this many; one less when it does not fit
    lea r8, [rbx+rbp*2]
    invoke re_match, rsi, rdi, r8, r15
    test rax, rax
    jnz .done
    cmp rbp, r13
    jbe .fail
    dec rbp
    jmp .rest
.matched:
    mov rax, rbx
    jmp .done
.fail:
    xor eax, eax
.done:
    EPILOG 96
