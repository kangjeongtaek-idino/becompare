; pic.asm - Picture Compare session (PC-01): GDI+ decoding, pixel diff map, overlay, rotate/flip, zoom, tolerance
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"
%include "inc/export.inc"

extern sb_free, PostMessageW, wcscpy, g_batch, g_dx_path, g_exitcode, g_hframe
extern icon_set, g_rot, g_flh, g_flv, g_zout, g_zin, g_fit
extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetWindowTextW, SetWindowTextW
extern GetParent, InvalidateRect, BeginPaint, EndPaint, StretchDIBits, SetStretchBltMode, SetScrollInfo, GetScrollInfo
extern GdiplusStartup, GdipCreateBitmapFromFile, GdipGetImageWidth, GdipGetImageHeight
extern GdipBitmapLockBits, GdipBitmapUnlockBits, GdipDisposeImage
extern malloc, calloc, free, memcpy, memset, wcslen, wcsrchr, _snwprintf, wcstol
extern g_hinst, th_bg, th_fg, th_panel, th_panelbrush, th_bgbrush
extern mk, reg_class, scl, set_status, msgbox
extern mk_btn, mk_chk, mk_lbl, mk_edit, get_chk, ctl_color, theme_checks
extern pb_create, pb_layout, pb_load, pb_collect, pb_command
extern set_tab_title

global pic_register, pic_create

; IMG { pix*, dd w, dd h }
%define IM_PIX 0
%define IM_W   8
%define IM_H   12
%define IM_SIZE 16

%define PX_A      0
%define PX_B      16
%define PX_CAN    32          ; canvas (diff map or overlay), W*H*4
%define PX_W      40
%define PX_H      44
%define PX_NDIFF  48
%define PX_TOL    56
%define PX_MODE   64
%define PX_ZOOM   72
%define PX_SX     80
%define PX_SY     88
%define PX_HPANE  96
%define PX_LOADED 104
%define PX_PATHA  112
%define PX_PATHB  1152
%define PX_SIZE   2208

%define ID_BTN_ROT   521
%define ID_BTN_FLH   522
%define ID_BTN_FLV   523
%define ID_BTN_ZOUT  524
%define ID_BTN_ZIN   525
%define ID_BTN_FIT   526
%define ID_COMBO     527
%define ID_LBL_TOL   530
%define ID_EDIT_TOL  531
%define ID_LBL_INFO  550
%define ID_LBL_CUR   551
%define ID_PPANE     560
%define ID_BTN_DOC   561

%define CB_ADDSTRING 0x0143
%define CB_SETCURSEL 0x014E
%define CB_GETCURSEL 0x0147
%define CBS_DROPDOWNLIST 3
%define CBN_SELCHANGE 1
%define WM_MOUSEMOVE 0x0200

section .rdata
WSTR cn_pic, "BCPic"
WSTR cn_pane, "BCPicPane"
WSTR cn_combo, "COMBOBOX"
WSTR s_empty, ""
WSTR s_b_rot, "↻ 90° 회전"
WSTR s_b_flh, "↔ 좌우 뒤집기"
WSTR s_b_flv, "↕ 상하 뒤집기"
WSTR s_b_zout, "－ 축소"
WSTR s_b_zin, "＋ 확대"
WSTR s_b_fit, "맞춤"
WSTR s_m0, "나란히 (A | B | 차이)"
WSTR s_m1, "차이 지도"
WSTR s_m2, "겹치기 (50%)"
WSTR s_l_tol, "RGB 허용 오차(0~255):"
WSTR s_tol0, "0"
WSTR f_info, "A %dx%d · B %dx%d · 다른 픽셀 %llu (%llu.%02llu%%) · 확대 %d%%"
WSTR f_cur, "(%d, %d)  A=#%06X  B=#%06X"
WSTR f_cur_a, "(%d, %d)  A=#%06X  B=—"
WSTR f_cur_b, "(%d, %d)  A=—  B=#%06X"
WSTR f_title, "%s ↔ %s"
WSTR f_opts, "%d|%s"
WSTR s_cantread, "이미지를 읽을 수 없습니다 (지원 형식: PNG, JPG, BMP, GIF, TIF)."

align 4
zoom_steps: dd 10, 25, 50, 75, 100, 150, 200, 300, 400, 800, 1600, 0

section .bss
alignb 8
g_gdip_token: resq 1
g_gdip_in:    resq 3

section .text

pic_register:
    PROLOG 64
    invoke reg_class, &cn_pic, &pv_proc, 0
    invoke reg_class, &cn_pane, &pp_proc, 0
    ; start GDI+ once
    invoke memset, &g_gdip_in, 0, 24
    mov dword [g_gdip_in], 1
    invoke GdiplusStartup, &g_gdip_token, &g_gdip_in, 0
    EPILOG 64

pic_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_pic, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- image data
; img_free(IMG*)
img_free:
    PROLOG 64
    mov rbx, rcx
    invoke free, [rbx+IM_PIX]
    mov qword [rbx+IM_PIX], 0
    mov dword [rbx+IM_W], 0
    mov dword [rbx+IM_H], 0
    EPILOG 64

; img_load(path, IMG*) -> eax 1 ok: decode with GDI+ into a top-down BGRA buffer
%define L_BMP  128
%define L_RECT 144               ; GpRect {x,y,w,h}
%define L_BD   160               ; BitmapData (32 bytes)
img_load:
    PROLOG 224
    mov rsi, rcx
    mov rdi, rdx
    invoke GdipCreateBitmapFromFile, rsi, &rsp+L_BMP
    test eax, eax
    jnz .fail
    mov rbx, [rsp+L_BMP]
    invoke GdipGetImageWidth, rbx, &rsp+L_RECT+8
    invoke GdipGetImageHeight, rbx, &rsp+L_RECT+12
    mov dword [rsp+L_RECT], 0
    mov dword [rsp+L_RECT+4], 0
    mov r12d, [rsp+L_RECT+8]         ; w
    mov r13d, [rsp+L_RECT+12]        ; h
    test r12d, r12d
    jz .disp
    test r13d, r13d
    jz .disp
    invoke GdipBitmapLockBits, rbx, &rsp+L_RECT, 1, 0x0026200A, &rsp+L_BD
    test eax, eax
    jnz .disp
    mov eax, r12d
    imul rax, r13
    lea rcx, [rax*4+16]
    call malloc
    mov r14, rax
    test rax, rax
    jz .unlock
    mov r15, [rsp+L_BD+16]           ; Scan0
    movsxd rbp, dword [rsp+L_BD+8]   ; stride
    xor esi, esi                     ; row
.rows:
    cmp esi, r13d
    jae .copied
    mov eax, esi
    imul rax, r12
    lea rcx, [r14+rax*4]
    mov rax, rsi
    imul rax, rbp
    lea rdx, [r15+rax]
    mov r8d, r12d
    shl r8, 2
    invoke memcpy, rcx, rdx, r8
    inc esi
    jmp .rows
.copied:
    invoke free, [rdi+IM_PIX]
    mov [rdi+IM_PIX], r14
    mov [rdi+IM_W], r12d
    mov [rdi+IM_H], r13d
    invoke GdipBitmapUnlockBits, rbx, &rsp+L_BD
    invoke GdipDisposeImage, rbx
    mov eax, 1
    EPILOG 224
.unlock:
    invoke GdipBitmapUnlockBits, rbx, &rsp+L_BD
.disp:
    invoke GdipDisposeImage, rbx
.fail:
    xor eax, eax
    EPILOG 224

; img_xform(IMG*, op): 0 rotate 90 clockwise, 1 flip horizontally, 2 flip vertically
img_xform:
    PROLOG 96
    mov rbx, rcx
    mov r12d, edx
    mov r13d, [rbx+IM_W]
    mov r14d, [rbx+IM_H]
    test r13d, r13d
    jz .d
    mov eax, r13d
    imul rax, r14
    lea rcx, [rax*4+16]
    call malloc
    mov r15, rax
    test rax, rax
    jz .d
    mov rsi, [rbx+IM_PIX]
    xor edi, edi                     ; dst row y'
    cmp r12d, 0
    jne .flip
    ; rotate: dst is h wide, w high: dst(x',y') = src(x=y', y=h-1-x')
.ry:
    cmp edi, r13d
    jae .rdone
    xor ecx, ecx                     ; x'
.rx:
    cmp ecx, r14d
    jae .rny
    mov eax, r14d
    dec eax
    sub eax, ecx                     ; h-1-x'
    imul rax, r13                    ; * w
    add rax, rdi                     ; + y'
    mov edx, [rsi+rax*4]
    mov eax, edi
    imul rax, r14                    ; y' * w' (w' = h)
    add rax, rcx
    mov [r15+rax*4], edx
    inc ecx
    jmp .rx
.rny:
    inc edi
    jmp .ry
.rdone:
    mov [rbx+IM_W], r14d
    mov [rbx+IM_H], r13d
    jmp .swap
.flip:
.fy:
    cmp edi, r14d
    jae .swap
    xor ecx, ecx
.fx:
    cmp ecx, r13d
    jae .fny
    mov r8d, ecx                     ; source x
    mov r9d, edi                     ; source y
    cmp r12d, 1
    jne .vflip
    mov r8d, r13d
    dec r8d
    sub r8d, ecx
    jmp .have
.vflip:
    mov r9d, r14d
    dec r9d
    sub r9d, edi
.have:
    mov eax, r9d
    imul rax, r13
    add rax, r8
    mov edx, [rsi+rax*4]
    mov eax, edi
    imul rax, r13
    add rax, rcx
    mov [r15+rax*4], edx
    inc ecx
    jmp .fx
.fny:
    inc edi
    jmp .fy
.swap:
    invoke free, [rbx+IM_PIX]
    mov [rbx+IM_PIX], r15
.d:
    EPILOG 96

; ---------------------------------------------------------------- canvas (diff map / overlay)
; px_maxdelta(ecx = pixel a, edx = pixel b) -> eax = largest |channel difference| over B, G, R
px_maxdelta:
    xor eax, eax
    mov r8d, 3
.l:
    movzx r9d, cl
    movzx r10d, dl
    sub r9d, r10d
    jns .abs
    neg r9d
.abs:
    cmp r9d, eax
    jbe .nm
    mov eax, r9d
.nm:
    shr ecx, 8
    shr edx, 8
    dec r8d
    jnz .l
    ret

; px_dim(ecx = pixel) -> eax = washed-out copy (c*3+1785)/10 per channel, opaque
px_dim:
    xor eax, eax
    mov r8d, ecx
    xor r9d, r9d
.l:
    movzx edx, r8b
    imul edx, edx, 3
    add edx, 1785
    imul edx, edx, 3277
    shr edx, 15
    mov ecx, r9d
    shl edx, cl
    or eax, edx
    shr r8d, 8
    add r9d, 8
    cmp r9d, 24
    jb .l
    or eax, 0xFF000000
    ret

; px_avg(ecx = a, edx = b) -> eax = per channel average
px_avg:
    and ecx, 0xFEFEFEFE
    and edx, 0xFEFEFEFE
    shr ecx, 1
    shr edx, 1
    lea eax, [rcx+rdx]
    or eax, 0xFF000000
    ret

; px_build(px): canvas of size max(W) x max(H); mode 2 = overlay, otherwise diff map. Counts differing pixels.
%define B_X    96
%define B_NDIF 104
%define B_HASA 112
%define B_HASB 116
%define B_PA   120
%define B_PB   124
%define B_IDX  128
%define B_COL  136
px_build:
    PROLOG 192
    mov rbx, rcx
    invoke free, [rbx+PX_CAN]
    mov qword [rbx+PX_CAN], 0
    mov qword [rbx+PX_NDIFF], 0
    mov eax, [rbx+PX_A+IM_W]
    cmp eax, [rbx+PX_B+IM_W]
    jae .w
    mov eax, [rbx+PX_B+IM_W]
.w:
    mov r12d, eax                    ; W
    mov eax, [rbx+PX_A+IM_H]
    cmp eax, [rbx+PX_B+IM_H]
    jae .h
    mov eax, [rbx+PX_B+IM_H]
.h:
    mov r13d, eax                    ; H
    mov [rbx+PX_W], r12d
    mov [rbx+PX_H], r13d
    test r12d, r12d
    jz .d
    test r13d, r13d
    jz .d
    mov eax, r12d
    imul rax, r13
    mov rcx, rax
    mov edx, 4
    invoke calloc, rcx, rdx
    mov [rbx+PX_CAN], rax
    test rax, rax
    jz .d
    mov r14, rax                     ; canvas
    mov rsi, [rbx+PX_A+IM_PIX]
    mov rdi, [rbx+PX_B+IM_PIX]
    mov r15d, [rbx+PX_TOL]
    mov qword [rsp+B_NDIF], 0
    xor ebp, ebp                     ; y
.y:
    cmp ebp, r13d
    jae .done
    mov dword [rsp+B_X], 0
.x:
    mov r9d, [rsp+B_X]
    cmp r9d, r12d
    jae .ny
    ; membership and pixels
    xor eax, eax
    cmp r9d, [rbx+PX_A+IM_W]
    jae .noa
    cmp ebp, [rbx+PX_A+IM_H]
    jae .noa
    mov eax, 1
.noa:
    mov [rsp+B_HASA], eax
    xor ecx, ecx
    test eax, eax
    jz .pa
    mov eax, ebp
    imul eax, [rbx+PX_A+IM_W]
    add eax, r9d
    mov ecx, [rsi+rax*4]
.pa:
    mov [rsp+B_PA], ecx
    xor eax, eax
    cmp r9d, [rbx+PX_B+IM_W]
    jae .nob
    cmp ebp, [rbx+PX_B+IM_H]
    jae .nob
    mov eax, 1
.nob:
    mov [rsp+B_HASB], eax
    xor ecx, ecx
    test eax, eax
    jz .pb
    mov eax, ebp
    imul eax, [rbx+PX_B+IM_W]
    add eax, r9d
    mov ecx, [rdi+rax*4]
.pb:
    mov [rsp+B_PB], ecx
    mov eax, ebp
    imul rax, r12
    add rax, r9
    mov [rsp+B_IDX], rax
    ; ---- cases
    cmp dword [rsp+B_HASA], 0
    jne .hasa
    cmp dword [rsp+B_HASB], 0
    jne .onlyb
    mov dword [rsp+B_COL], 0xFF808080
    jmp .store
.onlyb:
    inc qword [rsp+B_NDIF]
    mov eax, 0xFFFF00FF
    cmp qword [rbx+PX_MODE], 2
    jne .set
    mov eax, [rsp+B_PB]
    jmp .set
.hasa:
    cmp dword [rsp+B_HASB], 0
    jne .both
    inc qword [rsp+B_NDIF]
    mov eax, 0xFFFF00FF
    cmp qword [rbx+PX_MODE], 2
    jne .set
    mov eax, [rsp+B_PA]
    jmp .set
.both:
    mov ecx, [rsp+B_PA]
    mov edx, [rsp+B_PB]
    call px_maxdelta
    xor r8d, r8d
    cmp eax, r15d
    jbe .same
    mov r8d, 1
    inc qword [rsp+B_NDIF]
.same:
    cmp qword [rbx+PX_MODE], 2
    je .avg
    test r8d, r8d
    jz .dim
    mov eax, 0xFFFF0000              ; red (A R G B)
    jmp .set
.dim:
    mov ecx, [rsp+B_PA]
    call px_dim
    jmp .set
.avg:
    mov ecx, [rsp+B_PA]
    mov edx, [rsp+B_PB]
    call px_avg
.set:
    mov [rsp+B_COL], eax
.store:
    mov rax, [rsp+B_IDX]
    mov ecx, [rsp+B_COL]
    mov [r14+rax*4], ecx
    inc dword [rsp+B_X]
    jmp .x
.ny:
    inc ebp
    jmp .y
.done:
    mov rax, [rsp+B_NDIF]
    mov [rbx+PX_NDIFF], rax
.d:
    EPILOG 192

%include "pic_ui.inc"
%include "pic_doc.inc"
