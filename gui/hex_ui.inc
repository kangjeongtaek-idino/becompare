; hex_ui.inc - Hex Compare: paint, pane window, compare, find, save, session view

; hp_paint(hwnd, hx)
hp_paint:
    PROLOG 256
    mov rsi, rcx
    mov rbx, rdx
    invoke BeginPaint, rsi, &rsp+96                 ; PAINTSTRUCT at [rsp+96], rcPaint at +12
    mov rdi, rax
    mov [rbx+HX_PHDC], rdi
    invoke SelectObject, rdi, [g_mono]
    invoke SetBkMode, rdi, OPAQUE
    invoke FillRect, rdi, &rsp+108, [th_bgbrush]
    invoke GetClientRect, rsi, &rsp+176
    mov eax, [rsp+176+RC_B]
    mov [rsp+192], eax                              ; client bottom
    mov r12, [rbx+HX_TOP]
    xor r13d, r13d
.row:
    cmp r12, [rbx+HX_NV]
    jae .done
    mov rax, [rbx+HX_CH]
    imul rax, r13
    cmp eax, [rsp+192]
    jae .done
    mov [rbx+HX_PY], rax
    mov rcx, [rbx+HX_VROWS]
    mov r14d, [rcx+r12*4]
    shl r14, 4                                      ; first offset of the row
    invoke _snwprintf, &rsp+200, 12, &f_off, r14
    mov r15d, eax                                   ; digits written
    invoke SetBkColor, rdi, #[th_bg]
    invoke SetTextColor, rdi, #[th_same]
    invoke TextOutW, rdi, 0, [rbx+HX_PY], &rsp+200, r15
    xor r15d, r15d
.b:
    cmp r15d, 16
    jae .rn
    lea r8, [r14+r15]
    invoke hx_drawbyte, rbx, 0, r8, r15
    lea r8, [r14+r15]
    invoke hx_drawbyte, rbx, 1, r8, r15
    inc r15d
    jmp .b
.rn:
    inc r12
    inc r13d
    jmp .row
.done:
    invoke EndPaint, rsi, &rsp+96
    EPILOG 256

; hp_scrollto(hx, top): clamp and apply
hp_scrollto:
    PROLOG 64
    mov rbx, rcx
    mov rax, [rbx+HX_NV]
    sub rax, [rbx+HX_PAGE]
    jns .m
    xor eax, eax
.m:
    test rdx, rdx
    jns .p
    xor edx, edx
.p:
    cmp rdx, rax
    jbe .set
    mov rdx, rax
.set:
    mov [rbx+HX_TOP], rdx
    invoke hx_scrollbar, rbx
    invoke InvalidateRect, [rbx+HX_HPANE], 0, 0
    EPILOG 64

; hp_edit(hx, digit value): replace one nibble of the byte under the cursor
hp_edit:
    PROLOG 64
    mov rbx, rcx
    mov esi, edx
    mov rdi, [rbx+HX_BUFA]
    mov rax, [rbx+HX_SZA]
    lea r8, [rbx+HX_MODA]
    cmp qword [rbx+HX_CURP], 0
    je .own
    mov rdi, [rbx+HX_BUFB]
    mov rax, [rbx+HX_SZB]
    lea r8, [rbx+HX_MODB]
.own:
    mov rcx, [rbx+HX_CURO]
    cmp rcx, rax
    jae .d
    mov qword [r8], 1
    movzx eax, byte [rdi+rcx]
    cmp qword [rbx+HX_NIB], 0
    jne .low
    and eax, 15
    shl esi, 4
    or eax, esi
    mov [rdi+rcx], al
    mov qword [rbx+HX_NIB], 1
    jmp .recount
.low:
    and eax, 0xF0
    or eax, esi
    mov [rdi+rcx], al
    mov qword [rbx+HX_NIB], 2
.recount:
    mov rdx, [rbx+HX_CURO]
    shr rdx, 4
    mov rax, [rbx+HX_RDIFF]
    movzx r12d, byte [rax+rdx]
    mov r13, rdx
    invoke hx_diffrow, rbx, r13
    mov rax, [rbx+HX_RDIFF]
    movzx eax, byte [rax+r13]
    sub eax, r12d
    movsxd rax, eax
    add [rbx+HX_NDIFF], rax
    cmp qword [rbx+HX_NIB], 2
    jne .redraw
    mov rdx, [rbx+HX_CURO]
    inc rdx
    mov r8, [rbx+HX_CURP]
    invoke hx_setcursor, rbx, rdx, r8
    jmp .d
.redraw:
    invoke hx_status, rbx
    invoke InvalidateRect, [rbx+HX_HPANE], 0, 0
.d:
    EPILOG 64

; hp_proc: the owner-drawn pane
hp_proc:
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
    mov r14, rax                                    ; hx
    cmp edi, WM_PAINT
    je .paint
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_VSCROLL
    je .vscroll
    cmp edi, WM_MOUSEWHEEL
    je .wheel
    cmp edi, WM_LBUTTONDOWN
    je .click
    cmp edi, WM_KEYDOWN
    je .key
    cmp edi, WM_CHAR
    je .char
    cmp edi, 0x0087                                 ; WM_GETDLGCODE
    je .dlgcode
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 256
.one:
    mov eax, 1
    EPILOG 256
.dlgcode:
    mov eax, 0x0001|0x0004|0x0080                   ; DLGC_WANTARROWS | DLGC_WANTALLKEYS-ish
    or eax, 0x0002|0x0008
    EPILOG 256

.create:
    mov rax, [r13]
    mov r14, rax
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, r14
    invoke GetDC, rsi
    mov rbx, rax
    invoke SelectObject, rbx, [g_mono]
    invoke GetTextMetricsW, rbx, &rsp+96
    mov eax, [rsp+96+TM_HEIGHT]
    mov [r14+HX_CH], rax
    mov eax, [rsp+96+TM_AVECW]
    mov [r14+HX_CW], rax
    invoke ReleaseDC, rsi, rbx
    mov [r14+HX_HPANE], rsi
    xor eax, eax
    EPILOG 256

.paint:
    invoke hp_paint, rsi, r14
    xor eax, eax
    EPILOG 256

.size:
    invoke GetClientRect, rsi, &rsp+96
    mov eax, [rsp+96+RC_B]
    xor edx, edx
    mov rcx, [r14+HX_CH]
    test rcx, rcx
    jz .zero
    div rcx
    test rax, rax
    jnz .pg
    mov eax, 1
.pg:
    mov [r14+HX_PAGE], rax
    invoke hx_scrollbar, r14
    jmp .zero

.vscroll:
    movzx eax, r12w
    mov rdx, [r14+HX_TOP]
    cmp eax, SB_LINEUP
    jne .v1
    dec rdx
    jmp .vgo
.v1:
    cmp eax, SB_LINEDOWN
    jne .v2
    inc rdx
    jmp .vgo
.v2:
    cmp eax, SB_PAGEUP
    jne .v3
    sub rdx, [r14+HX_PAGE]
    jmp .vgo
.v3:
    cmp eax, SB_PAGEDOWN
    jne .v4
    add rdx, [r14+HX_PAGE]
    jmp .vgo
.v4:
    cmp eax, 4
    je .vthumb
    cmp eax, SB_THUMBTRACK
    jne .v5
.vthumb:
    mov dword [rsp+96], SI_SIZE
    mov dword [rsp+100], SIF_TRACKPOS
    invoke GetScrollInfo, rsi, SB_VERT, &rsp+96
    mov eax, [rsp+96+SI_TRACK]
    mov rdx, rax
    jmp .vgo
.v5:
    cmp eax, SB_TOP
    jne .v6
    xor edx, edx
    jmp .vgo
.v6:
    cmp eax, SB_BOTTOM
    jne .zero
    mov rdx, [r14+HX_NV]
.vgo:
    invoke hp_scrollto, r14, rdx
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
    mov rdx, [r14+HX_TOP]
    add rdx, rax
    invoke hp_scrollto, r14, rdx
    jmp .zero

.click:
    movsx eax, r13w                                 ; x
    mov rcx, r13
    shr rcx, 16
    movsx ecx, cx                                   ; y
    mov rdx, [r14+HX_CW]
    test rdx, rdx
    jz .zero
    xor edx, edx
    div qword [r14+HX_CW]
    mov r8d, eax                                    ; column
    mov eax, ecx
    xor edx, edx
    div qword [r14+HX_CH]
    add rax, [r14+HX_TOP]                           ; visible row
    cmp rax, [r14+HX_NV]
    jae .zero
    mov rcx, [r14+HX_VROWS]
    mov r15d, [rcx+rax*4]
    shl r15, 4                                      ; row base offset
    xor r9d, r9d                                    ; pane
    xor r10d, r10d                                  ; nibble
    cmp r8d, COL_HEXA
    jb .zero
    cmp r8d, COL_HEXA+48
    jb .hexa
    cmp r8d, COL_ASCA
    jb .zero
    cmp r8d, COL_ASCA+16
    jb .asca
    cmp r8d, COL_HEXB
    jb .zero
    cmp r8d, COL_HEXB+48
    jb .hexb
    cmp r8d, COL_ASCB
    jb .zero
    cmp r8d, COL_ASCB+16
    jae .zero
    mov r9d, 1
    sub r8d, COL_ASCB
    jmp .setc
.asca:
    sub r8d, COL_ASCA
    jmp .setc
.hexb:
    mov r9d, 1
    sub r8d, COL_HEXB
    jmp .hexi
.hexa:
    sub r8d, COL_HEXA
.hexi:
    mov eax, r8d
    xor edx, edx
    mov ecx, 3
    div ecx
    mov r8d, eax
    cmp edx, 1
    jne .setc
    mov r10d, 1
.setc:
    add r15, r8
    mov rbx, r9
    mov r12d, r10d
    invoke hx_setcursor, r14, r15, rbx
    mov [r14+HX_NIB], r12
    invoke SetFocus, rsi
    jmp .zero

.key:
    mov rax, [r14+HX_CURO]
    mov r15, [r14+HX_CURP]
    cmp r12d, VK_TAB
    je .tab
    cmp r12d, 0x25
    je .left
    cmp r12d, 0x27
    je .right
    cmp r12d, VK_UP
    je .up
    cmp r12d, VK_DOWN
    je .down
    cmp r12d, VK_PRIOR
    je .pgup
    cmp r12d, VK_NEXT
    je .pgdn
    cmp r12d, VK_HOME
    je .home
    cmp r12d, VK_END
    je .end
    jmp .zero
.left:
    mov ecx, 1
    jmp .dec
.up:
    mov ecx, 16
    jmp .dec
.pgup:
    mov rcx, [r14+HX_PAGE]
    shl rcx, 4
.dec:
    xor edx, edx
    cmp rax, rcx
    jb .kset
    mov rdx, rax
    sub rdx, rcx
    jmp .kset
.right:
    lea rdx, [rax+1]
    jmp .kset
.down:
    lea rdx, [rax+16]
    jmp .kset
.pgdn:
    mov rcx, [r14+HX_PAGE]
    shl rcx, 4
    lea rdx, [rax+rcx]
    jmp .kset
.home:
    mov rdx, rax
    and rdx, ~15
    jmp .kset
.end:
    mov rdx, rax
    or rdx, 15
    jmp .kset
.tab:
    mov rdx, rax
    xor r15, 1
.kset:
    invoke hx_setcursor, r14, rdx, r15
    jmp .zero

.char:
    mov eax, r12d
    cmp eax, '0'
    jb .zero
    cmp eax, '9'
    jbe .dig
    or eax, 0x20
    cmp eax, 'a'
    jb .zero
    cmp eax, 'f'
    ja .zero
    sub eax, 'a'-10
    jmp .hv
.dig:
    sub eax, '0'
.hv:
    invoke hp_edit, r14, rax
    jmp .zero

.zero:
    xor eax, eax
    EPILOG 256

; ---------------------------------------------------------------- compare / find / save
; hx_free(hx)
hx_free:
    PROLOG 64
    mov rbx, rcx
    invoke free, [rbx+HX_BUFA]
    invoke free, [rbx+HX_BUFB]
    invoke free, [rbx+HX_RDIFF]
    mov qword [rbx+HX_BUFA], 0
    mov qword [rbx+HX_BUFB], 0
    mov qword [rbx+HX_RDIFF], 0
    EPILOG 64

; hx_compare(hwnd, hx, sess)
hx_compare:
    PROLOG 256
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rbx+HX_PATHA, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, &rbx+HX_PATHB, 519
    invoke hx_free, rbx
    invoke read_file_bytes, &rbx+HX_PATHA, &rbx+HX_SZA
    mov [rbx+HX_BUFA], rax
    test rax, rax
    jz .err
    invoke read_file_bytes, &rbx+HX_PATHB, &rbx+HX_SZB
    mov [rbx+HX_BUFB], rax
    test rax, rax
    jz .err
    mov qword [rbx+HX_MODA], 0
    mov qword [rbx+HX_MODB], 0
    mov rax, [rbx+HX_SZA]
    cmp rax, [rbx+HX_SZB]
    jae .mx
    mov rax, [rbx+HX_SZB]
.mx:
    add rax, 15
    shr rax, 4
    mov [rbx+HX_ROWS], rax
    lea rcx, [rax+16]
    mov edx, 1
    invoke calloc, rcx, rdx
    mov [rbx+HX_RDIFF], rax
    xor r13d, r13d
    xor r14d, r14d                   ; differing bytes
.rows:
    cmp r13, [rbx+HX_ROWS]
    jae .counted
    invoke hx_diffrow, rbx, r13
    mov rax, [rbx+HX_RDIFF]
    movzx eax, byte [rax+r13]
    add r14, rax
    inc r13
    jmp .rows
.counted:
    mov [rbx+HX_NDIFF], r14
    invoke get_chk, rsi, ID_CHK_ONLY
    mov [rbx+HX_FLAGS], rax
    invoke hx_rebuild, rbx
    mov qword [rbx+HX_TOP], 0
    mov qword [rbx+HX_CURO], 0
    mov qword [rbx+HX_CURP], 0
    mov qword [rbx+HX_NIB], 0
    mov qword [rbx+HX_LOADED], 1
    invoke hx_scrollbar, rbx
    invoke hx_status, rbx
    invoke InvalidateRect, [rbx+HX_HPANE], 0, 1
    invoke wcsrchr, &rbx+HX_PATHA, 0x5C
    lea r13, [rbx+HX_PATHA]
    test rax, rax
    jz .ta
    lea r13, [rax+2]
.ta:
    invoke wcsrchr, &rbx+HX_PATHB, 0x5C
    lea r14, [rbx+HX_PATHB]
    test rax, rax
    jz .tb
    lea r14, [rax+2]
.tb:
    invoke _snwprintf, &rsp+96, 60, &f_title, r13, r14
    invoke set_tab_title, r12, &rsp+96
    test qword [g_batch], 4           ; unattended: the first compare ends the run
    jz .okb
    test qword [g_batch], 0x10        ; /xlsx file: export it first
    jz .okx
    and qword [g_batch], ~0x10
    invoke hx_xlsx_path, rbx, &g_xl_path
    test eax, eax
    jnz .okx
    mov qword [g_exitcode], 2
.okx:
    invoke batch_done, [rbx+HX_NDIFF] ; exit code like /report: 0 same, 1 different, 2 error
.okb:
    EPILOG 256
.err:
    invoke msgbox, rsi, &s_cantread, MB_OK|MB_ICONWARNING
    EPILOG 256

; hx_find(hwnd, hx)
hx_find:
    PROLOG 512
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+HX_LOADED], 0
    je .d
    invoke hx_parse, rsi, ID_EDIT_FIND, &rsp+320
    mov r13d, eax
    lea r12, [rsp+320]
    test r13d, r13d
    jz .d
    mov rdi, [rbx+HX_BUFA]
    mov r14, [rbx+HX_SZA]
    cmp qword [rbx+HX_CURP], 0
    je .buf
    mov rdi, [rbx+HX_BUFB]
    mov r14, [rbx+HX_SZB]
.buf:
    mov r15, [rbx+HX_CURO]
    inc r15                          ; start after the cursor
    mov rbp, r14
    sub rbp, r13
    jb .nf                           ; pattern longer than the file
    inc rbp                          ; number of start positions
    mov rsi, r15
    xor r10d, r10d                   ; wrapped flag
.try:
    cmp rsi, rbp
    jb .cmp
    test r10d, r10d
    jnz .nf
    mov r10d, 1
    xor esi, esi
.cmp:
    xor ecx, ecx
.mc:
    cmp ecx, r13d
    jae .found
    lea rax, [rsi+rcx]
    mov al, [rdi+rax]
    cmp al, [r12+rcx]
    jne .no
    inc ecx
    jmp .mc
.no:
    inc rsi
    cmp r10d, 0
    je .try
    cmp rsi, r15
    jbe .try
    jmp .nf
.found:
    invoke hx_setcursor, rbx, rsi, [rbx+HX_CURP]
    jmp .d
.nf:
    invoke set_status, &s_notfound
.d:
    EPILOG 512

; hx_save(hwnd, hx, pane)
hx_save:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov r13d, ID_EDIT_A
    mov rdi, [rbx+HX_BUFA]
    mov r14, [rbx+HX_SZA]
    lea r15, [rbx+HX_MODA]
    test r12d, r12d
    jz .go
    mov r13d, ID_EDIT_B
    mov rdi, [rbx+HX_BUFB]
    mov r14, [rbx+HX_SZB]
    lea r15, [rbx+HX_MODB]
.go:
    test rdi, rdi
    jz .d
    invoke GetDlgItem, rsi, r13
    invoke GetWindowTextW, rax, &rsp+96, 519
    cmp word [rsp+96], 0
    je .nopath
    invoke safe_write2, &rsp+96, rdi, r14, 0, 0
    test eax, eax
    jnz .saved
    invoke save_error, rsi
    jmp .d
.saved:
    mov qword [r15], 0
    invoke set_status, &s_saved
    jmp .d
.nopath:
    invoke msgbox, rsi, &s_nopath, MB_OK|MB_ICONWARNING
.d:
    EPILOG 1200

; ---------------------------------------------------------------- layout, theme, window procedure
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

hx_layout:
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
    PLACE ID_BTN_PREV, 40
    PLACE ID_BTN_NEXT, 40
    PLACE ID_CHK_ONLY, 130
    PLACE ID_BTN_SVA, 90
    PLACE ID_BTN_SVB, 100
    PLACE ID_BTN_XLS, 90
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_LBL_FIND, 270
    PLACE ID_EDIT_FIND, 200
    PLACE ID_BTN_FIND, 90
    PLACE ID_LBL_INFO, 600
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_LBL_REPL, 270
    PLACE ID_EDIT_REPL, 200
    PLACE ID_BTN_REPL, 90
    PLACE ID_BTN_REPLALL, 110
    lea r13d, [r13+r15]
    add r13d, r14d
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .h
    xor ebp, ebp
.h:
    mov ebx, [rsp+104]
    MVID rsi, ID_PANE, 0, r13, rbx, rbp
    EPILOG 192

hx_theme:
    PROLOG 64
    mov rsi, rcx
    invoke theme_checks, rsi
    invoke GetDlgItem, rsi, ID_PANE
    invoke InvalidateRect, rax, 0, 1
    EPILOG 64

hx_proc:
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
    cmp edi, BC_NEXTDIFF
    je .b_next
    cmp edi, BC_PREVDIFF
    je .b_prev
    cmp edi, BC_COLLECT
    je .collect
    cmp edi, BC_ISDIRTY
    je .isdirty
    cmp edi, BC_SAVE
    je .saveall
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
    invoke calloc, 1, HX_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    invoke pb_create, rsi
    invoke mk_btn, rsi, &s_b_prev, ID_BTN_PREV
    invoke mk_btn, rsi, &s_b_next, ID_BTN_NEXT
    invoke icon_set, rsi, ID_BTN_PREV, &g_prev, &s_b_prev
    invoke icon_set, rsi, ID_BTN_NEXT, &g_next, &s_b_next
    invoke mk_chk, rsi, &s_c_only, ID_CHK_ONLY, 0
    invoke mk_btn, rsi, &s_b_sva, ID_BTN_SVA
    invoke mk_btn, rsi, &s_b_svb, ID_BTN_SVB
    invoke mk_btn, rsi, &s_b_xls, ID_BTN_XLS
    invoke mk_lbl, rsi, &s_l_find, ID_LBL_FIND
    invoke mk_edit, rsi, ID_EDIT_FIND
    invoke mk_btn, rsi, &s_b_find, ID_BTN_FIND
    invoke mk_lbl, rsi, &s_l_repl, ID_LBL_REPL
    invoke mk_edit, rsi, ID_EDIT_REPL
    invoke mk_btn, rsi, &s_b_repl, ID_BTN_REPL
    invoke mk_btn, rsi, &s_b_replall, ID_BTN_REPLALL
    invoke mk_lbl, rsi, &s_empty, ID_LBL_INFO
    invoke CreateWindowExW, 0, &cn_pane, 0, WS_CHILD|WS_VISIBLE|WS_VSCROLL|WS_BORDER|WS_TABSTOP, 0, 0, 10, 10, rsi, ID_PANE, [g_hinst], r15
    mov [r15+HX_HPANE], rax
    invoke pb_load, rsi, r14
    ; options: "1" = only differing rows
    cmp word [r14+SS_OPTS], '1'
    jne .nopt
    invoke GetDlgItem, rsi, ID_CHK_ONLY
    invoke SendMessageW, rax, BM_SETCHECK, 1, 0
.nopt:
    invoke theme_checks, rsi
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320

.size:
    invoke hx_layout, rsi
    jmp .zero
.run:
    invoke hx_compare, rsi, r15, r14
    jmp .zero
.collect:
    invoke pb_collect, rsi, r14
    invoke get_chk, rsi, ID_CHK_ONLY
    add eax, '0'
    mov [r14+SS_OPTS], ax
    mov word [r14+SS_OPTS+2], 0
    jmp .zero
.saveall:
    cmp qword [r15+HX_MODA], 0
    je .sv2
    invoke hx_save, rsi, r15, 0
.sv2:
    cmp qword [r15+HX_MODB], 0
    je .zero
    invoke hx_save, rsi, r15, 1
    jmp .zero
.theme:
    invoke hx_theme, rsi
    jmp .zero

.command:
    movzx ebx, r12w
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_BROWSE_B
    je .browse
    cmp ebx, ID_SWAP
    je .browse
    cmp ebx, ID_BTN_PREV
    je .b_prev
    cmp ebx, ID_BTN_NEXT
    je .b_next
    cmp ebx, ID_BTN_FIND
    je .b_find
    cmp ebx, ID_BTN_REPL
    je .b_repl
    cmp ebx, ID_BTN_REPLALL
    je .b_replall
    cmp ebx, ID_BTN_SVA
    je .b_sva
    cmp ebx, ID_BTN_SVB
    je .b_svb
    cmp ebx, ID_BTN_XLS
    je .b_xls
    cmp ebx, ID_CHK_ONLY
    je .b_only
    jmp .def
.browse:
    invoke pb_command, rsi, rbx, 0
    jmp .zero
.b_prev:
    invoke hx_step, r15, 0
    jmp .zero
.b_next:
    invoke hx_step, r15, 1
    jmp .zero
.b_find:
    invoke hx_find, rsi, r15
    jmp .zero
.b_repl:
    invoke hx_replace, rsi, r15, 0
    jmp .zero
.b_replall:
    invoke hx_replace, rsi, r15, 1
    jmp .zero
.b_sva:
    invoke hx_save, rsi, r15, 0
    jmp .zero
.b_svb:
    invoke hx_save, rsi, r15, 1
    jmp .zero
.b_xls:
    invoke hx_export, rsi, r15
    jmp .zero
.b_only:
    cmp qword [r15+HX_LOADED], 0
    je .zero
    invoke get_chk, rsi, ID_CHK_ONLY
    mov [r15+HX_FLAGS], rax
    invoke hx_rebuild, r15
    invoke hx_scrollbar, r15
    invoke InvalidateRect, [r15+HX_HPANE], 0, 1
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+HX_HPANE]
    jmp .zero
.isdirty:
    mov rax, [r15+HX_MODA]
    or rax, [r15+HX_MODB]
    EPILOG 320
.destroy:
    invoke hx_free, r15
    invoke free, [r15+HX_VROWS]
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320
