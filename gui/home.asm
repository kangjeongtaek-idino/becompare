; home.asm - Home tab (modern card-based session launcher + recent sessions) and settings/recent list
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern CreateWindowExW, DefWindowProcW, SendMessageW, PostMessageW, MoveWindow, GetClientRect
extern GetDlgItem, FillRect, SetTextColor, SetBkColor, RedrawWindow, SetWindowLongPtrW, GetWindowLongPtrW
extern GetEnvironmentVariableW, CreateDirectoryW, GetPrivateProfileStringW, GetPrivateProfileIntW
extern WritePrivateProfileStringW, SetBkMode, SetFocus, GetDlgCtrlID
extern GetSysColorBrush, GetSysColor, SelectObject, DrawTextW, GetFileAttributesExW, GetSystemTimeAsFileTime, _wcsicmp, wcsrchr
extern CreateSolidBrush, CreatePen, RoundRect, DeleteObject
extern type_names, tab_titles, th_same
extern wcscat, wcscmp, _snwprintf
extern g_hinst, g_hframe, g_font, g_fontb, th_bg, th_fg, th_panel, th_panelbrush, th_bgbrush, g_dark
extern g_font_title, g_font_sub, g_font_desc, g_font_badge, g_card_iconfont
extern th_card_bg, th_card_border, th_card_hover_bg, th_card_hover_border, th_accent, th_hl
extern mk, reg_class, scl
extern ini_prepare, open_any_file, g_sess, g_nsess

global home_register, home_create, ini_path_init, settings_load, recent_add, g_ini

%define ID_TITLE   3000
%define ID_RECENT  3001
%define ID_RLABEL  3002
%define ID_RDEL    3003
%define ID_HINT    3004
%define BS_OWNERDRAW 0x000B
%define MAX_RECENT 10

section .rdata
cn_home:    dw __utf16__("BCHome"), 0
cn_static:  dw __utf16__("STATIC"), 0
cn_button:  dw __utf16__("BUTTON"), 0
cn_listbox: dw __utf16__("LISTBOX"), 0
s_empty:    dw 0

; App Header strings
WSTR s_app_title, "BeCompare"
WSTR s_hint_text, "비교할 세션을 선택하거나 파일·폴더를 창으로 끌어다 놓으세요."
WSTR s_rlabel,    "최근 세션"
WSTR s_del_btn,   "기록 삭제"
WSTR s_chevron,   "›"
WSTR s_pro,       "PRO"

WSTR s_localapp, "LOCALAPPDATA"
WSTR s_dirname,  "\BeCompareAsm"
WSTR s_inifile,  "\settings.ini"
WSTR sec_ui,     "UI"
WSTR k_dark,     "Dark"
WSTR sec_recent, "Recent"
WSTR f_r,        "R%d"

; 11 Tool Titles
WSTR t_folder, "폴더 비교"
WSTR t_text,   "텍스트 비교"
WSTR t_table,  "표 비교"
WSTR t_hex,    "헥스 비교"
WSTR t_pic,    "이미지 비교"
WSTR t_merge,  "텍스트 병합"
WSTR t_ver,    "버전 비교"
WSTR t_reg,    "레지스트리 비교"
WSTR t_media,  "미디어 비교"
WSTR t_edit,   "텍스트 편집"
WSTR t_fmerge, "폴더 병합"

; 11 Tool Descriptions
WSTR d_folder, "두 폴더 계층 나란히 비교 · 동기화"
WSTR d_text,   "문서 라인별 세부 차이 비교 및 편집"
WSTR d_table,  "CSV / TSV / Excel 행·열 구조 비교"
WSTR d_hex,    "바이너리 바이트 단위 정밀 비교"
WSTR d_pic,    "이미지 픽셀 차이·회전·확대 비교"
WSTR d_merge,  "2-way / 3-way 텍스트 병합 충돌 해결"
WSTR d_ver,    "EXE · DLL 버전 리소스 및 헤더 비교"
WSTR d_reg,    "Windows 레지스트리 키·값 재귀 비교"
WSTR d_media,  "MP3 · FLAC 오디오 태그 및 CRC 비교"
WSTR d_edit,   "단일 파일 고속 텍스트 편집기"
WSTR d_fmerge, "3개 폴더 계층 병합 및 변경점 통합"

; 11 MDL2 Glyphs
g_folder:  dw 0xED25, 0    ; FolderOpen
g_text:    dw 0xE8A5, 0    ; Document
g_table:   dw 0xF0E3, 0    ; Table
g_hex:     dw 0xE943, 0    ; Code
g_pic:     dw 0xEB9F, 0    ; Photo
g_merge:   dw 0xE8AB, 0    ; Branch
g_ver:     dw 0xE946, 0    ; Tag
g_reg:     dw 0xE8D7, 0    ; Tools
g_media:   dw 0xEC4F, 0    ; Audio
g_edit:    dw 0xE70F, 0    ; Edit
g_fmerge:  dw 0xE8B7, 0    ; Folder

align 8
card_glyphs: dq g_folder, g_text, g_table, g_hex, g_pic, g_merge, g_ver, g_reg, g_media, g_edit, g_fmerge
card_titles: dq t_folder, t_text, t_table, t_hex, t_pic, t_merge, t_ver, t_reg, t_media, t_edit, t_fmerge
card_descs:  dq d_folder, d_text, d_table, d_hex, d_pic, d_merge, d_ver, d_reg, d_media, d_edit, d_fmerge

align 4
pro_flags: dd 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 1

; Logical visual layout order for cards:
; Row 0: Folder Compare, Text Compare, Table Compare
; Row 1: Folder Merge (Pro), Text Merge (Pro), Text Editor
; Row 2: Image Compare, Media Compare, Hex Compare
; Row 3: Version Compare (50%), Registry Compare (Pro, 50%)
align 4
tool_order:
    dd 1001, 1002, 1003
    dd 1011, 1006, 1010
    dd 1005, 1009, 1004
    dd 1007, 1008

; BGR palette for card icon badges
align 4
badge_fg_dark:
    dd 0x00B4FF, 0xFF7040, 0x60D020, 0xF860A0, 0x7050FF, 0xFF9030, 0xD0C000, 0x2090FF, 0xA030F0, 0x20D0F0, 0x50E030
badge_bg_dark:
    dd 0x24323C, 0x362420, 0x1E3424, 0x342032, 0x282038, 0x362A20, 0x2C301C, 0x222C38, 0x281C32, 0x223234, 0x1E3424
badge_fg_light:
    dd 0xEB6325, 0x699605, 0x88940D, 0xF16663, 0x481DE1, 0x0677D9, 0x695547, 0x2626DC, 0xEA3393, 0xC78402, 0xE5464F
badge_bg_light:
    dd 0xFFF6EF, 0xF5FDEC, 0xFAFD70, 0xFFF2EE, 0xF2F1FF, 0xC7F3FE, 0xF9F5F1, 0xF2F2FE, 0xFFF5FA, 0xFFF9F0, 0xFFF2EE

section .bss
alignb 8
g_ini: resw 520

section .text

home_register:
    PROLOG 64
    invoke reg_class, &cn_home, &home_proc, 0
    EPILOG 64

; home_create(parent, sess) -> hwnd
home_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_home, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- settings
ini_path_init:
    PROLOG 64
    invoke GetEnvironmentVariableW, &s_localapp, &g_ini, 450
    invoke wcscat, &g_ini, &s_dirname
    invoke CreateDirectoryW, &g_ini, 0
    invoke wcscat, &g_ini, &s_inifile
    invoke ini_prepare, &g_ini
    EPILOG 64

settings_load:
    PROLOG 64
    invoke GetPrivateProfileIntW, &sec_ui, &k_dark, 0, &g_ini
    mov [g_dark], rax
    EPILOG 64

; recent_add(rcx=path)
recent_add:
    PROLOG 1200
    mov r12, rcx
    lea rsi, [rsp+96]                ; key buffer (16 wchar)
    lea rdi, [rsp+128]               ; value buffer (520 wchar)
    invoke _snwprintf, rsi, 16, &f_r, 0
    invoke GetPrivateProfileStringW, &sec_recent, rsi, &s_empty, rdi, 520, &g_ini
    invoke wcscmp, rdi, r12
    test eax, eax
    jz .done
    ; an older entry for the same path is overwritten by the shift, so it moves to the top
    mov ebx, 1
.findk:
    cmp ebx, MAX_RECENT-1
    jae .gotk
    invoke _snwprintf, rsi, 16, &f_r, rbx
    invoke GetPrivateProfileStringW, &sec_recent, rsi, &s_empty, rdi, 520, &g_ini
    invoke wcscmp, rdi, r12
    test eax, eax
    jz .gotk
    inc ebx
    jmp .findk
.gotk:
    dec ebx
.shift:
    invoke _snwprintf, rsi, 16, &f_r, rbx
    invoke GetPrivateProfileStringW, &sec_recent, rsi, &s_empty, rdi, 520, &g_ini
    lea rax, [rbx+1]
    invoke _snwprintf, rsi, 16, &f_r, rax
    invoke WritePrivateProfileStringW, &sec_recent, rsi, rdi, &g_ini
    dec ebx
    jns .shift
    invoke _snwprintf, rsi, 16, &f_r, 0
    invoke WritePrivateProfileStringW, &sec_recent, rsi, r12, &g_ini
    ; refresh the Home list
    lea rax, [g_sess]
    mov rax, [rax]
    test rax, rax
    jz .done
    invoke PostMessageW, [rax+SS_HVIEW], WM_APP+30, 0, 0
.done:
    EPILOG 1200

; fill_recent(hwnd)
fill_recent:
    PROLOG 1200
    mov r12, rcx
    invoke GetDlgItem, r12, ID_RECENT
    test rax, rax
    jz .done
    mov r13, rax
    invoke SendMessageW, r13, LB_RESETCONTENT, 0, 0
    lea rsi, [rsp+96]
    lea rdi, [rsp+128]
    xor ebx, ebx
.l:
    cmp ebx, MAX_RECENT
    jae .done
    invoke _snwprintf, rsi, 16, &f_r, rbx
    invoke GetPrivateProfileStringW, &sec_recent, rsi, &s_empty, rdi, 520, &g_ini
    cmp word [rdi], 0
    je .n
    invoke SendMessageW, r13, LB_ADDSTRING, 0, rdi
.n:
    inc ebx
    jmp .l
.done:
    EPILOG 1200

; ---------------------------------------------------------------- layout
home_layout:
    PROLOG 256
    mov rsi, rcx
    lea rbx, [rsp+96]
    invoke GetClientRect, rsi, rbx

    ; 1. Margin & Gaps
    mov ecx, 28
    call scl
    mov [rsp+116], eax               ; min_margin = scl(28)

    mov ecx, 14
    call scl
    mov r15d, eax                    ; gap = scl(14)

    mov ecx, 66
    call scl
    mov r14d, eax                    ; button height = scl(66)

    ; 2. Responsive 3-column width:
    ; available = client_width - 2 * min_margin - 2 * gap
    mov eax, [rbx+RC_R]
    sub eax, [rbx+RC_L]
    sub eax, [rsp+116]
    sub eax, [rsp+116]
    sub eax, r15d
    sub eax, r15d
    xor edx, edx
    mov ecx, 3
    div ecx                          ; eax = card width
    mov r13d, eax

    ; Clamp card width: min scl(280), max scl(380)
    mov ecx, 280
    call scl
    cmp r13d, eax
    cmovb r13d, eax
    mov ecx, 380
    call scl
    cmp r13d, eax
    cmova r13d, eax

    ; Total grid width = card_width * 3 + gap * 2
    mov eax, r13d
    lea eax, [rax*2+r13]
    add eax, r15d
    add eax, r15d
    mov [rsp+128], eax               ; grid_width in rsp+128

    ; Center the grid horizontally: margin = (client_width - grid_width) / 2
    mov eax, [rbx+RC_R]
    sub eax, [rbx+RC_L]
    sub eax, [rsp+128]
    shr eax, 1
    cmp eax, [rsp+116]
    cmovb eax, [rsp+116]
    mov r12d, eax                    ; r12d = centered margin

    ; 3. Title: x = margin, y = scl(20)
    invoke GetDlgItem, rsi, ID_TITLE
    mov [rsp+112], rax
    mov ecx, 20
    call scl
    mov [rsp+120], eax               ; title_y
    mov ecx, 360
    call scl
    mov edi, eax                     ; title width
    mov ecx, 30
    call scl
    mov [rsp+116], eax               ; title height
    invoke MoveWindow, [rsp+112], r12, [rsp+120], rdi, [rsp+116], 1

    ; 4. Hint Subtitle: x = margin, y = title_y + title height + scl(4)
    mov ebp, [rsp+120]
    add ebp, [rsp+116]
    mov ecx, 4
    call scl
    add ebp, eax                     ; ebp = hint_y

    invoke GetDlgItem, rsi, ID_HINT
    mov [rsp+112], rax
    mov ecx, 20
    call scl
    mov [rsp+116], eax               ; hint height
    movsxd rbp, ebp
    invoke MoveWindow, [rsp+112], r12, rbp, [rsp+128], [rsp+116], 1

    ; 5. Grid Top Y = hint_y + hint height + scl(16)
    mov ecx, 16
    call scl
    add ebp, eax
    add ebp, [rsp+116]
    mov [rsp+120], ebp               ; grid_top_y

    ; 6. Layout 11 Session Launcher Cards with tool_order
    ; Row 0..2 (slots 0..8): 3 columns each
    ; Row 3 (slots 9..10): 2 columns, width = (grid_width - gap) / 2
    mov eax, [rsp+128]
    sub eax, r15d
    shr eax, 1
    mov [rsp+132], eax               ; row4_card_width = (grid_width - gap) / 2

    xor ebp, ebp
.btn:
    cmp ebp, 11
    jae .btndone

    cmp ebp, 9
    jae .row4

    ; Standard 3-column rows (slots 0..8)
    mov eax, ebp
    xor edx, edx
    mov ecx, 3
    div ecx                          ; eax=row (0..2), edx=col (0..2)
    mov edi, eax                     ; edi = row

    mov r8d, r13d
    add r8d, r15d
    imul edx, r8d
    add edx, r12d
    mov [rsp+124], edx               ; x

    mov r8d, r14d
    add r8d, r15d
    imul edi, r8d
    add edi, [rsp+120]               ; y

    lea rax, [tool_order]
    mov eax, [rax+rbp*4]             ; get button ID from tool_order
    invoke GetDlgItem, rsi, rax
    movsxd rdx, dword [rsp+124]
    movsxd rdi, edi
    invoke MoveWindow, rax, rdx, rdi, r13, r14, 1
    inc ebp
    jmp .btn

.row4:
    ; Row 3: 2 balanced half-width cards (slots 9 and 10)
    ; y = grid_top_y + 3 * (height + gap)
    mov eax, r14d
    add eax, r15d
    imul eax, 3
    add eax, [rsp+120]
    mov edi, eax                     ; y

    ; x = margin (for slot 9) or margin + row4_width + gap (for slot 10)
    mov edx, r12d
    cmp ebp, 9
    je .r4_x
    add edx, [rsp+132]
    add edx, r15d
.r4_x:
    mov [rsp+124], edx               ; x

    lea rax, [tool_order]
    mov eax, [rax+rbp*4]             ; get button ID
    invoke GetDlgItem, rsi, rax
    movsxd rdx, dword [rsp+124]
    movsxd rdi, edi
    movsxd r10, dword [rsp+132]      ; width = row4_card_width
    invoke MoveWindow, rax, rdx, rdi, r10, r14, 1
    inc ebp
    jmp .btn

.btndone:

    ; 7. Section 2 Heading ("최근 세션") & Delete Button:
    ; bottom of buttons = grid_top_y + 4 * (height + gap) - gap
    mov eax, r14d
    add eax, r15d
    shl eax, 2                       ; 4 * (height + gap)
    sub eax, r15d
    add eax, [rsp+120]
    mov ebp, eax                     ; bottom of buttons

    mov ecx, 22
    call scl
    add ebp, eax                     ; rlabel_y = bottom of buttons + scl(22)

    ; RLABEL
    invoke GetDlgItem, rsi, ID_RLABEL
    mov [rsp+112], rax
    mov ecx, 24
    call scl
    mov [rsp+116], eax               ; rlabel height
    mov ecx, 300
    call scl
    mov edi, eax                     ; rlabel width
    movsxd rbp, ebp
    invoke MoveWindow, [rsp+112], r12, rbp, rdi, [rsp+116], 1

    ; RDEL (right-aligned with grid)
    invoke GetDlgItem, rsi, ID_RDEL
    mov [rsp+112], rax
    mov ecx, 26
    call scl
    mov [rsp+116], eax               ; rdel height
    mov ecx, 120
    call scl
    mov edi, eax                     ; rdel width
    ; rdel_x = margin + grid_width - rdel_width
    mov eax, r12d
    add eax, [rsp+128]
    sub eax, edi
    movsxd r10, eax                  ; r10 = rdel x
    invoke MoveWindow, [rsp+112], r10, rbp, rdi, [rsp+116], 1

    ; 8. Recent Listbox
    mov ecx, 30
    call scl
    add ebp, eax                     ; list_y = rlabel_y + scl(30)

    invoke GetDlgItem, rsi, ID_RECENT
    mov [rsp+112], rax
    mov edi, [rsp+128]               ; width = total grid width
    ; height = client_bottom - list_y - scl(20)
    mov ecx, 20
    call scl
    mov edx, eax
    mov eax, [rbx+RC_B]
    sub eax, ebp
    sub eax, edx
    cmp eax, 80
    jge .h
    mov eax, 80
.h:
    invoke MoveWindow, [rsp+112], r12, rbp, rdi, rax, 1
    EPILOG 256

; ---------------------------------------------------------------- window procedure
home_proc:
    PROLOG 192
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_COMMAND
    je .command
    cmp edi, 0x2C
    je .measure
    cmp edi, 0x2B
    je .draw
    cmp edi, WM_ERASEBKGND
    je .erase
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLORLISTBOX
    je .clist
    cmp edi, BC_THEME
    je .theme
    cmp edi, WM_APP+30
    je .refresh
    cmp edi, WM_SETFOCUS
    je .focus
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 192

.create:
    mov rax, [r13]
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, rax
    invoke mk, rsi, &cn_static, &s_app_title, 0, ID_TITLE
    mov rbx, rax
    invoke SendMessageW, rbx, WM_SETFONT, [g_font_title], 1

    invoke mk, rsi, &cn_static, &s_hint_text, 0, ID_HINT
    mov rbx, rax
    invoke SendMessageW, rbx, WM_SETFONT, [g_font_desc], 1

    xor ebx, ebx
.mkbtn:
    cmp ebx, 11
    jae .mkdone
    mov eax, 1001
    add eax, ebx
    invoke mk, rsi, &cn_button, &s_empty, BS_OWNERDRAW|WS_TABSTOP, rax
    inc ebx
    jmp .mkbtn
.mkdone:
    invoke mk, rsi, &cn_static, &s_rlabel, 0, ID_RLABEL
    mov rbx, rax
    invoke SendMessageW, rbx, WM_SETFONT, [g_fontb], 1

    invoke mk, rsi, &cn_listbox, &s_empty, LBS_NOTIFY|WS_VSCROLL|WS_BORDER|WS_TABSTOP|0x50, ID_RECENT
    invoke mk, rsi, &cn_button, &s_del_btn, BS_OWNERDRAW|WS_TABSTOP, ID_RDEL
    invoke fill_recent, rsi
    xor eax, eax
    EPILOG 192

.size:
    invoke home_layout, rsi
    xor eax, eax
    EPILOG 192

.command:
    movzx eax, r12w
    cmp eax, 1001
    jb .chk_list
    cmp eax, 1011
    ja .chk_list
    invoke SendMessageW, [g_hframe], WM_COMMAND, rax, 0
    xor eax, eax
    EPILOG 192
.chk_list:
    cmp eax, ID_RDEL
    je .rdel
    cmp eax, ID_RECENT
    jne .def
    mov rax, r12
    shr eax, 16
    cmp eax, LBN_DBLCLK
    jne .def
    lea rdi, [rsp+96]
    invoke GetDlgItem, rsi, ID_RECENT
    mov rbx, rax
    invoke SendMessageW, rbx, LB_GETCURSEL, 0, 0
    movsxd rax, eax
    test rax, rax
    js .none
    invoke SendMessageW, rbx, LB_GETTEXT, rax, rdi
    invoke open_any_file, rdi
.none:
    xor eax, eax
    EPILOG 192

.erase:
    lea rbx, [rsp+96]
    invoke GetClientRect, rsi, rbx
    invoke FillRect, r12, rbx, [th_panelbrush]
    mov eax, 1
    EPILOG 192

.cstatic:
    mov rbx, r13                      ; hwnd of static control
    invoke GetDlgCtrlID, rbx
    cmp eax, ID_HINT
    je .c_hint
    invoke SetTextColor, r12, #[th_fg]
    invoke SetBkColor, r12, #[th_panel]
    mov rax, [th_panelbrush]
    EPILOG 192
.c_hint:
    invoke SetTextColor, r12, #[th_same]
    invoke SetBkColor, r12, #[th_panel]
    mov rax, [th_panelbrush]
    EPILOG 192

.clist:
    invoke SetTextColor, r12, #[th_fg]
    invoke SetBkColor, r12, #[th_bg]
    mov rax, [th_bgbrush]
    EPILOG 192

.theme:
    invoke RedrawWindow, rsi, 0, 0, 0x0585
    xor eax, eax
    EPILOG 192

.measure:
    mov ecx, 54
    call scl
    mov [r13+16], eax
    mov eax, 1
    EPILOG 192

.draw:
    invoke home_drawitem, r13
    mov eax, 1
    EPILOG 192

.rdel:
    invoke home_remove, rsi
    xor eax, eax
    EPILOG 192

.refresh:
    invoke fill_recent, rsi
    xor eax, eax
    EPILOG 192

.focus:
    invoke GetDlgItem, rsi, 1001
    invoke SetFocus, rax
    xor eax, eax
    EPILOG 192

; ---------------------------------------------------------------- owner-drawn cards & controls

; home_drawitem(DRAWITEMSTRUCT*)
home_drawitem:
    PROLOG 128
    mov rbx, rcx
    cmp dword [rbx], 4                ; ODT_BUTTON
    je .btn
    cmp dword [rbx], 2                ; ODT_LISTBOX
    je .list
    xor eax, eax
    EPILOG 128
.btn:
    mov eax, [rbx+4]                  ; CtlID
    cmp eax, ID_RDEL
    je .draw_del
    cmp eax, 1001
    jb .bdone
    cmp eax, 1011
    ja .bdone
    invoke home_draw_card, rbx
    mov eax, 1
    EPILOG 128
.draw_del:
    invoke home_draw_rdel, rbx
    mov eax, 1
    EPILOG 128
.bdone:
    xor eax, eax
    EPILOG 128
.list:
    invoke home_draw_listitem, rbx
    mov eax, 1
    EPILOG 128

; home_draw_card(DRAWITEMSTRUCT* in rcx)
home_draw_card:
    PROLOG 512
    mov rbx, rcx
    mov r12d, [rbx+4]                 ; CtlID
    sub r12d, 1001                    ; index 0..10
    mov r13d, [rbx+16]                ; itemState
    mov rsi, [rbx+32]                 ; hDC
    lea r14, [rbx+40]                 ; rcItem

    ; 1. Pick background and border color
    cmp qword [g_dark], 0
    jz .light_card
    test r13d, 1                      ; ODS_SELECTED (pressed)
    jnz .dark_pressed
    test r13d, 0x40 | 0x10            ; ODS_HOTLIGHT | ODS_FOCUS
    jnz .dark_hover
    mov eax, 0x28211C                 ; Normal background: dark slate
    mov edx, 0x42362E                 ; Normal border
    jmp .card_colors_done
.dark_hover:
    mov eax, 0x342B24                 ; Hover background
    mov edx, 0xFA9638                 ; Hover border: vibrant sky-blue
    jmp .card_colors_done
.dark_pressed:
    mov eax, 0x1E1815                 ; Pressed background
    mov edx, 0xFA9638                 ; Pressed border
    jmp .card_colors_done

.light_card:
    test r13d, 1
    jnz .light_pressed
    test r13d, 0x40 | 0x10
    jnz .light_hover
    mov eax, 0xFFFFFF
    mov edx, 0xEBE7E5
    jmp .card_colors_done
.light_hover:
    mov eax, 0xFCFAF8
    mov edx, 0xE1D5CB
    jmp .card_colors_done
.light_pressed:
    mov eax, 0xF9F5F1
    mov edx, 0xB8A394
.card_colors_done:
    mov [rsp+96], eax
    mov [rsp+100], edx

    ; Draw Card Rounded Rectangle
    movsxd rax, dword [rsp+96]
    invoke CreateSolidBrush, rax
    mov [rsp+104], rax
    movsxd rax, dword [rsp+100]
    invoke CreatePen, PS_SOLID, 1, rax
    mov [rsp+112], rax
    invoke SelectObject, rsi, [rsp+104]
    mov [rsp+120], rax
    invoke SelectObject, rsi, [rsp+112]
    mov [rsp+128], rax

    mov ecx, 8
    call scl
    mov r11, rax                      ; corner radius = 8

    movsxd rdi, dword [r14]           ; left
    movsxd rbp, dword [r14+4]         ; top
    movsxd r15, dword [r14+8]         ; right
    movsxd r10, dword [r14+12]        ; bottom
    invoke RoundRect, rsi, rdi, rbp, r15, r10, r11, r11

    invoke SelectObject, rsi, [rsp+120]
    invoke DeleteObject, [rsp+104]
    invoke SelectObject, rsi, [rsp+128]
    invoke DeleteObject, [rsp+112]

    ; 2. Left Icon Badge Box
    mov ecx, 12
    call scl
    mov [rsp+136], eax                ; badge_x_offset
    mov ecx, 42
    call scl
    mov [rsp+140], eax                ; badge_size
    mov ecx, 8
    call scl
    mov [rsp+144], eax                ; badge_radius

    ; badge rect in [rsp+152]:
    mov eax, [r14]
    add eax, [rsp+136]
    mov [rsp+152], eax                ; b_left
    mov eax, [r14+12]
    sub eax, [r14+4]
    sub eax, [rsp+140]
    shr eax, 1
    add eax, [r14+4]
    mov [rsp+156], eax                ; b_top
    mov eax, [rsp+152]
    add eax, [rsp+140]
    mov [rsp+160], eax                ; b_right
    mov eax, [rsp+156]
    add eax, [rsp+140]
    mov [rsp+164], eax                ; b_bottom

    cmp qword [g_dark], 0
    jz .b_light
    lea rax, [badge_fg_dark]
    mov eax, [rax+r12*4]
    mov [rsp+168], eax                ; badge_fg
    lea rax, [badge_bg_dark]
    mov eax, [rax+r12*4]
    mov [rsp+172], eax                ; badge_bg
    jmp .b_draw
.b_light:
    lea rax, [badge_fg_light]
    mov eax, [rax+r12*4]
    mov [rsp+168], eax                ; badge_fg
    lea rax, [badge_bg_light]
    mov eax, [rax+r12*4]
    mov [rsp+172], eax                ; badge_bg
.b_draw:
    movsxd rax, dword [rsp+172]
    invoke CreateSolidBrush, rax
    mov [rsp+176], rax
    movsxd rax, dword [rsp+172]
    invoke CreatePen, PS_SOLID, 1, rax
    mov [rsp+184], rax
    invoke SelectObject, rsi, [rsp+176]
    mov [rsp+192], rax
    invoke SelectObject, rsi, [rsp+184]
    mov [rsp+200], rax

    movsxd rdi, dword [rsp+152]
    movsxd rbp, dword [rsp+156]
    movsxd r15, dword [rsp+160]
    movsxd r10, dword [rsp+164]
    movsxd r11, dword [rsp+144]
    invoke RoundRect, rsi, rdi, rbp, r15, r10, r11, r11

    invoke SelectObject, rsi, [rsp+192]
    invoke DeleteObject, [rsp+176]
    invoke SelectObject, rsi, [rsp+200]
    invoke DeleteObject, [rsp+184]

    ; Draw Badge Icon Glyph
    invoke SelectObject, rsi, [g_card_iconfont]
    mov [rsp+208], rax
    invoke SetBkMode, rsi, TRANSPARENT
    movsxd rax, dword [rsp+168]
    invoke SetTextColor, rsi, rax
    lea rax, [card_glyphs]
    mov rax, [rax+r12*8]
    invoke DrawTextW, rsi, rax, 1, &rsp+152, DT_CENTER|DT_VCENTER|DT_SINGLELINE
    invoke SelectObject, rsi, [rsp+208]

    ; 3. Card Title
    invoke SelectObject, rsi, [g_font_sub]
    mov [rsp+216], rax
    invoke SetBkMode, rsi, TRANSPARENT

    test r13d, 0x40 | 0x10
    jnz .t_hover
    movsxd rax, dword [th_fg]
    invoke SetTextColor, rsi, rax
    jmp .t_rect
.t_hover:
    cmp qword [g_dark], 0
    jz .t_hover_light
    invoke SetTextColor, rsi, 0xFA9638
    jmp .t_rect
.t_hover_light:
    invoke SetTextColor, rsi, 0xEB6325
.t_rect:
    mov ecx, 64
    call scl
    mov edx, [r14]
    add edx, eax
    mov [rsp+224], edx                ; title left
    mov ecx, 12
    call scl
    mov edx, [r14+4]
    add edx, eax
    mov [rsp+228], edx                ; title top
    mov ecx, 52
    call scl
    mov edx, [r14+8]
    sub edx, eax
    mov [rsp+232], edx                ; title right
    mov ecx, 22
    call scl
    add eax, [rsp+228]
    mov [rsp+236], eax                ; title bottom

    lea rax, [card_titles]
    mov rax, [rax+r12*8]
    invoke DrawTextW, rsi, rax, -1, &rsp+224, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX
    invoke SelectObject, rsi, [rsp+216]

    ; 4. Pro Pill Badge (if pro)
    lea rax, [pro_flags]
    cmp dword [rax+r12*4], 1
    jne .p_skip

    mov ecx, 14
    call scl
    mov r8d, eax                      ; margin = scl(14)
    mov ecx, 34
    call scl
    mov r9d, eax                      ; pill_w = scl(34)
    mov ecx, 18
    call scl
    mov r10d, eax                     ; pill_h = scl(18)

    mov eax, [r14+8]
    sub eax, r8d
    mov [rsp+248], eax                ; p_right
    sub eax, r9d
    mov [rsp+240], eax                ; p_left
    mov eax, [r14+4]
    add eax, r8d
    mov [rsp+244], eax                ; p_top
    add eax, r10d
    mov [rsp+252], eax                ; p_bottom

    cmp qword [g_dark], 0
    jz .p_light
    invoke CreateSolidBrush, 0x302420
    mov [rsp+256], rax
    invoke CreatePen, PS_SOLID, 1, 0x443630
    jmp .p_brush
.p_light:
    invoke CreateSolidBrush, 0xF9F5F1
    mov [rsp+256], rax
    invoke CreatePen, PS_SOLID, 1, 0xF0E8E2
.p_brush:
    mov [rsp+264], rax
    invoke SelectObject, rsi, [rsp+256]
    mov [rsp+272], rax
    invoke SelectObject, rsi, [rsp+264]
    mov [rsp+280], rax

    mov ecx, 4
    call scl
    mov r11, rax
    movsxd rdi, dword [rsp+240]
    movsxd rbp, dword [rsp+244]
    movsxd r15, dword [rsp+248]
    movsxd r10, dword [rsp+252]
    invoke RoundRect, rsi, rdi, rbp, r15, r10, r11, r11

    invoke SelectObject, rsi, [rsp+272]
    invoke DeleteObject, [rsp+256]
    invoke SelectObject, rsi, [rsp+280]
    invoke DeleteObject, [rsp+264]

    invoke SelectObject, rsi, [g_font_badge]
    mov [rsp+288], rax
    invoke SetBkMode, rsi, TRANSPARENT
    cmp qword [g_dark], 0
    jz .p_txt_l
    invoke SetTextColor, rsi, 0xB8A394
    jmp .p_txt_draw
.p_txt_l:
    invoke SetTextColor, rsi, 0x8B7464
.p_txt_draw:
    invoke DrawTextW, rsi, &s_pro, -1, &rsp+240, DT_CENTER|DT_VCENTER|DT_SINGLELINE
    invoke SelectObject, rsi, [rsp+288]
.p_skip:

    ; 5. Card Description
    invoke SelectObject, rsi, [g_font_desc]
    mov [rsp+296], rax
    invoke SetBkMode, rsi, TRANSPARENT
    movsxd rax, dword [th_same]
    invoke SetTextColor, rsi, rax

    mov ecx, 64
    call scl
    mov edx, [r14]
    add edx, eax
    mov [rsp+304], edx                ; d_left
    mov ecx, 38
    call scl
    mov edx, [r14+4]
    add edx, eax
    mov [rsp+308], edx                ; d_top
    mov ecx, 24
    call scl
    mov edx, [r14+8]
    sub edx, eax
    mov [rsp+312], edx                ; d_right
    mov ecx, 18
    call scl
    add eax, [rsp+308]
    mov [rsp+316], eax                ; d_bottom

    lea rax, [card_descs]
    mov rax, [rax+r12*8]
    invoke DrawTextW, rsi, rax, -1, &rsp+304, DT_LEFT|DT_SINGLELINE|DT_NOPREFIX|DT_END_ELLIPSIS
    invoke SelectObject, rsi, [rsp+296]

    ; 6. Right Chevron Arrow
    mov ecx, 20
    call scl
    mov edx, [r14+8]
    sub edx, eax
    mov [rsp+320], edx
    mov edx, [r14+4]
    mov [rsp+324], edx
    mov ecx, 6
    call scl
    mov edx, [r14+8]
    sub edx, eax
    mov [rsp+328], edx
    mov edx, [r14+12]
    mov [rsp+332], edx

    invoke SelectObject, rsi, [g_font_sub]
    mov [rsp+336], rax
    invoke SetBkMode, rsi, TRANSPARENT
    test r13d, 0x40 | 0x10
    jnz .ch_hover
    movsxd rax, dword [th_same]
    invoke SetTextColor, rsi, rax
    jmp .ch_ready
.ch_hover:
    cmp qword [g_dark], 0
    jz .ch_hover_light
    invoke SetTextColor, rsi, 0xFA9638
    jmp .ch_ready
.ch_hover_light:
    invoke SetTextColor, rsi, 0xEB6325
.ch_ready:
    invoke DrawTextW, rsi, &s_chevron, 1, &rsp+320, DT_CENTER|DT_VCENTER|DT_SINGLELINE
    invoke SelectObject, rsi, [rsp+336]

    EPILOG 512

; home_draw_rdel(DRAWITEMSTRUCT* in rcx)
home_draw_rdel:
    PROLOG 256
    mov rbx, rcx
    mov r13d, [rbx+16]                ; itemState
    mov rsi, [rbx+32]                 ; hDC
    lea r14, [rbx+40]                 ; rcItem

    cmp qword [g_dark], 0
    jz .rd_light
    test r13d, 1                      ; pressed
    jnz .rd_press
    test r13d, 0x40 | 0x10            ; hover
    jnz .rd_hover
    mov eax, 0x241E1A                 ; normal bg
    mov edx, 0x42362E                 ; normal border
    jmp .rd_ready
.rd_hover:
    mov eax, 0x302220                 ; hover bg
    mov edx, 0x5050F8                 ; hover border: coral/red
    jmp .rd_ready
.rd_press:
    mov eax, 0x1A1412
    mov edx, 0x5050F8
    jmp .rd_ready
.rd_light:
    test r13d, 1
    jnz .rd_press_l
    test r13d, 0x40 | 0x10
    jnz .rd_hover_l
    mov eax, 0xFFFFFF
    mov edx, 0xEBE7E5
    jmp .rd_ready
.rd_hover_l:
    mov eax, 0xF2F2FE
    mov edx, 0xA5A5FC
    jmp .rd_ready
.rd_press_l:
    mov eax, 0xE2E2FE
    mov edx, 0x2626DC
.rd_ready:
    movsxd r15, edx
    movsxd rax, eax
    invoke CreateSolidBrush, rax
    mov [rsp+96], rax
    invoke CreatePen, PS_SOLID, 1, r15
    mov [rsp+104], rax
    invoke SelectObject, rsi, [rsp+96]
    mov [rsp+112], rax
    invoke SelectObject, rsi, [rsp+104]
    mov [rsp+120], rax

    mov ecx, 6
    call scl
    mov r11, rax
    movsxd rdi, dword [r14]
    movsxd rbp, dword [r14+4]
    movsxd r15, dword [r14+8]
    movsxd r10, dword [r14+12]
    invoke RoundRect, rsi, rdi, rbp, r15, r10, r11, r11

    invoke SelectObject, rsi, [rsp+112]
    invoke DeleteObject, [rsp+96]
    invoke SelectObject, rsi, [rsp+120]
    invoke DeleteObject, [rsp+104]

    invoke SelectObject, rsi, [g_font_desc]
    mov [rsp+128], rax
    invoke SetBkMode, rsi, TRANSPARENT
    test r13d, 0x40 | 0x10
    jnz .rd_txt_hover
    movsxd rax, dword [th_fg]
    invoke SetTextColor, rsi, rax
    jmp .rd_draw_txt
.rd_txt_hover:
    cmp qword [g_dark], 0
    jz .rd_thover_l
    invoke SetTextColor, rsi, 0x8080FF   ; light coral
    jmp .rd_draw_txt
.rd_thover_l:
    invoke SetTextColor, rsi, 0x2626DC
.rd_draw_txt:
    invoke DrawTextW, rsi, &s_del_btn, -1, r14, DT_CENTER|DT_VCENTER|DT_SINGLELINE
    invoke SelectObject, rsi, [rsp+128]

    EPILOG 256

; ---------------------------------------------------------------- recent list (owner drawn: type, files, path, age)
section .rdata
WSTR sec_session, "Session"
WSTR hk_type, "Type"
WSTR hk_a, "A"
WSTR hk_b, "B"
WSTR f_l1, "%s  ·  %s  ↔  %s"
WSTR f_l1b, "%s  ·  %s"
WSTR f_l1c, "%s"
WSTR f_l2, "%s   ·   %s"
WSTR s_just, "방금"
WSTR f_min, "%d분 전"
WSTR f_hour, "%d시간 전"
WSTR f_day, "%d일 전"
WSTR s_nolabel, "세션"

section .text

; home_draw_listitem(DRAWITEMSTRUCT* in rcx)
home_draw_listitem:
    PROLOG 4480
    mov rbx, rcx
    mov esi, [rbx+8]
    cmp esi, -1
    je .d
    mov r12d, [rbx+16]                ; itemState
    mov r13, [rbx+32]                 ; hDC
    lea r14, [rbx+40]                 ; rcItem
    invoke SendMessageW, [rbx+24], LB_GETTEXT, rsi, &rsp+96

    test r12d, 1                      ; selected
    jz .norm
    cmp qword [g_dark], 0
    jz .sel_l
    invoke CreateSolidBrush, 0x302824
    jmp .fill
.sel_l:
    invoke CreateSolidBrush, 0xF9F5F1
    jmp .fill
.norm:
    cmp qword [g_dark], 0
    jz .norm_l
    invoke CreateSolidBrush, 0x24201D
    jmp .fill
.norm_l:
    invoke CreateSolidBrush, 0xFFFFFF
.fill:
    mov [rsp+4440], rax
    invoke FillRect, r13, r14, [rsp+4440]
    invoke DeleteObject, [rsp+4440]
    invoke SetBkMode, r13, 1

    ; Draw left indicator accent bar if selected
    test r12d, 1
    jz .no_bar
    mov ecx, 4
    call scl
    mov edx, [r14]
    add edx, eax
    mov eax, [r14]
    mov [rsp+4448], eax
    mov eax, [r14+4]
    mov [rsp+4452], eax
    mov [rsp+4456], edx
    mov eax, [r14+12]
    mov [rsp+4460], eax
    movsxd rax, dword [th_hl]
    invoke CreateSolidBrush, rax
    mov [rsp+4440], rax
    lea rdx, [rsp+4448]
    invoke FillRect, r13, rdx, [rsp+4440]
    invoke DeleteObject, [rsp+4440]
.no_bar:

    ; session data
    mov word [rsp+1200], 0
    mov word [rsp+2200], 0
    mov word [rsp+3300], 0
    invoke GetPrivateProfileStringW, &sec_session, &hk_type, &s_empty, &rsp+1200, 40, &rsp+96
    invoke GetPrivateProfileStringW, &sec_session, &hk_a, &s_empty, &rsp+2200, 519, &rsp+96
    invoke GetPrivateProfileStringW, &sec_session, &hk_b, &s_empty, &rsp+3300, 519, &rsp+96

    ; type label
    lea r8, [s_nolabel]
    xor esi, esi
.t:
    cmp esi, 12
    jae .tdone
    lea rax, [type_names]
    invoke _wcsicmp, &rsp+1200, [rax+rsi*8]
    test eax, eax
    jz .thit
    inc esi
    jmp .t
.thit:
    lea rax, [tab_titles]
    mov r8, [rax+rsi*8]
.tdone:
    mov [rsp+4350], r8                ; label

    ; file name tails
    lea rdi, [rsp+2200]
    invoke wcsrchr, rdi, 0x5C
    test rax, rax
    jz .ta
    lea rdi, [rax+2]
.ta:
    mov [rsp+4360], rdi
    lea rdi, [rsp+3300]
    invoke wcsrchr, rdi, 0x5C
    test rax, rax
    jz .tb
    lea rdi, [rax+2]
.tb:
    mov [rsp+4368], rdi
    cmp word [rsp+2200], 0
    je .l1c
    cmp word [rsp+3300], 0
    je .l1b
    invoke _snwprintf, &rsp+1300, 190, &f_l1, [rsp+4350], [rsp+4360], [rsp+4368]
    jmp .l1d
.l1b:
    invoke _snwprintf, &rsp+1300, 190, &f_l1b, [rsp+4350], [rsp+4360]
    jmp .l1d
.l1c:
    lea rdi, [rsp+96]
    invoke wcsrchr, rdi, 0x5C
    test rax, rax
    jz .l1e
    lea rdi, [rax+2]
.l1e:
    invoke _snwprintf, &rsp+1300, 190, &f_l1c, rdi
.l1d:
    ; age of the file
    mov word [rsp+1800], 0
    lea rdi, [s_empty]
    mov qword [rsp+4376], 0
    invoke GetFileAttributesExW, &rsp+96, 0, &rsp+4400
    test eax, eax
    jz .l2
    invoke GetSystemTimeAsFileTime, &rsp+4392
    mov rax, [rsp+4392]
    sub rax, [rsp+4400+20]
    jbe .l2
    xor edx, edx
    mov ecx, 10000000
    div rcx                           ; seconds
    lea rdi, [s_just]
    cmp rax, 60
    jb .l2
    xor edx, edx
    mov ecx, 60
    div rcx
    lea r8, [f_min]
    cmp rax, 60
    jb .age
    xor edx, edx
    mov ecx, 60
    div rcx
    lea r8, [f_hour]
    cmp rax, 24
    jb .age
    xor edx, edx
    mov ecx, 24
    div rcx
    lea r8, [f_day]
.age:
    mov r9, rax
    invoke _snwprintf, &rsp+1700, 30, r8, r9
    lea rdi, [rsp+1700]
.l2:
    invoke _snwprintf, &rsp+1800, 190, &f_l2, &rsp+96, rdi

    ; paint the two lines with refined fonts
    mov ecx, 16
    call scl
    mov esi, eax                      ; left padding
    mov ecx, 6
    call scl
    mov edi, eax                      ; top padding
    mov eax, [r14]
    add eax, esi
    mov [rsp+4420], eax
    mov eax, [r14+4]
    add eax, edi
    mov [rsp+4424], eax
    mov eax, [r14+8]
    sub eax, esi
    mov [rsp+4428], eax
    mov ecx, 22
    call scl
    add eax, [rsp+4424]
    mov [rsp+4432], eax
    mov r12d, eax                     ; y of the second line

    invoke SelectObject, r13, [g_font_sub]
    movsxd rax, dword [th_fg]
    invoke SetTextColor, r13, rax
    invoke DrawTextW, r13, &rsp+1300, -1, &rsp+4420, 0x8820
    mov eax, r12d
    mov [rsp+4424], eax
    mov ecx, 18
    call scl
    add eax, r12d
    mov [rsp+4432], eax
    invoke SelectObject, r13, [g_font_desc]
    movsxd rax, dword [th_same]
    invoke SetTextColor, r13, rax
    invoke DrawTextW, r13, &rsp+1800, -1, &rsp+4420, 0x8820
.d:
    EPILOG 4480

; home_remove(hwnd): drop the selected entry from the recent list
home_remove:
    PROLOG 1200
    mov r12, rcx
    invoke GetDlgItem, r12, ID_RECENT
    mov r13, rax
    invoke SendMessageW, r13, LB_GETCURSEL, 0, 0
    movsxd rbx, eax
    test rbx, rbx
    js .d
    lea rsi, [rsp+96]
    lea rdi, [rsp+128]
.sh:
    cmp ebx, MAX_RECENT-1
    jae .last
    lea eax, [rbx+1]
    invoke _snwprintf, rsi, 16, &f_r, rax
    invoke GetPrivateProfileStringW, &sec_recent, rsi, &s_empty, rdi, 520, &g_ini
    invoke _snwprintf, rsi, 16, &f_r, rbx
    invoke WritePrivateProfileStringW, &sec_recent, rsi, rdi, &g_ini
    inc ebx
    jmp .sh
.last:
    invoke _snwprintf, rsi, 16, &f_r, rbx
    invoke WritePrivateProfileStringW, &sec_recent, rsi, &s_empty, &g_ini
    invoke fill_recent, r12
.d:
    EPILOG 1200
