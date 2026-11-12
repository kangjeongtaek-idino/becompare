; text_gutter.inc - line-number gutters (with diff bars and inline copy glyphs) and the diff overview map

%define GT_TV   0
%define GT_SIDE 8
%define GT_HV   16

%define EM_POSFROMCHAR 0x00D6

section .rdata
WSTR cn_gutter, "BCGutter"
WSTR cn_mini, "BCMini"
WSTR f_lineno, "%d"
glyph_fwd: dw 0xE72A, 0
glyph_bak: dw 0xE72B, 0

section .text

; gt_metrics(pane, &first, &y0, &h): first visible line, its y in the pane client area, line pitch
gt_metrics:
    PROLOG 128
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    mov r14, r9
    invoke SendMessageW, rbx, EM_GETFIRSTVISIBLELINE, 0, 0
    mov [r12], eax
    mov r15d, eax
    invoke SendMessageW, rbx, EM_LINEINDEX, r15, 0
    movsxd rsi, eax
    invoke SendMessageW, rbx, EM_POSFROMCHAR, &rsp+96, rsi
    mov eax, [rsp+100]
    mov [r13], eax                   ; y of the first visible line
    lea esi, [r15+1]
    invoke SendMessageW, rbx, EM_LINEINDEX, rsi, 0
    movsxd rsi, eax
    test rsi, rsi
    js .def
    invoke SendMessageW, rbx, EM_POSFROMCHAR, &rsp+96, rsi
    mov eax, [rsp+100]
    sub eax, [r13]
    cmp eax, 4
    jl .def
    mov [r14], eax
    EPILOG 128
.def:
    mov dword [r14], 16
    EPILOG 128

; gt_paint(hwnd, gt)
%define P_PS   96
%define P_RC   176
%define P_BUF  200
%define P_FIRST 240
%define P_Y0   244
%define P_H    248
%define P_BR   256
gt_paint:
    PROLOG 384
    mov rsi, rcx
    mov rbx, rdx
    mov r12, [rbx+GT_TV]
    invoke BeginPaint, rsi, &rsp+P_PS
    mov rdi, rax
    invoke GetClientRect, rsi, &rsp+P_RC
    invoke FillRect, rdi, &rsp+P_RC, [th_panelbrush]
    mov r13, [r12+TV_HA]
    cmp qword [rbx+GT_SIDE], 0
    je .pane
    mov r13, [r12+TV_HB]
.pane:
    invoke gt_metrics, r13, &rsp+P_FIRST, &rsp+P_Y0, &rsp+P_H
    invoke SelectObject, rdi, [g_font]
    invoke SetBkMode, rdi, 1
    invoke SetTextColor, rdi, #[th_same]
    mov r14d, [rsp+P_FIRST]          ; line
    mov r15d, [rsp+P_Y0]             ; y
.row:
    cmp r14, [r12+TV_NROWS]
    jae .done
    cmp r15d, [rsp+P_RC+12]
    jge .done
    mov rax, r14
    shl rax, 4
    add rax, [r12+TV_ROWS]
    mov rbp, rax                     ; row
    ; band: the gutter row carries the colour of its block, so the gutters and panes read as one ribbon
    mov ecx, [rbp+4]
    cmp qword [rbx+GT_SIDE], 0
    je .tidx
    mov ecx, [rbp+8]
.tidx:
    cmp ecx, -1
    je .tgap
    mov eax, [rbp]
    test eax, eax
    jz .notint
    mov ecx, [th_tc_mod]
    cmp eax, 1
    je .tfill
    mov ecx, [th_tc_del]
    cmp eax, 2
    je .tfill
    mov ecx, [th_tc_ins]
    jmp .tfill
.tgap:
    mov ecx, [th_tc_gap]             ; this side has no line in the block
.tfill:
    invoke CreateSolidBrush, rcx
    mov [rsp+P_BR+16], rax
    mov dword [rsp+P_BR], 0
    mov [rsp+P_BR+4], r15d
    mov eax, [rsp+P_RC+8]
    mov [rsp+P_BR+8], eax
    mov eax, r15d
    add eax, [rsp+P_H]
    mov [rsp+P_BR+12], eax
    invoke FillRect, rdi, &rsp+P_BR, [rsp+P_BR+16]
    invoke DeleteObject, [rsp+P_BR+16]
.notint:
    invoke bm_has, r12, r14
    test eax, eax
    jz .nobm
    mov dword [rsp+P_BR], 0
    lea eax, [r15+2]
    mov [rsp+P_BR+4], eax
    mov dword [rsp+P_BR+8], 6
    mov eax, r15d
    add eax, [rsp+P_H]
    sub eax, 2
    mov [rsp+P_BR+12], eax
    invoke CreateSolidBrush, 0xD07020
    mov [rsp+P_BR+16], rax
    invoke FillRect, rdi, &rsp+P_BR, [rsp+P_BR+16]
    invoke DeleteObject, [rsp+P_BR+16]
.nobm:
    mov ecx, [rbp+4]
    cmp qword [rbx+GT_SIDE], 0
    je .idx
    mov ecx, [rbp+8]
.idx:
    cmp ecx, -1
    je .next                         ; filler row: nothing to number
    lea r13d, [rcx+1]
    invoke _snwprintf, &rsp+P_BUF, 12, &f_lineno, r13
    ; rect for the number: [2, y, width-22, y+h]
    mov dword [rsp+P_BR], 2
    mov [rsp+P_BR+4], r15d
    mov eax, [rsp+P_RC+8]
    sub eax, 22
    mov [rsp+P_BR+8], eax
    mov eax, r15d
    add eax, [rsp+P_H]
    mov [rsp+P_BR+12], eax
    invoke DrawTextW, rdi, &rsp+P_BUF, -1, &rsp+P_BR, 0x0826   ; DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
    mov eax, [rbp]                   ; kind
    test eax, eax
    jz .next
    ; colour bar on the inner edge
    mov ecx, [th_tc_modb]            ; modified: blue
    cmp eax, 1
    je .bar
    mov ecx, [th_tc_delb]            ; only on the left: deleted, grey
    cmp eax, 2
    je .bar
    mov ecx, [th_tc_insb]            ; only on the right: inserted, green
.bar:
    invoke CreateSolidBrush, rcx
    mov [rsp+P_BR+16], rax
    mov eax, [rsp+P_RC+8]
    sub eax, 5
    mov [rsp+P_BR], eax
    mov [rsp+P_BR+4], r15d
    mov eax, [rsp+P_RC+8]
    mov [rsp+P_BR+8], eax
    mov eax, r15d
    add eax, [rsp+P_H]
    mov [rsp+P_BR+12], eax
    invoke FillRect, rdi, &rsp+P_BR, [rsp+P_BR+16]
    invoke DeleteObject, [rsp+P_BR+16]
    ; inline copy glyph on the first row of a block
    mov eax, [rbp+12]
    cmp eax, -1
    je .next
    mov rcx, [r12+TV_BLK]
    mov ecx, [rcx+rax*8]
    cmp rcx, r14
    jne .next
    invoke SelectObject, rdi, [g_iconfont]
    invoke SetTextColor, rdi, #[th_fg]
    mov dword [rsp+P_BR], 2
    mov [rsp+P_BR+4], r15d
    mov eax, [rsp+P_RC+8]
    sub eax, 6
    mov [rsp+P_BR+8], eax
    mov eax, r15d
    add eax, [rsp+P_H]
    mov [rsp+P_BR+12], eax
    lea rdx, [glyph_fwd]
    cmp qword [rbx+GT_SIDE], 0
    je .gl
    lea rdx, [glyph_bak]
.gl:
    invoke DrawTextW, rdi, rdx, 1, &rsp+P_BR, 0x0824            ; DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX
    invoke SelectObject, rdi, [g_font]
    invoke SetTextColor, rdi, #[th_same]
.next:
    inc r14
    mov eax, [rsp+P_H]
    add r15d, eax
    jmp .row
.done:
    invoke EndPaint, rsi, &rsp+P_PS
    EPILOG 384

; gt_click(hwnd, gt, y): clicking the glyph of a block copies that block
gt_click:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d                    ; y
    mov r13, [rbx+GT_TV]
    mov r14, [r13+TV_HA]
    cmp qword [rbx+GT_SIDE], 0
    je .m
    mov r14, [r13+TV_HB]
.m:
    invoke gt_metrics, r14, &rsp+96, &rsp+100, &rsp+104
    mov eax, r12d
    sub eax, [rsp+100]
    js .d
    xor edx, edx
    mov ecx, [rsp+104]
    test ecx, ecx
    jz .d
    div ecx
    add eax, [rsp+96]                ; row
    cmp rax, [r13+TV_NROWS]
    jae .d
    shl rax, 4
    add rax, [r13+TV_ROWS]
    cmp dword [rax], 0
    je .d
    mov ecx, [rax+12]                ; block
    cmp ecx, -1
    je .d
    mov rdx, [r13+TV_BLK]
    mov edx, [rdx+rcx*8]             ; first row of the block
    mov r8, rax
    sub r8, [r13+TV_ROWS]
    shr r8, 4
    cmp rdx, r8
    jne .d
    mov [r13+TV_FORCE], rcx
    mov r14d, ID_BTN_CLR
    cmp qword [rbx+GT_SIDE], 0
    je .go
    mov r14d, ID_BTN_CRL
.go:
    invoke SendMessageW, [rbx+GT_HV], WM_COMMAND, r14, 0
.d:
    EPILOG 192

gt_proc:
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
    cmp edi, WM_LBUTTONDOWN
    je .click
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
    invoke gt_paint, rsi, r14
    xor eax, eax
    EPILOG 256
.click:
    mov rax, r13
    shr rax, 16
    movsx r8d, ax
    invoke gt_click, rsi, r14, r8
    xor eax, eax
    EPILOG 256

; ---------------------------------------------------------------- overview map
; mm_paint(hwnd, tv)
%define M_PS  96
%define M_RC  176
%define M_BR  200
%define M_FIRST 224
%define M_Y0  228
%define M_H   232
mm_paint:
    PROLOG 320
    mov rsi, rcx
    mov rbx, rdx
    invoke BeginPaint, rsi, &rsp+M_PS
    mov rdi, rax
    invoke GetClientRect, rsi, &rsp+M_RC
    invoke FillRect, rdi, &rsp+M_RC, [th_panelbrush]
    mov r12, [rbx+TV_NROWS]
    test r12, r12
    jz .done
    mov r13d, [rsp+M_RC+12]          ; height
    xor r14d, r14d                   ; block
.b:
    cmp r14, [rbx+TV_NBLK]
    jae .view
    mov rax, [rbx+TV_BLK]
    mov ecx, [rax+r14*8]             ; start row
    mov edx, [rax+r14*8+4]           ; end row
    mov r15d, ecx
    ; y1 = start*H/n ; y2 = max(end*H/n, y1+3)
    mov eax, ecx
    imul rax, r13
    xor edx, edx
    div r12
    mov [rsp+M_BR+4], eax
    mov rax, [rbx+TV_BLK]
    mov ecx, [rax+r14*8+4]
    mov eax, ecx
    imul rax, r13
    xor edx, edx
    div r12
    mov ecx, [rsp+M_BR+4]
    add ecx, 3
    cmp eax, ecx
    jge .y2
    mov eax, ecx
.y2:
    mov [rsp+M_BR+12], eax
    mov dword [rsp+M_BR], 2
    mov eax, [rsp+M_RC+8]
    sub eax, 2
    mov [rsp+M_BR+8], eax
    ; colour by the kind of the first row
    mov rax, r15
    shl rax, 4
    add rax, [rbx+TV_ROWS]
    mov eax, [rax]
    mov ecx, [th_tc_modb]
    cmp eax, 1
    je .col
    mov ecx, [th_tc_delb]
    cmp eax, 2
    je .col
    mov ecx, [th_tc_insb]
.col:
    invoke CreateSolidBrush, rcx
    mov rbp, rax
    invoke FillRect, rdi, &rsp+M_BR, rbp
    invoke DeleteObject, rbp
    inc r14
    jmp .b
.view:
    ; the viewport of the left pane
    invoke gt_metrics, [rbx+TV_HA], &rsp+M_FIRST, &rsp+M_Y0, &rsp+M_H
    invoke GetClientRect, [rbx+TV_HA], &rsp+M_BR
    mov eax, [rsp+M_BR+12]
    xor edx, edx
    mov ecx, [rsp+M_H]
    test ecx, ecx
    jz .done
    div ecx
    mov r14d, eax                    ; page in lines
    mov eax, [rsp+M_FIRST]
    imul rax, r13
    xor edx, edx
    div r12
    mov [rsp+M_BR+4], eax
    mov eax, r14d
    imul rax, r13
    xor edx, edx
    div r12
    add eax, [rsp+M_BR+4]
    cmp eax, r13d
    jle .vb
    mov eax, r13d
.vb:
    mov [rsp+M_BR+12], eax
    mov dword [rsp+M_BR], 0
    mov eax, [rsp+M_RC+8]
    mov [rsp+M_BR+8], eax
    invoke CreateSolidBrush, #[th_fg]
    mov rbp, rax
    invoke FrameRect, rdi, &rsp+M_BR, rbp
    invoke DeleteObject, rbp
.done:
    invoke EndPaint, rsi, &rsp+M_PS
    EPILOG 320

; mm_click(hwnd, tv, y): scroll both panes so that the clicked position is centred
mm_click:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    cmp qword [rbx+TV_NROWS], 0
    je .d
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, [rsp+108]
    test ecx, ecx
    jz .d
    mov eax, r12d
    imul rax, [rbx+TV_NROWS]
    xor edx, edx
    div rcx
    mov r13d, eax                    ; target line
    invoke gt_metrics, [rbx+TV_HA], &rsp+96, &rsp+100, &rsp+104
    invoke GetClientRect, [rbx+TV_HA], &rsp+112
    mov eax, [rsp+124]
    xor edx, edx
    mov ecx, [rsp+104]
    test ecx, ecx
    jz .d
    div ecx
    shr eax, 1
    sub r13d, eax                    ; centre the click
    jns .p
    xor r13d, r13d
.p:
    sub r13d, [rsp+96]
    movsxd r14, r13d
    mov qword [rbx+TV_GUARD], 1
    invoke SendMessageW, [rbx+TV_HA], EM_LINESCROLL, 0, r14
    invoke SendMessageW, [rbx+TV_HB], EM_LINESCROLL, 0, r14
    mov qword [rbx+TV_GUARD], 0
    invoke tv_redraw_aux, rbx
.d:
    EPILOG 192

mm_proc:
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
    cmp edi, WM_LBUTTONDOWN
    je .click
    cmp edi, 0x0200                  ; WM_MOUSEMOVE (drag)
    je .drag
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
    invoke mm_paint, rsi, r14
    xor eax, eax
    EPILOG 256
.drag:
    test r12d, 1                     ; MK_LBUTTON
    jz .def
.click:
    mov rax, r13
    shr rax, 16
    movsx r8d, ax
    invoke mm_click, rsi, r14, r8
    xor eax, eax
    EPILOG 256

; tv_redraw_aux(tv): gutters and the overview map follow scroll / text changes
tv_redraw_aux:
    PROLOG 64
    mov rbx, rcx
    invoke InvalidateRect, [rbx+TV_HGL], 0, 0
    invoke InvalidateRect, [rbx+TV_HGR], 0, 0
    invoke InvalidateRect, [rbx+TV_HMM], 0, 0
    EPILOG 64

; bm_has(tv, row) -> eax 1 when the row carries a bookmark
bm_has:
    xor eax, eax
    mov r8d, [rcx+TV_NBM]
    xor r9d, r9d
.l:
    cmp r9d, r8d
    jae .d
    cmp [rcx+TV_BM+r9*4], edx
    je .y
    inc r9d
    jmp .l
.y:
    mov eax, 1
.d:
    ret

; tv_bm_goto(hwnd, tv, row): caret of both panes onto the row
tv_bm_goto:
    PROLOG 96
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    invoke SendMessageW, [rbx+TV_HA], EM_LINEINDEX, r12, 0
    invoke SendMessageW, [rbx+TV_HA], EM_SETSEL, rax, rax
    invoke SendMessageW, [rbx+TV_HA], EM_SCROLLCARET, 0, 0
    invoke SendMessageW, [rbx+TV_HB], EM_LINEINDEX, r12, 0
    invoke SendMessageW, [rbx+TV_HB], EM_SETSEL, rax, rax
    invoke SendMessageW, [rbx+TV_HB], EM_SCROLLCARET, 0, 0
    invoke tv_redraw_aux, rbx
    EPILOG 96

; tv_bm_toggle(hwnd, tv): bookmark on / off for the caret row
tv_bm_toggle:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke tv_caretrow, rbx
    mov edi, eax
    mov ecx, [rbx+TV_NBM]
    xor edx, edx
.f:
    cmp edx, ecx
    jae .add
    cmp [rbx+TV_BM+rdx*4], edi
    je .rem
    inc edx
    jmp .f
.rem:
    lea eax, [rdx+1]
.sh:
    cmp eax, ecx
    jae .shd
    mov r8d, [rbx+TV_BM+rax*4]
    mov [rbx+TV_BM+rdx*4], r8d
    inc eax
    inc edx
    jmp .sh
.shd:
    dec ecx
    mov [rbx+TV_NBM], ecx
    jmp .redraw
.add:
    cmp ecx, 64
    jae .redraw
    mov [rbx+TV_BM+rcx*4], edi
    inc ecx
    mov [rbx+TV_NBM], ecx
.redraw:
    invoke tv_redraw_aux, rbx
    EPILOG 64

; tv_bm_step(hwnd, tv, dir 1 next / 0 previous) -> eax 1 when a bookmark exists
tv_bm_step:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    cmp dword [rbx+TV_NBM], 0
    je .none
    invoke tv_caretrow, rbx
    mov edi, eax                      ; current row
    mov r13d, -1                      ; best after / before the caret
    mov r14d, -1                      ; best overall (wrap)
    xor ecx, ecx
.l:
    cmp ecx, [rbx+TV_NBM]
    jae .pick
    mov eax, [rbx+TV_BM+rcx*4]
    test r12d, r12d
    jz .prev
    cmp eax, edi
    jle .wrapn
    cmp r13d, -1
    je .setn
    cmp eax, r13d
    jge .wrapn
.setn:
    mov r13d, eax
.wrapn:
    cmp r14d, -1
    je .setw
    cmp eax, r14d
    jge .nx
.setw:
    mov r14d, eax
    jmp .nx
.prev:
    cmp eax, edi
    jge .wrapp
    cmp r13d, -1
    je .setp
    cmp eax, r13d
    jle .wrapp
.setp:
    mov r13d, eax
.wrapp:
    cmp r14d, -1
    je .setq
    cmp eax, r14d
    jle .nx
.setq:
    mov r14d, eax
.nx:
    inc ecx
    jmp .l
.pick:
    mov r8d, r13d
    cmp r8d, -1
    jne .go
    mov r8d, r14d
.go:
    invoke tv_bm_goto, rsi, rbx, r8
    mov eax, 1
    EPILOG 64
.none:
    xor eax, eax
    EPILOG 64
