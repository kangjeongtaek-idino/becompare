section .rdata
WSTR s_c_ws, "공백 표시"
section .text

; text_ws.inc - show whitespace (IntelliJ "Show Whitespaces")
;
;   RichEdit cannot draw whitespace marks, so the displayed text carries them: a blank becomes U+00B7 (middle
;   dot) and a tab U+2192 (arrow), one character for one character - every offset used for colouring stays
;   valid. The marks are painted grey after the syntax colours.
;   Like folding it is display only: TV_TXA / TV_TXB keep the texts, the panes are read only (TV_FOLDED is
;   set) and whatever reads or writes the pane text leaves the mode first (tv_unfold).

; tv_ws_marks(tv): put the marks into TV_BUFA / TV_BUFB (the label line of a folded fragment is left alone)
tv_ws_marks:
    PROLOG 64
    mov rbx, rcx
    mov r12, [rbx+TV_ROWS]
    mov r13, [rbx+TV_NROWS]
    mov rsi, [rbx+TV_BUFA]
    xor r14d, r14d                   ; pane
.pane:
    xor edi, edi                     ; row = line
.c:
    movzx eax, word [rsi]
    test eax, eax
    jz .pd
    cmp eax, 13
    jne .ch
    inc rdi
    jmp .n
.ch:
    cmp rdi, r13
    jae .sub
    mov rcx, rdi
    shl rcx, 4
    cmp dword [r12+rcx], 4
    je .n
.sub:
    cmp eax, ' '
    jne .tab
    mov word [rsi], 0x00B7
    jmp .n
.tab:
    cmp eax, 9
    jne .n
    mov word [rsi], 0x2192
.n:
    add rsi, 2
    jmp .c
.pd:
    test r14d, r14d
    jnz .d
    inc r14d
    mov rsi, [rbx+TV_BUFB]
    jmp .pane
.d:
    EPILOG 64

; tv_ws_color(tv): grey for the runs of marks in both panes (a budget keeps huge files responsive)
tv_ws_color:
    PROLOG 96
    mov rbx, rcx
    cmp qword [rbx+TV_WS], 0
    je .d
    mov r15d, 0xBBBBBB
    cmp qword [g_dark], 0
    je .lt
    mov r15d, 0x5A5A5A
.lt:
    xor r13d, r13d
    mov ebp, 40000
.pane:
    mov rdi, [rbx+TV_HA]
    test r13d, r13d
    jz .p
    mov rdi, [rbx+TV_HB]
.p:
    invoke rich_get_text, rdi, &rsp+80
    mov r14, rax
    test rax, rax
    jz .nx
    invoke rich_batch, rdi, 1
    xor esi, esi
.s:
    cmp rsi, [rsp+80]
    jae .sd
    movzx eax, word [r14+rsi*2]
    cmp eax, 0x00B7
    je .run
    cmp eax, 0x2192
    je .run
    inc rsi
    jmp .s
.run:
    mov r12, rsi
.r:
    inc rsi
    cmp rsi, [rsp+80]
    jae .rd
    movzx eax, word [r14+rsi*2]
    cmp eax, 0x00B7
    je .r
    cmp eax, 0x2192
    je .r
.rd:
    test ebp, ebp
    jz .sd
    dec ebp
    invoke rich_fg, rdi, r12, rsi, r15
    jmp .s
.sd:
    invoke rich_batch, rdi, 0
    invoke free, r14
    invoke rich_unsel, rdi
.nx:
    inc r13d
    cmp r13d, 2
    jb .pane
.d:
    EPILOG 96
