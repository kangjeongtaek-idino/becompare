; rich_keys.inc - line editing keys of the IntelliJ editor in every editable RichEdit pane
;   (text compare panes, the merge output, the editor)
;
;   Ctrl+D              duplicate the line of the caret, or the selection when there is one
;   Alt+Shift+Up/Down   move the line of the caret (or every line the selection touches) up / down
;   Ctrl+Shift+Up/Down  the same
;   Ctrl+Shift+J        join the line with the next one (or all lines of the selection): the line break and
;                       the blanks around it become one blank
;   Ctrl+Shift+U        toggle the case of the selection, or of the word at the caret
;
;   Every action is one replacement through EM_REPLACESEL, so the editor's own undo takes it back in one
;   step. A read-only pane (folded / unified view, the merge inputs) does not take the keys.

extern GetClassNameW, _wcsicmp, memcpy, IsCharLowerW, CharUpperBuffW, CharLowerBuffW

global rich_key

%define EM_EXLINEFROMCHAR_ 0x0436
%define EM_GETTEXTRANGE_   0x044B

%define RK_CR   64                   ; CHARRANGE: the selection
%define RK_BLK  72                   ; dword x 5: first line, last line, start of the first, end of the last, lines
%define RK_RNG  96                   ; CHARRANGE: what is replaced
%define RK_SEL  104                  ; CHARRANGE: the selection afterwards
%define RK_T    112                  ; text read from the pane
%define RK_N    120                  ; text that goes in
%define RK_CLS  128                  ; wchar[32]
%define RK_A    192                  ; dword x 3: scratch
%define RK_B    196
%define RK_C    200

section .text

; rk_isword(ecx = character) -> eax 1 for a letter, digit, '_' or anything not ASCII (changes eax only)
rk_isword:
    mov eax, 1
    cmp ecx, 0x80
    jae .d
    cmp ecx, '_'
    je .d
    cmp ecx, '0'
    jb .n
    cmp ecx, '9'
    jbe .d
    cmp ecx, 'A'
    jb .n
    cmp ecx, 'Z'
    jbe .d
    cmp ecx, 'a'
    jb .n
    cmp ecx, 'z'
    jbe .d
.n:
    xor eax, eax
.d:
    ret

; rk_get(h, a, b) -> rax the characters a..b-1 (line breaks are CR), to free
rk_get:
    PROLOG 96
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov eax, r13d
    sub eax, r12d
    jae .len
    xor eax, eax
.len:
    lea rcx, [rax*2+8]
    call malloc
    mov rsi, rax
    mov word [rsi], 0
    cmp r13d, r12d
    jbe .d
    mov [rsp+64], r12d
    mov [rsp+68], r13d
    mov [rsp+72], rsi
    invoke SendMessageW, rbx, EM_GETTEXTRANGE_, 0, &rsp+64
.d:
    mov rax, rsi
    EPILOG 96

; rk_put(h, range*, text, selection*): replace the range (one undo step) and set the selection
rk_put:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12, r9
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, rsi
    invoke SendMessageW, rbx, EM_REPLACESEL, 1, rdi
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, r12
    invoke SendMessageW, rbx, EM_SCROLLCARET, 0, 0
    EPILOG 64

; rk_block(h, cpMin, cpMax, out dword[5]): the lines the selection touches
rk_block:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14, r9
    invoke SendMessageW, rbx, EM_EXLINEFROMCHAR_, 0, r12
    mov [r14], eax
    invoke SendMessageW, rbx, EM_EXLINEFROMCHAR_, 0, r13
    mov r15d, eax
    cmp r13d, r12d
    jbe .one
    cmp r15d, [r14]
    jbe .one
    invoke SendMessageW, rbx, EM_LINEINDEX, r15, 0
    cmp eax, r13d                    ; a selection that ends at the start of a line does not take that line in
    jne .one
    dec r15d
.one:
    mov [r14+4], r15d
    invoke SendMessageW, rbx, EM_LINEINDEX, #[r14], 0
    mov [r14+8], eax
    invoke SendMessageW, rbx, EM_LINEINDEX, r15, 0
    mov esi, eax
    invoke SendMessageW, rbx, EM_LINELENGTH, rsi, 0
    add eax, esi
    mov [r14+12], eax
    invoke SendMessageW, rbx, EM_GETLINECOUNT, 0, 0
    mov [r14+16], eax
    EPILOG 64

; rk_swap(text = first CR second, length of first, length of second) -> rax second CR first, to free
rk_swap:
    PROLOG 64
    mov rsi, rcx
    mov r12d, edx
    mov r13d, r8d
    lea eax, [r12+r13+2]
    lea rcx, [rax*2]
    call malloc
    mov rbx, rax
    lea rdx, [rsi+r12*2+2]
    lea r8, [r13*2]
    invoke memcpy, rbx, rdx, r8
    mov word [rbx+r13*2], 13
    lea rcx, [rbx+r13*2+2]
    lea r8, [r12*2]
    invoke memcpy, rcx, rsi, r8
    lea eax, [r12+r13+1]
    mov word [rbx+rax*2], 0
    mov rax, rbx
    EPILOG 64

; rich_key(hwnd, virtual key, modifiers 1 Ctrl | 2 Shift | 4 Alt) -> eax 1 when the key was one of the
;   line editing keys and the window is an editable RichEdit pane (the key is used up then)
rich_key:
    PROLOG 208
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    xor esi, esi                     ; 1 duplicate, 2 join, 3 case, 4 up, 5 down
    cmp r13d, 1
    jne .k3
    cmp r12d, 'D'
    jne .no
    mov esi, 1
    jmp .is
.k3:
    cmp r13d, 3
    jne .k6
    mov esi, 2
    cmp r12d, 'J'
    je .is
    mov esi, 3
    cmp r12d, 'U'
    je .is
    jmp .arrow
.k6:
    cmp r13d, 6
    jne .no
.arrow:
    mov esi, 4
    cmp r12d, 0x26                   ; VK_UP
    je .is
    mov esi, 5
    cmp r12d, 0x28                   ; VK_DOWN
    jne .no
.is:
    invoke GetClassNameW, rbx, &rsp+RK_CLS, 32
    invoke _wcsicmp, &rsp+RK_CLS, &cn_rich
    test eax, eax
    jnz .no
    invoke GetWindowLongPtrW, rbx, GWL_STYLE
    test eax, 0x800                  ; ES_READONLY
    jnz .no
    invoke SendMessageW, rbx, EM_EXGETSEL, 0, &rsp+RK_CR
    mov r14d, [rsp+RK_CR]
    mov r15d, [rsp+RK_CR+4]
    invoke rk_block, rbx, r14, r15, &rsp+RK_BLK
    xor eax, eax
    mov [rsp+RK_T], rax
    mov [rsp+RK_N], rax
    cmp esi, 1
    je .dup
    cmp esi, 2
    je .join
    cmp esi, 3
    je .case
    cmp esi, 4
    je .up
    jmp .down

    ; ---- Ctrl+D
.dup:
    cmp r14d, r15d
    jne .dupsel
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    mov edi, [rsp+RK_BLK+12]
    sub edi, [rsp+RK_BLK+8]          ; length of the line
    lea rcx, [rdi*2+8]
    call malloc
    mov [rsp+RK_N], rax
    mov word [rax], 13               ; a line break and the line again, behind the line
    lea rcx, [rax+2]
    lea r8, [rdi*2+2]
    invoke memcpy, rcx, [rsp+RK_T], r8
    mov eax, [rsp+RK_BLK+12]
    mov [rsp+RK_RNG], eax
    mov [rsp+RK_RNG+4], eax
    lea eax, [r14+rdi+1]             ; the caret keeps its column, on the copy
    mov [rsp+RK_SEL], eax
    mov [rsp+RK_SEL+4], eax
    jmp .put
.dupsel:
    invoke rk_get, rbx, r14, r15     ; the selection again, behind it; the copy is selected
    mov [rsp+RK_N], rax
    mov [rsp+RK_RNG], r15d
    mov [rsp+RK_RNG+4], r15d
    mov [rsp+RK_SEL], r15d
    lea eax, [r15+r15]
    sub eax, r14d
    mov [rsp+RK_SEL+4], eax
    jmp .put

    ; ---- move up: the line above goes behind the block
.up:
    mov eax, [rsp+RK_BLK]
    test eax, eax
    jz .done
    dec eax
    invoke SendMessageW, rbx, EM_LINEINDEX, rax, 0
    mov edi, eax                     ; start of the line above
    invoke rk_get, rbx, rdi, #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    mov eax, [rsp+RK_BLK+8]
    sub eax, edi
    dec eax
    mov [rsp+RK_A], eax              ; its length
    mov ecx, [rsp+RK_BLK+12]
    sub ecx, [rsp+RK_BLK+8]
    mov [rsp+RK_B], ecx
    invoke rk_swap, [rsp+RK_T], #[rsp+RK_A], #[rsp+RK_B]
    mov [rsp+RK_N], rax
    mov [rsp+RK_RNG], edi
    mov eax, [rsp+RK_BLK+12]
    mov [rsp+RK_RNG+4], eax
    mov eax, [rsp+RK_A]
    inc eax
    mov ecx, r14d
    sub ecx, eax
    mov [rsp+RK_SEL], ecx
    mov ecx, r15d
    sub ecx, eax
    mov [rsp+RK_SEL+4], ecx
    jmp .put

    ; ---- move down: the line below goes in front of the block
.down:
    mov eax, [rsp+RK_BLK+4]
    inc eax
    cmp eax, [rsp+RK_BLK+16]
    jae .done
    invoke SendMessageW, rbx, EM_LINEINDEX, rax, 0
    mov edi, eax
    invoke SendMessageW, rbx, EM_LINELENGTH, rdi, 0
    mov [rsp+RK_B], eax              ; length of the line below
    add edi, eax
    mov [rsp+RK_C], edi              ; its end
    invoke rk_get, rbx, #[rsp+RK_BLK+8], rdi
    mov [rsp+RK_T], rax
    mov eax, [rsp+RK_BLK+12]
    sub eax, [rsp+RK_BLK+8]
    mov [rsp+RK_A], eax
    invoke rk_swap, [rsp+RK_T], #[rsp+RK_A], #[rsp+RK_B]
    mov [rsp+RK_N], rax
    mov eax, [rsp+RK_BLK+8]
    mov [rsp+RK_RNG], eax
    mov eax, [rsp+RK_C]
    mov [rsp+RK_RNG+4], eax
    mov eax, [rsp+RK_B]
    inc eax
    lea ecx, [r14+rax]
    mov [rsp+RK_SEL], ecx
    lea ecx, [r15+rax]
    mov [rsp+RK_SEL+4], ecx
    jmp .put

    ; ---- Ctrl+Shift+J
.join:
    mov eax, [rsp+RK_BLK+4]          ; the last line that is taken in: the next one at least
    cmp eax, [rsp+RK_BLK]
    ja .jl
    mov eax, [rsp+RK_BLK]
    inc eax
.jl:
    cmp eax, [rsp+RK_BLK+16]
    jae .done
    invoke SendMessageW, rbx, EM_LINEINDEX, rax, 0
    mov edi, eax
    invoke SendMessageW, rbx, EM_LINELENGTH, rdi, 0
    add edi, eax
    mov [rsp+RK_C], edi
    invoke rk_get, rbx, #[rsp+RK_BLK+8], rdi
    mov [rsp+RK_N], rax
    mov rsi, rax                     ; read
    mov rdi, rax                     ; write (never ahead of the read position)
    mov dword [rsp+RK_A], -1         ; where the first two lines meet
.jc:
    movzx eax, word [rsi]
    test eax, eax
    jz .je
    cmp eax, 13
    je .jbrk
    mov [rdi], ax
    add rdi, 2
    add rsi, 2
    jmp .jc
.jbrk:
    cmp rdi, [rsp+RK_N]              ; the blanks in front of the line break go
    jbe .jt1
    movzx eax, word [rdi-2]
    cmp eax, ' '
    je .jstrip
    cmp eax, 9
    jne .jt1
.jstrip:
    sub rdi, 2
    jmp .jbrk
.jt1:
    add rsi, 2
.js:
    movzx eax, word [rsi]            ; and those behind it
    cmp eax, ' '
    je .jskip
    cmp eax, 9
    jne .jo
.jskip:
    add rsi, 2
    jmp .js
.jo:
    cmp dword [rsp+RK_A], -1
    jne .jb
    mov rax, rdi
    sub rax, [rsp+RK_N]
    shr rax, 1
    mov [rsp+RK_A], eax
.jb:
    cmp rdi, [rsp+RK_N]              ; one blank between two parts that both have text
    jbe .jc
    movzx eax, word [rsi]
    test eax, eax
    jz .jc
    cmp eax, 13
    je .jc
    mov word [rdi], ' '
    add rdi, 2
    jmp .jc
.je:
    mov word [rdi], 0
    mov eax, [rsp+RK_BLK+8]
    mov [rsp+RK_RNG], eax
    mov ecx, [rsp+RK_C]
    mov [rsp+RK_RNG+4], ecx
    add eax, [rsp+RK_A]
    mov [rsp+RK_SEL], eax
    mov [rsp+RK_SEL+4], eax
    jmp .put

    ; ---- Ctrl+Shift+U
.case:
    mov [rsp+RK_RNG], r14d
    mov [rsp+RK_RNG+4], r15d
    mov [rsp+RK_SEL], r14d
    mov [rsp+RK_SEL+4], r15d
    cmp r14d, r15d
    jne .cget
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]    ; no selection: the word at the caret
    mov [rsp+RK_T], rax
    mov rsi, rax
    mov edi, r14d
    sub edi, [rsp+RK_BLK+8]          ; the caret in the line
    mov edx, [rsp+RK_BLK+12]
    sub edx, [rsp+RK_BLK+8]          ; length of the line
    mov r8d, edi
.cw1:
    test r8d, r8d
    jz .cw2
    movzx ecx, word [rsi+r8*2-2]
    call rk_isword
    test eax, eax
    jz .cw2
    dec r8d
    jmp .cw1
.cw2:
    mov r9d, edi
.cw3:
    cmp r9d, edx
    jae .cw4
    movzx ecx, word [rsi+r9*2]
    call rk_isword
    test eax, eax
    jz .cw4
    inc r9d
    jmp .cw3
.cw4:
    cmp r8d, r9d
    jae .done
    mov eax, [rsp+RK_BLK+8]
    add r8d, eax
    add r9d, eax
    mov [rsp+RK_RNG], r8d
    mov [rsp+RK_RNG+4], r9d
.cget:
    invoke rk_get, rbx, #[rsp+RK_RNG], #[rsp+RK_RNG+4]
    mov [rsp+RK_N], rax
    mov edi, [rsp+RK_RNG+4]
    sub edi, [rsp+RK_RNG]
    xor esi, esi
.cl:
    cmp esi, edi                     ; a small letter anywhere: all capitals, else all small
    jae .lower
    mov rax, [rsp+RK_N]
    movzx ecx, word [rax+rsi*2]
    invoke IsCharLowerW, rcx
    test eax, eax
    jnz .upper
    inc esi
    jmp .cl
.upper:
    invoke CharUpperBuffW, [rsp+RK_N], rdi
    jmp .put
.lower:
    invoke CharLowerBuffW, [rsp+RK_N], rdi

.put:
    invoke rk_put, rbx, &rsp+RK_RNG, [rsp+RK_N], &rsp+RK_SEL
.done:
    invoke free, [rsp+RK_T]
    invoke free, [rsp+RK_N]
    mov eax, 1
    EPILOG 208
.no:
    xor eax, eax
    EPILOG 208
