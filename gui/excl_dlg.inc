; excl_dlg.inc - choosing what to exclude from a folder comparison   (included by folder.asm)
;   * right-click menu on the result list: exclude by name / by extension / only this location
;   * "제외 선택…" dialog: check list of the active patterns, the folders and extensions found by the
;     last scan (with item counts), common presets and patterns typed by hand

extern GetCursorPos, wcschr

%define XD_LIST   911
%define XD_EDIT   912
%define XD_ADD    913
%define XD_CLEAR  914
%define XD_HINT   915
%define XD_OK     1
%define XD_CANCEL 2
%define LVS_EX_CHECKBOXES 4
%define XD_MAXITEMS 3000
%define EXCL_MAX  255                ; characters the exclude field / session string can hold

; item: { wchar* text, dd kind (0 active, 1 folder, 2 extension, 3 preset, 4 typed), dd count, dd checked, dd pad }
%define XI_TEXT  0
%define XI_KIND  8
%define XI_COUNT 12
%define XI_CHK   16
%define XI_SIZE  24

section .rdata
WSTR cn_xd, "BCExcludePick"
WSTR s_xd_title, "제외 항목 선택"
WSTR s_xd_hint, "체크한 항목은 비교에서 제외됩니다. 이름·확장자 패턴은 모든 하위 폴더에 적용되고(* ? 사용 가능), \\로 시작하는 패턴은 그 위치의 항목만 제외합니다."
WSTR s_xd_ok, "확인"
WSTR s_xd_cancel, "취소"
WSTR s_xd_add, "패턴 추가"
WSTR s_xd_clear, "모두 해제"
WSTR h_xd_pat, "제외 패턴"
WSTR h_xd_kind, "종류"
WSTR h_xd_cnt, "항목 수"
WSTR s_xk0, "사용 중"
WSTR s_xk1, "폴더"
WSTR s_xk2, "확장자"
WSTR s_xk3, "자주 쓰는 항목"
WSTR s_xk4, "직접 입력"
WSTR s_x_toolong, "제외 목록이 너무 깁니다(최대 255자). 일부 항목의 체크를 해제하거나 *.확장자 같은 패턴으로 묶어 주세요."
WSTR s_x_partial, "일부 항목은 추가하지 못했습니다. 제외 목록은 최대 255자이고, 이름에 ; 또는 , 가 있는 항목은 넣을 수 없습니다."
WSTR s_x_badpat, "패턴에는 ; 과 , 를 쓸 수 없습니다."
WSTR f_xd_star, "*%s"
WSTR f_xd_root, "\%s"
WSTR f_cx_name, "이름으로 제외:  %.80s"
WSTR f_cx_ext, "확장자로 제외:  *%.40s"
WSTR f_cx_path, "이 위치만 제외:  \%.120s"
WSTR f_cx_names, "선택한 %u개 항목을 이름으로 제외"
WSTR s_cx_exts, "선택한 항목을 확장자로 제외"
WSTR f_cx_paths, "선택한 %u개 항목을 이 위치만 제외"
WSTR s_cx_edit, "제외 항목 선택…"
WSTR s_xp1, ".git"
WSTR s_xp2, ".svn"
WSTR s_xp3, ".vs"
WSTR s_xp4, ".idea"
WSTR s_xp5, "node_modules"
WSTR s_xp6, "bin"
WSTR s_xp7, "obj"
WSTR s_xp8, "__pycache__"
WSTR s_xp9, "*.tmp"
WSTR s_xp10, "*.bak"
WSTR s_xp11, "*.log"
WSTR s_xp12, "*.obj"
WSTR s_xp13, "*.pdb"
WSTR s_xp14, "Thumbs.db"
WSTR s_xp15, ".DS_Store"
align 8
xd_kinds:   dq s_xk0, s_xk1, s_xk2, s_xk3, s_xk4
xd_presets: dq s_xp1, s_xp2, s_xp3, s_xp4, s_xp5, s_xp6, s_xp7, s_xp8, s_xp9, s_xp10, s_xp11, s_xp12, s_xp13, s_xp14, s_xp15
%define XD_NPRESET 15

section .bss
alignb 8
g_xd_items:  resq 3                  ; vec { ptr, count, cap }
g_xd_result: resq 1
g_xd_out:    resq 1                  ; wchar[256]: in = current list, out = chosen list
g_xd_wc:     resq 1

section .text

; fold_excluded(fold, rel dir, name) -> eax 1 when the entry is filtered out by the exclude list
fold_excluded:
    PROLOG 2320
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    cmp word [r12+F_EXCL], 0
    je .no
    lea rdi, [rsp+96]
    mov word [rdi], 0x5C
    mov word [rdi+2], 0
    invoke wcschr, &r12+F_EXCL, 0x5C
    test rax, rax
    jz .match                        ; no location pattern: the path is never looked at
    cmp word [r13], 0
    je .root
    invoke _snwprintf, &rsp+98, 1100, &f_path, r13, r14
    jmp .term
.root:
    invoke _snwprintf, &rsp+98, 1100, &f_str, r14
.term:
    mov word [rsp+98+2200], 0
.match:
    invoke excl_match2, &r12+F_EXCL, r14, rdi
    EPILOG 2320
.no:
    xor eax, eax
    EPILOG 2320

; excl_add(list wchar[256], pattern) -> eax 1 added or already there, 0 no room / unusable pattern
excl_add:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    xor r12d, r12d                   ; pattern length
.len:
    movzx eax, word [rdi+r12*2]
    test eax, eax
    jz .lend
    cmp eax, ';'
    je .fail
    cmp eax, ','
    je .fail
    inc r12
    jmp .len
.lend:
    test r12, r12
    jz .fail
    mov rbx, rsi
.tok:
    movzx eax, word [rbx]
    test eax, eax
    jz .append
    cmp eax, ';'
    je .sep
    cmp eax, ','
    je .sep
    mov r13, rbx
.tend:
    movzx eax, word [r13]
    test eax, eax
    jz .cmp
    cmp eax, ';'
    je .cmp
    cmp eax, ','
    je .cmp
    add r13, 2
    jmp .tend
.cmp:
    mov rax, r13
    sub rax, rbx
    shr rax, 1
    cmp rax, r12
    jne .nexttok
    invoke _wcsnicmp, rbx, rdi, r12
    test eax, eax
    jz .ok
.nexttok:
    mov rbx, r13
    jmp .tok
.sep:
    add rbx, 2
    jmp .tok
.append:
    invoke wcslen, rsi
    mov r14, rax
    lea rax, [r14+r12+1]
    cmp rax, EXCL_MAX
    ja .fail
    test r14, r14
    jz .cat
    cmp word [rsi+r14*2-2], ';'
    je .cat
    mov word [rsi+r14*2], ';'
    inc r14
.cat:
    lea rcx, [rsi+r14*2]
    invoke wcscpy, rcx, rdi
.ok:
    mov eax, 1
    EPILOG 64
.fail:
    xor eax, eax
    EPILOG 64

; ---------------------------------------------------------------- candidate list

; xd_find(text) -> rax item or 0
xd_find:
    PROLOG 64
    mov rsi, rcx
    xor ebx, ebx
.l:
    cmp rbx, [g_xd_items+8]
    jae .none
    imul rdi, rbx, XI_SIZE
    add rdi, [g_xd_items]
    invoke _wcsicmp, [rdi+XI_TEXT], rsi
    test eax, eax
    jz .hit
    inc rbx
    jmp .l
.hit:
    mov rax, rdi
    EPILOG 64
.none:
    xor eax, eax
    EPILOG 64

; xd_add(text, kind, count, checked) -> rax item (0 when the list is full)
xd_add:
    PROLOG 64
    mov rsi, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    invoke xd_find, rsi
    test rax, rax
    jz .new
    add [rax+XI_COUNT], r13d
    or [rax+XI_CHK], r14d
    EPILOG 64
.new:
    cmp qword [g_xd_items+8], XD_MAXITEMS
    jae .full
    invoke vec_push, &g_xd_items, XI_SIZE
    mov rdi, rax
    invoke str_dup, rsi
    mov [rdi+XI_TEXT], rax
    mov [rdi+XI_KIND], r12d
    mov [rdi+XI_COUNT], r13d
    mov [rdi+XI_CHK], r14d
    mov dword [rdi+20], 0
    mov rax, rdi
    EPILOG 64
.full:
    xor eax, eax
    EPILOG 64

; xd_cmp(a, b): active first, then folders, extensions, presets; larger counts first
xd_cmp:
    PROLOG 64
    mov eax, [rcx+XI_KIND]
    cmp eax, [rdx+XI_KIND]
    jb .lt
    ja .gt
    mov eax, [rcx+XI_COUNT]
    cmp eax, [rdx+XI_COUNT]
    ja .lt
    jb .gt
    invoke _wcsicmp, [rcx+XI_TEXT], [rdx+XI_TEXT]
    EPILOG 64
.lt:
    mov eax, -1
    EPILOG 64
.gt:
    mov eax, 1
    EPILOG 64

; xd_build(fold): fill g_xd_items from the current list (g_xd_out), the scan result and the presets
xd_build:
    PROLOG 2320
    mov rbx, rcx
    mov qword [g_xd_items+8], 0
    ; active patterns
    mov rsi, [g_xd_out]
.tok:
    movzx eax, word [rsi]
    test eax, eax
    jz .scan
    cmp eax, ';'
    je .sep
    cmp eax, ','
    je .sep
    lea rdi, [rsp+96]
    xor ecx, ecx
.tc:
    movzx eax, word [rsi]
    test eax, eax
    jz .te
    cmp eax, ';'
    je .te
    cmp eax, ','
    je .te
    cmp ecx, 1000
    jae .tskip
    mov [rdi+rcx*2], ax
    inc ecx
.tskip:
    add rsi, 2
    jmp .tc
.te:
    mov word [rdi+rcx*2], 0
    invoke xd_add, rdi, 0, 0, 1
    jmp .tok
.sep:
    add rsi, 2
    jmp .tok
    ; folders and extensions of the last scan (not while a scan is still filling the entry vector)
.scan:
    cmp qword [rbx+F_RUNNING], 0
    jne .presets
    xor r12d, r12d
.e:
    cmp r12, [rbx+F_NENT]
    jae .presets
    mov rax, r12
    shl rax, 6
    add rax, [rbx+F_ENTS]
    mov r13, rax
    mov r14, [r13+E_NAME]
    mov eax, [r13+E_NAMEOFF]
    lea r14, [r14+rax*2]             ; leaf name
    invoke wcschr, r14, ';'
    test rax, rax
    jnz .en
    invoke wcschr, r14, ','
    test rax, rax
    jnz .en
    test dword [r13+E_FLAGS], EF_DIRA|EF_DIRB
    jz .file
    mov r15d, [r13+E_SUBEND]
    sub r15d, r12d                   ; the folder itself + everything below it
    invoke xd_add, r14, 1, r15, 0
    jmp .en
.file:
    invoke wcsrchr, r14, '.'
    test rax, rax
    jz .en
    cmp rax, r14
    je .en
    cmp word [rax+2], 0
    je .en
    mov r15, rax
    invoke wcslen, r15
    cmp rax, 24
    ja .en
    invoke _snwprintf, &rsp+96, 60, &f_xd_star, r15
    invoke xd_add, &rsp+96, 2, 1, 0
.en:
    inc r12
    jmp .e
.presets:
    xor r12d, r12d
.p:
    lea rax, [xd_presets]
    mov rcx, [rax+r12*8]
    invoke xd_add, rcx, 3, 0, 0
    inc r12d
    cmp r12d, XD_NPRESET
    jb .p
    mov rax, [g_xd_items+8]
    cmp rax, 1
    jbe .d
    invoke qsort, [g_xd_items], rax, XI_SIZE, &xd_cmp
.d:
    EPILOG 2320

; xd_free()
xd_free:
    PROLOG 64
    xor ebx, ebx
.l:
    cmp rbx, [g_xd_items+8]
    jae .d
    imul rax, rbx, XI_SIZE
    add rax, [g_xd_items]
    invoke free, [rax+XI_TEXT]
    inc rbx
    jmp .l
.d:
    mov qword [g_xd_items+8], 0
    EPILOG 64

; xd_setcheck(hlist, row or -1 for all, checked 0/1)
xd_setcheck:
    PROLOG 192
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    invoke memset, &rsp+96, 0, 88
    mov eax, 0x1000
    test rdi, rdi
    jz .st
    mov eax, 0x2000
.st:
    mov [rsp+96+12], eax             ; state image 1 = unchecked, 2 = checked
    mov dword [rsp+96+16], 0xF000
    invoke SendMessageW, rbx, LVM_SETITEMSTATE, rsi, &rsp+96
    EPILOG 192

; xd_insert(hlist, row): add the row for item[row]
xd_insert:
    PROLOG 256
    mov rbx, rcx
    mov r12, rdx
    imul r13, r12, XI_SIZE
    add r13, [g_xd_items]
    invoke memset, &rsp+96, 0, 88
    mov dword [rsp+96], LVIF_TEXT
    mov [rsp+96+4], r12d
    mov rax, [r13+XI_TEXT]
    mov [rsp+96+24], rax
    invoke SendMessageW, rbx, LVM_INSERTITEMW, 0, &rsp+96
    invoke memset, &rsp+96, 0, 88
    mov dword [rsp+96+8], 1
    mov eax, [r13+XI_KIND]
    lea rcx, [xd_kinds]
    mov rax, [rcx+rax*8]
    mov [rsp+96+24], rax
    invoke SendMessageW, rbx, LVM_SETITEMTEXTW, r12, &rsp+96
    cmp dword [r13+XI_COUNT], 0
    je .chk
    mov eax, [r13+XI_COUNT]
    invoke fmt_size, &rsp+192, rax
    invoke memset, &rsp+96, 0, 88
    mov dword [rsp+96+8], 2
    lea rax, [rsp+192]
    mov [rsp+96+24], rax
    invoke SendMessageW, rbx, LVM_SETITEMTEXTW, r12, &rsp+96
.chk:
    mov r8d, [r13+XI_CHK]
    invoke xd_setcheck, rbx, r12, r8
    EPILOG 256

; xd_collect(hwnd) -> eax 1 ok (g_xd_out filled) / 0 the checked patterns do not fit
xd_collect:
    PROLOG 640
    mov rsi, rcx
    invoke GetDlgItem, rsi, XD_LIST
    mov rbx, rax
    lea rdi, [rsp+96]                ; wchar[256]
    mov word [rdi], 0
    xor r12d, r12d
    xor r14d, r14d                   ; overflow
.l:
    cmp r12, [g_xd_items+8]
    jae .done
    invoke SendMessageW, rbx, LVM_GETITEMSTATE, r12, 0xF000
    shr eax, 12
    cmp eax, 2
    jne .n
    imul r13, r12, XI_SIZE
    add r13, [g_xd_items]
    invoke excl_add, rdi, [r13+XI_TEXT]
    test eax, eax
    jnz .n
    mov r14d, 1
.n:
    inc r12
    jmp .l
.done:
    test r14d, r14d
    jnz .long
    invoke wcscpy, [g_xd_out], rdi
    mov eax, 1
    EPILOG 640
.long:
    invoke msgbox, rsi, &s_x_toolong, MB_OK|MB_ICONWARNING
    xor eax, eax
    EPILOG 640

; xd_typed(hwnd): add (or just check) the pattern typed in the edit box
xd_typed:
    PROLOG 704
    mov rsi, rcx
    invoke GetDlgItem, rsi, XD_EDIT
    mov r12, rax
    invoke GetWindowTextW, r12, &rsp+96, 250
    ; trim blanks on both sides
    lea rdi, [rsp+96]
.lt:
    cmp word [rdi], ' '
    jne .rt
    add rdi, 2
    jmp .lt
.rt:
    invoke wcslen, rdi
.rtl:
    test rax, rax
    jz .d
    cmp word [rdi+rax*2-2], ' '
    jne .have
    dec rax
    mov word [rdi+rax*2], 0
    jmp .rtl
.have:
    invoke wcschr, rdi, ';'
    test rax, rax
    jnz .bad
    invoke wcschr, rdi, ','
    test rax, rax
    jnz .bad
    invoke GetDlgItem, rsi, XD_LIST
    mov rbx, rax
    invoke xd_find, rdi
    test rax, rax
    jnz .exists
    mov r13, [g_xd_items+8]          ; row of the new item
    invoke xd_add, rdi, 4, 0, 1
    test rax, rax
    jz .d
    invoke xd_insert, rbx, r13
    jmp .show
.exists:
    sub rax, [g_xd_items]
    xor edx, edx
    mov ecx, XI_SIZE
    div rcx
    mov r13, rax
    invoke xd_setcheck, rbx, r13, 1
.show:
    invoke SendMessageW, rbx, LVM_ENSUREVISIBLE, r13, 0
    invoke SetWindowTextW, r12, &s_empty
.d:
    EPILOG 704
.bad:
    invoke msgbox, rsi, &s_x_badpat, MB_OK|MB_ICONWARNING
    EPILOG 704

; xd_layout(hwnd)
xd_layout:
    PROLOG 192
    mov rsi, rcx
    invoke GetClientRect, rsi, &rsp+96
    mov r12d, [rsp+104]              ; width
    mov r13d, [rsp+108]              ; height
    mov ecx, 8
    call scl
    mov r14d, eax                    ; margin
    mov ecx, 28
    call scl
    mov r15d, eax                    ; row height
    ; hint (two text lines)
    mov ebx, r12d
    sub ebx, r14d
    sub ebx, r14d
    lea ebp, [r15+r15]
    MVID rsi, XD_HINT, r14, r14, rbx, rbp
    ; list
    lea edi, [rbp+r14]
    add edi, r14d                    ; list top
    mov eax, r13d
    sub eax, edi
    sub eax, r15d
    sub eax, r15d
    sub eax, r14d
    sub eax, r14d
    sub eax, r14d
    jns .lh
    xor eax, eax
.lh:
    mov ebp, eax                     ; list height
    MVID rsi, XD_LIST, r14, rdi, rbx, rbp
    ; typed pattern row
    add edi, ebp
    add edi, r14d
    mov ecx, 110
    call scl
    mov ebp, eax                     ; button width
    mov ebx, r12d
    sub ebx, r14d
    sub ebx, ebp                     ; x of the add button
    MVID rsi, XD_ADD, rbx, rdi, rbp, r15
    sub ebx, r14d
    sub ebx, r14d                    ; edit width
    jns .ew
    xor ebx, ebx
.ew:
    MVID rsi, XD_EDIT, r14, rdi, rbx, r15
    ; bottom row
    add edi, r15d
    add edi, r14d
    MVID rsi, XD_CLEAR, r14, rdi, rbp, r15
    mov ebx, r12d
    sub ebx, r14d
    sub ebx, ebp
    MVID rsi, XD_CANCEL, rbx, rdi, rbp, r15
    sub ebx, ebp
    sub ebx, r14d
    MVID rsi, XD_OK, rbx, rdi, rbp, r15
    EPILOG 192

xd_proc:
    PROLOG 320
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
    cmp edi, WM_CLOSE
    je .close
    cmp edi, WM_ERASEBKGND
    je .erase
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLOREDIT
    je .cedit
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 320
.create:
    invoke mk_lbl, rsi, &s_xd_hint, XD_HINT
    invoke mk, rsi, &cn_list, 0, LVS_REPORT|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER, XD_LIST
    mov rbx, rax
    invoke SendMessageW, rbx, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_CHECKBOXES
    invoke lv_dark_header, rbx
    xor ebp, ebp
.col:
    cmp ebp, 3
    jae .cd
    invoke memset, &rsp+96, 0, 40
    mov dword [rsp+96], LVCF_FMT|LVCF_WIDTH|LVCF_TEXT
    lea rax, [h_xd_pat]
    mov ecx, 330
    cmp ebp, 0
    je .ct
    lea rax, [h_xd_kind]
    mov ecx, 130
    cmp ebp, 1
    je .ct
    lea rax, [h_xd_cnt]
    mov ecx, 90
    mov dword [rsp+100], 1           ; right aligned
.ct:
    mov [rsp+112], rax
    call scl
    mov [rsp+104], eax
    invoke SendMessageW, rbx, LVM_INSERTCOLUMNW, rbp, &rsp+96
    inc ebp
    jmp .col
.cd:
    invoke SendMessageW, rbx, LVM_SETBKCOLOR, 0, #[th_bg]
    invoke SendMessageW, rbx, LVM_SETTEXTCOLOR, 0, #[th_fg]
    invoke SendMessageW, rbx, LVM_SETTEXTBKCOLOR, 0, #[th_bg]
    lea rdx, [s_light_theme]
    cmp qword [g_dark], 0
    je .th
    lea rdx, [s_dark_theme]
.th:
    invoke SetWindowTheme, rbx, rdx, 0
    invoke SendMessageW, rbx, WM_SETREDRAW, 0, 0
    xor r14d, r14d
.fill:
    cmp r14, [g_xd_items+8]
    jae .filled
    invoke xd_insert, rbx, r14
    inc r14
    jmp .fill
.filled:
    invoke SendMessageW, rbx, WM_SETREDRAW, 1, 0
    invoke mk_edit, rsi, XD_EDIT
    invoke mk_btn, rsi, &s_xd_add, XD_ADD
    invoke mk_btn, rsi, &s_xd_clear, XD_CLEAR
    invoke mk_btn, rsi, &s_xd_ok, XD_OK
    invoke mk_btn, rsi, &s_xd_cancel, XD_CANCEL
    invoke xd_layout, rsi
    xor eax, eax
    EPILOG 320
.size:
    invoke xd_layout, rsi
    jmp .zero
.command:
    movzx eax, r12w
    cmp eax, XD_OK
    je .ok
    cmp eax, XD_CANCEL
    je .close
    cmp eax, XD_ADD
    je .add
    cmp eax, XD_CLEAR
    jne .zero
    invoke GetDlgItem, rsi, XD_LIST
    invoke xd_setcheck, rax, -1, 0
    jmp .zero
.add:
    invoke xd_typed, rsi
    jmp .zero
.ok:
    invoke xd_collect, rsi
    test eax, eax
    jz .zero
    mov qword [g_xd_result], 1
    jmp .zero
.close:
    mov qword [g_xd_result], 0
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

; excl_dialog(fold, list wchar[256] in/out) -> eax 1 when confirmed
excl_dialog:
    PROLOG 256
    mov rsi, rcx
    mov [g_xd_out], rdx
    mov rbx, [g_hframe]
    cmp qword [g_xd_wc], 0
    jne .have
    invoke reg_class, &cn_xd, &xd_proc, [th_panelbrush]
    mov qword [g_xd_wc], 1
.have:
    invoke xd_build, rsi
    mov ecx, 640
    call scl
    mov r12d, eax                    ; width
    mov ecx, 600
    call scl
    mov r14d, eax                    ; height
    invoke GetWindowRect, rbx, &rsp+96
    mov eax, [rsp+104]
    sub eax, [rsp+96]
    sub eax, r12d
    sar eax, 1
    add eax, [rsp+96]
    mov r13d, eax                    ; x
    mov eax, [rsp+108]
    sub eax, [rsp+100]
    sub eax, r14d
    sar eax, 1
    add eax, [rsp+100]
    mov r15d, eax                    ; y
    mov qword [g_xd_result], -1
    invoke CreateWindowExW, 1, &cn_xd, &s_xd_title, WS_POPUP|WS_CAPTION|WS_SYSMENU|WS_THICKFRAME|WS_VISIBLE|WS_CLIPCHILDREN, r13, r15, r12, r14, rbx, 0, [g_hinst], 0
    mov rdi, rax
    test rax, rax
    jz .fail
    invoke EnableWindow, rbx, 0
.loop:
    cmp qword [g_xd_result], -1
    jne .done
    invoke GetMessageW, &rsp+96, 0, 0, 0
    test eax, eax
    jle .quit
    invoke TranslateMessage, &rsp+96
    invoke DispatchMessageW, &rsp+96
    jmp .loop
.quit:
    mov qword [g_xd_result], 0
    invoke PostQuitMessage, 0
.done:
    invoke EnableWindow, rbx, 1
    invoke DestroyWindow, rdi
    invoke SetFocus, rbx
    invoke xd_free
    mov rax, [g_xd_result]
    EPILOG 256
.fail:
    invoke xd_free
    xor eax, eax
    EPILOG 256

; excl_apply(hwnd, fold, list): put the list into the exclude field and compare again
excl_apply:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, r8
    invoke GetDlgItem, rsi, ID_EDIT_EXCL
    invoke SetWindowTextW, rax, rdi
    cmp qword [rbx+F_RUNNING], 0
    jne .run
    cmp qword [rbx+F_NENT], 0
    jne .run
    invoke GetDlgItem, rsi, ID_EDIT_A  ; nothing compared yet: only start when both paths are filled in
    invoke GetWindowTextLengthW, rax
    test eax, eax
    jz .d
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextLengthW, rax
    test eax, eax
    jz .d
.run:
    invoke fold_start, rsi, rbx
.d:
    EPILOG 64

; fold_excl_dialog(hwnd, fold): the "제외 선택…" button
fold_excl_dialog:
    PROLOG 640
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+F_SYNCRUN], 0
    jne .busy
    invoke GetDlgItem, rsi, ID_EDIT_EXCL
    invoke GetWindowTextW, rax, &rsp+96, EXCL_MAX
    invoke excl_dialog, rbx, &rsp+96
    test eax, eax
    jz .d
    invoke excl_apply, rsi, rbx, &rsp+96
.d:
    EPILOG 640
.busy:
    invoke msgbox, rsi, &s_busy, MB_OK|MB_ICONWARNING
    EPILOG 640

; fold_ctx(hwnd, fold): right-click menu of the result list
%define CX_BUF  96                   ; wchar[256] working copy of the exclude list
%define CX_PAT  640                  ; wchar[1100] one pattern
%define CX_LBL  2848                 ; wchar[300] menu text
%define CX_PT   3456                 ; POINT
fold_ctx:
    PROLOG 3488
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+F_SYNCRUN], 0
    jne .d
    call CreatePopupMenu
    mov rdi, rax
    cmp qword [rbx+F_RUNNING], 0
    jne .editonly
    invoke SendMessageW, [rbx+F_HLIST], LVM_GETSELECTEDCOUNT, 0, 0
    mov r15d, eax                    ; selected rows
    invoke SendMessageW, [rbx+F_HLIST], LVM_GETNEXTITEM, -1, LVNI_SELECTED
    movsxd r12, eax
    test r12, r12
    js .editonly
    cmp r12, [rbx+F_NVIS]
    jae .editonly
    invoke fold_ent, rbx, r12
    mov r13, rax
    mov r14, [r13+E_NAME]
    mov eax, [r13+E_NAMEOFF]
    lea r14, [r14+rax*2]             ; leaf name
    cmp r15d, 1
    ja .multi
    invoke _snwprintf, &rsp+CX_LBL, 290, &f_cx_name, r14
    mov word [rsp+CX_LBL+580], 0
    invoke AppendMenuW, rdi, MF_STRING, 1, &rsp+CX_LBL
    test dword [r13+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .mpath
    invoke wcsrchr, r14, '.'
    test rax, rax
    jz .mpath
    cmp rax, r14
    je .mpath
    cmp word [rax+2], 0
    je .mpath
    invoke _snwprintf, &rsp+CX_LBL, 290, &f_cx_ext, rax
    mov word [rsp+CX_LBL+580], 0
    invoke AppendMenuW, rdi, MF_STRING, 2, &rsp+CX_LBL
.mpath:
    invoke _snwprintf, &rsp+CX_LBL, 290, &f_cx_path, [r13+E_NAME]
    mov word [rsp+CX_LBL+580], 0
    invoke AppendMenuW, rdi, MF_STRING, 3, &rsp+CX_LBL
    jmp .msep
.multi:
    invoke _snwprintf, &rsp+CX_LBL, 290, &f_cx_names, r15
    invoke AppendMenuW, rdi, MF_STRING, 1, &rsp+CX_LBL
    invoke AppendMenuW, rdi, MF_STRING, 2, &s_cx_exts
    invoke _snwprintf, &rsp+CX_LBL, 290, &f_cx_paths, r15
    invoke AppendMenuW, rdi, MF_STRING, 3, &rsp+CX_LBL
.msep:
    invoke AppendMenuW, rdi, MF_SEPARATOR, 0, 0
.editonly:
    invoke AppendMenuW, rdi, MF_STRING, 4, &s_cx_edit
    invoke GetCursorPos, &rsp+CX_PT
    mov r14d, [rsp+CX_PT]
    mov r15d, [rsp+CX_PT+4]
    invoke TrackPopupMenu, rdi, TPM_RETURNCMD|TPM_RIGHTBUTTON, r14, r15, 0, rsi, 0
    mov ebp, eax
    invoke DestroyMenu, rdi
    test ebp, ebp
    jz .d
    cmp ebp, 4
    je .dlg
    ; apply the chosen kind of pattern to every selected row
    invoke GetDlgItem, rsi, ID_EDIT_EXCL
    invoke GetWindowTextW, rax, &rsp+CX_BUF, EXCL_MAX
    xor r15d, r15d                   ; something did not fit
    mov r12, -1
.n:
    invoke SendMessageW, [rbx+F_HLIST], LVM_GETNEXTITEM, r12, LVNI_SELECTED
    movsxd r12, eax
    test r12, r12
    js .apply
    cmp r12, [rbx+F_NVIS]
    jae .apply
    invoke fold_ent, rbx, r12
    mov r13, rax
    mov r14, [r13+E_NAME]
    mov eax, [r13+E_NAMEOFF]
    lea r14, [r14+rax*2]
    cmp ebp, 2
    je .pext
    cmp ebp, 3
    je .ppath
    invoke _snwprintf, &rsp+CX_PAT, 1090, &f_str, r14
    jmp .padd
.pext:
    test dword [r13+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .n
    invoke wcsrchr, r14, '.'
    test rax, rax
    jz .n
    cmp rax, r14
    je .n
    cmp word [rax+2], 0
    je .n
    invoke _snwprintf, &rsp+CX_PAT, 1090, &f_xd_star, rax
    jmp .padd
.ppath:
    invoke _snwprintf, &rsp+CX_PAT, 1090, &f_xd_root, [r13+E_NAME]
.padd:
    mov word [rsp+CX_PAT+2180], 0
    invoke excl_add, &rsp+CX_BUF, &rsp+CX_PAT
    test eax, eax
    jnz .n
    mov r15d, 1
    jmp .n
.apply:
    test r15d, r15d
    jz .set
    invoke msgbox, rsi, &s_x_partial, MB_OK|MB_ICONWARNING
.set:
    invoke excl_apply, rsi, rbx, &rsp+CX_BUF
.d:
    EPILOG 3488
.dlg:
    invoke fold_excl_dialog, rsi, rbx
    EPILOG 3488
