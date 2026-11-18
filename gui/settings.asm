; settings.asm - CU-01 settings dialog (fonts, colours, shortcuts), CU-02 policy, EX-01 external tools
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem, EnableWindow, GetWindowRect
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetWindowTextW, SetWindowTextW, DestroyWindow
extern GetMessageW, TranslateMessage, DispatchMessageW, PostQuitMessage
extern GetPrivateProfileStringW, GetPrivateProfileIntW, WritePrivateProfileStringW, GetPrivateProfileSectionW
extern RegOpenKeyExW, RegQueryValueExW, RegCloseKey, CreateProcessW, CloseHandle, GetFileAttributesW
extern malloc, calloc, free, memset, wcslen, wcscpy, wcscat, wcsncpy, wcsrchr, wcschr, _wcsicmp, _wcsnicmp, _snwprintf, wcstoul
extern g_hinst, g_dark, th_diff, th_lonly, th_ronly, th_type, th_panelbrush, th_bg, th_fg
extern mk, reg_class, scl, msgbox, mk_btn, mk_lbl, mk_edit, ctl_color
extern g_ini, theme_set, apply_theme_all

global g_policy, g_keytab, policy_load, keys_load, settings_colors, ext_launch, settings_show, settings_font

; ---------------------------------------------------------------- data
%define POL_SYNC     1
%define POL_EXTERNAL 2
%define POL_FTP      4
%define POL_SETTINGS 8

%define SG_OK     1
%define SG_CANCEL 2
%define SG_DEFS   3
%define ID_EXT    540
%define ID_POL    541
%define ID_NOTE   542

section .rdata
WSTR cn_sg, "BCSettings"
WSTR cn_edit, "EDIT"
WSTR s_empty, ""
WSTR s_sg_title, "설정 — 글꼴 · 색 · 단축키 · 외부 도구"
WSTR s_sg_ok, "확인"
WSTR s_sg_cancel, "취소"
WSTR s_sg_defs, "기본값으로"
WSTR s_sg_note, "글꼴 변경은 프로그램을 다시 시작하면 적용됩니다. 색은 현재 모드(라이트/다크)의 값이며 즉시 적용됩니다(열려 있는 비교는 다시 실행해야 완전히 바뀝니다)."
WSTR s_sg_ext, "외부 도구 (한 줄에 하나: .확장자=명령줄  —  %1 = 왼쪽 파일, %2 = 오른쪽 파일, 폴더 비교에서 더블클릭하면 실행):"
WSTR s_sg_locked, "관리자 정책으로 설정이 잠겨 있습니다."
WSTR s_pol_none, "적용 중인 관리자 정책: 없음"
WSTR s_pol_head, "적용 중인 관리자 정책:"
WSTR s_pol_sync, " 복사·삭제·동기화 차단 ·"
WSTR s_pol_ext, " 외부 도구 차단 ·"
WSTR s_pol_ftp, " FTP 차단 ·"
WSTR s_pol_set, " 설정 변경 차단 ·"
WSTR s_pol_blocked, "관리자 정책으로 차단된 기능입니다."
WSTR sec_fonts, "Fonts"
WSTR sec_colors, "Colors"
WSTR sec_keys, "Keys"
WSTR sec_ext, "External"
WSTR k_monoface, "MonoFace"
WSTR k_monosize, "MonoSize"
WSTR k_uisize, "UISize"
WSTR k_ldiff, "Light_Diff"
WSTR k_ddiff, "Dark_Diff"
WSTR k_llon, "Light_LeftOnly"
WSTR k_dlon, "Dark_LeftOnly"
WSTR k_lron, "Light_RightOnly"
WSTR k_dron, "Dark_RightOnly"
WSTR k_ltype, "Light_Type"
WSTR k_dtype, "Dark_Type"
WSTR k_run, "Run"
WSTR k_save, "Save"
WSTR k_close, "CloseTab"
WSTR k_next, "NextTab"
WSTR k_nd, "NextDiff"
WSTR k_pd, "PrevDiff"
WSTR k_bm, "Bookmark"
WSTR k_nm, "NextMark"
WSTR k_pm, "PrevMark"
WSTR d_face, "Consolas"
WSTR d_msize, "10"
WSTR d_usize, "9"
WSTR d_run, "F5"
WSTR d_save, "Ctrl+S"
WSTR d_close, "Ctrl+W"
WSTR d_next, "Ctrl+Tab"
WSTR d_nd, "F8"
WSTR d_pd, "Shift+F8"
WSTR d_bm, "Ctrl+F2"
WSTR d_nm, "F2"
WSTR d_pm, "Shift+F2"
WSTR l_face, "고정폭 글꼴 이름:"
WSTR l_msize, "고정폭 글꼴 크기(pt):"
WSTR l_usize, "화면 글꼴 크기(pt):"
WSTR l_cdiff, "다른 줄 색 (RRGGBB):"
WSTR l_clon, "왼쪽에만 있음 색:"
WSTR l_cron, "오른쪽에만 있음 색:"
WSTR l_ctype, "형식 다름 색:"
WSTR l_krun, "비교 실행:"
WSTR l_ksave, "저장:"
WSTR l_kclose, "탭 닫기:"
WSTR l_knext, "다음 탭:"
WSTR l_knd, "다음 차이:"
WSTR l_kpd, "이전 차이:"
WSTR l_kbm, "북마크 토글:"
WSTR l_knm, "다음 북마크:"
WSTR l_kpm, "이전 북마크:"
WSTR f_hex6, "%06X"
WSTR s_pol_key, "Software\Policies\BeCompareAsm"
WSTR pv_sync, "DisableSync"
WSTR pv_ext, "DisableExternalTools"
WSTR pv_ftp, "DisableFtp"
WSTR pv_set, "DisableSettings"
WSTR s_ctrl, "ctrl"
WSTR s_shift, "shift"
WSTR s_alt, "alt"
WSTR kn_tab, "tab"
WSTR kn_esc, "esc"
WSTR kn_enter, "enter"
WSTR kn_space, "space"
WSTR kn_pgup, "pgup"
WSTR kn_pgdn, "pgdn"
WSTR kn_end, "end"
WSTR kn_home, "home"
WSTR kn_left, "left"
WSTR kn_up, "up"
WSTR kn_right, "right"
WSTR kn_down, "down"
WSTR kn_ins, "ins"
WSTR kn_del, "del"

%macro SROW 8
    dq %1, %2, %3, %4, %5, %6
    dd %7, %8
    dq 0
%endmacro

align 8
st_rows:
    SROW l_face, sec_fonts, k_monoface, k_monoface, d_face, 0, 0, 501
    SROW l_msize, sec_fonts, k_monosize, k_monosize, d_msize, 0, 0, 502
    SROW l_usize, sec_fonts, k_uisize, k_uisize, d_usize, 0, 0, 503
    SROW l_cdiff, sec_colors, k_ldiff, k_ddiff, 0, th_diff, 1, 504
    SROW l_clon, sec_colors, k_llon, k_dlon, 0, th_lonly, 1, 505
    SROW l_cron, sec_colors, k_lron, k_dron, 0, th_ronly, 1, 506
    SROW l_ctype, sec_colors, k_ltype, k_dtype, 0, th_type, 1, 507
    SROW l_krun, sec_keys, k_run, k_run, d_run, 0, 0, 511
    SROW l_ksave, sec_keys, k_save, k_save, d_save, 0, 0, 512
    SROW l_kclose, sec_keys, k_close, k_close, d_close, 0, 0, 513
    SROW l_knext, sec_keys, k_next, k_next, d_next, 0, 0, 514
    SROW l_knd, sec_keys, k_nd, k_nd, d_nd, 0, 0, 515
    SROW l_kpd, sec_keys, k_pd, k_pd, d_pd, 0, 0, 516
    SROW l_kbm, sec_keys, k_bm, k_bm, d_bm, 0, 0, 517
    SROW l_knm, sec_keys, k_nm, k_nm, d_nm, 0, 0, 518
    SROW l_kpm, sec_keys, k_pm, k_pm, d_pm, 0, 0, 519
st_rows_end:
%define ST_NROWS 16

; shortcut table keys: section order matches g_keytab slots
align 8
key_names: dq k_run, k_save, k_close, k_next, k_nd, k_pd, k_bm, k_nm, k_pm
key_defs:  dq d_run, d_save, d_close, d_next, d_nd, d_pd, d_bm, d_nm, d_pm
named_keys:
    dq kn_tab, 9, kn_esc, 27, kn_enter, 13, kn_space, 32, kn_pgup, 33, kn_pgdn, 34, kn_end, 35, kn_home, 36
    dq kn_left, 37, kn_up, 38, kn_right, 39, kn_down, 40, kn_ins, 45, kn_del, 46, 0, 0
pol_vals:  dq pv_sync, pv_ext, pv_ftp, pv_set
pol_texts: dq s_pol_sync, s_pol_ext, s_pol_ftp, s_pol_set

section .bss
alignb 8
g_policy: resq 1
g_keytab: resq 9                      ; per slot: dd vk, dd modifiers (1 ctrl, 2 shift, 4 alt)
g_sg_result: resq 1
g_sg_wc: resq 1

section .text

; ---------------------------------------------------------------- policy (CU-02)
; policy_load(): administrator policy from HKCU and HKLM (HKLM applied last), DWORD values under Software\Policies\BeCompareAsm
policy_load:
    PROLOG 64
    mov qword [g_policy], 0
    mov rcx, 0xFFFFFFFF80000001       ; HKCU
    call policy_one
    mov rcx, 0xFFFFFFFF80000002       ; HKLM
    call policy_one
    EPILOG 64

; policy_one(hive)
policy_one:
    PROLOG 128
    mov r12, rcx
    invoke RegOpenKeyExW, r12, &s_pol_key, 0, 0x20019, &rsp+96
    test eax, eax
    jnz .out
    xor ebx, ebx
.v:
    cmp ebx, 4
    jae .close
    mov dword [rsp+104], 0
    mov dword [rsp+108], 4
    lea rax, [pol_vals]
    mov rdx, [rax+rbx*8]
    invoke RegQueryValueExW, [rsp+96], rdx, 0, 0, &rsp+104, &rsp+108
    test eax, eax
    jnz .n
    cmp dword [rsp+104], 0
    je .n
    mov eax, 1
    mov ecx, ebx
    shl eax, cl
    or [g_policy], rax
.n:
    inc ebx
    jmp .v
.close:
    invoke RegCloseKey, [rsp+96]
.out:
    EPILOG 128

; ---------------------------------------------------------------- shortcuts (CU-01)
; parse_key(wchar* text) -> eax = vk | mods << 16 (0 when invalid)
parse_key:
    PROLOG 192
    mov rsi, rcx
    xor r12d, r12d                    ; mods
    xor r13d, r13d                    ; vk
.tok:
    ; skip blanks
    cmp word [rsi], ' '
    jne .go
    add rsi, 2
    jmp .tok
.go:
    lea rdi, [rsp+96]
    xor ecx, ecx
.cp:
    movzx eax, word [rsi]
    test eax, eax
    jz .end
    cmp eax, '+'
    je .end
    cmp eax, ' '
    je .skipsp
    cmp ecx, 12
    jae .skipsp
    mov [rdi+rcx*2], ax
    inc ecx
.skipsp:
    add rsi, 2
    jmp .cp
.end:
    mov word [rdi+rcx*2], 0
    movzx r14d, word [rsi]            ; terminator
    test ecx, ecx
    jz .nexttok
    invoke _wcsicmp, rdi, &s_ctrl
    test eax, eax
    jnz .n1
    or r12d, 1
    jmp .nexttok
.n1:
    invoke _wcsicmp, rdi, &s_shift
    test eax, eax
    jnz .n2
    or r12d, 2
    jmp .nexttok
.n2:
    invoke _wcsicmp, rdi, &s_alt
    test eax, eax
    jnz .n3
    or r12d, 4
    jmp .nexttok
.n3:
    ; the key itself
    movzx eax, word [rdi]
    cmp word [rdi+2], 0
    jne .multi
    cmp eax, 'a'
    jb .up
    cmp eax, 'z'
    ja .up
    sub eax, 32
.up:
    mov r13d, eax
    jmp .nexttok
.multi:
    mov eax, [rdi]
    or eax, 0x20
    cmp ax, 'f'
    jne .named
    ; F1..F24
    lea rcx, [rdi+2]
    xor eax, eax
.fd:
    movzx edx, word [rcx]
    test edx, edx
    jz .fdone
    sub edx, '0'
    cmp edx, 9
    ja .named
    imul eax, 10
    add eax, edx
    add rcx, 2
    jmp .fd
.fdone:
    test eax, eax
    jz .named
    cmp eax, 24
    ja .named
    add eax, 0x6F
    mov r13d, eax
    jmp .nexttok
.named:
    lea r15, [named_keys]
.nl:
    mov rax, [r15]
    test rax, rax
    jz .nexttok
    invoke _wcsicmp, rdi, rax
    test eax, eax
    jz .nh
    add r15, 16
    jmp .nl
.nh:
    mov r13d, [r15+8]
.nexttok:
    test r14d, r14d
    jz .fin
    add rsi, 2
    jmp .tok
.fin:
    mov eax, r12d
    shl eax, 16
    or eax, r13d
    test r13d, r13d
    jnz .ok
    xor eax, eax
.ok:
    EPILOG 192

; keys_load(): fill g_keytab from [Keys] (defaults when absent)
keys_load:
    PROLOG 512
    xor ebx, ebx
.k:
    cmp ebx, 9
    jae .d
    lea rax, [key_names]
    mov rdx, [rax+rbx*8]
    lea rax, [key_defs]
    mov r9, [rax+rbx*8]
    invoke GetPrivateProfileStringW, &sec_keys, rdx, r9, &rsp+96, 60, &g_ini
    invoke parse_key, &rsp+96
    test eax, eax
    jnz .set
    lea rax, [key_defs]
    mov rcx, [rax+rbx*8]
    invoke parse_key, rcx
.set:
    movzx ecx, ax
    shr eax, 16
    lea rdx, [g_keytab]
    mov [rdx+rbx*8], ecx
    mov [rdx+rbx*8+4], eax
    inc ebx
    jmp .k
.d:
    EPILOG 512

; ---------------------------------------------------------------- colours / fonts
; settings_colors(): per-mode colour overrides from [Colors]; called by theme_set after the defaults are set
settings_colors:
    PROLOG 512
    lea rsi, [st_rows]
    mov ebx, 3
.r:
    cmp ebx, 7
    jae .d
    mov eax, ebx
    shl eax, 6
    lea rdi, [rsi+rax]
    mov rdx, [rdi+16]
    cmp qword [g_dark], 0
    je .key
    mov rdx, [rdi+24]
.key:
    invoke GetPrivateProfileStringW, &sec_colors, rdx, &s_empty, &rsp+96, 20, &g_ini
    cmp word [rsp+96], 0
    je .n
    invoke wcstoul, &rsp+96, 0, 16
    ; RRGGBB -> COLORREF
    mov ecx, eax
    and ecx, 0xFF
    shl ecx, 16
    mov edx, eax
    and edx, 0xFF00
    or ecx, edx
    shr eax, 16
    and eax, 0xFF
    or ecx, eax
    mov rax, [rdi+40]
    mov [rax], ecx
.n:
    inc ebx
    jmp .r
.d:
    EPILOG 512

; settings_font(which 0 mono face / 1 mono size / 2 ui size, out wchar[64]) -> eax size (for 1, 2) ; reads [Fonts]
settings_font:
    PROLOG 256
    mov r12d, ecx
    mov r13, rdx
    cmp r12d, 0
    jne .num
    invoke GetPrivateProfileStringW, &sec_fonts, &k_monoface, &d_face, r13, 60, &g_ini
    EPILOG 256
.num:
    lea rdx, [k_monosize]
    mov r8d, 10
    cmp r12d, 1
    je .q
    lea rdx, [k_uisize]
    mov r8d, 9
.q:
    invoke GetPrivateProfileIntW, &sec_fonts, rdx, r8, &g_ini
    cmp eax, 6
    jge .lo
    mov eax, 6
.lo:
    cmp eax, 40
    jle .ok
    mov eax, 40
.ok:
    EPILOG 256

; ---------------------------------------------------------------- external tools (EX-01)
; ext_launch(pathA, pathB) -> eax 1 when an external tool was started
ext_launch:
    PROLOG 2704
    mov rsi, rcx
    mov rdi, rdx
    test qword [g_policy], POL_EXTERNAL
    jnz .no
    invoke wcsrchr, rsi, '.'
    test rax, rax
    jz .no
    invoke GetPrivateProfileStringW, &sec_ext, rax, &s_empty, &rsp+96, 1000, &g_ini
    cmp word [rsp+96], 0
    je .no
    ; expand %1 / %2 into the command line
    lea r12, [rsp+96]                 ; template
    lea r13, [rsp+2200]               ; output: 252 wchar up to the end of the frame
    xor r14d, r14d                    ; out index
.e:
    movzx eax, word [r12]
    test eax, eax
    jz .run
    cmp eax, '%'
    jne .lit
    movzx ecx, word [r12+2]
    cmp ecx, '1'
    je .p1
    cmp ecx, '2'
    jne .lit
    mov r15, rdi
    jmp .sub
.p1:
    mov r15, rsi
.sub:
    add r12, 4
    cmp r14d, 245
    jae .no
    mov word [r13+r14*2], '"'
    inc r14d
.sc:
    movzx eax, word [r15]
    test eax, eax
    jz .sq
    cmp r14d, 245
    jae .no                          ; too long: a path cut short would start the tool on another file
    mov [r13+r14*2], ax
    inc r14d
    add r15, 2
    jmp .sc
.sq:
    mov word [r13+r14*2], '"'
    inc r14d
    jmp .e
.lit:
    cmp r14d, 245
    jae .no
    mov [r13+r14*2], ax
    inc r14d
    add r12, 2
    jmp .e
.run:
    mov word [r13+r14*2], 0
    invoke memset, &rsp+1200, 0, 104
    mov dword [rsp+1200], 104        ; STARTUPINFOW.cb
    invoke CreateProcessW, 0, r13, 0, 0, 0, 0, 0, 0, &rsp+1200, &rsp+1320
    test eax, eax
    jz .no
    invoke CloseHandle, [rsp+1320]
    invoke CloseHandle, [rsp+1328]
    mov eax, 1
    EPILOG 2704
.no:
    xor eax, eax
    EPILOG 2704

; ---------------------------------------------------------------- the dialog
%macro SPLACE 4                       ; x y w h (unscaled), hwnd in rax
    mov rbx, rax
    mov ecx, %1
    call scl
    mov r12d, eax
    mov ecx, %2
    call scl
    mov r13d, eax
    mov ecx, %3
    call scl
    mov r14d, eax
    mov ecx, %4
    call scl
    invoke MoveWindow, rbx, r12, r13, r14, rax, 1
%endmacro

; st_fill(hwnd, defaults 0/1): load every row from the INI (or reset to defaults)
st_fill:
    PROLOG 512
    mov rsi, rcx
    mov r15d, edx
    xor ebx, ebx
.r:
    cmp ebx, ST_NROWS
    jae .ext
    mov eax, ebx
    shl eax, 6
    lea rdi, [st_rows]
    add rdi, rax
    ; default text
    mov r13, [rdi+32]
    cmp dword [rdi+48], 1
    jne .plain
    mov rax, [rdi+40]
    mov eax, [rax]
    mov ecx, eax
    and ecx, 0xFF
    shl ecx, 16
    mov edx, eax
    and edx, 0xFF00
    or ecx, edx
    shr eax, 16
    and eax, 0xFF
    or ecx, eax
    mov r14d, ecx
    invoke _snwprintf, &rsp+300, 20, &f_hex6, r14
    lea r13, [rsp+300]
.plain:
    mov rdx, [rdi+16]
    cmp dword [rdi+48], 1
    jne .key
    cmp qword [g_dark], 0
    je .key
    mov rdx, [rdi+24]
.key:
    test r15d, r15d
    jnz .usedef
    invoke GetPrivateProfileStringW, [rdi+8], rdx, r13, &rsp+96, 60, &g_ini
    jmp .set
.usedef:
    invoke wcscpy, &rsp+96, r13
.set:
    mov eax, [rdi+52]
    invoke GetDlgItem, rsi, rax
    invoke SetWindowTextW, rax, &rsp+96
    inc ebx
    jmp .r
.ext:
    ; external tools: section -> lines
    test r15d, r15d
    jnz .clr
    invoke calloc, 1, 16384
    mov rdi, rax
    invoke GetPrivateProfileSectionW, &sec_ext, rdi, 8000, &g_ini
    invoke calloc, 1, 20000
    mov r12, rax
    mov r13, rdi
    mov r14, r12
.cv:
    cmp word [r13], 0
    je .cvd
.ln:
    movzx eax, word [r13]
    add r13, 2
    test eax, eax
    jz .eol
    mov [r14], ax
    add r14, 2
    jmp .ln
.eol:
    mov word [r14], 13
    mov word [r14+2], 10
    add r14, 4
    jmp .cv
.cvd:
    invoke GetDlgItem, rsi, ID_EXT
    invoke SetWindowTextW, rax, r12
    invoke free, r12
    invoke free, rdi
    jmp .pol
.clr:
    invoke GetDlgItem, rsi, ID_EXT
    invoke SetWindowTextW, rax, &s_empty
.pol:
    EPILOG 512

; st_save(hwnd)
st_save:
    PROLOG 512
    mov rsi, rcx
    xor ebx, ebx
.r:
    cmp ebx, ST_NROWS
    jae .ext
    mov eax, ebx
    shl eax, 6
    lea rdi, [st_rows]
    add rdi, rax
    mov eax, [rdi+52]
    invoke GetDlgItem, rsi, rax
    invoke GetWindowTextW, rax, &rsp+96, 60
    mov rdx, [rdi+16]
    cmp dword [rdi+48], 1
    jne .write
    cmp qword [g_dark], 0
    je .cl
    mov rdx, [rdi+24]
.cl:
    invoke wcstoul, &rsp+96, 0, 16
    and eax, 0xFFFFFF
    invoke _snwprintf, &rsp+96, 20, &f_hex6, rax
    mov rdx, [rdi+16]
    cmp qword [g_dark], 0
    je .write
    mov rdx, [rdi+24]
.write:
    invoke WritePrivateProfileStringW, [rdi+8], rdx, &rsp+96, &g_ini
    inc ebx
    jmp .r
.ext:
    invoke WritePrivateProfileStringW, &sec_ext, 0, 0, &g_ini
    invoke calloc, 1, 20000
    mov r12, rax
    invoke GetDlgItem, rsi, ID_EXT
    invoke GetWindowTextW, rax, r12, 9000
    mov r13, r12
.line:
    cmp word [r13], 0
    je .done
    mov r14, r13                      ; line start
.find:
    movzx eax, word [r13]
    test eax, eax
    jz .gotline
    cmp eax, 13
    je .gotline
    cmp eax, 10
    je .gotline
    add r13, 2
    jmp .find
.gotline:
    movzx r15d, word [r13]
    mov word [r13], 0
    invoke wcschr, r14, '='
    test rax, rax
    jz .nextline
    mov word [rax], 0
    lea r8, [rax+2]
    invoke WritePrivateProfileStringW, &sec_ext, r14, r8, &g_ini
.nextline:
    test r15d, r15d
    jz .done
    add r13, 2
.skipnl:
    movzx eax, word [r13]
    cmp eax, 13
    je .adv
    cmp eax, 10
    jne .line
.adv:
    add r13, 2
    jmp .skipnl
.done:
    invoke free, r12
    EPILOG 512

; sg_layout(hwnd)
sg_create:
    PROLOG 192
    mov rsi, rcx
    xor ebp, ebp
.rows:
    cmp ebp, ST_NROWS
    jae .rest
    mov eax, ebp
    shl eax, 6
    lea rdi, [st_rows]
    add rdi, rax
    mov r15d, 16
    mov eax, 12
    mov edx, 175
    mov r8d, 150
    mov r9d, 170
    cmp ebp, 7
    jb .col1
    ; right column: shortcuts
    lea ecx, [rbp-7]
    imul ecx, 30
    lea r15d, [rcx+16]
    mov eax, 400
    mov edx, 495
    mov r8d, 90
    mov r9d, 150
    jmp .place
.col1:
    mov ecx, ebp
    imul ecx, 30
    lea r15d, [rcx+16]
.place:
    mov [rsp+96], eax                 ; label x
    mov [rsp+100], edx                ; edit x
    mov [rsp+104], r8d                ; label width
    mov [rsp+108], r9d                ; edit width
    invoke mk_lbl, rsi, [rdi], 0
    mov rbx, rax
    mov ecx, [rsp+96]
    call scl
    mov r12d, eax
    mov ecx, r15d
    add ecx, 3
    call scl
    mov r13d, eax
    mov ecx, [rsp+104]
    call scl
    mov r14d, eax
    mov ecx, 22
    call scl
    invoke MoveWindow, rbx, r12, r13, r14, rax, 1
    mov eax, [rdi+52]
    invoke mk_edit, rsi, rax
    mov rbx, rax
    mov ecx, [rsp+100]
    call scl
    mov r12d, eax
    mov ecx, r15d
    call scl
    mov r13d, eax
    mov ecx, [rsp+108]
    call scl
    mov r14d, eax
    mov ecx, 24
    call scl
    invoke MoveWindow, rbx, r12, r13, r14, rax, 1
    inc ebp
    jmp .rows
.rest:
    invoke mk_lbl, rsi, &s_sg_ext, 0
    SPLACE 12, 292, 740, 22
    invoke mk, rsi, &cn_edit, 0, WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|WS_VSCROLL|WS_TABSTOP|ES_WANTRETURN, ID_EXT
    SPLACE 12, 316, 740, 130
    invoke mk_lbl, rsi, &s_empty, ID_POL
    SPLACE 12, 454, 740, 22
    invoke mk_lbl, rsi, &s_sg_note, ID_NOTE
    SPLACE 12, 480, 740, 44
    invoke mk_btn, rsi, &s_sg_ok, SG_OK
    SPLACE 440, 540, 100, 28
    invoke mk_btn, rsi, &s_sg_cancel, SG_CANCEL
    SPLACE 548, 540, 100, 28
    invoke mk_btn, rsi, &s_sg_defs, SG_DEFS
    SPLACE 12, 540, 130, 28
    invoke st_fill, rsi, 0
    ; policy line
    invoke calloc, 1, 1200
    mov r12, rax
    xor ebx, ebx
    mov r13d, 0
    invoke wcscpy, r12, &s_pol_head
.pl:
    cmp ebx, 4
    jae .pdone
    mov eax, 1
    mov ecx, ebx
    shl eax, cl
    test [g_policy], rax
    jz .pn
    lea rax, [pol_texts]
    invoke wcscat, r12, [rax+rbx*8]
    inc r13d
.pn:
    inc ebx
    jmp .pl
.pdone:
    test r13d, r13d
    jnz .pset
    invoke wcscpy, r12, &s_pol_none
.pset:
    invoke GetDlgItem, rsi, ID_POL
    invoke SetWindowTextW, rax, r12
    invoke free, r12
    EPILOG 192

sg_proc:
    PROLOG 320
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_COMMAND
    je .command
    cmp edi, WM_CLOSE
    je .close
    cmp edi, WM_ERASEBKGND
    je .erase
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLOREDIT
    je .cedit
    cmp edi, WM_CTLCOLORBTN
    je .cstatic
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 320
.create:
    invoke sg_create, rsi
    xor eax, eax
    EPILOG 320
.command:
    movzx eax, r12w
    cmp eax, SG_OK
    je .ok
    cmp eax, SG_CANCEL
    je .close
    cmp eax, SG_DEFS
    jne .zero
    invoke st_fill, rsi, 1
    jmp .zero
.ok:
    invoke st_save, rsi
    mov qword [g_sg_result], 1
    jmp .zero
.close:
    mov qword [g_sg_result], 0
    jmp .zero
.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.zero:
    xor eax, eax
    EPILOG 320

; settings_show(owner)
settings_show:
    PROLOG 256
    mov rbx, rcx
    test qword [g_policy], POL_SETTINGS
    jz .go
    invoke msgbox, rbx, &s_sg_locked, MB_OK|MB_ICONWARNING
    EPILOG 256
.go:
    cmp qword [g_sg_wc], 0
    jne .have
    invoke reg_class, &cn_sg, &sg_proc, [th_panelbrush]
    mov qword [g_sg_wc], 1
.have:
    invoke GetWindowRect, rbx, &rsp+96
    mov ecx, 780
    call scl
    mov r12d, eax
    mov ecx, 650
    call scl
    mov r14d, eax
    mov eax, [rsp+104]
    sub eax, [rsp+96]
    sub eax, r12d
    sar eax, 1
    add eax, [rsp+96]
    mov r13d, eax
    mov eax, [rsp+108]
    sub eax, [rsp+100]
    sub eax, r14d
    sar eax, 1
    add eax, [rsp+100]
    mov r15d, eax
    mov qword [g_sg_result], -1
    invoke CreateWindowExW, 1, &cn_sg, &s_sg_title, WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE|WS_CLIPCHILDREN, r13, r15, r12, r14, rbx, 0, [g_hinst], 0
    mov rdi, rax
    test rax, rax
    jz .fail
    invoke EnableWindow, rbx, 0
.loop:
    cmp qword [g_sg_result], -1
    jne .done
    invoke GetMessageW, &rsp+96, 0, 0, 0
    test eax, eax
    jle .quit
    invoke TranslateMessage, &rsp+96
    invoke DispatchMessageW, &rsp+96
    jmp .loop
.quit:
    mov qword [g_sg_result], 0
    invoke PostQuitMessage, 0
.done:
    invoke EnableWindow, rbx, 1
    invoke DestroyWindow, rdi
    invoke SetFocus, rbx
    mov rax, [g_sg_result]
    test rax, rax
    jz .out
    call keys_load
    invoke theme_set, [g_dark]
    call apply_theme_all
.out:
    EPILOG 256
.fail:
    xor eax, eax
    EPILOG 256

; ---------------------------------------------------------------- tiny modal text input
extern IsDialogMessageW
global input_box

section .rdata
WSTR cn_ib, "BCInputBox"
WSTR s_ib_ok, "확인"
WSTR s_ib_cancel, "취소"

section .bss
alignb 8
g_ib_result: resq 1
g_ib_wc: resq 1

section .text

ib_proc:
    PROLOG 320
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_COMMAND
    je .command
    cmp edi, WM_CLOSE
    je .close
    cmp edi, WM_ERASEBKGND
    je .erase
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLOREDIT
    je .cedit
    cmp edi, WM_CTLCOLORBTN
    je .cstatic
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 320
.create:
    mov rax, [r13]
    mov r14, [rax]                    ; buffer
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, r14
    invoke mk_edit, rsi, 801
    mov rbx, rax
    invoke SetWindowTextW, rbx, r14
    mov ecx, 12
    call scl
    mov r12d, eax
    mov ecx, 14
    call scl
    mov r13d, eax
    mov ecx, 480
    call scl
    mov r15d, eax
    mov ecx, 26
    call scl
    invoke MoveWindow, rbx, r12, r13, r15, rax, 1
    invoke mk_btn, rsi, &s_ib_ok, 1
    mov rbx, rax
    invoke SendMessageW, rbx, 0xF4, 1, 1             ; BM_SETSTYLE: default push button
    mov ecx, 296
    call scl
    mov r12d, eax
    mov ecx, 58
    call scl
    mov r13d, eax
    mov ecx, 90
    call scl
    mov r15d, eax
    mov ecx, 28
    call scl
    invoke MoveWindow, rbx, r12, r13, r15, rax, 1
    invoke mk_btn, rsi, &s_ib_cancel, 2
    mov rbx, rax
    mov ecx, 396
    call scl
    mov r12d, eax
    mov ecx, 28
    call scl
    invoke MoveWindow, rbx, r12, r13, r15, rax, 1
    invoke GetDlgItem, rsi, 801
    mov rbx, rax
    invoke SetFocus, rbx
    invoke SendMessageW, rbx, EM_SETSEL, 0, -1
    xor eax, eax
    EPILOG 320
.command:
    movzx eax, r12w
    cmp eax, 1
    je .ok
    cmp eax, 2
    je .close
    jmp .zero
.ok:
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    mov r14, rax
    invoke GetDlgItem, rsi, 801
    invoke GetWindowTextW, rax, r14, 259
    mov qword [g_ib_result], 1
    jmp .zero
.close:
    mov qword [g_ib_result], 0
    jmp .zero
.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.zero:
    xor eax, eax
    EPILOG 320

; input_box(owner, title, buf (in: initial text, out: result, 260 wchar)) -> eax 1 when confirmed
input_box:
    PROLOG 256
    mov rbx, rcx
    mov rsi, rdx
    mov [rsp+192], r8
    cmp qword [g_ib_wc], 0
    jne .have
    invoke reg_class, &cn_ib, &ib_proc, [th_panelbrush]
    mov qword [g_ib_wc], 1
.have:
    invoke GetWindowRect, rbx, &rsp+96
    mov ecx, 520
    call scl
    mov r12d, eax
    mov ecx, 130
    call scl
    mov r14d, eax
    mov eax, [rsp+104]
    sub eax, [rsp+96]
    sub eax, r12d
    sar eax, 1
    add eax, [rsp+96]
    mov r13d, eax
    mov eax, [rsp+108]
    sub eax, [rsp+100]
    sub eax, r14d
    sar eax, 1
    add eax, [rsp+100]
    mov r15d, eax
    mov qword [g_ib_result], -1
    invoke CreateWindowExW, 1, &cn_ib, rsi, WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_VISIBLE|WS_CLIPCHILDREN, r13, r15, r12, r14, rbx, 0, [g_hinst], &rsp+192
    mov rdi, rax
    test rax, rax
    jz .fail
    invoke EnableWindow, rbx, 0
.loop:
    cmp qword [g_ib_result], -1
    jne .done
    invoke GetMessageW, &rsp+96, 0, 0, 0
    test eax, eax
    jle .quit
    invoke IsDialogMessageW, rdi, &rsp+96
    test eax, eax
    jnz .loop
    invoke TranslateMessage, &rsp+96
    invoke DispatchMessageW, &rsp+96
    jmp .loop
.quit:
    mov qword [g_ib_result], 0
    invoke PostQuitMessage, 0
.done:
    invoke EnableWindow, rbx, 1
    invoke DestroyWindow, rdi
    invoke SetFocus, rbx
    mov rax, [g_ib_result]
    EPILOG 256
.fail:
    xor eax, eax
    EPILOG 256
