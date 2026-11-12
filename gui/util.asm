; util.asm - shared GUI helpers: globals, controls, fonts/DPI, theme, dialogs
%include "inc/macros.inc"
%include "inc/win.inc"

extern CreateWindowExW, SendMessageW, RegisterClassExW, LoadCursorW, CreateFontW, LoadImageW
extern CreateSolidBrush, DeleteObject, GetDeviceCaps, GetDC, ReleaseDC
extern MessageBoxW, GetOpenFileNameW, GetSaveFileNameW, PostMessageW
extern g_batch, g_exitcode
extern SHBrowseForFolderW, SHGetPathFromIDListW, CoTaskMemFree
extern wcslen, wcscpy, memset, malloc, _snwprintf, wcscmp
extern GetDpiForSystem

global g_iconfont
extern settings_font, settings_colors
global g_hinst, g_hframe, g_htab, g_hstatus, g_font, g_fontb, g_mono, g_dpi, g_dark
global g_font_title, g_font_sub, g_font_desc, g_font_badge, g_card_iconfont
global th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_type, th_same, th_hl, th_conf
global th_tc_mod, th_tc_modin, th_tc_ins, th_tc_del, th_tc_modb, th_tc_insb, th_tc_delb, th_tc_gap
global th_card_bg, th_card_border, th_card_hover_bg, th_card_hover_border, th_accent
global th_bgbrush, th_panelbrush
global save_error
global set_status_part
global mk, reg_class, scl, set_status, msgbox, browse_folder, browse_open, browse_save, browse_save_as, browse_open_as
global theme_set, fonts_init, str_dup, s_appname, set_ctl_theme

section .rdata
WSTR s_save_failed, "저장하지 못했습니다. 파일 경로, 쓰기 권한 및 남은 공간을 확인하세요. 변경 내용은 유지됩니다."
WSTR s_appname, "BeCompare"
WSTR s_segoe, "Segoe UI"
WSTR s_consolas, "Consolas"
WSTR s_mdl2, "Segoe MDL2 Assets"
filter_all: dw __utf16__("모든 파일 (*.*)"), 0, __utf16__("*.*"), 0, 0

section .bss
alignb 8
g_hinst:        resq 1
g_hframe:       resq 1
g_htab:         resq 1
g_hstatus:      resq 1
g_font:         resq 1
g_fontb:        resq 1
g_mono:         resq 1
g_iconfont:     resq 1
g_font_title:   resq 1
g_font_sub:     resq 1
g_font_desc:    resq 1
g_font_badge:   resq 1
g_card_iconfont: resq 1
g_dpi:          resq 1
g_dark:         resq 1
th_bg:          resd 1
th_fg:          resd 1
th_panel:       resd 1
th_diff:        resd 1
th_lonly:       resd 1
th_ronly:       resd 1
th_type:        resd 1
th_same:        resd 1
th_hl:          resd 1
th_conf:        resd 1
; text compare palette (IntelliJ-style): modified / inserted / deleted lines, inline changes, markers
th_tc_mod:      resd 1
th_tc_modin:    resd 1
th_tc_ins:      resd 1
th_tc_del:      resd 1
th_tc_modb:     resd 1
th_tc_insb:     resd 1
th_tc_delb:     resd 1
th_tc_gap:      resd 1
th_card_bg:           resd 1
th_card_border:       resd 1
th_card_hover_bg:     resd 1
th_card_hover_border: resd 1
th_accent:            resd 1
                resd 1
th_bgbrush:     resq 1
th_panelbrush:  resq 1

section .text

save_error:
    lea rdx, [s_save_failed]
    mov r8d, MB_OK|MB_ICONWARNING
    jmp msgbox

; scl(rcx=px) -> eax = px * dpi / 96
scl:
    mov rax, rcx
    imul rax, [g_dpi]
    xor edx, edx
    mov ecx, 96
    div rcx
    ret

; mk(parent, class, text, style, id) -> hwnd   (adds WS_CHILD|WS_VISIBLE, sets UI font)
mk:
    PROLOG 128
    mov r12, rcx
    mov r13, [rsp+128+112]
    or r9d, WS_CHILD|WS_VISIBLE
    invoke CreateWindowExW, 0, rdx, r8, r9, 0, 0, 0, 0, r12, r13, [g_hinst], 0
    mov rbx, rax
    test rax, rax
    jz .done
    invoke SendMessageW, rbx, WM_SETFONT, [g_font], 1
.done:
    mov rax, rbx
    EPILOG 128

; reg_class(rcx=name, rdx=wndproc, r8=brush)
reg_class:
    PROLOG 192
    mov rsi, rcx
    mov r12, rdx
    mov r13, r8
    lea rbx, [rsp+96]
    xor eax, eax
    mov ecx, 10
.z:
    mov [rbx+rcx*8-8], rax
    dec rcx
    jnz .z
    mov dword [rbx], WC_SIZE
    mov dword [rbx+WC_STYLE], 0x0003          ; CS_HREDRAW|CS_VREDRAW
    mov [rbx+WC_PROC], r12
    mov rax, [g_hinst]
    mov [rbx+WC_INST], rax
    mov [rbx+WC_BRUSH], r13
    mov [rbx+WC_CLASS], rsi
    invoke LoadImageW, [g_hinst], 1, IMAGE_ICON, 32, 32, LR_SHARED
    mov [rbx+WC_ICON], rax
    invoke LoadImageW, [g_hinst], 1, IMAGE_ICON, 16, 16, LR_SHARED
    mov [rbx+WC_ICONSM], rax
    invoke LoadCursorW, 0, IDC_ARROW
    mov [rbx+WC_CURSOR], rax
    invoke RegisterClassExW, rbx
    EPILOG 192

; set_status(rcx=text)
set_status:
    PROLOG 64
    mov rbx, rcx
    invoke SendMessageW, [g_hstatus], SB_SETTEXTW, 0, rbx
    EPILOG 64

; set_status_part(part, text): 0 message, 1 summary, 2 details
set_status_part:
    PROLOG 64
    mov rbx, rdx
    mov rsi, rcx
    invoke SendMessageW, [g_hstatus], SB_SETTEXTW, rsi, rbx
    EPILOG 64

; msgbox(hwnd, text, flags) -> id
; In an unattended run (/report, /sync, /xlsx, /docx, /snapshot, /exit) nobody can dismiss a notice, and the
; minimized window would wait forever. A plain OK box there means the job cannot go on: record the error
; (exit code 2) and close instead. A question gets the answer that writes nothing: No, or Cancel when there
; is no No (for the "save changes?" prompt on close that means the window closes without saving).
msgbox:
    PROLOG 64
    mov rbx, r8
    test qword [g_batch], 4
    jz .show
    mov eax, ebx
    and eax, 0x0F
    jz .notice
    cmp eax, 3                        ; MB_YESNOCANCEL
    je .no
    cmp eax, MB_YESNO
    je .no
    mov eax, 2                        ; IDCANCEL
    EPILOG 64
.no:
    mov eax, 7                        ; IDNO
    EPILOG 64
.notice:
    mov qword [g_exitcode], 2
    cmp qword [g_hframe], 0
    je .quiet
    invoke PostMessageW, [g_hframe], WM_CLOSE, 0, 0
.quiet:
    mov eax, 1                        ; IDOK
    EPILOG 64
.show:
    invoke MessageBoxW, rcx, rdx, &s_appname, rbx
    EPILOG 64

; str_dup(rcx=wide string) -> malloc'd copy
str_dup:
    PROLOG 64
    mov rsi, rcx
    invoke wcslen, rsi
    lea rcx, [rax*2+2]
    call malloc
    mov rbx, rax
    test rax, rax
    jz .d
    invoke wcscpy, rbx, rsi
.d:
    mov rax, rbx
    EPILOG 64

; ---------------------------------------------------------------- fonts / theme

fonts_init:
    PROLOG 288
    invoke GetDpiForSystem
    test eax, eax
    jnz .ok
    mov eax, 96
.ok:
    mov [g_dpi], rax
    ; height = -(9 * dpi / 72)
    mov r12, rax
    mov ecx, 2
    xor edx, edx
    call settings_font
    imul rax, r12
    xor edx, edx
    mov ecx, 72
    div rcx
    neg rax
    mov rbx, rax
    mov ecx, 1
    xor edx, edx
    call settings_font
    imul rax, r12
    xor edx, edx
    mov ecx, 72
    div rcx
    neg rax
    mov r13, rax
    xor ecx, ecx
    lea rdx, [rsp+96]
    call settings_font
    invoke CreateFontW, rbx, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_segoe
    mov [g_font], rax
    invoke CreateFontW, rbx, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_segoe
    mov [g_fontb], rax
    invoke CreateFontW, r13, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 49, &rsp+96
    mov [g_mono], rax
    invoke CreateFontW, rbx, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_mdl2
    mov [g_iconfont], rax

    ; title font: size 16 bold
    mov rax, 16
    imul rax, r12
    xor edx, edx
    mov ecx, 72
    div rcx
    neg rax
    mov r14, rax
    invoke CreateFontW, r14, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_segoe
    mov [g_font_title], rax

    ; sub font: size 11 bold
    mov rax, 11
    imul rax, r12
    xor edx, edx
    mov ecx, 72
    div rcx
    neg rax
    mov r14, rax
    invoke CreateFontW, r14, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_segoe
    mov [g_font_sub], rax

    ; desc font: size 9 normal
    invoke CreateFontW, rbx, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_segoe
    mov [g_font_desc], rax

    ; badge font: size 8 bold
    mov rax, 8
    imul rax, r12
    xor edx, edx
    mov ecx, 72
    div rcx
    neg rax
    mov r14, rax
    invoke CreateFontW, r14, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_segoe
    mov [g_font_badge], rax

    ; card icon font: size 18
    mov rax, 18
    imul rax, r12
    xor edx, edx
    mov ecx, 72
    div rcx
    neg rax
    mov r14, rax
    invoke CreateFontW, r14, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 5, 0, &s_mdl2
    mov [g_card_iconfont], rax
    EPILOG 288

; theme_set(rcx=dark 0/1): fills colours and brushes
theme_set:
    PROLOG 64
    mov [g_dark], rcx
    mov rbx, rcx
    test rbx, rbx
    jnz .dark
    mov dword [th_bg], 0xFFFFFF
    mov dword [th_fg], 0x2A170F
    mov dword [th_panel], 0xFAF9F8
    mov dword [th_diff], 0xE2E2FE
    mov dword [th_lonly], 0xE7FCDC
    mov dword [th_ronly], 0xFEF2E0
    mov dword [th_type], 0xC7F3FE
    mov dword [th_same], 0x8B7464
    mov dword [th_hl], 0xEB6325
    mov dword [th_conf], 0x7171F8
    mov dword [th_tc_mod], 0xF9E8DC          ; IntelliJ light: modified #DCE8F9
    mov dword [th_tc_modin], 0xF0C7A9        ;   inline change #A9C7F0
    mov dword [th_tc_ins], 0xDCF1DC          ;   inserted #DCF1DC
    mov dword [th_tc_del], 0xE8E8E8          ;   deleted #E8E8E8
    mov dword [th_tc_modb], 0xDB8B4B         ;   markers: blue / green / grey
    mov dword [th_tc_insb], 0x5FB85F
    mov dword [th_tc_delb], 0xA0A0A0
    mov dword [th_tc_gap], 0xF2F0EE          ;   empty side of a changed block #EEF0F2
    mov dword [th_card_bg], 0xFFFFFF
    mov dword [th_card_border], 0xEBE7E5
    mov dword [th_card_hover_bg], 0xFCFAF8
    mov dword [th_card_hover_border], 0xB8A394
    mov dword [th_accent], 0xEB6325
    jmp .brushes
.dark:
    mov dword [th_bg], 0x221C18
    mov dword [th_fg], 0xF6F4F3
    mov dword [th_panel], 0x181412
    mov dword [th_diff], 0x362024
    mov dword [th_lonly], 0x223D26
    mov dword [th_ronly], 0x223554
    mov dword [th_type], 0x442C1B
    mov dword [th_same], 0x9E9994
    mov dword [th_hl], 0xFA9638
    mov dword [th_conf], 0x1E70B0
    mov dword [th_tc_mod], 0x4E3A2B          ; IntelliJ dark: modified #2B3A4E
    mov dword [th_tc_modin], 0x7A573D        ;   inline change #3D577A
    mov dword [th_tc_ins], 0x2F4026          ;   inserted #26402F
    mov dword [th_tc_del], 0x403D3A          ;   deleted #3A3D40
    mov dword [th_tc_modb], 0xD68F5C         ;   markers: blue / green / grey
    mov dword [th_tc_insb], 0x5E9A4E
    mov dword [th_tc_delb], 0x857F7B
    mov dword [th_tc_gap], 0x1B1816          ;   empty side #16181B
    mov dword [th_card_bg], 0x28211C
    mov dword [th_card_border], 0x42362E
    mov dword [th_card_hover_bg], 0x342B24
    mov dword [th_card_hover_border], 0xFA9638
    mov dword [th_accent], 0xFA9638
.brushes:
    call settings_colors
    mov rcx, [th_bgbrush]
    test rcx, rcx
    jz .b1
    call DeleteObject
.b1:
    mov ecx, [th_bg]
    call CreateSolidBrush
    mov [th_bgbrush], rax
    mov rcx, [th_panelbrush]
    test rcx, rcx
    jz .b2
    call DeleteObject
.b2:
    mov ecx, [th_panel]
    call CreateSolidBrush
    mov [th_panelbrush], rax
    EPILOG 64

; ---------------------------------------------------------------- dialogs

; browse_folder(hwnd, out wchar[520]) -> eax 1 if chosen
browse_folder:
    PROLOG 1200
    mov r12, rcx
    mov r13, rdx
    lea rdi, [rsp+96]                 ; BROWSEINFOW (64)
    lea rsi, [rsp+160]                ; display name buffer
    invoke memset, rdi, 0, 64
    mov [rdi], r12
    mov [rdi+16], rsi
    mov dword [rdi+32], BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE
    invoke SHBrowseForFolderW, rdi
    test rax, rax
    jz .none
    mov rbx, rax
    invoke SHGetPathFromIDListW, rbx, r13
    mov r12d, eax
    invoke CoTaskMemFree, rbx
    mov eax, r12d
    EPILOG 1200
.none:
    xor eax, eax
    EPILOG 1200

; browse_open(hwnd, out wchar[520]) -> eax 1 if chosen
browse_open:
    PROLOG 320
    xor r8d, r8d
    mov r10d, OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST
    jmp ofn_common
; browse_save(hwnd, out) -> eax
browse_save:
    PROLOG 320
    mov r8d, 1
    mov r10d, OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST
ofn_common:
    mov r12, rcx
    mov r13, rdx
    mov r14d, r8d
    mov r15d, r10d
    lea rdi, [rsp+96]                 ; OPENFILENAMEW (152)
    invoke memset, rdi, 0, OFN_SIZE
    mov dword [rdi], OFN_SIZE
    mov [rdi+8], r12
    lea rax, [filter_all]
    mov [rdi+24], rax
    mov dword [rdi+44], 1
    mov [rdi+48], r13
    mov dword [rdi+56], 520
    mov [rdi+96], r15d
    test r14d, r14d
    jnz .save
    invoke GetOpenFileNameW, rdi
    EPILOG 320
.save:
    invoke GetSaveFileNameW, rdi
    EPILOG 320

; browse_save_as(hwnd, out wchar[520] (in: suggested name), filter, default extension) -> eax 1 if chosen
browse_save_as:
    PROLOG 320
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    lea rdi, [rsp+96]                 ; OPENFILENAMEW (152)
    invoke memset, rdi, 0, OFN_SIZE
    mov dword [rdi], OFN_SIZE
    mov [rdi+8], r12
    mov [rdi+24], r14
    mov dword [rdi+44], 1
    mov [rdi+48], r13
    mov dword [rdi+56], 520
    mov dword [rdi+96], OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST
    mov [rdi+104], r15
    invoke GetSaveFileNameW, rdi
    EPILOG 320

; browse_open_as(hwnd, out wchar[520], filter) -> eax 1 if chosen
browse_open_as:
    PROLOG 320
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    lea rdi, [rsp+96]                 ; OPENFILENAMEW (152)
    invoke memset, rdi, 0, OFN_SIZE
    mov dword [rdi], OFN_SIZE
    mov [rdi+8], r12
    mov [rdi+24], r14
    mov dword [rdi+44], 1
    mov [rdi+48], r13
    mov dword [rdi+56], 520
    mov dword [rdi+96], OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST
    invoke GetOpenFileNameW, rdi
    EPILOG 320

; set_ctl_theme: stub hook for per-control theming (rcx = hwnd)
set_ctl_theme:
    ret
