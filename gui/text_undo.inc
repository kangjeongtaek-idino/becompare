; text_undo.inc - Text Compare: undo / redo across compares (Ctrl+Z, Ctrl+Y / Ctrl+Shift+Z)
;
;   The editor's own undo only reaches back to the last compare: every compare fills the panes again and
;   that empties its undo buffer. So whenever a compare finds that the text of a side is not the stored
;   one any more (typed, replaced, "→ / ← 가져오기"), text_side puts the stored text on the undo stack
;   (see undo.asm; the tag of a step is the side).
;   Ctrl+Z first lets the pane with the caret take back what was typed since the last compare; when it has
;   nothing, the newest step is put back and the files are compared again. A new change empties the redo stack.

%define UN_STEP  96                  ; the step taken off: {side, text, bytes} -> 120
%define UN_CUR   120                 ; the step made of the current text -> 144
%define UN_MSG   144                 ; a step was applied

; tv_undo(hwnd, tv, sess, redo 0 / 1)
tv_undo:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov r13d, r9d
    mov qword [rsp+UN_MSG], 0
    cmp qword [rbx+TV_FOLDED], 0     ; display only panes: nothing was typed
    jne .own
    mov rdi, [rbx+TV_HA]
    cmp qword [rbx+TV_LASTP], 0
    je .pane
    mov rdi, [rbx+TV_HB]
.pane:
    mov r14d, EM_CANUNDO
    mov r15d, EM_UNDO
    test r13d, r13d
    jz .ask
    mov r14d, EM_CANREDO
    mov r15d, EM_REDO
.ask:
    invoke SendMessageW, rdi, r14, 0, 0
    test eax, eax
    jz .own
    invoke SendMessageW, rdi, r15, 0, 0
    EPILOG 160
.own:
    cmp dword [rbx+TV_LDA], 0
    je .none
    cmp dword [rbx+TV_LDB], 0
    je .none
    cmp qword [rbx+TV_FOLDED], 0
    jne .pop
    ; what was typed in the other pane since the last compare becomes a step of its own
    mov qword [rbx+TV_USLD], 0
    invoke text_side, rsi, rbx, 0
    invoke text_side, rsi, rbx, 1
    cmp dword [rbx+TV_LDA], 0
    je .none
    cmp dword [rbx+TV_LDB], 0
    je .none
    cmp qword [rbx+TV_USLD], 0       ; a path was changed and its file read: show that instead
    jne .show
.pop:
    invoke us_pop, &rbx+TV_UST, r13, &rsp+UN_STEP
    test eax, eax
    jz .none
    mov r14, [rsp+UN_STEP]           ; side
    mov rax, r14
    shl rax, 5
    lea r15, [rbx+rax+TV_TXA]        ; its TXT
    mov [rsp+UN_CUR], r14
    mov rax, [r15]
    mov [rsp+UN_CUR+8], rax
    mov rax, [r15+8]
    lea rax, [rax*2+2]
    mov [rsp+UN_CUR+16], rax
    mov edi, r13d
    xor edi, 1
    invoke us_push, &rbx+TV_UST, rdi, &rsp+UN_CUR
    mov rax, [rsp+UN_STEP+8]
    mov [r15], rax
    mov rax, [rsp+UN_STEP+16]
    sub rax, 2
    shr rax, 1
    mov [r15+8], rax
    invoke SendMessageW, [rbx+r14*8+TV_HA], EM_SETMODIFY, 1, 0   ; not what the file holds (as far as we know)
    mov qword [rsp+UN_MSG], 1
.show:
    mov qword [rbx+TV_EXPN], 0
    mov qword [rbx+TV_FOLDED], 1     ; text_side keeps the stored texts instead of reading the panes
    invoke SendMessageW, [rbx+TV_HA], EM_GETFIRSTVISIBLELINE, 0, 0
    mov r14d, eax
    invoke text_compare, rsi, rbx, r12
    invoke SendMessageW, [rbx+TV_HA], EM_GETFIRSTVISIBLELINE, 0, 0
    sub r14d, eax
    movsxd r14, r14d
    mov qword [rbx+TV_GUARD], 1
    invoke SendMessageW, [rbx+TV_HA], EM_LINESCROLL, 0, r14
    invoke SendMessageW, [rbx+TV_HB], EM_LINESCROLL, 0, r14
    mov qword [rbx+TV_GUARD], 0
    cmp qword [rsp+UN_MSG], 0
    je .d
    lea rcx, [s_un_done]
    test r13d, r13d
    jz .say
    lea rcx, [s_un_redone]
.say:
    call set_status
.d:
    EPILOG 160
.none:
    lea rcx, [s_un_none]
    test r13d, r13d
    jz .say
    lea rcx, [s_un_noredo]
    jmp .say
