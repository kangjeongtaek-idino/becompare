; pic_ui.inc - Picture Compare: pane painting, scrolling, session view

%define PX_PN   2192
%define PX_PWD  2196
%define PX_PHT  2200
%define PGAP    6

; pp_metrics(hwnd, px): panel count and size from the client area
pp_metrics:
    PROLOG 128
    mov rsi, rcx
    mov rbx, rdx
    invoke GetClientRect, rsi, &rsp+96
    mov r8d, 1
    cmp qword [rbx+PX_MODE], 0
    jne .n
    mov r8d, 3
.n:
    mov [rbx+PX_PN], r8d
    mov eax, [rsp+96+RC_R]
    mov ecx, r8d
    dec ecx
    imul ecx, ecx, PGAP
    sub eax, ecx
    xor edx, edx
    div r8d
    test eax, eax
    jnz .w
    mov eax, 1
.w:
    mov [rbx+PX_PWD], eax
    mov eax, [rsp+96+RC_B]
    test eax, eax
    jnz .h
    mov eax, 1
.h:
    mov [rbx+PX_PHT], eax
    EPILOG 128

; pp_scrollbars(hwnd, px)
pp_scrollbars:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    mov rcx, [rbx+PX_ZOOM]
    test rcx, rcx
    jnz .z
    mov ecx, 100
.z:
    ; visible source size
    mov eax, [rbx+PX_PWD]
    imul rax, 100
    xor edx, edx
    div rcx
    mov r12, rax                     ; vw
    mov eax, [rbx+PX_PHT]
    imul rax, 100
    xor edx, edx
    div rcx
    mov r13, rax                     ; vh
    ; clamp the positions
    mov eax, [rbx+PX_W]
    sub rax, r12
    jns .cx
    xor eax, eax
.cx:
    cmp [rbx+PX_SX], rax
    jbe .cy
    mov [rbx+PX_SX], rax
.cy:
    mov eax, [rbx+PX_H]
    sub rax, r13
    jns .cy2
    xor eax, eax
.cy2:
    cmp [rbx+PX_SY], rax
    jbe .set
    mov [rbx+PX_SY], rax
.set:
    mov dword [rsp+96], SI_SIZE
    mov dword [rsp+100], SIF_RANGE|SIF_PAGE|SIF_POS
    mov dword [rsp+104], 0
    mov eax, [rbx+PX_W]
    dec eax
    jns .mx
    xor eax, eax
.mx:
    mov [rsp+108], eax
    mov [rsp+112], r12d
    mov rax, [rbx+PX_SX]
    mov [rsp+116], eax
    invoke SetScrollInfo, rsi, SB_HORZ, &rsp+96, 1
    mov eax, [rbx+PX_H]
    dec eax
    jns .my
    xor eax, eax
.my:
    mov [rsp+108], eax
    mov [rsp+112], r13d
    mov rax, [rbx+PX_SY]
    mov [rsp+116], eax
    invoke SetScrollInfo, rsi, SB_VERT, &rsp+96, 1
    EPILOG 160

; pp_fit(hwnd, px): zoom so that the whole canvas fits one panel
pp_fit:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke pp_metrics, rsi, rbx
    mov ecx, [rbx+PX_W]
    test ecx, ecx
    jz .one
    mov eax, [rbx+PX_PWD]
    imul rax, 100
    xor edx, edx
    div rcx
    mov r8, rax                      ; fit by width
    mov ecx, [rbx+PX_H]
    test ecx, ecx
    jz .one
    mov eax, [rbx+PX_PHT]
    imul rax, 100
    xor edx, edx
    div rcx
    cmp rax, r8
    jae .m
    mov r8, rax
.m:
    cmp r8, 400
    jbe .lo
    mov r8d, 400
.lo:
    test r8, r8
    jnz .s
    mov r8d, 1
.s:
    mov [rbx+PX_ZOOM], r8
    jmp .fin
.one:
    mov qword [rbx+PX_ZOOM], 100
.fin:
    mov qword [rbx+PX_SX], 0
    mov qword [rbx+PX_SY], 0
    invoke pp_scrollbars, rsi, rbx
    EPILOG 64

; ---------------------------------------------------------------- painting
%define S_PS   128               ; PAINTSTRUCT (72)
%define S_BMI  208               ; BITMAPINFOHEADER (40)
%define S_X0   256
%define S_DW   264
%define S_DH   272
%define S_SX   280
%define S_SY   288
%define S_CW   296
%define S_CH   304
%define S_PIX  312

pp_paint:
    PROLOG 384
    mov rsi, rcx
    mov rbx, rdx
    invoke BeginPaint, rsi, &rsp+S_PS
    mov rdi, rax
    invoke FillRect, rdi, &rsp+S_PS+12, [th_bgbrush]
    cmp qword [rbx+PX_LOADED], 0
    je .end
    mov r8d, 3                       ; COLORONCOLOR
    cmp qword [rbx+PX_ZOOM], 100
    jae .m
    mov r8d, 4                       ; HALFTONE
.m:
    invoke SetStretchBltMode, rdi, r8
    xor r12d, r12d                   ; panel index
.panel:
    cmp r12d, [rbx+PX_PN]
    jae .end
    ; image of this panel
    mov r13, [rbx+PX_CAN]
    mov r14d, [rbx+PX_W]
    mov r15d, [rbx+PX_H]
    cmp qword [rbx+PX_MODE], 0
    jne .have
    cmp r12d, 2
    jae .have
    lea rax, [rbx+PX_A]
    test r12d, r12d
    jz .pick
    lea rax, [rbx+PX_B]
.pick:
    mov r13, [rax+IM_PIX]
    mov r14d, [rax+IM_W]
    mov r15d, [rax+IM_H]
.have:
    test r13, r13
    jz .next
    test r14d, r14d
    jz .next
    test r15d, r15d
    jz .next
    mov [rsp+S_PIX], r13
    mov rcx, [rbx+PX_ZOOM]
    ; visible source size
    mov eax, [rbx+PX_PWD]
    imul rax, 100
    xor edx, edx
    div rcx
    mov r8, rax                      ; vw
    mov eax, [rbx+PX_PHT]
    imul rax, 100
    xor edx, edx
    div rcx
    mov r9, rax                      ; vh
    ; source origin, clipped to this image
    mov rax, [rbx+PX_SX]
    cmp rax, r14
    jb .sx
    lea rax, [r14-1]
.sx:
    mov [rsp+S_SX], rax
    mov r10, r14
    sub r10, rax                     ; w - sx
    cmp r8, r10
    jbe .cw
    mov r8, r10
.cw:
    mov [rsp+S_CW], r8
    mov rax, [rbx+PX_SY]
    cmp rax, r15
    jb .sy
    lea rax, [r15-1]
.sy:
    mov [rsp+S_SY], rax
    mov r10, r15
    sub r10, rax
    cmp r9, r10
    jbe .ch
    mov r9, r10
.ch:
    mov [rsp+S_CH], r9
    ; destination size
    mov rax, [rsp+S_CW]
    imul rax, rcx
    xor edx, edx
    mov r10d, 100
    div r10
    test rax, rax
    jnz .dw
    mov eax, 1
.dw:
    mov [rsp+S_DW], rax
    mov rax, [rsp+S_CH]
    imul rax, rcx
    xor edx, edx
    div r10
    test rax, rax
    jnz .dh
    mov eax, 1
.dh:
    mov [rsp+S_DH], rax
    mov eax, [rbx+PX_PWD]
    add eax, PGAP
    imul eax, r12d
    mov [rsp+S_X0], rax
    ; BITMAPINFOHEADER (top-down 32-bit)
    invoke memset, &rsp+S_BMI, 0, 40
    mov dword [rsp+S_BMI], 40
    mov [rsp+S_BMI+4], r14d
    mov eax, r15d
    neg eax
    mov [rsp+S_BMI+8], eax
    mov word [rsp+S_BMI+12], 1
    mov word [rsp+S_BMI+14], 32
    invoke StretchDIBits, rdi, [rsp+S_X0], 0, [rsp+S_DW], [rsp+S_DH], [rsp+S_SX], [rsp+S_SY], [rsp+S_CW], [rsp+S_CH], [rsp+S_PIX], &rsp+S_BMI, 0, 0x00CC0020
.next:
    inc r12d
    jmp .panel
.end:
    invoke EndPaint, rsi, &rsp+S_PS
    EPILOG 384

; pp_pixel(img/canvas pointer info...) helpers are inlined in the mouse handler

; pp_scroll(hwnd, px, bar, code)
pp_scroll:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d                    ; bar
    mov r13d, r9d                    ; code
    mov rcx, [rbx+PX_ZOOM]
    mov eax, [rbx+PX_PWD]
    cmp r12d, SB_VERT
    jne .h
    mov eax, [rbx+PX_PHT]
.h:
    imul rax, 100
    xor edx, edx
    div rcx
    mov r14, rax                     ; page in source pixels
    lea r15, [rbx+PX_SX]
    cmp r12d, SB_VERT
    jne .p
    lea r15, [rbx+PX_SY]
.p:
    mov rax, [r15]
    mov ecx, 16
    cmp r13d, SB_LINEUP
    jne .l2
    sub rax, rcx
    jmp .set
.l2:
    cmp r13d, SB_LINEDOWN
    jne .l3
    add rax, rcx
    jmp .set
.l3:
    cmp r13d, SB_PAGEUP
    jne .l4
    sub rax, r14
    jmp .set
.l4:
    cmp r13d, SB_PAGEDOWN
    jne .l5
    add rax, r14
    jmp .set
.l5:
    cmp r13d, 4
    je .thumb
    cmp r13d, SB_THUMBTRACK
    jne .d
.thumb:
    mov dword [rsp+96], SI_SIZE
    mov dword [rsp+100], SIF_TRACKPOS
    invoke GetScrollInfo, rsi, r12, &rsp+96
    mov eax, [rsp+96+SI_TRACK]
.set:
    test rax, rax
    jns .st
    xor eax, eax
.st:
    mov [r15], rax
    invoke pp_scrollbars, rsi, rbx
    invoke InvalidateRect, rsi, 0, 0
.d:
    EPILOG 192

; pp_cursor(hwnd, px, x, y): show the pixel under the mouse in the "cursor" label
pp_cursor:
    PROLOG 320
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d                    ; x
    mov r13d, r9d                    ; y
    cmp qword [rbx+PX_LOADED], 0
    je .d
    mov ecx, [rbx+PX_PWD]
    add ecx, PGAP
    mov eax, r12d
    xor edx, edx
    div ecx
    mov r14d, edx                    ; x inside the panel
    mov rcx, [rbx+PX_ZOOM]
    mov eax, r14d
    imul rax, 100
    xor edx, edx
    div rcx
    add rax, [rbx+PX_SX]
    mov r14, rax                     ; source x
    mov eax, r13d
    imul rax, 100
    xor edx, edx
    div rcx
    add rax, [rbx+PX_SY]
    mov r15, rax                     ; source y
    ; A pixel
    mov qword [rsp+96], -1
    mov qword [rsp+104], -1
    lea rdi, [rbx+PX_A]
    cmp r14d, [rdi+IM_W]
    jae .b
    cmp r15d, [rdi+IM_H]
    jae .b
    mov eax, r15d
    imul eax, [rdi+IM_W]
    add eax, r14d
    mov rcx, [rdi+IM_PIX]
    mov eax, [rcx+rax*4]
    and eax, 0x00FFFFFF
    mov [rsp+96], rax
.b:
    lea rdi, [rbx+PX_B]
    cmp r14d, [rdi+IM_W]
    jae .fmt
    cmp r15d, [rdi+IM_H]
    jae .fmt
    mov eax, r15d
    imul eax, [rdi+IM_W]
    add eax, r14d
    mov rcx, [rdi+IM_PIX]
    mov eax, [rcx+rax*4]
    and eax, 0x00FFFFFF
    mov [rsp+104], rax
.fmt:
    ; the pixels are 32bpp ARGB dwords (0xAARRGGBB): the low 24 bits read RRGGBB as they are
    ; (they used to be byte-swapped here, which showed red as #0000FF)
    cmp qword [rsp+96], -1
    je .onlyb
    cmp qword [rsp+104], -1
    je .onlya
    invoke _snwprintf, &rsp+112, 80, &f_cur, r14, r15, [rsp+96], [rsp+104]
    jmp .show
.onlya:
    invoke _snwprintf, &rsp+112, 80, &f_cur_a, r14, r15, [rsp+96]
    jmp .show
.onlyb:
    cmp qword [rsp+104], -1
    je .d
    invoke _snwprintf, &rsp+112, 80, &f_cur_b, r14, r15, [rsp+104]
.show:
    invoke GetParent, rsi
    invoke GetDlgItem, rax, ID_LBL_CUR
    invoke SetWindowTextW, rax, &rsp+112
.d:
    EPILOG 320

; pp_proc: the drawing pane
pp_proc:
    PROLOG 256
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_ERASEBKGND
    je .one
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    test rax, rax
    jz .def
    mov r14, rax
    cmp edi, WM_PAINT
    je .paint
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_HSCROLL
    je .hscroll
    cmp edi, WM_VSCROLL
    je .vscroll
    cmp edi, WM_MOUSEWHEEL
    je .wheel
    cmp edi, WM_MOUSEMOVE
    je .move
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 256
.one:
    mov eax, 1
    EPILOG 256
.create:
    mov rax, [r13]
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, rax
    xor eax, eax
    EPILOG 256
.paint:
    invoke pp_paint, rsi, r14
    jmp .zero
.size:
    invoke pp_metrics, rsi, r14
    invoke pp_scrollbars, rsi, r14
    jmp .zero
.hscroll:
    movzx r9d, r12w
    invoke pp_scroll, rsi, r14, SB_HORZ, r9
    jmp .zero
.vscroll:
    movzx r9d, r12w
    invoke pp_scroll, rsi, r14, SB_VERT, r9
    jmp .zero
.wheel:
    mov rax, r12
    shr rax, 16
    movsx eax, ax
    neg eax
    imul eax, eax, 3
    cdq
    mov ecx, 120
    idiv ecx
    movsxd rax, eax
    imul rax, 16
    add rax, [r14+PX_SY]
    test rax, rax
    jns .wy
    xor eax, eax
.wy:
    mov [r14+PX_SY], rax
    invoke pp_scrollbars, rsi, r14
    invoke InvalidateRect, rsi, 0, 0
    jmp .zero
.move:
    movsx r8d, r13w
    mov r9, r13
    shr r9, 16
    movsx r9d, r9w
    invoke pp_cursor, rsi, r14, r8, r9
    jmp .zero
.zero:
    xor eax, eax
    EPILOG 256

%include "pic_ui2.inc"
