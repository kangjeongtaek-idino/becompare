; pathbar.asm - control helpers shared by all views (path bar, buttons, checkboxes, colours)
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern GetDlgItem, MoveWindow, SendMessageW, SetWindowTextW, GetWindowTextW
extern SetTextColor, SetBkColor, SetBkMode, CreatePopupMenu, AppendMenuW, TrackPopupMenu, DestroyMenu, GetWindowRect
extern memset
extern mk, scl, browse_folder, browse_open, g_iconfont, g_hinst, CreateWindowExW, GetParent, GetWindowLongPtrW, PostMessageW
extern th_bg, th_fg, th_panel, th_bgbrush, th_panelbrush

global mk_icon, tip_add, glyph_swap, icon_set, tips_apply
global g_base, g_edit, g_run
global g_rot, g_flh, g_flv, g_zout, g_zin, g_fit, g_usel, g_user, g_both, g_save
global g_copyr, g_copyl, g_mover, g_movel, g_del, g_sync, g_pause, g_report, g_snap, g_expand, g_collapse, g_prev, g_next
extern GetDlgItem, SetWindowTextW
global ctl_color, mk_btn, mk_chk, mk_lbl, mk_edit, get_chk
global pb_create, pb_layout, pb_load, pb_collect, pb_command

section .rdata
WSTR cn_button, "BUTTON"
WSTR cn_static, "STATIC"
WSTR cn_edit, "EDIT"
WSTR s_dots, "…"
WSTR cn_tip, "tooltips_class32"
WSTR s_tip_swap, "좌우 경로 바꾸기"
glyph_swap: dw 0xE8AB, 0
WSTR s_compare, "비교 (F5)"
WSTR s_pop_folder, "폴더 선택…"
WSTR s_pop_file, "압축 파일(ZIP) · 스냅샷(.bcss) 선택…"

section .bss
alignb 8
g_tip: resq 1
section .text

; ctl_color(hdc, kind): kind 0 = edit/list (content colours), 1 = static/button (panel colours) -> brush
ctl_color:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    invoke SetTextColor, rbx, #[th_fg]
    test rsi, rsi
    jnz .panel
    invoke SetBkColor, rbx, #[th_bg]
    mov rax, [th_bgbrush]
    EPILOG 64
.panel:
    invoke SetBkColor, rbx, #[th_panel]
    mov rax, [th_panelbrush]
    EPILOG 64

; mk_btn(parent, text, id)
mk_btn:
    PROLOG 64
    mov rsi, rdx
    mov rdi, r8
    invoke mk, rcx, &cn_button, rsi, WS_TABSTOP, rdi
    EPILOG 64

; mk_lbl(parent, text, id)
mk_lbl:
    PROLOG 64
    mov rsi, rdx
    mov rdi, r8
    invoke mk, rcx, &cn_static, rsi, 0, rdi
    EPILOG 64

; mk_icon(parent, glyph string, id): flat icon button using the Segoe MDL2 glyph font
mk_icon:
    PROLOG 64
    mov rsi, rdx
    mov rdi, r8
    invoke mk, rcx, &cn_button, rsi, WS_TABSTOP, rdi
    mov rbx, rax
    test rax, rax
    jz .d
    invoke SendMessageW, rbx, WM_SETFONT, [g_iconfont], 1
.d:
    mov rax, rbx
    EPILOG 64

; tip_add(control hwnd, text): tooltip for a control (text must stay valid)
tip_add:
    PROLOG 192
    mov rsi, rcx
    mov rdi, rdx
    cmp qword [g_tip], 0
    jne .have
    invoke CreateWindowExW, 8, &cn_tip, 0, WS_POPUP|3, #0x80000000, #0x80000000, #0x80000000, #0x80000000, 0, 0, [g_hinst], 0
    mov [g_tip], rax
    invoke SendMessageW, [g_tip], 0x0418, 0, 420        ; TTM_SETMAXTIPWIDTH: longer texts wrap
.have:
    invoke memset, &rsp+96, 0, 72
    mov dword [rsp+96], 64             ; TTTOOLINFOW_V2_SIZE: the size every version of the control accepts
    mov dword [rsp+100], 0x11          ; TTF_IDISHWND | TTF_SUBCLASS
    invoke GetParent, rsi
    mov [rsp+104], rax
    mov [rsp+112], rsi
    mov [rsp+144], rdi
    invoke SendMessageW, [g_tip], 0x0432, 0, &rsp+96     ; TTM_ADDTOOLW
    EPILOG 192

; tips_apply(parent, table): a tooltip for each control of the parent that the table names.
; table = pairs { control id, text }, ended by a zero id; controls that do not exist are skipped.
tips_apply:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
.l:
    mov rdi, [rbx]
    test rdi, rdi
    jz .d
    invoke GetDlgItem, rsi, rdi
    test rax, rax
    jz .n
    invoke tip_add, rax, [rbx+8]
.n:
    add rbx, 16
    jmp .l
.d:
    EPILOG 64

; mk_edit(parent, id)
mk_edit:
    PROLOG 64
    mov rbx, rdx
    invoke mk, rcx, &cn_edit, 0, WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP, rbx
    mov rbx, rax
    invoke SendMessageW, rbx, EM_SETLIMITTEXT, 519, 0
    mov rax, rbx
    EPILOG 64

; mk_chk(parent, text, id, checked)
mk_chk:
    PROLOG 64
    mov rbx, r9
    mov rsi, rdx
    mov rdi, r8
    invoke mk, rcx, &cn_button, rsi, BS_AUTOCHECKBOX|WS_TABSTOP, rdi
    mov rsi, rax
    invoke SendMessageW, rsi, BM_SETCHECK, rbx, 0
    mov rax, rsi
    EPILOG 64

; get_chk(parent, id) -> eax 0/1
get_chk:
    PROLOG 64
    invoke GetDlgItem, rcx, rdx
    invoke SendMessageW, rax, BM_GETCHECK, 0, 0
    and eax, 1
    EPILOG 64

; pb_create(hview): A/B edits, browse buttons, compare button
pb_create:
    PROLOG 64
    mov rsi, rcx
    invoke mk_edit, rsi, ID_EDIT_A
    invoke mk_edit, rsi, ID_EDIT_B
    invoke mk_btn, rsi, &s_dots, ID_BROWSE_A
    invoke mk_btn, rsi, &s_dots, ID_BROWSE_B
    invoke mk_icon, rsi, &glyph_swap, ID_SWAP
    invoke tip_add, rax, &s_tip_swap
    invoke mk_btn, rsi, &s_compare, ID_COMPARE
    invoke tips_apply, rsi, &tt_pb
    EPILOG 64

; pb_layout(hview, width, y) -> eax = y of the row below the path bar
pb_layout:
    PROLOG 96
    mov rsi, rcx
    mov r12d, edx                    ; width
    mov r13d, r8d                    ; y
    mov ecx, 4
    call scl
    mov r14d, eax                    ; gap
    mov ecx, 30
    call scl
    mov r15d, eax                    ; browse / swap button width
    mov ecx, 26
    call scl
    mov ebp, eax                     ; row height
    ; half = (width - 4*gap - swap) / 2
    mov eax, r14d
    shl eax, 2
    add eax, r15d
    mov edx, r12d
    sub edx, eax
    jns .pos
    xor edx, edx
.pos:
    shr edx, 1
    mov edi, edx                     ; half
    mov ebx, edi
    sub ebx, r15d                    ; edit width
    MVID rsi, ID_EDIT_A, r14, r13, rbx, rbp
    lea r12d, [r14+rbx]
    MVID rsi, ID_BROWSE_A, r12, r13, r15, rbp
    lea r12d, [r14+rdi]
    add r12d, r14d                   ; x of the swap button
    MVID rsi, ID_SWAP, r12, r13, r15, rbp
    add r12d, r15d
    add r12d, r14d                   ; x of the B cell
    MVID rsi, ID_EDIT_B, r12, r13, rbx, rbp
    add r12d, ebx
    MVID rsi, ID_BROWSE_B, r12, r13, r15, rbp
    lea eax, [r13+rbp]
    add eax, r14d
    EPILOG 96

; pb_load(hview, sess)
pb_load:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke SetWindowTextW, rax, &rdi+SS_PATHA
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke SetWindowTextW, rax, &rdi+SS_PATHB
    EPILOG 64

; pb_collect(hview, sess)
pb_collect:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rdi+SS_PATHA, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, &rdi+SS_PATHB, 519
    EPILOG 64

; pb_command(hview, id, isfolder) -> eax 1 if a browse button was handled
pb_command:
    PROLOG 2304
    mov rsi, rcx
    mov r12d, edx
    mov r13d, r8d
    cmp r12d, ID_SWAP
    je .swap
    mov ebx, ID_EDIT_A
    cmp r12d, ID_BROWSE_A
    je .go
    mov ebx, ID_EDIT_B
    cmp r12d, ID_BROWSE_B
    je .go
    xor eax, eax
    EPILOG 2304
.swap:
    lea r14, [rsp+96]
    lea r15, [rsp+1144]
    invoke GetDlgItem, rsi, ID_EDIT_A
    mov rdi, rax
    invoke GetWindowTextW, rdi, r14, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    mov rbx, rax
    invoke GetWindowTextW, rbx, r15, 519
    invoke SetWindowTextW, rdi, r15
    invoke SetWindowTextW, rbx, r14
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    test rax, rax
    jz .swd
    mov rax, [rax+SS_TYPE]
    cmp rax, T_TEXT                  ; text views may hold unsaved edits: do not reload automatically
    je .swd
    cmp rax, T_MERGE
    je .swd
    invoke PostMessageW, rsi, WM_COMMAND, ID_COMPARE, 0
.swd:
    mov eax, 1
    EPILOG 2304
.go:
    invoke GetDlgItem, rsi, rbx
    mov rdi, rax
    lea r14, [rsp+96]
    invoke GetWindowTextW, rdi, r14, 519
    test r13d, r13d
    jz .file
    call CreatePopupMenu
    mov r15, rax
    invoke AppendMenuW, r15, MF_STRING, 1, &s_pop_folder
    invoke AppendMenuW, r15, MF_STRING, 2, &s_pop_file
    invoke GetDlgItem, rsi, r12
    invoke GetWindowRect, rax, &rsp+2200
    mov ebx, [rsp+2200]
    mov r13d, [rsp+2212]
    invoke TrackPopupMenu, r15, TPM_RETURNCMD|TPM_RIGHTBUTTON, rbx, r13, 0, rsi, 0
    mov ebx, eax
    invoke DestroyMenu, r15
    cmp ebx, 1
    je .fld
    cmp ebx, 2
    jne .done
    invoke browse_open, rsi, r14
    jmp .res
.fld:
    invoke browse_folder, rsi, r14
    jmp .res
.file:
    invoke browse_open, rsi, r14
.res:
    test eax, eax
    jz .done
    invoke SetWindowTextW, rdi, r14
.done:
    mov eax, 1
    EPILOG 2304

; ---------------------------------------------------------------- checkbox theming (dark mode)
extern EnumChildWindows, GetClassNameW, _wcsicmp, GetWindowLongPtrW, SetWindowTheme, InvalidateRect
extern g_dark
global theme_checks

section .rdata
WSTR s_emptyw, ""
section .text

; tc_cb(hwnd, lparam) -> TRUE : unthemed check boxes honour WM_CTLCOLORSTATIC text colours
tc_cb:
    PROLOG 192
    mov rbx, rcx
    invoke GetClassNameW, rbx, &rsp+96, 32
    invoke _wcsicmp, &rsp+96, &cn_button
    test eax, eax
    jnz .no
    invoke GetWindowLongPtrW, rbx, GWL_STYLE
    and eax, 0xF
    cmp eax, 3
    jne .no
    cmp qword [g_dark], 0
    je .light
    invoke SetWindowTheme, rbx, &s_emptyw, &s_emptyw
    jmp .inv
.light:
    invoke SetWindowTheme, rbx, 0, 0
.inv:
    invoke InvalidateRect, rbx, 0, 1
.no:
    mov eax, 1
    EPILOG 192

; theme_checks(hview)
theme_checks:
    PROLOG 64
    invoke EnumChildWindows, rcx, &tc_cb, 0
    EPILOG 64

; ---------------------------------------------------------------- dark list-view header (custom draw through a subclass)
extern SetWindowSubclass, DefSubclassProc, FillRect, DrawTextW, GetClientRect, CreatePen, SelectObject, MoveToEx, LineTo
extern th_fg, th_panelbrush, th_panel
global lv_dark_header

; lvh_proc(hwnd, msg, wparam, lparam, uId, ref): handles the header's NM_CUSTOMDRAW while the dark theme is on
lvh_proc:
    PROLOG 560
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_NOTIFY
    jne .def
    cmp qword [g_dark], 0
    je .def
    cmp dword [r13+NMH_CODE], NM_CUSTOMDRAW
    jne .def
    invoke SendMessageW, rsi, 0x101F, 0, 0          ; LVM_GETHEADER
    cmp rax, [r13+NMH_FROM]
    jne .def
    mov eax, [r13+CD_STAGE]
    cmp eax, CDDS_PREPAINT
    jne .item
    lea r14, [r13+40]                               ; rc
    invoke FillRect, [r13+CD_HDC], r14, [th_panelbrush]
    mov eax, 0x30                                   ; NOTIFYITEMDRAW | NOTIFYPOSTPAINT
    EPILOG 560
.item:
    cmp eax, 2                                      ; CDDS_POSTPAINT: the empty area right of the last column
    je .post
    cmp eax, CDDS_ITEMPREPAINT
    jne .def
    mov rbx, [r13+CD_HDC]
    lea r14, [r13+40]
    invoke FillRect, rbx, r14, [th_panelbrush]
    ; header item text and format
    invoke memset, &rsp+96, 0, 72
    mov dword [rsp+96], 6                           ; HDI_TEXT | HDI_FORMAT
    lea rax, [rsp+176]
    mov [rsp+96+8], rax
    mov dword [rsp+96+24], 120
    mov r15, [r13+CD_ITEMSPEC]
    invoke SendMessageW, [r13+NMH_FROM], 0x120B, r15, &rsp+96   ; HDM_GETITEMW
    invoke SetBkMode, rbx, 1
    invoke SetTextColor, rbx, #[th_fg]
    ; text rectangle (padded)
    mov eax, [r13+40]
    add eax, 8
    mov [rsp+432], eax
    mov eax, [r13+44]
    mov [rsp+436], eax
    mov eax, [r13+48]
    sub eax, 8
    mov [rsp+440], eax
    mov eax, [r13+52]
    mov [rsp+444], eax
    mov r8d, 0x0824                                 ; DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS ... left
    mov eax, [rsp+96+28]
    test eax, 1                                     ; HDF_RIGHT
    jz .draw
    or r8d, 2                                       ; DT_RIGHT
.draw:
    mov r14d, r8d
    invoke DrawTextW, rbx, &rsp+176, -1, &rsp+432, r14
    mov eax, 4                                      ; CDRF_SKIPDEFAULT
    EPILOG 560
.post:
    mov r14, [r13+NMH_FROM]
    invoke SendMessageW, r14, 0x1200, 0, 0          ; HDM_GETITEMCOUNT
    test eax, eax
    jz .def
    dec eax
    invoke SendMessageW, r14, 0x1207, rax, &rsp+432 ; HDM_GETITEMRECT
    mov eax, [rsp+440]
    mov [rsp+448], eax                              ; left = right edge of the last item
    invoke GetClientRect, r14, &rsp+464
    mov eax, [rsp+464+8]
    mov [rsp+456], eax                              ; right
    mov eax, [rsp+464]
    mov [rsp+448+0], eax
    mov eax, [rsp+440]
    mov [rsp+448], eax
    mov eax, [rsp+464+4]
    mov [rsp+452], eax
    mov eax, [rsp+464+12]
    mov [rsp+460], eax
    invoke FillRect, [r13+CD_HDC], &rsp+448, [th_panelbrush]
    xor eax, eax
    EPILOG 560
.def:
    invoke DefSubclassProc, rsi, rdi, r12, r13
    EPILOG 560

; lv_dark_header(hlist)
lv_dark_header:
    PROLOG 64
    invoke SetWindowSubclass, rcx, &lvh_proc, 1, 0
    EPILOG 64

section .rdata
align 2
g_copyr:    dw 0xE8C8, 0xE72A, 0
g_copyl:    dw 0xE72B, 0xE8C8, 0
g_mover:    dw 0xE8DE, 0xE72A, 0
g_movel:    dw 0xE72B, 0xE8DE, 0
g_del:      dw 0xE74D, 0
g_sync:     dw 0xE895, 0
g_pause:    dw 0xE769, 0
g_report:   dw 0xE9F9, 0
g_snap:     dw 0xE722, 0
g_expand:   dw 0xE70D, 0
g_collapse: dw 0xE70E, 0
g_prev:     dw 0xE70E, 0
g_rot:      dw 0xE7AD, 0
g_flh:      dw 0xE8AB, 0
g_flv:      dw 0xE8CB, 0
g_zout:     dw 0xE71F, 0
g_zin:      dw 0xE8A3, 0
g_fit:      dw 0xE740, 0
g_usel:     dw 0xE72B, 0xE73E, 0
g_user:     dw 0xE73E, 0xE72A, 0
g_both:     dw 0xE8FD, 0
g_save:     dw 0xE74E, 0
g_base:     dw 0xE81C, 0
g_edit:     dw 0xE70F, 0
g_run:      dw 0xE768, 0
g_next:     dw 0xE70D, 0
section .text

; icon_set(parent, id, glyph, tooltip): turn an existing button into a glyph button with a tooltip
icon_set:
    PROLOG 64
    mov rbx, r8
    mov rdi, r9
    invoke GetDlgItem, rcx, rdx
    mov rsi, rax
    test rax, rax
    jz .d
    invoke SetWindowTextW, rsi, rbx
    invoke SendMessageW, rsi, WM_SETFONT, [g_iconfont], 1
    invoke tip_add, rsi, rdi
.d:
    EPILOG 64

; ---------------------------------------------------------------- tooltips of this view
section .rdata
WSTR tt_pb_0, "왼쪽 경로"
WSTR tt_pb_1, "오른쪽 경로"
WSTR tt_pb_2, "왼쪽 경로 찾아보기"
WSTR tt_pb_3, "오른쪽 경로 찾아보기"
WSTR tt_pb_4, "비교를 실행합니다 (F5)"
align 8
tt_pb:
    dq ID_EDIT_A, tt_pb_0
    dq ID_EDIT_B, tt_pb_1
    dq ID_BROWSE_A, tt_pb_2
    dq ID_BROWSE_B, tt_pb_3
    dq ID_COMPARE, tt_pb_4
    dq 0, 0
section .text
