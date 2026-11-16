; text_goline.inc - go to a line by its number (IntelliJ "Go to Line", Ctrl+G)
;
;   The number is a line of the pane that has the focus - the right pane when it has, else the left one
;   (in the unified view always the left file).
;   The row carrying that line is put near the top of both panes and the caret goes to its start.
;   A line hidden in a folded fragment has no row: the first visible row behind it is taken instead.

section .rdata
WSTR s_gl_title, "줄로 이동 - 줄 번호"
align 8

section .text

%define GL_BUF 96                    ; wchar[260] -> 616
%define GL_FOCUS 624

; tv_gotoline(hwnd, tv)
tv_gotoline:
    PROLOG 640
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+TV_NROWS], 0
    je .d
    invoke GetFocus
    mov [rsp+GL_FOCUS], rax          ; before the input box takes it
    mov word [rsp+GL_BUF], 0
    invoke input_box, [g_hframe], &s_gl_title, &rsp+GL_BUF     ; the frame is the owner: the whole window waits
    test eax, eax
    jz .d
    invoke wcstoul, &rsp+GL_BUF, 0, 10
    test eax, eax
    jz .d
    lea r12d, [rax-1]                ; line index
    mov r14d, 4                      ; row field: left line
    cmp qword [rbx+TV_UNI], 0
    jne .side
    mov rax, [rsp+GL_FOCUS]
    cmp rax, [rbx+TV_HB]
    jne .side
    mov r14d, 8                      ; right line
.side:
    mov rdi, [rbx+TV_ROWS]
    mov r15, [rbx+TV_NROWS]
    xor r13d, r13d                   ; row
    mov rbp, -1                      ; last row that has a line on that side
.f:
    cmp r13, r15
    jae .last
    mov rax, r13
    shl rax, 4
    add rax, rdi
    cmp dword [rax], 4               ; the label of a folded fragment has no line
    je .fn
    mov ecx, [rax+r14]
    cmp ecx, -1
    je .fn
    mov rbp, r13
    cmp ecx, r12d
    jae .hit
.fn:
    inc r13
    jmp .f
.last:                               ; beyond the end: the last line
    mov r13, rbp
    cmp r13, -1
    je .d
.hit:
    mov qword [rbx+TV_GUARD], 1
    xor r14d, r14d
.pane:
    mov rdi, [rbx+TV_HA]
    test r14d, r14d
    jz .go
    mov rdi, [rbx+TV_HB]
.go:
    invoke SendMessageW, rdi, EM_LINEINDEX, r13, 0
    movsxd r15, eax
    invoke SendMessageW, rdi, EM_SETSEL, r15, r15
    invoke SendMessageW, rdi, EM_GETFIRSTVISIBLELINE, 0, 0
    mov ecx, r13d
    sub ecx, 3
    jns .t
    xor ecx, ecx
.t:
    sub ecx, eax
    movsxd r15, ecx
    invoke SendMessageW, rdi, EM_LINESCROLL, 0, r15
    inc r14d
    cmp r14d, 2
    jb .pane
    mov qword [rbx+TV_GUARD], 0
    mov rdi, [rbx+TV_HA]
    cmp qword [rbx+TV_UNI], 0
    jne .focus
    mov rax, [rsp+GL_FOCUS]
    cmp rax, [rbx+TV_HB]
    jne .focus
    mov rdi, [rbx+TV_HB]
.focus:
    invoke SetFocus, rdi
    invoke tv_redraw_aux, rbx
.d:
    EPILOG 640
