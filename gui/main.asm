; main.asm - entry point, frame window, tabs (sessions), menu, message loop
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern SetTimer, KillTimer, EnableWindow
extern OleInitialize, InitCommonControlsEx, LoadLibraryW, GetModuleHandleW
extern CreateWindowExW, ShowWindow, UpdateWindow, GetMessageW, TranslateMessage, DispatchMessageW, LoadImageW
extern DefWindowProcW, PostQuitMessage, DestroyWindow, SendMessageW, PostMessageW
extern SetWindowPos, MoveWindow, GetClientRect, GetWindowRect, SetFocus, GetKeyState, GetDlgCtrlID
extern CreateMenu, CreatePopupMenu, AppendMenuW, SetMenu, CheckMenuItem, DrawMenuBar
extern FillRect, DragAcceptFiles, DragQueryFileW, DragFinish, GetDlgItem, SetWindowTextW
extern InvalidateRect, RedrawWindow, GetWindowTextW, GetFileAttributesW
extern GetCommandLineW, CommandLineToArgvW, LocalFree
extern DwmSetWindowAttribute
extern GetCursorPos, ScreenToClient, TrackPopupMenu, DestroyMenu
extern calloc, free, wcslen, wcscpy, wcsncpy, wcscmp, _wcsicmp, wcsrchr, _snwprintf, wcscat
extern WritePrivateProfileStringW, GetPrivateProfileStringW, GetPrivateProfileIntW
extern CreateFileW, WriteFile, CloseHandle, DeleteFileW
extern g_hinst, g_hframe, g_htab, g_hstatus, g_font, g_dpi, g_dark
extern th_panelbrush
extern mk, reg_class, scl, set_status, msgbox, browse_open_as, browse_save_as
extern theme_set, fonts_init, s_appname

; supplied by other modules
extern home_register, home_create, ini_path_init, settings_load, recent_add, g_ini
extern folder_register, folder_create
extern text_register, text_create
extern table_register, table_create
extern hex_register, hex_create
extern pic_register, pic_create
extern merge_register, merge_create
extern props_register, props_create, edit_create
extern fmerge_register, fmerge_create
extern script_check, shell_early, shell_pending, shell_install
extern settings_show, policy_load, keys_load, g_keytab, update_check, update_start, clip_compare, patch_open, patch_cmd

global WinMain
global add_session, sess_new, sess_open, set_tab_title, type_for_path, cur_view
global tab_titles
global g_sess, g_nsess, ini_prepare, apply_theme_all, type_names, open_any_file, g_batch, g_rep_path, g_exitcode, g_save_path, g_savews_path, g_snap_path, g_xl_path, g_dx_path

section .rdata
WSTR s_unsaved_close, "저장하지 않은 변경 내용이 있습니다. 저장하시겠습니까? (예: 저장, 아니요: 버리기, 취소: 계속 편집)"
WSTR cn_frame, "BCFrame"
WSTR cn_tab, "SysTabControl32"
WSTR cn_status, "msctls_statusbar32"
WSTR s_empty, ""
WSTR s_msftedit, "msftedit.dll"
WSTR s_ready, "준비"

WSTR m_file, "파일(&F)"
WSTR m_view, "보기(&V)"
WSTR m_sess, "세션(&S)"
WSTR m_help, "도움말(&H)"
WSTR m_nf, "새 폴더 비교"
WSTR m_nt, "새 텍스트 비교"
WSTR m_nb, "새 표 비교"
WSTR m_nh, "새 헥스 비교"
WSTR m_np, "새 이미지 비교"
WSTR m_nm, "새 텍스트 병합 (Pro)"
WSTR m_nv, "새 버전 비교"
WSTR m_nr, "새 레지스트리 비교"
WSTR m_nmd, "새 미디어 비교"
WSTR m_ned, "새 텍스트 편집"
WSTR m_set, "설정(&S)..."
WSTR m_shon, "탐색기 오른쪽 클릭 메뉴 등록"
WSTR m_shoff, "탐색기 오른쪽 클릭 메뉴 해제"
WSTR s_shell_on, "탐색기 오른쪽 클릭 메뉴에 등록했습니다 (현재 사용자). 항목: 왼쪽으로 선택 / 선택한 항목과 비교"
WSTR s_shell_off, "탐색기 오른쪽 클릭 메뉴 등록을 해제했습니다."
WSTR sw_shellcmp, "/shellcompare"
WSTR sw_patch, "patch"
WSTR m_nfm, "새 폴더 병합 (3-way)"
WSTR m_os, "세션 열기..."
WSTR m_ss, "세션 저장..."
WSTR m_ow, "워크스페이스 열기..."
WSTR m_sw, "워크스페이스 저장..."
WSTR m_ct, `탭 닫기\tCtrl+W`
WSTR m_ex, "종료"
WSTR tm_close, "닫기"
WSTR tm_others, "다른 탭 닫기"
WSTR tm_all, "모든 탭 닫기"
WSTR tm_unmod, "수정되지 않은 탭 닫기"
WSTR tm_right, "오른쪽 탭 닫기"
WSTR m_li, "라이트 모드"
WSTR m_dk, "다크 모드"
WSTR m_rn, `비교 실행\tF5`
WSTR m_cn, `취소\tEsc`
WSTR m_sv, `저장\tCtrl+S`
WSTR m_upd, "업데이트 확인..."
WSTR m_clip, "클립보드와 텍스트 비교"
WSTR m_patch, "패치 파일(diff) 보기..."
WSTR m_ab, "정보"
WSTR s_about, `BeCompare\nx64 NASM / Win32 GUI\n\nPRD(파일·폴더 비교 도구 기능 정리)를 기반으로 한 구현입니다.`

WSTR tt_home, "홈"
WSTR tt_folder, "폴더 비교"
WSTR tt_text, "텍스트 비교"
WSTR tt_table, "표 비교"
WSTR tt_hex, "헥스 비교"
WSTR tt_pic, "이미지 비교"
WSTR tt_merge, "텍스트 병합"
WSTR tt_ver, "버전 비교"
WSTR tt_reg, "레지스트리 비교"
WSTR tt_media, "미디어 비교"
WSTR tt_edit, "텍스트 편집"
WSTR tt_fmerge, "폴더 병합"

WSTR tn_home, "home"
WSTR tn_folder, "folder"
WSTR tn_text, "text"
WSTR tn_table, "table"
WSTR tn_hex, "hex"
WSTR tn_pic, "picture"
WSTR tn_merge, "merge"
WSTR tn_ver, "version"
WSTR tn_reg, "registry"
WSTR tn_media, "media"
WSTR tn_edit, "edit"
WSTR tn_fmerge, "foldermerge"
; command-line aliases
WSTR al_dir, "dir"
WSTR al_csv, "csv"
WSTR al_pic, "pic"
WSTR al_image, "image"
WSTR al_diff, "diff"

WSTR k_type, "Type"
WSTR k_a, "A"
WSTR k_b, "B"
WSTR k_c, "C"
WSTR k_out, "Out"
WSTR k_opts, "Opts"
WSTR k_count, "Count"
WSTR k_active, "Active"
WSTR s_de_bcs, "bcs"
WSTR s_de_bcw, "bcw"
flt_open_sess: dw __utf16__("세션 (*.bcs)"), 0, __utf16__("*.bcs"), 0, __utf16__("워크스페이스 (*.bcw)"), 0, __utf16__("*.bcw"), 0, __utf16__("모든 파일 (*.*)"), 0, __utf16__("*.*"), 0, 0
flt_open_ws:   dw __utf16__("워크스페이스 (*.bcw)"), 0, __utf16__("*.bcw"), 0, __utf16__("세션 (*.bcs)"), 0, __utf16__("*.bcs"), 0, __utf16__("모든 파일 (*.*)"), 0, __utf16__("*.*"), 0, 0
flt_save_sess: dw __utf16__("세션 (*.bcs)"), 0, __utf16__("*.bcs"), 0, __utf16__("모든 파일 (*.*)"), 0, __utf16__("*.*"), 0, 0
flt_save_ws:   dw __utf16__("워크스페이스 (*.bcw)"), 0, __utf16__("*.bcw"), 0, __utf16__("모든 파일 (*.*)"), 0, __utf16__("*.*"), 0, 0
WSTR s_home_nosave, "홈 탭은 세션으로 저장할 수 없습니다. 저장할 비교 탭을 선택한 뒤 다시 시도하세요."
WSTR s_ws_empty, "저장할 탭이 없습니다. 비교 탭을 하나 이상 연 뒤 다시 시도하세요."
WSTR f_open_fail, `열 수 없습니다:\n%s\n\n세션(.bcs) 또는 워크스페이스(.bcw) 파일이 아니거나, 탭을 더 열 수 없습니다.`
WSTR f_save_fail, `저장하지 못했습니다:\n%s\n\n이 위치에 쓸 수 있는지 확인하세요.`
WSTR sw_report, "/report"
WSTR sw_sync, "/sync"
WSTR sw_exit, "/exit"
WSTR sw_save, "/save"
WSTR sw_snap, "/snapshot"
WSTR sw_xlsx, "/xlsx"
WSTR sw_docx, "/docx"
WSTR sw_savews, "/savews"
WSTR sw_mirror, "mirror"
WSTR sw_twoway, "twoway"
WSTR sec_session, "Session"
WSTR sec_ws, "Workspace"
WSTR sec_ui, "UI"
WSTR k_dark, "Dark"
WSTR f_tab, "Tab%d"
WSTR f_int, "%d"
WSTR f_closable, "%s  ×"
WSTR x_bcs, ".bcs"
WSTR x_bcw, ".bcw"
WSTR e_png, ".png"
WSTR e_jpg, ".jpg"
WSTR e_jpeg, ".jpeg"
WSTR e_bmp, ".bmp"
WSTR e_gif, ".gif"
WSTR e_tif, ".tif"
WSTR e_csv, ".csv"
WSTR e_tsv, ".tsv"
WSTR e_xlsx, ".xlsx"
WSTR e_xlsm, ".xlsm"
WSTR e_exe, ".exe"
WSTR e_dll, ".dll"
WSTR e_bin, ".bin"
WSTR e_dat, ".dat"
WSTR e_obj, ".obj"
WSTR e_lib, ".lib"
WSTR e_zip, ".zip"
WSTR e_pdb, ".pdb"
WSTR e_mp3, ".mp3"
WSTR e_flac, ".flac"

align 8
type_names: dq tn_home, tn_folder, tn_text, tn_table, tn_hex, tn_pic, tn_merge, tn_ver, tn_reg, tn_media, tn_edit, tn_fmerge
tab_titles: dq tt_home, tt_folder, tt_text, tt_table, tt_hex, tt_pic, tt_merge, tt_ver, tt_reg, tt_media, tt_edit, tt_fmerge
pic_exts:   dq e_png, e_jpg, e_jpeg, e_bmp, e_gif, e_tif, 0
tbl_exts:   dq e_csv, e_tsv, e_xlsx, e_xlsm, 0
media_exts: dq e_mp3, e_flac, 0
hex_exts:   dq e_exe, e_dll, e_bin, e_dat, e_obj, e_lib, e_zip, e_pdb, 0

section .bss
alignb 8
g_close_tab: resq 1
g_close_all: resq 1
g_bulk_mode: resq 1                  ; BULK_* while a "close several tabs" command runs
g_bulk_ref:  resq 1                  ; the session the tab menu was opened on
g_nsess:    resq 1
g_sess:     resq MAX_SESS
g_batch:    resq 1
g_exitcode: resq 1
g_rep_path: resw 520
g_save_path: resw 520
g_snap_path: resw 520
g_xl_path: resw 520
g_dx_path: resw 520
g_savews_path: resw 520

section .text

; tab context menu commands (TrackPopupMenu return values); TM_CLOSE+n is BULK mode n
%define TM_CLOSE     1
%define BULK_OTHERS  1
%define BULK_ALL     2
%define BULK_UNMOD   3
%define BULK_RIGHT   4

; ---------------------------------------------------------------- WinMain
WinMain:
    PROLOG 256
    mov [g_hinst], rcx
    call script_check
    test eax, eax
    jz .noscript
    mov rax, [g_exitcode]
    EPILOG 256
.noscript:
    call shell_early
    test eax, eax
    jz .noshell
    xor eax, eax
    EPILOG 256
.noshell:
    invoke OleInitialize, 0
    lea rbx, [rsp+96]
    mov dword [rbx], 8
    mov dword [rbx+4], 0xFF
    invoke InitCommonControlsEx, rbx
    invoke LoadLibraryW, &s_msftedit
    call ini_path_init
    call policy_load
    call keys_load
    call fonts_init
    call settings_load
    invoke theme_set, [g_dark]

    invoke reg_class, &cn_frame, &frame_proc, 0
    call home_register
    call folder_register
    call text_register
    call table_register
    call hex_register
    call pic_register
    call merge_register
    call props_register
    call fmerge_register

    mov ecx, 1280
    call scl
    mov r12d, eax
    mov ecx, 800
    call scl
    mov r13d, eax
    invoke CreateWindowExW, WS_EX_ACCEPTFILES, &cn_frame, &s_appname, WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN, #CW_USEDEFAULT, #CW_USEDEFAULT, r12, r13, 0, 0, [g_hinst], 0
    test rax, rax
    jz .exit
    mov rsi, rax
    invoke LoadImageW, [g_hinst], 1, IMAGE_ICON, 32, 32, LR_SHARED
    invoke SendMessageW, rsi, WM_SETICON, ICON_BIG, rax
    invoke LoadImageW, [g_hinst], 1, IMAGE_ICON, 16, 16, LR_SHARED
    invoke SendMessageW, rsi, WM_SETICON, ICON_SMALL, rax
    call build_menu
    invoke SetMenu, rsi, rax
    invoke theme_apply_title, rsi

    invoke add_session, T_HOME, 0, 0, 0
    call parse_cmdline
    mov edx, SW_SHOWNORMAL
    test qword [g_batch], 4
    jz .showit
    mov edx, 7                        ; SW_SHOWMINNOACTIVE for unattended runs
.showit:
    invoke ShowWindow, rsi, rdx
    invoke UpdateWindow, rsi
    test qword [g_batch], 4
    jnz .loop
    call update_start

.loop:
    lea rbx, [rsp+96]
    invoke GetMessageW, rbx, 0, 0, 0
    test eax, eax
    jle .exit
    mov eax, [rbx+MSG_MSG]
    cmp eax, WM_KEYDOWN
    jne .disp
    invoke pre_key, rbx
    test eax, eax
    jnz .loop
.disp:
    invoke TranslateMessage, rbx
    invoke DispatchMessageW, rbx
    jmp .loop
.exit:
    mov rax, [g_exitcode]
    EPILOG 256

; theme_apply_title(hwnd): dark title bar attribute
theme_apply_title:
    PROLOG 96
    mov rbx, rcx
    mov eax, [g_dark]
    mov [rsp+96-16], eax
    invoke DwmSetWindowAttribute, rbx, 20, &rsp+80, 4
    EPILOG 96

; ---------------------------------------------------------------- menu
build_menu:
    PROLOG 96
    call CreateMenu
    mov r12, rax
    ; File
    call CreatePopupMenu
    mov rbx, rax
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_FOLDER, &m_nf
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_TEXT, &m_nt
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_TABLE, &m_nb
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_HEX, &m_nh
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_PIC, &m_np
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_MERGE, &m_nm
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_VER, &m_nv
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_REG, &m_nr
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_MEDIA, &m_nmd
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_EDIT, &m_ned
    invoke AppendMenuW, rbx, MF_STRING, IDM_NEW_FMERGE, &m_nfm
    invoke AppendMenuW, rbx, MF_STRING, IDM_CLIP, &m_clip
    invoke AppendMenuW, rbx, MF_STRING, IDM_PATCH, &m_patch
    invoke AppendMenuW, rbx, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, rbx, MF_STRING, IDM_OPEN_SESS, &m_os
    invoke AppendMenuW, rbx, MF_STRING, IDM_SAVE_SESS, &m_ss
    invoke AppendMenuW, rbx, MF_STRING, IDM_OPEN_WS, &m_ow
    invoke AppendMenuW, rbx, MF_STRING, IDM_SAVE_WS, &m_sw
    invoke AppendMenuW, rbx, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, rbx, MF_STRING, IDM_CLOSE_TAB, &m_ct
    invoke AppendMenuW, rbx, MF_STRING, IDM_EXIT, &m_ex
    invoke AppendMenuW, r12, MF_POPUP, rbx, &m_file
    ; Session
    call CreatePopupMenu
    mov rbx, rax
    invoke AppendMenuW, rbx, MF_STRING, IDM_RUN, &m_rn
    invoke AppendMenuW, rbx, MF_STRING, IDM_CANCEL, &m_cn
    invoke AppendMenuW, rbx, MF_STRING, IDM_SAVE, &m_sv
    invoke AppendMenuW, r12, MF_POPUP, rbx, &m_sess
    ; View
    call CreatePopupMenu
    mov rbx, rax
    invoke AppendMenuW, rbx, MF_STRING, IDM_LIGHT, &m_li
    invoke AppendMenuW, rbx, MF_STRING, IDM_DARK, &m_dk
    invoke AppendMenuW, rbx, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, rbx, MF_STRING, IDM_SETTINGS, &m_set
    invoke AppendMenuW, rbx, MF_STRING, IDM_SHELL_ON, &m_shon
    invoke AppendMenuW, rbx, MF_STRING, IDM_SHELL_OFF, &m_shoff
    invoke AppendMenuW, r12, MF_POPUP, rbx, &m_view
    ; Help
    call CreatePopupMenu
    mov rbx, rax
    invoke AppendMenuW, rbx, MF_STRING, IDM_UPDATE, &m_upd
    invoke AppendMenuW, rbx, MF_STRING, IDM_ABOUT, &m_ab
    invoke AppendMenuW, r12, MF_POPUP, rbx, &m_help
    mov rax, r12
    EPILOG 96

; ---------------------------------------------------------------- key pre-processing
; pre_key(msg*) -> eax = 1 when handled
pre_key:
    PROLOG 96
    mov rsi, rcx
    mov rdi, [rsi+MSG_WPARAM]
    invoke GetKeyState, VK_CONTROL
    xor r12d, r12d
    test ax, ax
    jns .noctl
    mov r12d, 1
.noctl:
    invoke GetKeyState, VK_SHIFT
    mov ebp, r12d
    test ax, ax
    jns .noshift
    or ebp, 2
.noshift:
    lea r8, [g_keytab]
    xor ebx, ebx
.kl:
    cmp ebx, 9
    jae .fixed
    cmp [r8+rbx*8], edi
    jne .kn
    cmp [r8+rbx*8+4], ebp
    je .khit
.kn:
    inc ebx
    jmp .kl
.khit:
    cmp ebx, 6
    je .bmtoggle
    cmp ebx, 7
    je .bmnext
    cmp ebx, 8
    je .bmprev
    cmp ebx, 0
    je .run
    cmp ebx, 1
    je .save
    cmp ebx, 2
    je .closetab
    cmp ebx, 3
    je .nexttab
    cmp ebx, 4
    je .nextdiff
    jmp .prevdiff
.fixed:
    cmp edi, 0x76                    ; VK_F7 / Shift+F7: next / previous difference (IntelliJ)
    jne .nof7
    test ebp, 1
    jnz .nof7
    test ebp, 2
    jnz .prevdiff
    jmp .nextdiff
.nof7:
    cmp edi, VK_ESCAPE
    je .esc
    cmp edi, VK_RETURN
    je .ret
.no:
    xor eax, eax
    EPILOG 96
.run:
    mov r13d, BC_RUN
    mov r14d, 1
    jmp .dosend
.esc:
    mov r13d, BC_CANCEL
    xor r14d, r14d
    jmp .dosend
.save:
    mov r13d, BC_SAVE
    mov r14d, 1
    jmp .dosend
.bmtoggle:
    mov r13d, BC_BOOKMARK
    jmp .markmsg
.bmnext:
    mov r13d, BC_NEXTMARK
    jmp .markmsg
.bmprev:
    mov r13d, BC_PREVMARK
.markmsg:
    call cur_view
    test rax, rax
    jz .no
    invoke SendMessageW, rax, r13, 0, 0
    test rax, rax
    jz .no
    mov eax, 1
    EPILOG 96
.nextdiff:
    mov r13d, BC_NEXTDIFF
    mov r14d, 1
    jmp .dosend
.prevdiff:
    mov r13d, BC_PREVDIFF
    mov r14d, 1
.dosend:
    call cur_view
    test rax, rax
    jz .dsx
    invoke SendMessageW, rax, r13, 0, 0
.dsx:
    mov eax, r14d
    EPILOG 96
.ret:
    invoke GetDlgCtrlID, [rsi+MSG_HWND]
    cmp eax, ID_EDIT_A
    je .rc
    cmp eax, ID_EDIT_B
    je .rc
    cmp eax, ID_EDIT_C
    je .rc
    jmp .no
.rc:
    call cur_view
    test rax, rax
    jz .no
    invoke SendMessageW, rax, WM_COMMAND, ID_COMPARE, 0
    mov eax, 1
    EPILOG 96
.closetab:
    invoke SendMessageW, [g_hframe], WM_COMMAND, IDM_CLOSE_TAB, 0
    mov eax, 1
    EPILOG 96
.nexttab:
    call cur_index
    inc rax
    cmp rax, [g_nsess]
    jb .nt1
    xor eax, eax
.nt1:
    invoke select_tab, rax
    mov eax, 1
    EPILOG 96

; cur_index -> rax (tab index or 0)
cur_index:
    PROLOG 64
    invoke SendMessageW, [g_htab], TCM_GETCURSEL, 0, 0
    movsxd rax, eax
    test rax, rax
    jns .ok
    xor eax, eax
.ok:
    EPILOG 64

; cur_view -> rax = view hwnd of the active tab (0 if none)
cur_view:
    PROLOG 64
    call cur_index
    cmp rax, [g_nsess]
    jae .none
    lea rcx, [g_sess]
    mov rcx, [rcx+rax*8]
    mov rax, [rcx+SS_HVIEW]
    EPILOG 64
.none:
    xor eax, eax
    EPILOG 64

; ---------------------------------------------------------------- sessions

; sess_new(type, A, B, opts) -> calloc'd session (not yet opened)
sess_new:
    PROLOG 64
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    invoke calloc, 1, SS_SIZE
    mov rbx, rax
    test rax, rax
    jz .done
    mov [rbx+SS_TYPE], r12
    test r13, r13
    jz .nb
    lea rcx, [rbx+SS_PATHA]
    invoke wcsncpy, rcx, r13, 519
.nb:
    test r14, r14
    jz .no
    lea rcx, [rbx+SS_PATHB]
    invoke wcsncpy, rcx, r14, 519
.no:
    test r15, r15
    jz .done
    lea rcx, [rbx+SS_OPTS]
    invoke wcsncpy, rcx, r15, 1023
.done:
    mov rax, rbx
    EPILOG 64

; sess_open(sess) -> sess : creates the view and the tab
sess_open:
    PROLOG 160
    mov rbx, rcx
    mov rax, [g_nsess]
    cmp rax, MAX_SESS
    jae .fail
    mov r12, [rbx+SS_TYPE]
    ; autorun when both paths are filled
    cmp word [rbx+SS_PATHA], 0
    je .na
    cmp word [rbx+SS_PATHB], 0
    je .na
    mov qword [rbx+SS_AUTO], 1
.na:
    cmp r12, T_HOME
    je .mkhome
    cmp r12, T_FOLDER
    je .mkfolder
    cmp r12, T_TEXT
    je .mktext
    cmp r12, T_TABLE
    je .mktable
    cmp r12, T_HEX
    je .mkhex
    cmp r12, T_PIC
    je .mkpic
    cmp r12, T_VER
    jae .mkprops
    invoke merge_create, [g_hframe], rbx
    jmp .made
.mkprops:
    cmp r12, T_EDIT
    jae .mkedit
    invoke props_create, [g_hframe], rbx
    jmp .made
.mkedit:
    cmp r12, T_FMERGE
    jae .mkfm
    invoke edit_create, [g_hframe], rbx
    jmp .made
.mkfm:
    invoke fmerge_create, [g_hframe], rbx
    jmp .made
.mkhome:
    invoke home_create, [g_hframe], rbx
    jmp .made
.mkfolder:
    invoke folder_create, [g_hframe], rbx
    jmp .made
.mktext:
    invoke text_create, [g_hframe], rbx
    jmp .made
.mktable:
    invoke table_create, [g_hframe], rbx
    jmp .made
.mkhex:
    invoke hex_create, [g_hframe], rbx
    jmp .made
.mkpic:
    invoke pic_create, [g_hframe], rbx
.made:
    test rax, rax
    jz .fail
    mov [rbx+SS_HVIEW], rax
    mov r13, [g_nsess]
    lea rcx, [g_sess]
    mov [rcx+r13*8], rbx
    inc qword [g_nsess]
    ; tab item
    lea rdi, [rsp+96]
    invoke memzero40, rdi
    mov dword [rdi+TCI_MASK], TCIF_TEXT
    lea rax, [tab_titles]
    mov rax, [rax+r12*8]
    mov [rdi+TCI_TEXT], rax
    invoke SendMessageW, [g_htab], TCM_INSERTITEMW, r13, rdi
    invoke set_tab_title, rbx, [rdi+TCI_TEXT]        ; adds the close mark
    invoke layout_frame
    invoke select_tab, r13
    cmp qword [rbx+SS_AUTO], 0
    je .ret
    invoke PostMessageW, [rbx+SS_HVIEW], BC_RUN, 0, 0
.ret:
    mov rax, rbx
    EPILOG 160
.fail:
    xor eax, eax
    EPILOG 160

; memzero40(rcx=ptr)
memzero40:
    xor eax, eax
    mov [rcx], rax
    mov [rcx+8], rax
    mov [rcx+16], rax
    mov [rcx+24], rax
    mov [rcx+32], rax
    ret

; add_session(type, A, B, opts) -> sess
add_session:
    PROLOG 64
    call sess_new
    test rax, rax
    jz .d
    invoke sess_open, rax
.d:
    EPILOG 64

; set_tab_title(sess, title)
; Every tab but Home gets a trailing close mark; frame_proc's NM_CLICK closes on it.
set_tab_title:
    PROLOG 704
    mov rbx, rcx
    mov r12, rdx
    xor r13d, r13d
.find:
    cmp r13, [g_nsess]
    jae .done
    lea rax, [g_sess]
    cmp [rax+r13*8], rbx
    je .found
    inc r13
    jmp .find
.found:
    test r13, r13
    jz .set
    lea rdi, [rsp+144]
    invoke _snwprintf, rdi, 259, &f_closable, r12
    mov word [rdi+518], 0
    mov r12, rdi
.set:
    lea rdi, [rsp+96]
    invoke memzero40, rdi
    mov dword [rdi+TCI_MASK], TCIF_TEXT
    mov [rdi+TCI_TEXT], r12
    invoke SendMessageW, [g_htab], TCM_SETITEMW, r13, rdi
.done:
    EPILOG 704

; select_tab(rcx=index)
select_tab:
    PROLOG 64
    mov r12, rcx
    xor ebx, ebx
.l:
    cmp rbx, [g_nsess]
    jae .done
    lea rax, [g_sess]
    mov rax, [rax+rbx*8]
    mov rsi, [rax+SS_HVIEW]
    mov edx, SW_HIDE
    cmp rbx, r12
    jne .sh
    mov edx, SW_SHOW
.sh:
    invoke ShowWindow, rsi, rdx
    inc rbx
    jmp .l
.done:
    invoke SendMessageW, [g_htab], TCM_SETCURSEL, r12, 0
    lea rax, [g_sess]
    mov rax, [rax+r12*8]
    mov rbx, [rax+SS_HVIEW]
    invoke SetWindowPos, rbx, 0, 0, 0, 0, 0, 0x13        ; HWND_TOP, NOSIZE|NOMOVE|NOACTIVATE
    invoke SetFocus, rbx
    invoke set_status, &s_ready
    EPILOG 64

; Return 1 for save/discard, 0 for cancel or failed save.
view_can_close:
    PROLOG 64
    mov rbx, rcx
    invoke SendMessageW, rbx, BC_ISDIRTY, 0, 0
    test rax, rax
    jz .yes
    invoke msgbox, rbx, &s_unsaved_close, 3|MB_ICONQUESTION
    cmp eax, 7                       ; IDNO / discard
    je .yes
    cmp eax, IDYES
    jne .no
    invoke SendMessageW, rbx, BC_SAVE, 0, 0
    invoke SendMessageW, rbx, BC_ISDIRTY, 0, 0
    test rax, rax
    jnz .no
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; close_current_tab
close_current_tab:
    PROLOG 64
    call cur_index
    test rax, rax
    jz .done                         ; the Home tab stays
    mov r12, rax
    lea rcx, [g_sess]
    mov rbx, [rcx+r12*8]
    cmp qword [g_close_tab], rbx
    je .approved
    invoke view_can_close, [rbx+SS_HVIEW]
    test eax, eax
    jz .done
.approved:
    invoke SendMessageW, [rbx+SS_HVIEW], BC_STOPPING, 0, 0
    test rax, rax
    jz .destroy
    mov [g_close_tab], rbx
    invoke EnableWindow, [g_hframe], 0
    invoke SetTimer, [g_hframe], 91, 100, 0
    jmp .done
.destroy:
    mov qword [g_close_tab], 0
    invoke DestroyWindow, [rbx+SS_HVIEW]
    invoke free, rbx
    ; shift array down
    mov rax, r12
.sh:
    lea rcx, [rax+1]
    cmp rcx, [g_nsess]
    jae .shd
    lea rdx, [g_sess]
    mov rsi, [rdx+rcx*8]
    mov [rdx+rax*8], rsi
    mov rax, rcx
    jmp .sh
.shd:
    dec qword [g_nsess]
    invoke SendMessageW, [g_htab], TCM_DELETEITEM, r12, 0
    mov rax, r12
    dec rax
    invoke select_tab, rax
    call layout_frame
.done:
    EPILOG 64

; tab_menu: right-click menu of the tab under the cursor
tab_menu:
    PROLOG 160
    lea rbx, [rsp+96]                ; TCHITTESTINFO (client pt, flags)
    lea rdi, [rsp+120]               ; screen pt
    invoke GetCursorPos, rdi
    mov rax, [rdi]
    mov [rbx], rax
    invoke ScreenToClient, [g_htab], rbx
    invoke SendMessageW, [g_htab], TCM_HITTEST, 0, rbx
    movsxd r14, eax                  ; tab index
    test r14, r14
    js .d
    cmp r14, [g_nsess]
    jae .d
    lea rax, [g_sess]
    mov r15, [rax+r14*8]
    call CreatePopupMenu
    mov r12, rax
    xor r13d, r13d                   ; Close: not for Home
    test r14, r14
    jnz .a1
    mov r13d, MF_GRAYED
.a1:
    invoke AppendMenuW, r12, r13, TM_CLOSE, &tm_close
    mov rax, [g_nsess]               ; Close others: needs a closable tab besides this one
    dec rax
    test r14, r14
    jz .a2
    dec rax
.a2:
    xor r13d, r13d
    test rax, rax
    jg .a3
    mov r13d, MF_GRAYED
.a3:
    invoke AppendMenuW, r12, r13, TM_CLOSE+BULK_OTHERS, &tm_others
    xor r13d, r13d                   ; Close all / unmodified: need any tab besides Home
    cmp qword [g_nsess], 1
    ja .a4
    mov r13d, MF_GRAYED
.a4:
    invoke AppendMenuW, r12, r13, TM_CLOSE+BULK_ALL, &tm_all
    invoke AppendMenuW, r12, r13, TM_CLOSE+BULK_UNMOD, &tm_unmod
    xor r13d, r13d                   ; Close to the right: needs a tab after this one
    lea rax, [r14+1]
    cmp rax, [g_nsess]
    jb .a5
    mov r13d, MF_GRAYED
.a5:
    invoke AppendMenuW, r12, r13, TM_CLOSE+BULK_RIGHT, &tm_right
    invoke TrackPopupMenu, r12, TPM_RETURNCMD|TPM_RIGHTBUTTON, #[rdi], #[rdi+4], 0, [g_hframe], 0
    mov ebx, eax
    invoke DestroyMenu, r12
    cmp ebx, TM_CLOSE
    jb .d
    je .one
    sub ebx, TM_CLOSE
    mov [g_bulk_mode], rbx
    mov [g_bulk_ref], r15
    call bulk_run
    jmp .d
.one:
    invoke select_tab, r14
    call close_current_tab
.d:
    EPILOG 160

; bulk_run: close every tab that g_bulk_mode selects, last tab first. Home always stays.
; A tab whose user answers Cancel ends the run. A tab that is still stopping its worker
; leaves g_bulk_mode set; the frame's close timer calls back in here once it is gone.
bulk_run:
    PROLOG 64
    mov r12, [g_nsess]
.next:
    dec r12
    test r12, r12
    jle .finish
    cmp r12, [g_nsess]
    jae .next
    lea rax, [g_sess]
    mov rbx, [rax+r12*8]
    mov rax, [g_bulk_mode]
    cmp rax, BULK_ALL
    je .do
    cmp rax, BULK_UNMOD
    je .unmod
    cmp rbx, [g_bulk_ref]
    jne .do
    cmp rax, BULK_RIGHT
    je .finish                       ; reached the tab itself: everything right of it is gone
    jmp .next                        ; BULK_OTHERS skips it
.unmod:
    invoke SendMessageW, [rbx+SS_HVIEW], BC_ISDIRTY, 0, 0
    test rax, rax
    jnz .next
.do:
    invoke select_tab, r12
    call close_current_tab
    cmp r12, [g_nsess]
    jae .next
    lea rax, [g_sess]
    cmp [rax+r12*8], rbx
    jne .next
    cmp qword [g_close_tab], 0       ; still there: waiting for its worker, or cancelled
    jne .d
    mov qword [g_bulk_mode], 0
    jmp .d
.finish:
    mov qword [g_bulk_mode], 0
    mov rbx, [g_bulk_ref]            ; back to the tab the menu was opened on, if it survived
    xor r12d, r12d
.find:
    cmp r12, [g_nsess]
    jae .d
    lea rax, [g_sess]
    cmp [rax+r12*8], rbx
    je .sel
    inc r12
    jmp .find
.sel:
    invoke select_tab, r12
.d:
    EPILOG 64

; layout_frame
layout_frame:
    PROLOG 160
    lea rbx, [rsp+96]                ; client rect
    lea r12, [rsp+112]               ; status rect
    invoke GetClientRect, [g_hframe], rbx
    mov ecx, 240
    call scl
    mov r14d, eax
    mov ecx, 360
    call scl
    mov r15d, eax
    mov eax, [rbx+RC_R]
    sub eax, r14d
    sub eax, r15d
    jns .pw
    mov eax, 120
.pw:
    mov [rsp+128], eax
    add eax, r15d
    mov [rsp+132], eax
    mov dword [rsp+136], -1
    invoke SendMessageW, [g_hstatus], SB_SETPARTS, 3, &rsp+128
    invoke SendMessageW, [g_hstatus], WM_SIZE, 0, 0
    invoke GetWindowRect, [g_hstatus], r12
    mov eax, [r12+RC_B]
    sub eax, [r12+RC_T]
    mov r13d, eax                    ; status height
    mov esi, [rbx+RC_R]              ; width
    mov edi, [rbx+RC_B]
    sub edi, r13d                    ; height without status
    mov [rbx+RC_B], edi
    invoke MoveWindow, [g_htab], 0, 0, rsi, rdi, 1
    invoke SendMessageW, [g_htab], TCM_ADJUSTRECT, 0, rbx
    mov r14d, [rbx+RC_L]
    mov r15d, [rbx+RC_T]
    mov esi, [rbx+RC_R]
    sub esi, r14d
    mov edi, [rbx+RC_B]
    sub edi, r15d
    xor r13d, r13d
.l:
    cmp r13, [g_nsess]
    jae .done
    lea rax, [g_sess]
    mov rax, [rax+r13*8]
    invoke MoveWindow, [rax+SS_HVIEW], r14, r15, rsi, rdi, 1
    inc r13
    jmp .l
.done:
    EPILOG 160

; apply_theme_all : tell every view to restyle, repaint the frame
apply_theme_all:
    PROLOG 64
    invoke theme_apply_title, [g_hframe]
    xor ebx, ebx
.l:
    cmp rbx, [g_nsess]
    jae .done
    lea rax, [g_sess]
    mov rax, [rax+rbx*8]
    invoke SendMessageW, [rax+SS_HVIEW], BC_THEME, 0, 0
    inc rbx
    jmp .l
.done:
    invoke RedrawWindow, [g_hframe], 0, 0, 0x0585      ; ERASE|INVALIDATE|ALLCHILDREN|FRAME|UPDATENOW
    EPILOG 64

; set_dark(rcx=0/1)
set_dark:
    PROLOG 160
    mov rbx, rcx
    invoke theme_set, rbx
    lea rdi, [rsp+96]
    invoke _snwprintf, rdi, 8, &f_int, rbx
    invoke WritePrivateProfileStringW, &sec_ui, &k_dark, rdi, &g_ini
    call apply_theme_all
    EPILOG 160

; ---------------------------------------------------------------- types by name / path

; type_from_name(rcx=wide) -> eax type or -1
type_from_name:
    PROLOG 64
    mov rsi, rcx
    xor ebx, ebx
.l:
    cmp ebx, 12
    jae .alias
    lea rax, [type_names]
    invoke _wcsicmp, rsi, [rax+rbx*8]
    test eax, eax
    jz .hit
    inc ebx
    jmp .l
.hit:
    mov eax, ebx
    EPILOG 64
.alias:
    invoke _wcsicmp, rsi, &al_dir
    test eax, eax
    jz .folder
    invoke _wcsicmp, rsi, &al_csv
    test eax, eax
    jz .table
    invoke _wcsicmp, rsi, &al_pic
    test eax, eax
    jz .pic
    invoke _wcsicmp, rsi, &al_image
    test eax, eax
    jz .pic
    invoke _wcsicmp, rsi, &al_diff
    test eax, eax
    jz .text
    mov eax, -1
    EPILOG 64
.folder:
    mov eax, T_FOLDER
    EPILOG 64
.table:
    mov eax, T_TABLE
    EPILOG 64
.pic:
    mov eax, T_PIC
    EPILOG 64
.text:
    mov eax, T_TEXT
    EPILOG 64

; ext_in_list(rcx=ext, rdx=list) -> eax
ext_in_list:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
.l:
    mov rax, [rdi]
    test rax, rax
    jz .no
    invoke _wcsicmp, rsi, rax
    test eax, eax
    jz .yes
    add rdi, 8
    jmp .l
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; type_for_path(rcx=path of a *file*) -> eax session type for opening it
type_for_path:
    PROLOG 64
    mov rsi, rcx
    invoke wcsrchr, rsi, '.'
    test rax, rax
    jz .text
    mov rbx, rax
    invoke ext_in_list, rbx, &pic_exts
    test eax, eax
    jnz .pic
    invoke ext_in_list, rbx, &tbl_exts
    test eax, eax
    jnz .table
    invoke ext_in_list, rbx, &media_exts
    test eax, eax
    jnz .media
    invoke ext_in_list, rbx, &hex_exts
    test eax, eax
    jnz .hex
.text:
    mov eax, T_TEXT
    EPILOG 64
.pic:
    mov eax, T_PIC
    EPILOG 64
.table:
    mov eax, T_TABLE
    EPILOG 64
.media:
    mov eax, T_MEDIA
    EPILOG 64
.hex:
    mov eax, T_HEX
    EPILOG 64

; ---------------------------------------------------------------- ini persistence

; ini_prepare(rcx=file): make sure the file exists as UTF-16 (BOM) so Unicode survives
ini_prepare:
    PROLOG 96
    mov rbx, rcx
    invoke GetFileAttributesW, rbx
    cmp eax, -1
    jne .ok
    invoke CreateFileW, rbx, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .ok
    mov rsi, rax
    mov word [rsp+96], 0xFEFF
    lea rdx, [rsp+96]
    lea r9, [rsp+104]
    invoke WriteFile, rsi, rdx, 2, r9, 0
    invoke CloseHandle, rsi
.ok:
    EPILOG 96

; session_write(sess, file, section)
session_write:
    PROLOG 96
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    invoke ini_prepare, r12
    mov rax, [rbx+SS_TYPE]
    lea r14, [type_names]
    mov r14, [r14+rax*8]
    invoke WritePrivateProfileStringW, r13, &k_type, r14, r12
    invoke WritePrivateProfileStringW, r13, &k_a, &rbx+SS_PATHA, r12
    invoke WritePrivateProfileStringW, r13, &k_b, &rbx+SS_PATHB, r12
    invoke WritePrivateProfileStringW, r13, &k_c, &rbx+SS_PATHC, r12
    invoke WritePrivateProfileStringW, r13, &k_out, &rbx+SS_PATHO, r12
    invoke WritePrivateProfileStringW, r13, &k_opts, &rbx+SS_OPTS, r12
    EPILOG 96

; session_read(file, section) -> sess opened (or 0)
session_read:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    lea rdi, [rsp+96]
    invoke GetPrivateProfileStringW, r13, &k_type, &s_empty, rdi, 32, r12
    invoke type_from_name, rdi
    cmp eax, -1
    je .fail
    mov r14d, eax
    invoke sess_new, r14, 0, 0, 0
    test rax, rax
    jz .fail
    mov rbx, rax
    invoke GetPrivateProfileStringW, r13, &k_a, &s_empty, &rbx+SS_PATHA, 519, r12
    invoke GetPrivateProfileStringW, r13, &k_b, &s_empty, &rbx+SS_PATHB, 519, r12
    invoke GetPrivateProfileStringW, r13, &k_c, &s_empty, &rbx+SS_PATHC, 519, r12
    invoke GetPrivateProfileStringW, r13, &k_out, &s_empty, &rbx+SS_PATHO, 519, r12
    invoke GetPrivateProfileStringW, r13, &k_opts, &s_empty, &rbx+SS_OPTS, 1023, r12
    invoke sess_open, rbx
    test rax, rax
    jnz .ret
    invoke free, rbx
    xor eax, eax
.ret:
    EPILOG 160
.fail:
    xor eax, eax
    EPILOG 160

; path_ext(rcx=path) -> rax = the file name's extension (".xxx"), or 0 when it has none
path_ext:
    PROLOG 64
    mov rsi, rcx
    invoke wcsrchr, rsi, '.'
    mov rbx, rax
    test rax, rax
    jz .d
    invoke wcsrchr, rsi, 92          ; a dot before the last backslash belongs to a folder name
    cmp rax, rbx
    jb .d
    xor ebx, ebx
.d:
    mov rax, rbx
    EPILOG 64

; file_msg(rcx=format with one %s, rdx=path): warning box; silent in unattended runs
file_msg:
    PROLOG 1648
    test qword [g_batch], 4
    jnz .d
    mov r12, rcx
    mov r13, rdx
    lea rdi, [rsp+96]
    invoke _snwprintf, rdi, 759, r12, r13
    mov word [rdi+1518], 0
    invoke msgbox, [g_hframe], rdi, MB_OK|MB_ICONWARNING
.d:
    EPILOG 1648

; save_session_dlg
save_session_dlg:
    PROLOG 1200
    call cur_index
    test rax, rax
    jnz .has
    invoke msgbox, [g_hframe], &s_home_nosave, MB_OK|MB_ICONINFO
    jmp .d
.has:
    lea rcx, [g_sess]
    mov rbx, [rcx+rax*8]
    lea rdi, [rsp+96]
    mov word [rdi], 0
    invoke browse_save_as, [g_hframe], rdi, &flt_save_sess, &s_de_bcs
    test eax, eax
    jz .d
    invoke DeleteFileW, rdi          ; replace, do not merge into an older file
    invoke SendMessageW, [rbx+SS_HVIEW], BC_COLLECT, 0, 0
    invoke session_write, rbx, rdi, &sec_session
    test eax, eax
    jz .fail
    invoke recent_add, rdi
    jmp .d
.fail:
    invoke file_msg, &f_save_fail, rdi
.d:
    EPILOG 1200

; open_file_dlg(rcx = 0 session first / 1 workspace first in the file type list)
open_file_dlg:
    PROLOG 1200
    lea rbx, [flt_open_sess]
    test ecx, ecx
    jz .go
    lea rbx, [flt_open_ws]
.go:
    lea rdi, [rsp+96]
    mov word [rdi], 0
    invoke browse_open_as, [g_hframe], rdi, rbx
    test eax, eax
    jz .d
    invoke open_any_file, rdi
.d:
    EPILOG 1200

; open_any_file(rcx=path) -> eax 1 when something was opened
; The extension picks what to try first; the other kind is tried when that finds nothing.
open_any_file:
    PROLOG 64
    mov rsi, rcx
    xor r12d, r12d                   ; 0 = session, 1 = workspace
    invoke path_ext, rsi
    test rax, rax
    jz .try
    invoke _wcsicmp, rax, &x_bcw
    test eax, eax
    jnz .try
    mov r12d, 1
.try:
    mov r13d, 2
.again:
    test r12d, r12d
    jnz .ws
    invoke session_read, rsi, &sec_session
    jmp .chk
.ws:
    invoke workspace_read, rsi
.chk:
    test rax, rax
    jnz .ok
    xor r12d, 1
    dec r13d
    jnz .again
    invoke file_msg, &f_open_fail, rsi
    xor eax, eax
    EPILOG 64
.ok:
    invoke recent_add, rsi
    mov eax, 1
    EPILOG 64

; save_workspace_dlg
save_workspace_dlg:
    PROLOG 1200
    cmp qword [g_nsess], 1
    ja .has
    invoke msgbox, [g_hframe], &s_ws_empty, MB_OK|MB_ICONINFO
    jmp .d
.has:
    lea rdi, [rsp+96]
    mov word [rdi], 0
    invoke browse_save_as, [g_hframe], rdi, &flt_save_ws, &s_de_bcw
    test eax, eax
    jz .d
    invoke workspace_write, rdi
    test eax, eax
    jz .fail
    invoke recent_add, rdi
    jmp .d
.fail:
    invoke file_msg, &f_save_fail, rdi
.d:
    EPILOG 1200

; workspace_write(rcx=file) -> eax nonzero when every value was written
; Replaces the file: [Workspace] Count / Active (tab index, 0 = Home) and one [TabN] per tab.
workspace_write:
    PROLOG 192
    mov r12, rcx
    invoke DeleteFileW, r12
    invoke ini_prepare, r12
    lea rdi, [rsp+96]
    mov rax, [g_nsess]
    dec rax
    invoke _snwprintf, rdi, 16, &f_int, rax
    invoke WritePrivateProfileStringW, &sec_ws, &k_count, rdi, r12
    mov r14d, eax                    ; stays nonzero while writes succeed
    call cur_index
    lea rdi, [rsp+96]
    invoke _snwprintf, rdi, 16, &f_int, rax
    invoke WritePrivateProfileStringW, &sec_ws, &k_active, rdi, r12
    mov ebx, 1
.l:
    cmp rbx, [g_nsess]
    jae .done
    lea rdi, [rsp+96]
    invoke _snwprintf, rdi, 16, &f_tab, rbx
    lea rax, [g_sess]
    mov r13, [rax+rbx*8]
    invoke SendMessageW, [r13+SS_HVIEW], BC_COLLECT, 0, 0
    lea rdi, [rsp+96]
    invoke session_write, r13, r12, rdi
    test eax, eax
    jnz .wok
    xor r14d, r14d
.wok:
    inc rbx
    jmp .l
.done:
    mov eax, r14d
    EPILOG 192

; workspace_read(rcx=file) -> rax = number of tabs opened
; The tabs are added after the ones already open; the saved active tab is selected again.
workspace_read:
    PROLOG 192
    mov r12, rcx
    invoke GetPrivateProfileIntW, &sec_ws, &k_count, 0, r12
    mov r13d, eax
    cmp r13, MAX_SESS
    jbe .cok
    mov r13d, MAX_SESS
.cok:
    invoke GetPrivateProfileIntW, &sec_ws, &k_active, 0, r12
    mov r15d, eax
    xor r14d, r14d                   ; session of the saved active tab
    xor ebp, ebp                     ; tabs opened
    mov ebx, 1
.l:
    cmp rbx, r13
    ja .done
    lea rdi, [rsp+96]
    invoke _snwprintf, rdi, 16, &f_tab, rbx
    lea rdi, [rsp+96]
    invoke session_read, r12, rdi
    test rax, rax
    jz .n
    inc ebp
    cmp rbx, r15
    jne .n
    mov r14, rax
.n:
    inc rbx
    jmp .l
.done:
    test r14, r14
    jz .ret
    xor ebx, ebx
.find:
    cmp rbx, [g_nsess]
    jae .ret
    lea rax, [g_sess]
    cmp [rax+rbx*8], r14
    je .sel
    inc rbx
    jmp .find
.sel:
    invoke select_tab, rbx
.ret:
    mov eax, ebp
    EPILOG 192

; ---------------------------------------------------------------- command line
; usage:  bcgui <type> [A] [B] [Base] [Out] [/report file.html] [/sync update|mirror|twoway] [/exit]
;         (/report file.xlsx or file.docx writes an Excel / Word export instead of HTML)
;         bcgui file.bcs | file.bcw
; parse_switches(argv, start, argc)
parse_switches:
    PROLOG 64
    mov rsi, rcx
    mov edi, edx
    mov r12d, r8d
.l:
    cmp edi, r12d
    jae .d
    mov rbx, [rsi+rdi*8]
    invoke _wcsicmp, rbx, &sw_report
    test eax, eax
    jnz .n1
    inc edi
    cmp edi, r12d
    jae .d
    mov rax, [rsi+rdi*8]
    invoke wcsncpy, &g_rep_path, rax, 519
    or qword [g_batch], 1|4
    jmp .next
.n1:
    invoke _wcsicmp, rbx, &sw_sync
    test eax, eax
    jnz .n2
    inc edi
    cmp edi, r12d
    jae .d
    mov rbx, [rsi+rdi*8]
    mov r13d, 1
    invoke _wcsicmp, rbx, &sw_mirror
    test eax, eax
    jnz .m2
    mov r13d, 2
.m2:
    invoke _wcsicmp, rbx, &sw_twoway
    test eax, eax
    jnz .m3
    mov r13d, 3
.m3:
    shl r13, 8
    or r13, 2|4
    or [g_batch], r13
    jmp .next
.n2:
    invoke _wcsicmp, rbx, &sw_exit
    test eax, eax
    jnz .n3
    or qword [g_batch], 4
    jmp .next
.n3:
    invoke _wcsicmp, rbx, &sw_save
    test eax, eax
    jnz .n4
    inc edi
    cmp edi, r12d
    jae .d
    mov rax, [rsi+rdi*8]
    invoke wcsncpy, &g_save_path, rax, 519
    jmp .next
.n4:
    invoke _wcsicmp, rbx, &sw_snap
    test eax, eax
    jnz .n5
    inc edi
    cmp edi, r12d
    jae .d
    mov rax, [rsi+rdi*8]
    invoke wcsncpy, &g_snap_path, rax, 519
    or qword [g_batch], 8|4
    jmp .next
.n5:
    invoke _wcsicmp, rbx, &sw_savews
    test eax, eax
    jnz .n6
    inc edi
    cmp edi, r12d
    jae .d
    mov rax, [rsi+rdi*8]
    invoke wcsncpy, &g_savews_path, rax, 519
    jmp .next
.n6:
    invoke _wcsicmp, rbx, &sw_xlsx
    test eax, eax
    jnz .n7
    inc edi
    cmp edi, r12d
    jae .d
    mov rax, [rsi+rdi*8]
    invoke wcsncpy, &g_xl_path, rax, 519
    or qword [g_batch], 0x10|4
    jmp .next
.n7:
    invoke _wcsicmp, rbx, &sw_docx
    test eax, eax
    jnz .next
    inc edi
    cmp edi, r12d
    jae .d
    mov rax, [rsi+rdi*8]
    invoke wcsncpy, &g_dx_path, rax, 519
    or qword [g_batch], 0x20|4
.next:
    inc edi
    jmp .l
.d:
    EPILOG 64

; save_requested(sess): /save file writes the session, /savews file the whole workspace
save_requested:
    PROLOG 64
    mov rbx, rcx
    test rbx, rbx
    jz .d
    cmp word [g_save_path], 0
    je .ws
    invoke SendMessageW, [rbx+SS_HVIEW], BC_COLLECT, 0, 0
    invoke DeleteFileW, &g_save_path
    invoke session_write, rbx, &g_save_path, &sec_session
.ws:
    cmp word [g_savews_path], 0
    je .d
    invoke workspace_write, &g_savews_path
.d:
    EPILOG 64

parse_cmdline:
    PROLOG 160
    invoke GetCommandLineW
    invoke CommandLineToArgvW, rax, &rsp+96
    test rax, rax
    jz .d
    mov rbx, rax
    mov r13d, [rsp+96]               ; argc
    mov r12d, r13d                   ; positional argc = index of the first /switch
    cmp r13d, 2
    jl .free
    mov ecx, 2
.sw:
    cmp ecx, r13d
    jae .swdone
    mov rax, [rbx+rcx*8]
    cmp word [rax], '/'
    je .swfound
    inc ecx
    jmp .sw
.swfound:
    mov r12d, ecx
.swdone:
    mov rsi, [rbx+8]                 ; argv[1]
    invoke _wcsicmp, rsi, &sw_shellcmp
    test eax, eax
    jz .shellcmp
    invoke _wcsicmp, rsi, &sw_patch
    test eax, eax
    jz .patchcmd
    invoke wcsrchr, rsi, '.'
    test rax, rax
    jz .bytype
    mov rdi, rax
    invoke _wcsicmp, rdi, &x_bcs
    test eax, eax
    jz .file
    invoke _wcsicmp, rdi, &x_bcw
    test eax, eax
    jz .file
.bytype:
    invoke type_from_name, rsi
    cmp eax, -1
    je .free
    cmp eax, T_HOME
    je .free
    mov r14d, eax
    xor edx, edx
    xor r8d, r8d
    cmp r12d, 3
    jl .mk
    mov rdx, [rbx+16]
    cmp r12d, 4
    jl .mk
    mov r8, [rbx+24]
.mk:
    invoke sess_new, r14, rdx, r8, 0
    test rax, rax
    jz .free
    mov r15, rax
    cmp r12d, 5
    jl .mk2
    mov rax, [rbx+32]
    invoke wcsncpy, &r15+SS_PATHC, rax, 519
.mk2:
    cmp r12d, 6
    jl .open
    mov rax, [rbx+40]
    invoke wcsncpy, &r15+SS_PATHO, rax, 519
.open:
    invoke parse_switches, rbx, r12, r13
    ; /xlsx and /docx are served by the session's first compare. Without both inputs, or on a session type
    ; that has no such export, nothing would ever consume the switch and the minimized window would stay
    ; open for good: end with exit code 2 instead.
    mov rax, [g_batch]
    test eax, 0x30
    jz .openok
    cmp word [r15+SS_PATHA], 0
    je .badsw
    cmp word [r15+SS_PATHB], 0
    je .badsw
    test eax, 0x10
    jz .ckdocx
    cmp r14d, T_TEXT
    je .ckdocx
    cmp r14d, T_TABLE
    je .ckdocx
    cmp r14d, T_HEX
    jne .badsw
.ckdocx:
    test eax, 0x20
    jz .openok
    cmp r14d, T_PIC
    je .openok
    cmp r14d, T_MERGE
    je .openok
    cmp r14d, T_VER
    je .openok
    cmp r14d, T_REG
    je .openok
    cmp r14d, T_MEDIA
    je .openok
    cmp r14d, T_FMERGE
    je .openok
.badsw:
    mov qword [g_exitcode], 2
    invoke PostMessageW, [g_hframe], WM_CLOSE, 0, 0
    jmp .free
.openok:
    invoke sess_open, r15
    invoke save_requested, r15
    jmp .free
.file:
    invoke parse_switches, rbx, r12, r13
    invoke open_any_file, rsi
    jmp .free
.patchcmd:
    cmp r13d, 3
    jl .free
    invoke patch_cmd, [rbx+16]
    jmp .free
.shellcmp:
    cmp r13d, 3
    jl .free
    invoke calloc, 1, 1100
    mov r14, rax
    invoke shell_pending, r14
    test eax, eax
    jz .free
    mov r15d, T_FOLDER
    invoke GetFileAttributesW, r14
    test eax, 0x10
    jnz .scmk
    invoke type_for_path, r14
    mov r15d, eax
.scmk:
    mov r8, [rbx+16]
    invoke sess_new, r15, r14, r8, 0
    test rax, rax
    jz .free
    invoke sess_open, rax
.free:
    invoke LocalFree, rbx
.d:
    EPILOG 160

; ---------------------------------------------------------------- frame window procedure
frame_proc:
    PROLOG 256
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_NOTIFY
    je .notify
    cmp edi, WM_COMMAND
    je .command
    cmp edi, WM_ERASEBKGND
    je .erase
    cmp edi, WM_DROPFILES
    je .drop
    cmp edi, WM_CLOSE
    je .close
    cmp edi, WM_TIMER
    je .close_timer
    cmp edi, WM_DESTROY
    je .destroy
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 256

.create:
    mov [g_hframe], rsi
    invoke mk, rsi, &cn_tab, 0, WS_CLIPSIBLINGS|TCS_FOCUSNEVER, 1
    mov [g_htab], rax
    invoke mk, rsi, &cn_status, &s_ready, SBARS_SIZEGRIP, 2
    mov [g_hstatus], rax
    xor eax, eax
    EPILOG 256

.size:
    call layout_frame
    xor eax, eax
    EPILOG 256

.erase:
    lea rbx, [rsp+96]
    invoke GetClientRect, rsi, rbx
    invoke FillRect, r12, rbx, [th_panelbrush]
    mov eax, 1
    EPILOG 256

.notify:
    mov rax, [r13+NMH_FROM]
    cmp rax, [g_htab]
    jne .def
    cmp dword [r13+NMH_CODE], NM_CLICK
    je .tabclick
    cmp dword [r13+NMH_CODE], NM_RCLICK
    je .tabmenu
    cmp dword [r13+NMH_CODE], TCN_SELCHANGE
    jne .def
    call cur_index
    invoke select_tab, rax
    xor eax, eax
    EPILOG 256
.tabmenu:
    call tab_menu
    jmp .ok0
.tabclick:
    ; a click on the close mark at the right end of a tab closes that tab
    lea rbx, [rsp+96]                ; TCHITTESTINFO (pt, flags)
    invoke GetCursorPos, rbx
    invoke ScreenToClient, [g_htab], rbx
    invoke SendMessageW, [g_htab], TCM_HITTEST, 0, rbx
    movsxd r14, eax
    test r14, r14
    jle .ok0                         ; outside the tabs, or Home
    call cur_index
    cmp rax, r14
    jne .ok0
    lea rdi, [rsp+112]
    invoke SendMessageW, [g_htab], TCM_GETITEMRECT, r14, rdi
    mov ecx, 22
    call scl
    mov edx, [rdi+RC_R]
    sub edx, eax
    cmp [rbx], edx
    jl .ok0
    ; posted: the tab control is still inside its own mouse handling here
    invoke PostMessageW, rsi, WM_COMMAND, IDM_CLOSE_TAB, 0
    jmp .ok0

.command:
    movzx eax, r12w
    cmp eax, IDM_NEW_FOLDER
    jb .cmd_other
    cmp eax, IDM_NEW_FMERGE
    ja .cmd_other
    sub eax, 1000                    ; 1001.. -> T_FOLDER..T_MERGE
    invoke add_session, rax, 0, 0, 0
    xor eax, eax
    EPILOG 256
.cmd_other:
    cmp eax, IDM_OPEN_SESS
    je .c_open
    cmp eax, IDM_OPEN_WS
    je .c_openws
    cmp eax, IDM_SAVE_SESS
    je .c_save
    cmp eax, IDM_SAVE_WS
    je .c_savews
    cmp eax, IDM_CLOSE_TAB
    je .c_close
    cmp eax, IDM_EXIT
    je .c_exit
    cmp eax, IDM_LIGHT
    je .c_light
    cmp eax, IDM_DARK
    je .c_dark
    cmp eax, IDM_RUN
    je .c_run
    cmp eax, IDM_CANCEL
    je .c_cancel
    cmp eax, IDM_SAVE
    je .c_vsave
    cmp eax, IDM_SETTINGS
    je .c_settings
    cmp eax, IDM_SHELL_ON
    je .c_shon
    cmp eax, IDM_SHELL_OFF
    je .c_shoff
    cmp eax, IDM_UPDATE
    je .c_update
    cmp eax, IDM_CLIP
    je .c_clip
    cmp eax, IDM_PATCH
    je .c_patch
    cmp eax, IDM_ABOUT
    je .c_about
    jmp .def
.c_settings:
    invoke settings_show, rsi
    jmp .ok0
.c_shon:
    mov ecx, 1
    call shell_install
    invoke msgbox, rsi, &s_shell_on, MB_OK|MB_ICONINFO
    jmp .ok0
.c_shoff:
    xor ecx, ecx
    call shell_install
    invoke msgbox, rsi, &s_shell_off, MB_OK|MB_ICONINFO
    jmp .ok0
.c_open:
    xor ecx, ecx
    call open_file_dlg
    jmp .ok0
.c_openws:
    mov ecx, 1
    call open_file_dlg
    jmp .ok0
.c_save:
    call save_session_dlg
    jmp .ok0
.c_savews:
    call save_workspace_dlg
    jmp .ok0
.c_close:
    call close_current_tab
    jmp .ok0
.c_exit:
    invoke PostMessageW, rsi, WM_CLOSE, 0, 0
    jmp .ok0
.c_light:
    xor ecx, ecx
    call set_dark
    jmp .ok0
.c_dark:
    mov ecx, 1
    call set_dark
    jmp .ok0
.c_run:
    mov ebx, BC_RUN
    jmp .tocur
.c_cancel:
    mov ebx, BC_CANCEL
    jmp .tocur
.c_vsave:
    mov ebx, BC_SAVE
.tocur:
    call cur_view
    test rax, rax
    jz .ok0
    invoke SendMessageW, rax, rbx, 0, 0
    jmp .ok0
.c_update:
    invoke update_check, rsi, 0
    jmp .ok0
.c_clip:
    invoke clip_compare, rsi
    jmp .ok0
.c_patch:
    invoke patch_open, rsi
    jmp .ok0
.c_about:
    invoke msgbox, rsi, &s_about, MB_OK|MB_ICONINFO
.ok0:
    xor eax, eax
    EPILOG 256

.close_timer:
    cmp r12d, 91
    jne .def
    cmp qword [g_close_all], 0
    jne .close
    mov rbx, [g_close_tab]
    test rbx, rbx
    jz .close_timer_done
    invoke SendMessageW, [rbx+SS_HVIEW], BC_STOPPING, 0, 0
    test rax, rax
    jnz .ok0
    ; The disabled frame prevents tab selection/removal during this wait.
    call close_current_tab
.close_timer_done:
    invoke KillTimer, rsi, 91
    invoke EnableWindow, rsi, 1
    cmp qword [g_bulk_mode], 0
    je .ok0
    call bulk_run                    ; carry on with the remaining tabs
    jmp .ok0
.close:
    cmp qword [g_close_all], 0
    jne .stop_workers
    xor ebx, ebx
.confirm_next:
    cmp rbx, [g_nsess]
    jae .confirmed
    lea rax, [g_sess]
    mov rdi, [rax+rbx*8]
    invoke SendMessageW, [rdi+SS_HVIEW], BC_ISDIRTY, 0, 0
    test rax, rax
    jz .confirm_advance
    invoke select_tab, rbx
    invoke view_can_close, [rdi+SS_HVIEW]
    test eax, eax
    jz .ok0
.confirm_advance:
    inc rbx
    jmp .confirm_next
.confirmed:
    mov qword [g_close_all], 1
.stop_workers:
    xor ebx, ebx
    xor ebp, ebp
.stop_next:
    cmp rbx, [g_nsess]
    jae .stopped
    lea rax, [g_sess]
    mov rdi, [rax+rbx*8]
    invoke SendMessageW, [rdi+SS_HVIEW], BC_STOPPING, 0, 0
    or rbp, rax
    inc rbx
    jmp .stop_next
.stopped:
    test rbp, rbp
    jz .close_now
    invoke EnableWindow, rsi, 0
    invoke SetTimer, rsi, 91, 100, 0
    jmp .ok0
.close_now:
    invoke KillTimer, rsi, 91
    invoke DestroyWindow, rsi
    jmp .ok0

.drop:
    invoke handle_drop, r12
    xor eax, eax
    EPILOG 256

.destroy:
    invoke PostQuitMessage, 0
    xor eax, eax
    EPILOG 256

; handle_drop(hdrop): two items -> new compare session, one item -> fills a path box
handle_drop:
    PROLOG 2208
    mov r12, rcx
    invoke DragQueryFileW, r12, 0xFFFFFFFF, 0, 0
    mov r13d, eax
    lea rsi, [rsp+96]                ; path 0
    lea rdi, [rsp+1136]              ; path 1
    test r13d, r13d
    jz .fin
    invoke DragQueryFileW, r12, 0, rsi, 520
    cmp r13d, 2
    jb .single
    invoke DragQueryFileW, r12, 1, rdi, 520
    invoke GetFileAttributesW, rsi
    mov ebx, T_FOLDER
    cmp eax, -1
    je .go
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jnz .go
    invoke type_for_path, rsi
    mov ebx, eax
.go:
    invoke add_session, rbx, rsi, rdi, 0
    jmp .fin
.single:
    call cur_view
    test rax, rax
    jz .fin
    mov rbx, rax
    invoke GetDlgItem, rbx, ID_EDIT_A
    test rax, rax
    jz .fin
    mov r14, rax
    invoke GetWindowTextW, r14, rdi, 8
    cmp word [rdi], 0
    je .seta
    invoke GetDlgItem, rbx, ID_EDIT_B
    test rax, rax
    jz .fin
    mov r14, rax
.seta:
    invoke SetWindowTextW, r14, rsi
.fin:
    invoke DragFinish, r12
    EPILOG 2208
