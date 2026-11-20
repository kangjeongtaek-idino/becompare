; Preferences for existing application behavior. Values use the same keys as the views.
section .rdata
WSTR sp_tabclass, "SysTabControl32"
WSTR sp_combo, "COMBOBOX"
WSTR sp_general, "일반 · 비교 · 기록"
WSTR sp_appearance, "글꼴 · 색상 · 단축키"
WSTR sp_external, "외부 도구"
sp_tabs: dq sp_general, sp_appearance, sp_external
WSTR sp_ui, "UI"
WSTR sp_dark, "Dark"
WSTR sp_text, "TextCompare"
WSTR sp_history, "History"
WSTR sp_enabled, "Enabled"
WSTR sp_editor, "Editor"
WSTR sp_autopair, "AutoPair"
WSTR sp_max, "MaxVersions"
WSTR sp_mb, "MaxFileMB"
WSTR sp_fold, "Fold"
WSTR sp_unified, "Unified"
WSTR sp_word, "WordHighlight"
WSTR sp_char, "CharHighlight"
WSTR sp_split, "SplitChanges"
WSTR sp_sync, "SyncScroll"
WSTR sp_ws, "Whitespace"
WSTR sp_numbers, "LineNumbers"
WSTR sp_blank, "IgnoreBlankLines"
WSTR sp_context, "FoldContext"
WSTR sp_lfold, "동일한 줄 접기"
WSTR sp_lunified, "통합 보기"
WSTR sp_lword, "변경된 단어 강조"
WSTR sp_lchar, "변경된 문자 강조"
WSTR sp_lsplit, "서로 관련된 변경 줄 짝맞춤"
WSTR sp_lsync, "좌우 스크롤 동기화"
WSTR sp_lws, "공백 문자 표시"
WSTR sp_lnumbers, "줄 번호 표시"
WSTR sp_lblank, "빈 줄 차이 무시"
WSTR sp_lhistory, "저장 전 로컬 기록 보관"
WSTR sp_lautopair, "괄호·따옴표 짝 자동 입력"
WSTR sp_intro, "텍스트 비교 기본값 (다음에 여는 비교 탭부터 적용)"
WSTR sp_ltheme, "화면 테마:"
WSTR sp_lmax, "파일당 보관 개수 (1~100):"
WSTR sp_lmb, "기록할 최대 크기 (MB, 1~1024):"
WSTR sp_lnote, "기록 개수·크기 제한은 이후 저장부터 적용됩니다. 보관을 꺼도 기존 기록은 유지됩니다."
WSTR sp_lcontext, "접기 주변 표시 줄 수:"
WSTR sp_light, "라이트"
WSTR sp_darkname, "다크"
WSTR sp_ctx1, "1줄"
WSTR sp_ctx2, "2줄"
WSTR sp_ctx3, "3줄"
WSTR sp_ctx4, "4줄"
WSTR sp_ctx8, "8줄"
sp_contexts: dq sp_ctx1, sp_ctx2, sp_ctx3, sp_ctx4, sp_ctx8
WSTR sp_decimal, "%u"
WSTR sp_invalid, "로컬 기록 보관 개수는 1~100, 최대 파일 크기는 1~1024 MB의 정수로 입력하세요."
align 8
; id, section, key, default, caption
sp_bools:
    dq 560, sp_text, sp_fold, 0, sp_lfold
    dq 561, sp_text, sp_unified, 0, sp_lunified
    dq 562, sp_text, sp_word, 1, sp_lword
    dq 563, sp_text, sp_char, 0, sp_lchar
    dq 564, sp_text, sp_split, 0, sp_lsplit
    dq 565, sp_text, sp_sync, 1, sp_lsync
    dq 566, sp_text, sp_ws, 0, sp_lws
    dq 567, sp_text, sp_numbers, 1, sp_lnumbers
    dq 568, sp_text, sp_blank, 0, sp_lblank
    dq 569, sp_history, sp_enabled, 1, sp_lhistory
    dq 574, sp_editor, sp_autopair, 1, sp_lautopair
section .text

sg_prefs_create:
    PROLOG 192
    mov rsi, rcx
    invoke mk, rsi, &sp_tabclass, 0, WS_TABSTOP, 550
    SPLACE 12, 12, 740, 30
    mov r15, rbx
    xor ebp, ebp
.tab:
    invoke memset, &rsp+96, 0, 40
    mov dword [rsp+96], 1           ; TCIF_TEXT
    lea rax, [sp_tabs]
    mov rax, [rax+rbp*8]
    mov [rsp+112], rax
    invoke SendMessageW, r15, TCM_INSERTITEMW, rbp, &rsp+96
    inc ebp
    cmp ebp, 3
    jb .tab
    xor ebp, ebp
.checks:
    imul rdi, rbp, 40
    lea rax, [sp_bools]
    add rdi, rax
    invoke mk_chk, rsi, [rdi+32], [rdi], 0
    mov r15, rax
    cmp ebp, 9
    je .history
    cmp ebp, 10
    je .pairs
    imul eax, ebp, 30
    add eax, 96
    mov [rsp+152], eax
    mov rax, r15
    SPLACE 24, [rsp+152], 360, 24
    jmp .next
.history:
    mov rax, r15
    SPLACE 400, 146, 300, 24
    jmp .next
.pairs:                              ; typing in the editor panes: under the text compare defaults
    mov rax, r15
    SPLACE 24, 424, 360, 24
.next:
    inc ebp
    cmp ebp, 11
    jb .checks
    invoke mk_lbl, rsi, &sp_intro, 580
    SPLACE 24, 60, 700, 26
    invoke mk_lbl, rsi, &sp_ltheme, 581
    SPLACE 400, 96, 100, 24
    invoke mk, rsi, &sp_combo, 0, WS_TABSTOP|WS_VSCROLL|3, 570
    SPLACE 510, 92, 160, 140
    invoke SendMessageW, rbx, 0x143, 0, &sp_light
    invoke SendMessageW, rbx, 0x143, 0, &sp_darkname
    invoke mk_lbl, rsi, &sp_lmax, 582
    SPLACE 400, 188, 244, 24
    invoke mk_edit, rsi, 572
    SPLACE 650, 184, 64, 24
    invoke mk_lbl, rsi, &sp_lmb, 583
    SPLACE 400, 228, 244, 24
    invoke mk_edit, rsi, 573
    SPLACE 650, 224, 64, 24
    invoke mk_lbl, rsi, &sp_lnote, 584
    SPLACE 400, 270, 310, 80
    invoke mk_lbl, rsi, &sp_lcontext, 585
    SPLACE 24, 384, 150, 24
    invoke mk, rsi, &sp_combo, 0, WS_TABSTOP|WS_VSCROLL|3, 571
    SPLACE 180, 380, 150, 180
    mov r15, rbx
    xor ebp, ebp
.contexts:
    lea rax, [sp_contexts]
    mov rdi, [rax+rbp*8]
    invoke SendMessageW, r15, 0x143, 0, rdi
    inc ebp
    cmp ebp, 5
    jb .contexts
    EPILOG 192

sg_page:
    PROLOG 64
    mov rsi, rcx
    invoke GetDlgItem, rsi, 550
    invoke SendMessageW, rax, 0x130B, 0, 0 ; TCM_GETCURSEL
    mov r15d, eax
    xor ebp, ebp
.rows:
    mov r12d, 0
    cmp r15d, 1
    jne .row_hide
    mov r12d, 5
.row_hide:
    lea edx, [rbp+600]
    invoke GetDlgItem, rsi, rdx
    invoke ShowWindow, rax, r12
    mov rax, rbp
    shl rax, 6
    lea rdi, [st_rows]
    mov edx, [rdi+rax+52]
    invoke GetDlgItem, rsi, rdx
    invoke ShowWindow, rax, r12
    inc ebp
    cmp ebp, ST_NROWS
    jb .rows
    mov ebp, 560
.general:
    xor r12d, r12d
    test r15d, r15d
    jnz .general_hide
    mov r12d, 5
.general_hide:
    invoke GetDlgItem, rsi, rbp
    invoke ShowWindow, rax, r12
    inc ebp
    cmp ebp, 586
    jb .general
    xor r12d, r12d
    cmp r15d, 2
    jne .external
    mov r12d, 5
.external:
    invoke GetDlgItem, rsi, 540
    invoke ShowWindow, rax, r12
    invoke GetDlgItem, rsi, 543
    invoke ShowWindow, rax, r12
    EPILOG 64

sg_prefs_fill:
    PROLOG 128
    mov rsi, rcx
    mov r15d, edx                  ; defaults
    xor ebp, ebp
.bool:
    imul rdi, rbp, 40
    lea rax, [sp_bools]
    add rdi, rax
    mov r12, [rdi+24]
    test r15d, r15d
    jnz .set
    invoke GetPrivateProfileIntW, [rdi+8], [rdi+16], r12, &g_ini
    xor r12d, r12d
    test eax, eax
    setnz r12b
.set:
    invoke GetDlgItem, rsi, [rdi]
    invoke SendMessageW, rax, BM_SETCHECK, r12, 0
    inc ebp
    cmp ebp, 11
    jb .bool
    xor r12d, r12d
    test r15d, r15d
    jnz .theme
    invoke GetPrivateProfileIntW, &sp_ui, &sp_dark, 0, &g_ini
    test eax, eax
    setnz r12b
.theme:
    invoke GetDlgItem, rsi, 570
    invoke SendMessageW, rax, 0x14E, r12, 0
    mov r12d, 2
    test r15d, r15d
    jnz .context
    invoke GetPrivateProfileIntW, &sp_text, &sp_context, 2, &g_ini
    cmp eax, 4
    ja .context
    mov r12d, eax
.context:
    invoke GetDlgItem, rsi, 571
    invoke SendMessageW, rax, 0x14E, r12, 0
    mov r12d, 30
    test r15d, r15d
    jnz .max
    invoke GetPrivateProfileIntW, &sp_history, &sp_max, 30, &g_ini
    cmp eax, 1
    jb .max
    cmp eax, 100
    ja .max
    mov r12d, eax
.max:
    invoke _snwprintf, &rsp+96, 12, &sp_decimal, r12
    invoke GetDlgItem, rsi, 572
    invoke SetWindowTextW, rax, &rsp+96
    mov r12d, 16
    test r15d, r15d
    jnz .mb
    invoke GetPrivateProfileIntW, &sp_history, &sp_mb, 16, &g_ini
    cmp eax, 1
    jb .mb
    cmp eax, 1024
    ja .mb
    mov r12d, eax
.mb:
    invoke _snwprintf, &rsp+96, 12, &sp_decimal, r12
    invoke GetDlgItem, rsi, 573
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 128

; Strict whole-string positive integer, -1 on invalid input.
sg_number:
    PROLOG 256
    mov r12d, r8d
    invoke GetDlgItem, rcx, rdx
    invoke GetWindowTextW, rax, &rsp+96, 60
    test eax, eax
    jz .bad
    invoke wcstoul, &rsp+96, &rsp+224, 10
    mov rdx, [rsp+224]
    cmp word [rdx], 0
    jne .bad
    cmp rax, 1
    jb .bad
    cmp rax, r12
    ja .bad
    EPILOG 256
.bad:
    mov rax, -1
    EPILOG 256

sg_prefs_validate:
    PROLOG 64
    mov rsi, rcx
    mov edi, 572
    invoke sg_number, rsi, rdi, 100
    cmp rax, -1
    je .bad
    mov edi, 573
    invoke sg_number, rsi, rdi, 1024
    cmp rax, -1
    je .bad
    mov eax, 1
    EPILOG 64
.bad:
    invoke GetDlgItem, rsi, 550
    invoke SendMessageW, rax, 0x130C, 0, 0 ; TCM_SETCURSEL
    invoke sg_page, rsi
    invoke GetDlgItem, rsi, rdi
    invoke SetFocus, rax
    invoke msgbox, rsi, &sp_invalid, MB_OK|MB_ICONWARNING
    xor eax, eax
    EPILOG 64

sg_prefs_save:
    PROLOG 128
    mov rsi, rcx
    xor ebp, ebp
.bool:
    imul rdi, rbp, 40
    lea rax, [sp_bools]
    add rdi, rax
    invoke get_chk, rsi, [rdi]
    movzx r12d, al
    invoke _snwprintf, &rsp+96, 12, &sp_decimal, r12
    invoke WritePrivateProfileStringW, [rdi+8], [rdi+16], &rsp+96, &g_ini
    inc ebp
    cmp ebp, 11
    jb .bool
    invoke GetDlgItem, rsi, 570
    invoke SendMessageW, rax, 0x147, 0, 0
    mov r12d, eax
    invoke _snwprintf, &rsp+96, 12, &sp_decimal, r12
    invoke WritePrivateProfileStringW, &sp_ui, &sp_dark, &rsp+96, &g_ini
    invoke GetDlgItem, rsi, 571
    invoke SendMessageW, rax, 0x147, 0, 0
    mov r12d, eax
    invoke _snwprintf, &rsp+96, 12, &sp_decimal, r12
    invoke WritePrivateProfileStringW, &sp_text, &sp_context, &rsp+96, &g_ini
    invoke sg_number, rsi, 572, 100
    mov r12, rax
    invoke _snwprintf, &rsp+96, 12, &sp_decimal, r12
    invoke WritePrivateProfileStringW, &sp_history, &sp_max, &rsp+96, &g_ini
    invoke sg_number, rsi, 573, 1024
    mov r12, rax
    invoke _snwprintf, &rsp+96, 12, &sp_decimal, r12
    invoke WritePrivateProfileStringW, &sp_history, &sp_mb, &rsp+96, &g_ini
    EPILOG 128
