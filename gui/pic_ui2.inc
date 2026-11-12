; pic_ui2.inc - Picture Compare: session view (info, compare, zoom, layout, window procedure)

; pv_info(hwnd, px)
pv_info:
    PROLOG 512
    mov rsi, rcx
    mov rbx, rdx
    mov eax, [rbx+PX_W]
    mov ecx, [rbx+PX_H]
    imul rax, rcx
    mov r8, rax                      ; W*H
    xor r12d, r12d                   ; whole percent
    xor r13d, r13d                   ; hundredths
    test r8, r8
    jz .fmt
    mov rax, [rbx+PX_NDIFF]
    imul rax, 10000
    xor edx, edx
    div r8
    xor edx, edx
    mov ecx, 100
    div rcx
    mov r12, rax
    mov r13, rdx
.fmt:
    mov r9d, [rbx+PX_A+IM_W]
    mov eax, [rbx+PX_A+IM_H]
    mov [rsp+32], rax
    mov eax, [rbx+PX_B+IM_W]
    mov [rsp+40], rax
    mov eax, [rbx+PX_B+IM_H]
    mov [rsp+48], rax
    mov rax, [rbx+PX_NDIFF]
    mov [rsp+56], rax
    mov [rsp+64], r12
    mov [rsp+72], r13
    mov rax, [rbx+PX_ZOOM]
    mov [rsp+80], rax
    lea rcx, [rsp+96]
    mov edx, 140
    lea r8, [f_info]
    call _snwprintf
    invoke GetDlgItem, rsi, ID_LBL_INFO
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 512

; pv_readopts(hwnd, px): tolerance (0..255) and display mode
pv_readopts:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    invoke GetDlgItem, rsi, ID_EDIT_TOL
    invoke GetWindowTextW, rax, &rsp+96, 12
    invoke wcstol, &rsp+96, 0, 10
    test eax, eax
    jns .p
    xor eax, eax
.p:
    cmp eax, 255
    jbe .s
    mov eax, 255
.s:
    mov [rbx+PX_TOL], rax
    invoke GetDlgItem, rsi, ID_COMBO
    invoke SendMessageW, rax, CB_GETCURSEL, 0, 0
    movsxd rax, eax
    test rax, rax
    jns .m
    xor eax, eax
.m:
    mov [rbx+PX_MODE], rax
    EPILOG 192

; pv_rebuild(hwnd, px): recompute the canvas and redraw (tolerance / mode / transform changes)
pv_rebuild:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke pv_readopts, rsi, rbx
    invoke px_build, rbx
    invoke pp_metrics, [rbx+PX_HPANE], rbx
    invoke pp_scrollbars, [rbx+PX_HPANE], rbx
    invoke InvalidateRect, [rbx+PX_HPANE], 0, 1
    invoke pv_info, rsi, rbx
    EPILOG 64

; pv_compare(hwnd, px, sess)
pv_compare:
    PROLOG 256
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rbx+PX_PATHA, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, &rbx+PX_PATHB, 519
    invoke img_load, &rbx+PX_PATHA, &rbx+PX_A
    test eax, eax
    jz .err
    invoke img_load, &rbx+PX_PATHB, &rbx+PX_B
    test eax, eax
    jz .err
    invoke pv_readopts, rsi, rbx
    invoke px_build, rbx
    mov qword [rbx+PX_LOADED], 1
    invoke pp_fit, [rbx+PX_HPANE], rbx
    invoke InvalidateRect, [rbx+PX_HPANE], 0, 1
    invoke pv_info, rsi, rbx
    invoke wcsrchr, &rbx+PX_PATHA, 0x5C
    lea r13, [rbx+PX_PATHA]
    test rax, rax
    jz .ta
    lea r13, [rax+2]
.ta:
    invoke wcsrchr, &rbx+PX_PATHB, 0x5C
    lea r14, [rbx+PX_PATHB]
    test rax, rax
    jz .tb
    lea r14, [rax+2]
.tb:
    invoke _snwprintf, &rsp+96, 60, &f_title, r13, r14
    invoke set_tab_title, r12, &rsp+96
    EPILOG 256
.err:
    invoke msgbox, rsi, &s_cantread, MB_OK|MB_ICONWARNING
    EPILOG 256

; pv_zoom(hwnd, px, dir 1 in / 0 out)
pv_zoom:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov rax, [rbx+PX_ZOOM]
    lea rcx, [zoom_steps]
    test r12d, r12d
    jz .out
    xor edx, edx
.in:
    mov r8d, [rcx+rdx*4]
    test r8d, r8d
    jz .done
    cmp r8, rax
    ja .set
    inc edx
    jmp .in
.out:
    xor edx, edx
    mov r9d, 10
.o:
    mov r8d, [rcx+rdx*4]
    test r8d, r8d
    jz .setr9
    cmp r8, rax
    jae .setr9
    mov r9, r8
    inc edx
    jmp .o
.setr9:
    mov r8, r9
.set:
    mov [rbx+PX_ZOOM], r8
    invoke pp_scrollbars, [rbx+PX_HPANE], rbx
    invoke InvalidateRect, [rbx+PX_HPANE], 0, 1
    invoke pv_info, rsi, rbx
.done:
    EPILOG 64

; ---------------------------------------------------------------- layout / options / window procedure
%macro PLACE 2
    mov ecx, %2
    call scl
    mov ebx, eax
    ; wrap to the next toolbar row when the control would leave the client area
    cmp r12d, r14d
    je %%fit
    lea eax, [r12+rbx]
    cmp eax, [rsp+104]
    jbe %%fit
    mov r12d, r14d
    lea r13d, [r13+r15]
    add r13d, r14d
%%fit:
    MVID rsi, %1, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
%endmacro

pv_layout:
    PROLOG 192
    mov rsi, rcx
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, 4
    call scl
    mov r14d, eax
    mov ecx, 26
    call scl
    mov r15d, eax
    mov edx, [rsp+104]
    invoke pb_layout, rsi, rdx, r14
    mov r13d, eax
    mov r12d, r14d
    PLACE ID_COMPARE, 96
    PLACE ID_BTN_ROT, 40
    PLACE ID_BTN_FLH, 40
    PLACE ID_BTN_FLV, 40
    PLACE ID_BTN_ZOUT, 40
    PLACE ID_BTN_ZIN, 40
    PLACE ID_BTN_FIT, 40
    PLACE ID_BTN_DOC, 90
    ; the combo box owns its drop-down height
    mov ecx, 190
    call scl
    mov ebx, eax
    lea ebp, [r15+r15*8]
    MVID rsi, ID_COMBO, r12, r13, rbx, rbp
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_LBL_TOL, 170
    PLACE ID_EDIT_TOL, 50
    PLACE ID_LBL_INFO, 520
    PLACE ID_LBL_CUR, 260
    lea r13d, [r13+r15]
    add r13d, r14d
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .h
    xor ebp, ebp
.h:
    mov ebx, [rsp+104]
    MVID rsi, ID_PPANE, 0, r13, rbx, rbp
    EPILOG 192

pv_collect:
    PROLOG 256
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    invoke pb_collect, rsi, rdi
    invoke pv_readopts, rsi, rbx
    invoke GetDlgItem, rsi, ID_EDIT_TOL
    invoke GetWindowTextW, rax, &rsp+96, 12
    mov r9, [rbx+PX_MODE]
    invoke _snwprintf, &rdi+SS_OPTS, 1023, &f_opts, r9, &rsp+96
    EPILOG 256

pv_opts_apply:
    PROLOG 64
    mov rsi, rcx
    lea rbx, [rdx+SS_OPTS]
    cmp word [rbx], 0
    je .d
    movzx r8d, word [rbx]
    sub r8d, '0'
    invoke GetDlgItem, rsi, ID_COMBO
    invoke SendMessageW, rax, CB_SETCURSEL, r8, 0
    invoke GetDlgItem, rsi, ID_EDIT_TOL
    invoke SetWindowTextW, rax, &rbx+4
.d:
    EPILOG 64

pv_proc:
    PROLOG 320
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_ERASEBKGND
    je .erase
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    test rax, rax
    jz .def
    mov r14, rax
    mov r15, [r14+SS_DATA]
    test r15, r15
    jz .def
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_COMMAND
    je .command
    cmp edi, BC_RUN
    je .run
    cmp edi, BC_COLLECT
    je .collect
    cmp edi, BC_THEME
    je .theme
    cmp edi, WM_CTLCOLOREDIT
    je .cedit
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLORBTN
    je .cstatic
    cmp edi, WM_SETFOCUS
    je .focus
    cmp edi, WM_DESTROY
    je .destroy
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 320

.create:
    mov r14, [r13]
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, r14
    invoke calloc, 1, PX_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    mov qword [r15+PX_ZOOM], 100
    invoke pb_create, rsi
    invoke mk_btn, rsi, &s_b_rot, ID_BTN_ROT
    invoke mk_btn, rsi, &s_b_flh, ID_BTN_FLH
    invoke mk_btn, rsi, &s_b_flv, ID_BTN_FLV
    invoke mk_btn, rsi, &s_b_zout, ID_BTN_ZOUT
    invoke mk_btn, rsi, &s_b_zin, ID_BTN_ZIN
    invoke mk_btn, rsi, &s_b_fit, ID_BTN_FIT
    invoke mk_btn, rsi, &s_b_doc, ID_BTN_DOC
    invoke icon_set, rsi, ID_BTN_ROT, &g_rot, &s_b_rot
    invoke icon_set, rsi, ID_BTN_FLH, &g_flh, &s_b_flh
    invoke icon_set, rsi, ID_BTN_FLV, &g_flv, &s_b_flv
    invoke icon_set, rsi, ID_BTN_ZOUT, &g_zout, &s_b_zout
    invoke icon_set, rsi, ID_BTN_ZIN, &g_zin, &s_b_zin
    invoke icon_set, rsi, ID_BTN_FIT, &g_fit, &s_b_fit
    invoke mk, rsi, &cn_combo, 0, CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP, ID_COMBO
    mov rbx, rax
    invoke SendMessageW, rbx, CB_ADDSTRING, 0, &s_m0
    invoke SendMessageW, rbx, CB_ADDSTRING, 0, &s_m1
    invoke SendMessageW, rbx, CB_ADDSTRING, 0, &s_m2
    invoke SendMessageW, rbx, CB_SETCURSEL, 0, 0
    invoke mk_lbl, rsi, &s_l_tol, ID_LBL_TOL
    invoke mk_edit, rsi, ID_EDIT_TOL
    invoke GetDlgItem, rsi, ID_EDIT_TOL
    invoke SetWindowTextW, rax, &s_tol0
    invoke mk_lbl, rsi, &s_empty, ID_LBL_INFO
    invoke mk_lbl, rsi, &s_empty, ID_LBL_CUR
    invoke CreateWindowExW, 0, &cn_pane, 0, WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_HSCROLL|WS_BORDER|WS_CLIPSIBLINGS, 0, 0, 10, 10, rsi, ID_PPANE, [g_hinst], r15
    mov [r15+PX_HPANE], rax
    invoke pb_load, rsi, r14
    invoke pv_opts_apply, rsi, r14
    invoke theme_checks, rsi
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320

.size:
    invoke pv_layout, rsi
    jmp .zero
.run:
    invoke pv_compare, rsi, r15, r14
    test qword [g_batch], 0x20        ; /docx file: export the first compare and close
    jz .zero
    and qword [g_batch], ~0x20
    cmp qword [r15+PX_LOADED], 0
    je .bdx_fail
    invoke px_docx_path, rsi, r15, &g_dx_path
    test eax, eax
    jnz .bdx_ok
.bdx_fail:
    mov qword [g_exitcode], 2
.bdx_ok:
    test qword [g_batch], 4
    jz .zero
    invoke PostMessageW, [g_hframe], WM_CLOSE, 0, 0
    jmp .zero
.doc:
    invoke px_export, rsi, r15
    jmp .zero
.collect:
    invoke pv_collect, rsi, r14, r15
    jmp .zero
.theme:
    invoke theme_checks, rsi
    invoke InvalidateRect, [r15+PX_HPANE], 0, 1
    jmp .zero

.command:
    movzx ebx, r12w
    mov eax, r12d
    shr eax, 16
    mov ebp, eax
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_BROWSE_B
    je .browse
    cmp ebx, ID_SWAP
    je .browse
    cmp qword [r15+PX_LOADED], 0
    je .def
    cmp ebx, ID_BTN_ROT
    je .rot
    cmp ebx, ID_BTN_FLH
    je .flh
    cmp ebx, ID_BTN_FLV
    je .flv
    cmp ebx, ID_BTN_ZIN
    je .zin
    cmp ebx, ID_BTN_ZOUT
    je .zout
    cmp ebx, ID_BTN_FIT
    je .fit
    cmp ebx, ID_BTN_DOC
    je .doc
    cmp ebx, ID_COMBO
    je .combo
    cmp ebx, ID_EDIT_TOL
    je .tol
    jmp .def
.browse:
    invoke pb_command, rsi, rbx, 0
    jmp .zero
.rot:
    xor ebx, ebx
    jmp .xform
.flh:
    mov ebx, 1
    jmp .xform
.flv:
    mov ebx, 2
.xform:
    invoke img_xform, &r15+PX_A, rbx
    invoke img_xform, &r15+PX_B, rbx
    invoke pv_rebuild, rsi, r15
    jmp .zero
.zin:
    invoke pv_zoom, rsi, r15, 1
    jmp .zero
.zout:
    invoke pv_zoom, rsi, r15, 0
    jmp .zero
.fit:
    invoke pp_fit, [r15+PX_HPANE], r15
    invoke InvalidateRect, [r15+PX_HPANE], 0, 1
    invoke pv_info, rsi, r15
    jmp .zero
.combo:
    cmp ebp, CBN_SELCHANGE
    jne .zero
    invoke pv_rebuild, rsi, r15
    invoke pp_fit, [r15+PX_HPANE], r15
    invoke InvalidateRect, [r15+PX_HPANE], 0, 1
    invoke pv_info, rsi, r15
    jmp .zero
.tol:
    cmp ebp, 0x0200                  ; EN_KILLFOCUS
    jne .zero
    invoke pv_rebuild, rsi, r15
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+PX_HPANE]
    jmp .zero
.destroy:
    invoke img_free, &r15+PX_A
    invoke img_free, &r15+PX_B
    invoke free, [r15+PX_CAN]
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320
