; hex.asm - Hex Compare session (HX-01): owner-drawn pane, diff highlight, nav, inline edit, find, save
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"
%include "inc/export.inc"

extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem, GetParent
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetWindowTextW, SetWindowTextW
extern InvalidateRect, SetWindowTheme, GetDC, ReleaseDC, SelectObject, GetTextMetricsW
extern BeginPaint, EndPaint, TextOutW, SetBkColor, SetTextColor, SetBkMode, SetScrollInfo, GetScrollInfo
extern GetSysColor, GetKeyState, SetCapture
extern safe_write2
extern sb_free, PostMessageW, g_batch, g_xl_path, g_exitcode, g_hframe, batch_done
extern CreateFileW, WriteFile, CloseHandle
extern malloc, calloc, realloc, free, memset, memcpy, wcslen, wcsrchr, _snwprintf
extern g_hinst, g_mono, th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_same, th_panelbrush, th_bgbrush
extern save_error
extern mk, reg_class, scl, set_status, msgbox
extern mk_btn, mk_chk, mk_lbl, mk_edit, get_chk, ctl_color, theme_checks
extern pb_create, pb_layout, pb_load, pb_collect, pb_command
extern icon_set, g_prev, g_next, read_file_bytes, set_tab_title

global hex_register, hex_create

; HX state
%define HX_BUFA   0
%define HX_SZA    8
%define HX_BUFB   16
%define HX_SZB    24
%define HX_ROWS   32
%define HX_RDIFF  40
%define HX_NDIFF  48
%define HX_VROWS  56
%define HX_NV     64
%define HX_TOP    72
%define HX_CURO   80
%define HX_CURP   88
%define HX_NIB    96
%define HX_FLAGS  104
%define HX_MODA   112
%define HX_MODB   120
%define HX_HPANE  128
%define HX_CW     136
%define HX_CH     144
%define HX_PAGE   152
%define HX_PHDC   160
%define HX_PY     168
%define HX_LOADED 176
%define HX_PATHA  184
%define HX_PATHB  1224
%define HX_SIZE   2272

%define COL_HEXA  10
%define COL_ASCA  59
%define COL_HEXB  78
%define COL_ASCB  127
%define COL_END   145

%define ID_BTN_PREV 411
%define ID_BTN_NEXT 412
%define ID_CHK_ONLY 413
%define ID_BTN_FIND 414
%define ID_BTN_SVA  415
%define ID_BTN_SVB  416
%define ID_BTN_REPL 417
%define ID_BTN_REPLALL 418
%define ID_BTN_XLS  419
%define ID_LBL_FIND 420
%define ID_LBL_REPL 423
%define ID_EDIT_REPL 424
%define ID_EDIT_FIND 421
%define ID_LBL_INFO 450
%define ID_PANE     460

%define TM_HEIGHT   0
%define TM_AVECW    20

section .rdata
WSTR cn_hex, "BCHex"
WSTR cn_pane, "BCHexPane"
WSTR s_empty, ""
WSTR s_b_prev, "▲ 이전 차이"
WSTR s_b_next, "▼ 다음 차이"
WSTR s_c_only, "차이 행만 표시"
WSTR s_b_find, "다음 찾기"
WSTR s_l_find, "찾기(16진 예: 4D 5A, 또는 텍스트):"
WSTR s_l_repl, "바꿀 값(찾는 값과 같은 바이트 수):"
WSTR s_b_repl, "바꾸기"
WSTR s_b_replall, "모두 바꾸기"
WSTR s_lenmis, "찾을 값과 바꿀 값의 바이트 수가 같아야 합니다 (헥스 편집은 덮어쓰기 방식입니다)."
WSTR f_replaced, "%u곳을 바꿨습니다."
WSTR s_norepl, "바꿀 대상을 찾지 못했습니다."
WSTR s_b_sva, "왼쪽 저장"
WSTR s_b_svb, "오른쪽 저장"
WSTR f_off, "%08llX"                 ; 8 digits, 9 from 4GB on (the column is 10 characters wide)
WSTR f_info, "오프셋 0x%llX · 왼쪽 %s · 오른쪽 %s · 크기 %llu / %llu · 다른 바이트 %llu"
WSTR f_byte, "%02X"
WSTR s_none, "--"
WSTR f_title, "%s ↔ %s"
WSTR s_cantread, "파일을 읽을 수 없습니다."
WSTR s_nodiff, "이동할 차이가 없습니다."
WSTR s_notfound, "찾을 수 없습니다."
WSTR s_saved, "저장했습니다."
WSTR s_nopath, "저장할 파일 경로가 없습니다."
WSTR f_opts, "%d"
WSTR hexdigits, "0123456789ABCDEF"

section .text

hex_register:
    PROLOG 64
    invoke reg_class, &cn_hex, &hx_proc, 0
    invoke reg_class, &cn_pane, &hp_proc, 0
    EPILOG 64

hex_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_hex, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- data helpers
; hx_diffrow(hx, row): recompute the number of differing bytes of one row
hx_diffrow:
    PROLOG 64
    mov rbx, rcx
    mov rax, rdx
    shl rax, 4                       ; first offset of the row
    mov rsi, [rbx+HX_BUFA]
    mov rdi, [rbx+HX_BUFB]
    xor r8d, r8d                     ; count
    xor r9d, r9d                     ; i
.l:
    cmp r9d, 16
    jae .d
    lea r10, [rax+r9]
    mov r11, [rbx+HX_SZA]
    mov rcx, [rbx+HX_SZB]
    cmp r10, r11
    jae .anone
    cmp r10, rcx
    jae .diff                        ; only in A
    movzx r11d, byte [rsi+r10]
    cmp r11b, [rdi+r10]
    je .n
.diff:
    inc r8d
    jmp .n
.anone:
    cmp r10, rcx
    jae .n                           ; beyond both
    inc r8d                          ; only in B
.n:
    inc r9d
    jmp .l
.d:
    mov rcx, rdx
    mov rax, [rbx+HX_RDIFF]
    mov [rax+rcx], r8b
    EPILOG 64

; hx_rebuild(hx): visible row list
hx_rebuild:
    PROLOG 64
    mov rbx, rcx
    mov rax, [rbx+HX_ROWS]
    lea rdx, [rax*4+8]
    invoke realloc, [rbx+HX_VROWS], rdx
    mov [rbx+HX_VROWS], rax
    xor ecx, ecx
    xor edx, edx
    mov r8, [rbx+HX_RDIFF]
    mov r9d, [rbx+HX_FLAGS]
.l:
    cmp rcx, [rbx+HX_ROWS]
    jae .d
    test r9d, 1
    jz .add
    cmp byte [r8+rcx], 0
    je .n
.add:
    mov [rax+rdx*4], ecx
    inc rdx
.n:
    inc rcx
    jmp .l
.d:
    mov [rbx+HX_NV], rdx
    mov rax, [rbx+HX_TOP]
    cmp rax, rdx
    jb .ok
    xor eax, eax
    test rdx, rdx
    jz .set
    lea rax, [rdx-1]
.set:
    mov [rbx+HX_TOP], rax
.ok:
    EPILOG 64

; hx_scrollbar(hx)
hx_scrollbar:
    PROLOG 96
    mov rbx, rcx
    mov dword [rsp+96-48], SI_SIZE          ; SCROLLINFO at [rsp+48]
    mov dword [rsp+52], SIF_RANGE|SIF_PAGE|SIF_POS
    mov dword [rsp+56], 0
    mov rax, [rbx+HX_NV]
    dec rax
    jns .m
    xor eax, eax
.m:
    mov [rsp+60], eax
    mov rax, [rbx+HX_PAGE]
    mov [rsp+64], eax
    mov rax, [rbx+HX_TOP]
    mov [rsp+68], eax
    invoke SetScrollInfo, [rbx+HX_HPANE], SB_VERT, &rsp+48, 1
    EPILOG 96

; hx_vindex(hx, row) -> rax visible index of a real row (or -1)
hx_vindex:
    mov rax, [rcx+HX_VROWS]
    xor r8d, r8d
    mov r9, [rcx+HX_NV]
.l:
    cmp r8, r9
    jae .no
    mov r10d, [rax+r8*4]
    cmp r10, rdx
    je .yes
    inc r8
    jmp .l
.yes:
    mov rax, r8
    ret
.no:
    mov rax, -1
    ret

; hx_status(hx): info label on the session view
hx_status:
    PROLOG 512
    mov rbx, rcx
    invoke GetParent, [rbx+HX_HPANE]
    mov rsi, rax
    mov rax, [rbx+HX_CURO]
    ; bytes under the cursor
    lea r12, [rsp+400]               ; text for A
    lea r13, [rsp+430]               ; text for B
    mov word [r12], '-'
    mov word [r12+2], '-'
    mov word [r12+4], 0
    mov word [r13], '-'
    mov word [r13+2], '-'
    mov word [r13+4], 0
    cmp rax, [rbx+HX_SZA]
    jae .b
    mov rcx, [rbx+HX_BUFA]
    movzx r14d, byte [rcx+rax]
    invoke _snwprintf, r12, 8, &f_byte, r14
    mov rax, [rbx+HX_CURO]
.b:
    cmp rax, [rbx+HX_SZB]
    jae .set
    mov rcx, [rbx+HX_BUFB]
    movzx r14d, byte [rcx+rax]
    invoke _snwprintf, r13, 8, &f_byte, r14
.set:
    invoke _snwprintf, &rsp+96, 140, &f_info, [rbx+HX_CURO], r12, r13, [rbx+HX_SZA], [rbx+HX_SZB], [rbx+HX_NDIFF]
    invoke GetDlgItem, rsi, ID_LBL_INFO
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 512

; hx_visible(hx): scroll so that the cursor row is on screen
hx_visible:
    PROLOG 64
    mov rbx, rcx
    mov rdx, [rbx+HX_CURO]
    shr rdx, 4
    invoke hx_vindex, rbx, rdx
    cmp rax, -1
    je .d
    mov rcx, [rbx+HX_TOP]
    cmp rax, rcx
    jb .above
    mov rdx, [rbx+HX_PAGE]
    add rdx, rcx
    cmp rax, rdx
    jb .d
    ; below: put the row at the bottom
    mov rdx, [rbx+HX_PAGE]
    sub rax, rdx
    inc rax
    jns .set
    xor eax, eax
    jmp .set
.above:
.set:
    mov [rbx+HX_TOP], rax
    invoke hx_scrollbar, rbx
.d:
    EPILOG 64

; hx_setcursor(hx, offset, pane)
hx_setcursor:
    PROLOG 64
    mov rbx, rcx
    mov rax, [rbx+HX_SZA]
    cmp rax, [rbx+HX_SZB]
    jae .mx
    mov rax, [rbx+HX_SZB]
.mx:
    test rax, rax
    jz .z
    dec rax
    cmp rdx, rax
    jbe .ok
    mov rdx, rax
.ok:
    mov [rbx+HX_CURO], rdx
    mov [rbx+HX_CURP], r8
    mov qword [rbx+HX_NIB], 0
    invoke hx_visible, rbx
    invoke hx_status, rbx
    invoke InvalidateRect, [rbx+HX_HPANE], 0, 0
    EPILOG 64
.z:
    mov qword [rbx+HX_CURO], 0
    EPILOG 64

; hx_step(hx, dir 1 next / 0 prev): jump to the next differing byte
hx_step:
    PROLOG 96
    mov rbx, rcx
    mov r12d, edx
    mov rsi, [rbx+HX_CURO]
    mov rax, [rbx+HX_SZA]
    cmp rax, [rbx+HX_SZB]
    jae .mx
    mov rax, [rbx+HX_SZB]
.mx:
    mov r13, rax                     ; limit (max size)
    test r12d, r12d
    jz .prev
    lea rdi, [rsi+1]
.nx:
    cmp rdi, r13
    jae .none
    mov rax, rdi
    shr rax, 4
    mov rcx, [rbx+HX_RDIFF]
    cmp byte [rcx+rax], 0
    jne .scan
    ; skip to the next row
    and rdi, ~15
    add rdi, 16
    jmp .nx
.scan:
    mov rax, rdi
    mov rcx, [rbx+HX_BUFA]
    mov rdx, [rbx+HX_BUFB]
    cmp rax, [rbx+HX_SZA]
    jae .hit
    cmp rax, [rbx+HX_SZB]
    jae .hit
    movzx r8d, byte [rcx+rax]
    cmp r8b, [rdx+rax]
    jne .hit
    inc rdi
    jmp .nx
.hit:
    mov rcx, rbx
    mov rdx, rdi
    mov r8, [rbx+HX_CURP]
    call hx_setcursor
    EPILOG 96
.prev:
    mov rdi, rsi
.pv:
    test rdi, rdi
    jz .none
    dec rdi
    cmp rdi, r13
    jae .pv
    mov rax, rdi
    shr rax, 4
    mov rcx, [rbx+HX_RDIFF]
    cmp byte [rcx+rax], 0
    jne .pscan
    and rdi, ~15
    jmp .pv
.pscan:
    mov rax, rdi
    mov rcx, [rbx+HX_BUFA]
    mov rdx, [rbx+HX_BUFB]
    cmp rax, [rbx+HX_SZA]
    jae .hit
    cmp rax, [rbx+HX_SZB]
    jae .hit
    movzx r8d, byte [rcx+rax]
    cmp r8b, [rdx+rax]
    jne .hit
    jmp .pv
.none:
    invoke set_status, &s_nodiff
    EPILOG 96

; ---------------------------------------------------------------- painting
; hx_drawbyte(hx, pane, offset, i)
hx_drawbyte:
    PROLOG 192
    mov rbx, rcx
    mov r12d, edx                    ; pane
    mov r13, r8                      ; offset
    mov r14d, r9d                    ; i
    mov rsi, [rbx+HX_BUFA]
    mov rcx, [rbx+HX_SZA]
    mov rdi, [rbx+HX_BUFB]
    mov rdx, [rbx+HX_SZB]
    test r12d, r12d
    jz .own
    xchg rsi, rdi
    xchg rcx, rdx
.own:
    ; rsi/rcx = own buffer/size, rdi/rdx = other buffer/size
    cmp r13, rcx
    jae .done
    movzx r15d, byte [rsi+r13]       ; the byte
    ; background
    mov eax, [th_bg]
    cmp r13, rdx
    jb .exists
    mov eax, [th_lonly]
    test r12d, r12d
    jz .bg
    mov eax, [th_ronly]
    jmp .bg
.exists:
    cmp r15b, [rdi+r13]
    je .bg
    mov eax, [th_diff]
.bg:
    mov ebp, eax
    mov ecx, [th_fg]
    mov r10d, ecx
    cmp r13, [rbx+HX_CURO]
    jne .colors
    cmp r12, [rbx+HX_CURP]
    jne .colors
    invoke GetSysColor, 13
    mov ebp, eax
    invoke GetSysColor, 14
    mov r10d, eax
.colors:
    mov [rsp+96], r10d
    invoke SetBkColor, [rbx+HX_PHDC], rbp
    invoke SetTextColor, [rbx+HX_PHDC], #[rsp+96]
    ; hex text
    lea rax, [hexdigits]
    mov ecx, r15d
    shr ecx, 4
    movzx edx, word [rax+rcx*2]
    mov [rsp+100], dx
    mov ecx, r15d
    and ecx, 15
    movzx edx, word [rax+rcx*2]
    mov [rsp+102], dx
    mov word [rsp+104], 0
    ; x = (colhex + i*3) * cw
    mov eax, COL_HEXA
    mov ecx, COL_ASCA
    test r12d, r12d
    jz .pos
    mov eax, COL_HEXB
    mov ecx, COL_ASCB
.pos:
    mov r8d, r14d
    lea r8d, [r8+r8*2]
    add r8d, eax
    imul r8, [rbx+HX_CW]             ; x of the hex digits
    mov [rsp+112], r8
    add ecx, r14d
    imul rcx, [rbx+HX_CW]
    mov [rsp+120], rcx               ; x of the ascii char
    invoke TextOutW, [rbx+HX_PHDC], [rsp+112], [rbx+HX_PY], &rsp+100, 2
    ; ascii
    mov eax, '.'
    cmp r15d, 0x20
    jb .asc
    cmp r15d, 0x7E
    ja .asc
    mov eax, r15d
.asc:
    mov [rsp+100], ax
    invoke TextOutW, [rbx+HX_PHDC], [rsp+120], [rbx+HX_PY], &rsp+100, 1
.done:
    EPILOG 192

%include "hex_ui.inc"
%include "hex_repl.inc"
%include "hex_xl.inc"
