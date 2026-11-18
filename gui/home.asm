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
extern mk, reg_class, scl, tip_add
extern ini_prepare, open_any_file, g_sess, g_nsess
extern ShowWindow, SetTimer, KillTimer, InvalidateRect, GetWindowTextW, SetWindowTextW
extern find_start, find_query2, find_path, fi_state, fi_vis
extern find_sort, find_time, find_size, find_flags, find_name, find_parent, find_volcount, find_volname
extern lv_dark_header, fmt_size, FileTimeToLocalFileTime, FileTimeToSystemTime, malloc
extern browse_folder, GetWindowTextLengthW, SetWindowPos
extern ShellExecuteW, CreatePopupMenu, AppendMenuW, TrackPopupMenu, DestroyMenu, ScreenToClient, GetWindowRect
extern OpenClipboard, EmptyClipboard, SetClipboardData, CloseClipboard, GlobalAlloc, GlobalLock, GlobalUnlock
extern memcpy, memset, wcslen, CharLowerBuffW, set_status
extern type_for_path, add_session, GetFileAttributesW
extern SHFileOperationW, SHObjectProperties, free, LoadLibraryW, GetProcAddress, FreeLibrary, GetAsyncKeyState
extern ImageList_Create, ImageList_AddMasked, GetDC, ReleaseDC, CreateCompatibleDC, CreateCompatibleBitmap, DeleteDC, DrawFrameControl, FindWindowW, SetForegroundWindow

global home_register, home_create, ini_path_init, settings_load, recent_add, g_ini

%define ID_TITLE   3000
%define ID_RECENT  3001
%define ID_RLABEL  3002
%define ID_RDEL    3003
%define ID_HINT    3004
%define ID_FIND_EDIT 3010
%define ID_FIND_BTN  3011
%define ID_FIND_LIST 3012
%define ID_FIND_KIND 3013
%define ID_FIND_EXT  3014
%define ID_FIND_PATH 3015
%define ID_FIND_LEXT 3016
%define ID_FIND_LPATH 3017
%define ID_FIND_DRIVE 3018
%define ID_FIND_CLOUD 3019
%define ID_FIND_RESET 3020
%define ID_FIND_CLR   3021
%define ID_FIND_PBROWSE 3022
%define ID_FIND_SELALL  3023
%define ID_FIND_COPY    3024
%define ID_FIND_SHARE   3025
%define ID_FIND_EMAIL   3026
%define ID_FIND_DEL     3027
%define ID_FIND_PROPS   3028
%define FIND_TIMER2  78                 ; search a moment after the last key
%define HF_CAP       4000000            ; results kept (and sorted) at most
%define HF_DIRSORT   50000              ; sorting by folder builds two paths per comparison
%define FA_OPEN   1                  ; actions on a found item (also the ids of the menu)
%define FA_OPENAS 2
%define FA_SHOW   3
%define FA_BC     4
%define FA_COPY   5
%define FA_SHARE  6
%define FA_EMAIL  7
%define FA_DEL    8
%define FA_SELALL 9
%define FA_PROPS  10
%define FIND_TIMER   77
%define BS_OWNERDRAW 0x000B
%define MAX_RECENT 10

section .rdata
cn_home:    dw __utf16__("BCHome"), 0
cn_static:  dw __utf16__("STATIC"), 0
cn_button:  dw __utf16__("BUTTON"), 0
cn_listbox: dw __utf16__("LISTBOX"), 0
cn_edit:    dw __utf16__("EDIT"), 0
cn_combo:   dw __utf16__("COMBOBOX"), 0
cn_lview:   dw __utf16__("SysListView32"), 0
s_empty:    dw 0

; App Header strings
WSTR s_app_title, "BeCompare"
WSTR s_hint_text, "비교할 세션을 선택하거나 파일·폴더를 창으로 끌어다 놓으세요."
WSTR s_rlabel,    "최근 세션"
WSTR s_del_btn,   "기록 삭제"
WSTR s_tip_rdel, "최근 세션 기록을 모두 지웁니다"
WSTR s_find_btn, "파일 찾기"
WSTR s_find_cue, "파일 이름의 일부를 입력하면 이 컴퓨터의 고정 드라이브에서 찾습니다"
WSTR s_tip_find_edit, "찾을 파일·폴더 이름의 일부 (대소문자 구분 없음). 필터까지 모두 비우면 최근 세션 목록으로 돌아갑니다"
WSTR s_tip_find_kind, "찾을 대상의 종류. 문서·이미지 같은 묶음은 그 확장자의 파일만 찾습니다"
WSTR s_tip_find_ext, "이 확장자의 파일만 찾습니다. 여러 개는 ; 로 구분 (예: pdf;docx). 적으면 종류의 확장자 묶음 대신 쓰입니다"
WSTR s_tip_find_path, "들어 있는 폴더 경로에 이 글자가 있는 것만 찾습니다 (예: Downloads)"
WSTR s_cue_ext, "pdf;docx"
WSTR s_lbl_ext, "확장자"
WSTR s_lbl_path, "폴더"
WSTR s_cue_path, "폴더 경로에 포함"
WSTR s_tip_find_drive, "이 드라이브에서만 찾습니다"
WSTR s_find_reset, "필터 초기화"
WSTR s_find_clr, "×"
WSTR s_find_dots, "…"
WSTR s_tip_find_reset, "드라이브·종류·클라우드·확장자·폴더 필터를 모두 처음 상태로 되돌립니다 (검색어는 그대로)"
WSTR s_tip_find_clr, "검색어 지우기"
WSTR s_tip_find_pbrowse, "폴더를 골라 폴더 필터에 넣습니다. 그 폴더와 하위 폴더 안에서만 찾습니다"
WSTR s_tip_find_cloud, "OneDrive 같은 클라우드 저장소 폴더 안의 항목을 포함할지, 뺄지, 그것만 볼지 고릅니다"
WSTR s_fd_all, "모든 드라이브"
WSTR s_fc0, "클라우드 포함"
WSTR s_fc1, "클라우드 제외"
WSTR s_fc2, "클라우드만"
WSTR s_fcol0, "이름"
WSTR s_fcol1, "수정한 날짜"
WSTR s_fcol2, "유형"
WSTR s_fcol3, "크기"
WSTR s_fcol4, "폴더"
WSTR f_find_date, "%04d-%02d-%02d %02d:%02d"
WSTR s_arr_up, " ▲"
WSTR s_arr_dn, " ▼"
WSTR f_colhdr, "%s%s"
WSTR s_ft_dir, "파일 폴더"
WSTR s_ft_file, "파일"
WSTR f_ft_ext, "%s 파일"
WSTR s_kb, "KB"
WSTR s_find_toomany, "결과가 5만 개를 넘으면 폴더 순으로 정렬하지 않고 이름 순으로 보여 줍니다."
WSTR f_find_capped, "파일 찾기 결과 %u개 (처음 400만 개만 표시) · 색인 %llu개"
align 8
fcol_text: dq s_fcol0, s_fcol1, s_fcol2, s_fcol3, s_fcol4
fcol_w:    dd 300, 140, 90, 90, 440
fcol_fmt:  dd 0, 0, 0, 1, 0                  ; the size column is right aligned
WSTR s_fk0, "전체"
WSTR s_fk1, "파일만"
WSTR s_fk2, "폴더만"
WSTR s_fk3, "문서"
WSTR s_fk4, "이미지"
WSTR s_fk5, "음악·동영상"
WSTR s_fk6, "압축 파일"
WSTR s_fk7, "소스 코드"
WSTR s_fk8, "실행 파일"
WSTR s_fm_open, "열기 (연결된 프로그램)"
WSTR s_fm_openas, "연결 프로그램 선택…"
WSTR s_fm_show, "폴더에서 보기"
WSTR s_fm_bc, "BeCompare에서 열기 (비교의 왼쪽으로)"
WSTR s_x_bcs, ".bcs"
WSTR s_x_bcw, ".bcw"
WSTR s_fm_copy, "경로 복사"
WSTR s_v_open, "open"
WSTR s_v_openas, "openas"
WSTR s_explorer, "explorer.exe"
WSTR f_select, `/select,"%s"`
WSTR s_copied, "경로를 클립보드에 복사했습니다."
WSTR s_noapp, "이 파일을 열 프로그램을 찾지 못했습니다."
WSTR s_find_selall, "전체 선택"
WSTR s_find_copy,   "복사"
WSTR s_find_share,  "공유"
WSTR s_find_email,  "이메일"
WSTR s_find_del,    "삭제"
WSTR s_find_props,  "속성"
WSTR s_tip_find_selall, "모든 검색 결과 항목의 체크를 켜거나 끕니다 (Ctrl+A)"
WSTR s_tip_find_copy,   "선택한 항목을 파일 및 경로로 클립보드에 복사합니다 (Ctrl+C)"
WSTR s_tip_find_share,  "선택한 항목을 SNS(카카오톡, 문자 등)로 공유·전송합니다"
WSTR s_tip_find_email,  "선택한 파일을 전자 메일에 첨부하여 보냅니다"
WSTR s_tip_find_del,    "선택한 항목을 휴지통으로 이동합니다 (Del)"
WSTR s_tip_find_props,  "선택한 파일·폴더의 등록정보 속성 창을 엽니다 (Alt+Enter)"
WSTR s_fm_share,    "공유 (카카오톡·문자)…"
WSTR s_fm_email,    "이메일로 보내기…"
WSTR s_fm_del,      "삭제 (휴지통으로)"
WSTR s_fm_props,    "속성(&R)"
WSTR s_fm_selall,   "모두 선택 / 해제"
WSTR s_sm_kakao,    "💬 카카오톡으로 전송"
WSTR s_sm_sms,      "📱 문자(메시지)로 전송"
WSTR s_sm_clip,     "📋 메신저 대화창 붙여넣기 복사 (Ctrl+V)"
WSTR s_sm_nearby,   "🌐 Windows 주변 기기 공유"
WSTR s_status_copied, "%u개 항목을 클립보드에 복사했습니다."
WSTR s_status_deleted, "%u개 항목을 휴지통으로 이동했습니다."
WSTR s_status_delfail, "삭제를 취소했거나 일부 항목을 지우지 못했습니다."
WSTR s_status_email, "이메일 프로그램을 열었습니다. 본문에 Ctrl+V로 파일을 첨부하세요."
WSTR s_status_props, "파일 속성 창을 열었습니다."
WSTR s_status_kakao, "파일을 복사했습니다. 카카오톡 대화창에서 Ctrl+V로 전송하세요."
WSTR s_status_sms,   "문자(메시지) 앱을 열었습니다. 대화창에서 Ctrl+V로 전송하세요."
WSTR s_status_sns_clip, "SNS·메신저(카카오톡, 문자 등)에 붙여넣기(Ctrl+V)할 수 있도록 복사했습니다."
WSTR s_status_share, "공유 메뉴를 열었습니다."
WSTR s_status_none,  "선택된 항목이 없습니다."
WSTR s_kakao_exe,   `C:\Program Files (x86)\Kakao\KakaoTalk\KakaoTalk.exe`
WSTR s_kakao_wnd,   "EVA_Window_DClose"
WSTR s_kakao_title, "카카오톡"
WSTR s_proto_sms,   "ms-phone:"
WSTR s_proto_share, "ms-crossdevice-share:"
WSTR s_mapi32,       "mapi32.dll"
s_mapisendmailw:    db "MAPISendMailW", 0
WSTR s_mail_sub,     "BeCompare 파일 공유"
WSTR f_mailto,       "mailto:?subject=BeCompare%20File%20Share"
WSTR f_find_ready_chk, "파일 찾기 결과 %u개 (%u개 선택됨) · 색인 %llu개"
%macro EXTS 1-*
  %rep %0
    dw __utf16__(%1), 0
    %rotate 1
  %endrep
    dw 0
%endmacro
fx_doc:  EXTS "txt","md","rtf","doc","docx","xls","xlsx","ppt","pptx","pdf","hwp","hwpx","csv","odt","ods","odp"
fx_img:  EXTS "png","jpg","jpeg","gif","bmp","webp","tif","tiff","svg","ico","heic","psd"
fx_med:  EXTS "mp3","flac","wav","ogg","m4a","aac","wma","mp4","mkv","avi","mov","wmv","webm","m4v"
fx_arc:  EXTS "zip","7z","rar","tar","gz","bz2","xz","iso","cab","tgz"
fx_src:  EXTS "c","h","cpp","hpp","cs","java","js","ts","jsx","tsx","py","go","rs","php","rb","swift","kt","asm","inc","sql","sh","ps1","bat","html","css","json","xml","yml","yaml"
fx_exe:  EXTS "exe","dll","msi","com","sys","scr"
align 8
; kind of each row of the combo box: { kind for the search, extension list }
fk_tab:  dq 0, 0,  1, 0,  2, 0,  1, fx_doc,  1, fx_img,  1, fx_med,  1, fx_arc,  1, fx_src,  1, fx_exe
fk_names: dq s_fk0, s_fk1, s_fk2, s_fk3, s_fk4, s_fk5, s_fk6, s_fk7, s_fk8
WSTR s_tip_find_btn, "드라이브의 파일 이름을 색인해 찾습니다. 첫 검색 때 색인을 만들고, 그 뒤로는 바로 찾습니다"
WSTR f_find_ready, "파일 찾기 결과 %u개 · 색인 %llu개"
WSTR f_find_busy, "파일 찾기 결과 %u개 · 색인 중 %llu개…"
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
alignb 8
hf_ids:  resq 1                     ; ids of the search results, in the order shown
hf_chk:  resq 1                     ; checked state (1 byte per item in hf_ids)
hf_chk_cnt: resq 1                  ; count of checked items
hf_n:    resq 1
hf_state_il: resq 1                  ; state image list for checkboxes
hf_key:  resq 1                     ; sort column: 0 name, 1 date, 2 type, 3 size, 4 folder
hf_desc: resq 1
hf_init: resq 1
hf_text: resw 1104                  ; text of the cell being drawn
hf_tmp:  resw 1104

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
    add ebp, eax                     ; y of the file search row = bottom of buttons + scl(22)

    ; 6b. File search: [ name ............................ ][ button ]
    ;                  [ drive ][ kind ][ cloud ] ext [....] folder [..........]
    movsxd rbp, ebp
    mov ecx, 26
    call scl
    mov [rsp+240], rax               ; row height (slots 216 .. 248 are used by these rows only)
    mov eax, r12d
    add eax, [rsp+128]
    mov [rsp+216], rax               ; right edge
    mov ecx, 96
    call scl
    mov edi, eax                     ; button width
    mov rax, [rsp+216]
    sub eax, edi
    mov [rsp+232], rax
    invoke GetDlgItem, rsi, ID_FIND_BTN
    mov [rsp+248], rax
    invoke MoveWindow, [rsp+248], [rsp+232], rbp, rdi, [rsp+240], 1
    mov ecx, 8
    call scl
    mov rdi, [rsp+232]
    sub edi, eax
    sub edi, r12d                    ; the name field takes what is left
    invoke GetDlgItem, rsi, ID_FIND_EDIT
    mov [rsp+248], rax
    invoke MoveWindow, [rsp+248], r12, rbp, rdi, [rsp+240], 1
    ; the clear button sits inside the right end of the name field
    mov ecx, 23
    call scl
    mov ecx, r12d
    add ecx, edi
    sub ecx, eax
    mov [rsp+232], rcx               ; x
    lea rax, [rbp+2]
    mov [rsp+200], rax               ; y
    mov rax, [rsp+240]
    sub rax, 4
    mov [rsp+192], rax               ; height
    mov ecx, 21
    call scl
    mov edi, eax                     ; width
    invoke GetDlgItem, rsi, ID_FIND_CLR
    mov [rsp+248], rax
    invoke MoveWindow, [rsp+248], [rsp+232], [rsp+200], rdi, [rsp+192], 1
    mov ecx, 32
    call scl
    add ebp, eax                     ; second row
    mov eax, r12d
    mov [rsp+232], rax               ; x of the next control
%macro FLEFT 2-3                     ; control id, width [, height: a combo box takes the height of its list]
    mov ecx, %2
    call scl
    mov edi, eax
    invoke GetDlgItem, rsi, %1
    mov [rsp+248], rax
  %if %0 == 3
    mov ecx, %3
    call scl
    mov [rsp+224], rax
    invoke MoveWindow, [rsp+248], [rsp+232], rbp, rdi, [rsp+224], 1
  %else
    invoke MoveWindow, [rsp+248], [rsp+232], rbp, rdi, [rsp+240], 1
  %endif
    mov ecx, 8
    call scl
    add eax, edi
    add qword [rsp+232], rax         ; width + gap
%endmacro
    FLEFT ID_FIND_DRIVE, 130, 300
    FLEFT ID_FIND_KIND, 120, 300
    FLEFT ID_FIND_CLOUD, 130, 200
    FLEFT ID_FIND_LEXT, 44
    FLEFT ID_FIND_EXT, 120
    FLEFT ID_FIND_LPATH, 30
    ; ... [ folder field ][...]  [ reset ]: the field takes what the two buttons leave
    mov ecx, 96
    call scl
    mov [rsp+208], rax               ; reset button
    mov ecx, 28
    call scl
    mov [rsp+200], rax               ; folder button
    mov ecx, 8
    call scl
    mov [rsp+192], rax               ; gap
    mov rdi, [rsp+216]
    sub rdi, [rsp+232]
    sub rdi, [rsp+208]
    sub rdi, [rsp+200]
    sub rdi, [rsp+192]
    sub rdi, [rsp+192]
    cmp edi, 40
    jge .fpw
    mov edi, 40
.fpw:
    invoke GetDlgItem, rsi, ID_FIND_PATH
    mov [rsp+248], rax
    invoke MoveWindow, [rsp+248], [rsp+232], rbp, rdi, [rsp+240], 1
    mov rax, [rsp+232]
    add rax, rdi
    add rax, [rsp+192]
    mov [rsp+232], rax
    invoke GetDlgItem, rsi, ID_FIND_PBROWSE
    mov [rsp+248], rax
    mov rdi, [rsp+200]
    invoke MoveWindow, [rsp+248], [rsp+232], rbp, rdi, [rsp+240], 1
    mov rax, [rsp+216]
    sub rax, [rsp+208]
    mov [rsp+232], rax
    invoke GetDlgItem, rsi, ID_FIND_RESET
    mov [rsp+248], rax
    mov rdi, [rsp+208]
    invoke MoveWindow, [rsp+248], [rsp+232], rbp, rdi, [rsp+240], 1
    mov ecx, 40
    call scl
    add ebp, eax                     ; rlabel_y

    ; Lay out action buttons for find results on the right of rlabel_y:
    mov eax, r12d
    add eax, [rsp+128]
    mov [rsp+232], rax               ; current right edge
    mov ecx, 26
    call scl
    mov [rsp+240], rax               ; button height = scl(26)
    mov ecx, 6
    call scl
    mov [rsp+192], rax               ; gap = scl(6)

%macro FBTN 2
    mov ecx, %2
    call scl
    mov rdi, rax                     ; btn width
    mov rax, [rsp+232]
    sub rax, rdi
    mov [rsp+232], rax               ; new x
    invoke GetDlgItem, rsi, %1
    mov [rsp+248], rax
    invoke MoveWindow, [rsp+248], [rsp+232], rbp, rdi, [rsp+240], 1
    mov rax, [rsp+192]
    sub [rsp+232], rax               ; subtract gap for next button
%endmacro

    FBTN ID_FIND_PROPS, 64
    FBTN ID_FIND_DEL, 64
    FBTN ID_FIND_EMAIL, 68
    FBTN ID_FIND_SHARE, 64
    FBTN ID_FIND_COPY, 64
    FBTN ID_FIND_SELALL, 80

    ; RLABEL
    invoke GetDlgItem, rsi, ID_RLABEL
    mov [rsp+112], rax
    mov ecx, 24
    call scl
    mov [rsp+116], eax               ; rlabel height
    mov rdi, [rsp+232]
    sub edi, r12d                    ; rlabel width
    cmp edi, 150
    jge .lbl_w_ok
    mov edi, 150
.lbl_w_ok:
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
    mov [rsp+116], eax
    invoke MoveWindow, [rsp+112], r12, rbp, rdi, [rsp+116], 1
    invoke GetDlgItem, rsi, ID_FIND_LIST             ; the search results use the same place
    invoke MoveWindow, rax, r12, rbp, rdi, [rsp+116], 1
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
    cmp edi, WM_CTLCOLOREDIT
    je .clist
    cmp edi, WM_TIMER
    je .timer
    cmp edi, WM_CONTEXTMENU
    je .ctxmenu
    cmp edi, WM_NOTIFY
    je .notify
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
    lea rcx, [card_descs]
    mov rdx, [rcx+rbx*8]             ; the card's own description is its tooltip
    invoke tip_add, rax, rdx
    inc ebx
    jmp .mkbtn
.mkdone:
    invoke mk, rsi, &cn_static, &s_rlabel, 0, ID_RLABEL
    mov rbx, rax
    invoke SendMessageW, rbx, WM_SETFONT, [g_fontb], 1

    invoke mk, rsi, &cn_listbox, &s_empty, LBS_NOTIFY|WS_VSCROLL|WS_BORDER|WS_TABSTOP|0x50, ID_RECENT
    invoke mk, rsi, &cn_button, &s_del_btn, BS_OWNERDRAW|WS_TABSTOP, ID_RDEL
    invoke tip_add, rax, &s_tip_rdel
    ; file search: a text field and a button above the list; its results take the place of the recent sessions
    invoke mk, rsi, &cn_edit, &s_empty, WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP|0x04000000, ID_FIND_EDIT    ; WS_CLIPSIBLINGS
    mov rbx, rax
    invoke SendMessageW, rbx, 0x1501, 1, &s_find_cue     ; EM_SETCUEBANNER
    invoke tip_add, rbx, &s_tip_find_edit
    mov ecx, 26
    call scl
    shl eax, 16
    mov r14d, eax
    invoke SendMessageW, rbx, 0x00D3, 2, r14             ; EM_SETMARGINS, EC_RIGHTMARGIN: room for the clear button
    ; a small button at the right end of the field that empties it. A child window is created below its
    ; older siblings, so it is raised above the field: otherwise the field covers it and takes its clicks.
    invoke mk, rsi, &cn_button, &s_find_clr, 0x8000, ID_FIND_CLR               ; BS_FLAT
    mov rbx, rax
    invoke SetWindowPos, rbx, 0, 0, 0, 0, 0, 0x13     ; HWND_TOP; no move, no size, no activation
    invoke ShowWindow, rbx, 0
    invoke tip_add, rbx, &s_tip_find_clr
    invoke mk, rsi, &cn_button, &s_find_dots, WS_TABSTOP, ID_FIND_PBROWSE
    invoke tip_add, rax, &s_tip_find_pbrowse
    invoke mk, rsi, &cn_button, &s_find_reset, WS_TABSTOP, ID_FIND_RESET
    invoke tip_add, rax, &s_tip_find_reset
    invoke mk, rsi, &cn_button, &s_find_btn, WS_TABSTOP, ID_FIND_BTN
    invoke tip_add, rax, &s_tip_find_btn
    invoke mk, rsi, &cn_button, &s_find_selall, WS_TABSTOP, ID_FIND_SELALL
    invoke tip_add, rax, &s_tip_find_selall
    invoke mk, rsi, &cn_button, &s_find_copy, WS_TABSTOP, ID_FIND_COPY
    invoke tip_add, rax, &s_tip_find_copy
    invoke mk, rsi, &cn_button, &s_find_share, WS_TABSTOP, ID_FIND_SHARE
    invoke tip_add, rax, &s_tip_find_share
    invoke mk, rsi, &cn_button, &s_find_email, WS_TABSTOP, ID_FIND_EMAIL
    invoke tip_add, rax, &s_tip_find_email
    invoke mk, rsi, &cn_button, &s_find_del, WS_TABSTOP, ID_FIND_DEL
    invoke tip_add, rax, &s_tip_find_del
    invoke mk, rsi, &cn_button, &s_find_props, WS_TABSTOP, ID_FIND_PROPS
    invoke tip_add, rax, &s_tip_find_props
    ; filters: kind, extensions, folder path
    invoke mk, rsi, &cn_combo, &s_empty, 3|WS_VSCROLL|WS_TABSTOP, ID_FIND_KIND      ; CBS_DROPDOWNLIST
    mov rbx, rax
    xor r14d, r14d
.fk:
    lea rax, [fk_names]
    mov r15, [rax+r14*8]
    invoke SendMessageW, rbx, 0x0143, 0, r15          ; CB_ADDSTRING
    inc r14d
    cmp r14d, 9
    jb .fk
    invoke SendMessageW, rbx, 0x014E, 0, 0            ; CB_SETCURSEL
    invoke tip_add, rbx, &s_tip_find_kind
    invoke mk, rsi, &cn_static, &s_lbl_ext, 0x0202, ID_FIND_LEXT       ; SS_RIGHT | SS_CENTERIMAGE
    invoke mk, rsi, &cn_static, &s_lbl_path, 0x0202, ID_FIND_LPATH
    invoke mk, rsi, &cn_edit, &s_empty, WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP, ID_FIND_EXT
    mov rbx, rax
    invoke SendMessageW, rbx, 0x1501, 1, &s_cue_ext
    invoke tip_add, rbx, &s_tip_find_ext
    invoke mk, rsi, &cn_edit, &s_empty, WS_BORDER|ES_AUTOHSCROLL|WS_TABSTOP, ID_FIND_PATH
    mov rbx, rax
    invoke SendMessageW, rbx, 0x1501, 1, &s_cue_path
    invoke tip_add, rbx, &s_tip_find_path
    invoke mk, rsi, &cn_combo, &s_empty, 3|WS_VSCROLL|WS_TABSTOP, ID_FIND_DRIVE
    mov rbx, rax
    invoke SendMessageW, rbx, 0x0143, 0, &s_fd_all
    invoke SendMessageW, rbx, 0x014E, 0, 0
    invoke tip_add, rbx, &s_tip_find_drive
    invoke mk, rsi, &cn_combo, &s_empty, 3|WS_VSCROLL|WS_TABSTOP, ID_FIND_CLOUD
    mov rbx, rax
    invoke SendMessageW, rbx, 0x0143, 0, &s_fc0
    invoke SendMessageW, rbx, 0x0143, 0, &s_fc1
    invoke SendMessageW, rbx, 0x0143, 0, &s_fc2
    invoke SendMessageW, rbx, 0x014E, 0, 0
    invoke tip_add, rbx, &s_tip_find_cloud
    ; results: a details list like Explorer's; a click on a column header sorts by it
    invoke mk, rsi, &cn_lview, 0, LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER, ID_FIND_LIST
    mov rbx, rax
    invoke ShowWindow, rbx, 0
    invoke SendMessageW, rbx, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_CHECKBOXES
    invoke lv_dark_header, rbx
    invoke home_find_init_il, rbx
    xor r14d, r14d
.fcol:
    xor eax, eax
    mov [rsp+96], rax
    mov [rsp+104], rax
    mov [rsp+112], rax
    mov [rsp+120], rax
    mov [rsp+128], rax
    mov dword [rsp+96], LVCF_FMT|LVCF_WIDTH|LVCF_TEXT
    lea rax, [fcol_fmt]
    mov eax, [rax+r14*4]
    mov [rsp+100], eax
    lea rax, [fcol_w]
    mov ecx, [rax+r14*4]
    call scl
    mov [rsp+104], eax
    lea rax, [fcol_text]
    mov rax, [rax+r14*8]
    mov [rsp+112], rax
    invoke SendMessageW, rbx, LVM_INSERTCOLUMNW, r14, &rsp+96
    inc r14d
    cmp r14d, 5
    jb .fcol
    cmp qword [hf_init], 0
    jne .fsorted
    mov qword [hf_init], 1
    mov qword [hf_key], 1            ; newest first
    mov qword [hf_desc], 1
.fsorted:
    invoke home_find_theme, rsi
    invoke home_find_show_actions, rsi, 0
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
    cmp eax, ID_FIND_BTN
    je .find
    cmp eax, ID_FIND_EDIT
    je .find_edit
    cmp eax, ID_FIND_EXT
    je .find_edit
    cmp eax, ID_FIND_PATH
    je .find_edit
    cmp eax, ID_FIND_KIND
    je .find_kind
    cmp eax, ID_FIND_DRIVE
    je .find_kind
    cmp eax, ID_FIND_CLOUD
    je .find_kind
    cmp eax, ID_FIND_CLR
    je .find_clr
    cmp eax, ID_FIND_RESET
    je .find_reset
    cmp eax, ID_FIND_PBROWSE
    je .find_browse
    cmp eax, ID_FIND_SELALL
    je .find_selall
    cmp eax, ID_FIND_COPY
    je .find_copy
    cmp eax, ID_FIND_SHARE
    je .find_share
    cmp eax, ID_FIND_EMAIL
    je .find_email
    cmp eax, ID_FIND_DEL
    je .find_del
    cmp eax, ID_FIND_PROPS
    je .find_props
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
    mov r14, rax
    invoke home_find_open, rbx, r14   ; own buffer: a path is longer than this frame has room for
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
    invoke home_find_theme, rsi
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

.find_kind:
    mov rax, r12
    shr eax, 16
    cmp eax, 1                        ; CBN_SELCHANGE
    jne .def
    jmp .find
.ctxmenu:
    invoke GetDlgItem, rsi, ID_FIND_LIST
    cmp rax, r12                      ; right click (or the menu key) on the list of found items
    jne .def
    invoke home_find_menu, rsi, r12, r13
    xor eax, eax
    EPILOG 192
.find_edit:
    mov rax, r12
    shr eax, 16
    cmp eax, 0x0300                   ; EN_CHANGE: search a moment after the last key
    jne .def
    invoke home_find_x, rsi
    invoke SetTimer, rsi, FIND_TIMER2, 250, 0
    xor eax, eax
    EPILOG 192
.find_clr:                            ; the x in the name field: empty it and keep typing there
    invoke GetDlgItem, rsi, ID_FIND_EDIT
    mov rbx, rax
    invoke SetWindowTextW, rbx, &s_empty
    invoke SetFocus, rbx
    invoke home_find_x, rsi
    jmp .find
.find_reset:                          ; every filter back to its first state; the name stays
    invoke home_find_reset, rsi
    jmp .find
.find_browse:                         ; choose the folder for the folder filter
    invoke home_find_browse, rsi
    test eax, eax
    jz .none
    jmp .find
.find_selall:
    invoke home_find_selall, rsi
    xor eax, eax
    EPILOG 192
.find_copy:
    invoke home_find_do_copy, rsi
    xor eax, eax
    EPILOG 192
.find_share:
    invoke home_find_do_share, rsi
    xor eax, eax
    EPILOG 192
.find_email:
    invoke home_find_do_email, rsi
    xor eax, eax
    EPILOG 192
.find_del:
    invoke home_find_do_del, rsi
    xor eax, eax
    EPILOG 192
.find_props:
    invoke home_find_do_props, rsi
    xor eax, eax
    EPILOG 192
.find:
    invoke KillTimer, rsi, FIND_TIMER2
    invoke home_find, rsi
    xor eax, eax
    EPILOG 192
.timer:
    cmp r12, FIND_TIMER2
    je .find
    cmp r12, FIND_TIMER               ; while the drives are being listed the results are refreshed
    jne .def
    ; ... but not while rows are checked or selected: the refresh puts other files on those rows (a menu or
    ; a Delete that follows would act on them) and wipes the check marks. It runs again once they are gone.
    cmp qword [hf_chk_cnt], 0
    jne .tskip
    invoke GetDlgItem, rsi, ID_FIND_LIST
    invoke SendMessageW, rax, 0x1032, 0, 0            ; LVM_GETSELECTEDCOUNT
    test eax, eax
    jnz .tskip
    invoke home_find, rsi
.tskip:
    xor eax, eax
    EPILOG 192
.notify:
    mov rax, [r13+NMH_ID]
    cmp eax, ID_FIND_LIST
    jne .def
    mov eax, [r13+NMH_CODE]
    cmp eax, LVN_GETDISPINFOW
    je .n_disp
    cmp eax, LVN_COLUMNCLICK
    je .n_col
    cmp eax, NM_DBLCLK
    je .n_open
    cmp eax, -4                       ; NM_RETURN
    je .n_enter
    cmp eax, -2                       ; NM_CLICK
    je .n_click
    cmp eax, -155                     ; LVN_KEYDOWN
    je .n_keydown
    cmp eax, -110                     ; LVN_ITEMCHANGED
    je .n_selchange
    cmp eax, -113                     ; LVN_ODSTATECHANGED
    je .n_selchange
    jmp .def
.n_selchange:
    invoke home_find_update_chk_lbl, rsi
    xor eax, eax
    EPILOG 192
.n_click:
    mov eax, [r13+32]                 ; ptAction.x
    mov [rsp+96], eax
    mov eax, [r13+36]                 ; ptAction.y
    mov [rsp+100], eax
    invoke SendMessageW, [r13+NMH_FROM], 0x1012, 0, &rsp+96   ; LVM_HITTEST
    cmp eax, -1
    je .none
    test dword [rsp+104], 8          ; LVHT_ONITEMSTATEICON
    jz .none
    cmp rax, [hf_n]
    jae .none
    mov rcx, [hf_chk]
    test rcx, rcx
    jz .none
    xor byte [rcx+rax], 1
    invoke SendMessageW, [r13+NMH_FROM], 0x1015, rax, rax     ; LVM_REDRAWITEMS
    invoke home_find_update_chk_lbl, rsi
    xor eax, eax
    EPILOG 192
.n_keydown:
    movzx eax, word [r13+24]         ; NMLVKEYDOWN.wVKey
    cmp eax, 0x20                    ; VK_SPACE
    je .k_space
    cmp eax, 0x2E                    ; VK_DELETE
    je .k_del
    cmp eax, 'A'
    je .k_a
    cmp eax, 'a'
    je .k_a
    cmp eax, 'C'
    je .k_c
    cmp eax, 'c'
    je .k_c
    cmp eax, 0x0D                    ; VK_RETURN
    jne .def
    invoke GetAsyncKeyState, 0x12    ; VK_MENU (Alt)
    test ax, 0x8000
    jz .def
    invoke home_find_do_props, rsi
    xor eax, eax
    EPILOG 192
.k_space:
    invoke SendMessageW, [r13+NMH_FROM], 0x1032, 0, 0 ; LVM_GETSELECTEDCOUNT
    cmp eax, 1
    jle .k_space_single
    mov r12, [r13+NMH_FROM]
    mov r14, -1
.k_sp_loop:
    invoke SendMessageW, r12, LVM_GETNEXTITEM, r14, 2 ; LVNI_SELECTED
    cmp eax, -1
    je .k_sp_done
    movsxd r14, eax
    cmp r14, [hf_n]
    jae .k_sp_done
    mov rcx, [hf_chk]
    test rcx, rcx
    jz .k_sp_done
    xor byte [rcx+r14], 1
    invoke SendMessageW, r12, 0x1015, r14, r14 ; LVM_REDRAWITEMS
    jmp .k_sp_loop
.k_sp_done:
    invoke home_find_update_chk_lbl, rsi
    xor eax, eax
    EPILOG 192
.k_space_single:
    invoke SendMessageW, [r13+NMH_FROM], LVM_GETNEXTITEM, -1, 1 ; LVNI_FOCUSED
    cmp eax, -1
    jne .k_toggle
    invoke SendMessageW, [r13+NMH_FROM], LVM_GETNEXTITEM, -1, 2 ; LVNI_SELECTED
    cmp eax, -1
    je .none
.k_toggle:
    cmp rax, [hf_n]
    jae .none
    mov rcx, [hf_chk]
    test rcx, rcx
    jz .none
    xor byte [rcx+rax], 1
    invoke SendMessageW, [r13+NMH_FROM], 0x1015, rax, rax     ; LVM_REDRAWITEMS
    invoke home_find_update_chk_lbl, rsi
    xor eax, eax
    EPILOG 192
.k_del:
    invoke home_find_do_del, rsi
    xor eax, eax
    EPILOG 192
.k_a:
    invoke GetAsyncKeyState, 0x11    ; VK_CONTROL
    test ax, 0x8000
    jz .def
    invoke home_find_selall, rsi
    xor eax, eax
    EPILOG 192
.k_c:
    invoke GetAsyncKeyState, 0x11    ; VK_CONTROL
    test ax, 0x8000
    jz .def
    invoke home_find_do_copy, rsi
    xor eax, eax
    EPILOG 192
.n_disp:
    invoke home_find_disp, r13
    xor eax, eax
    EPILOG 192
.n_col:
    mov ebx, [r13+28]                 ; NMLISTVIEW.iSubItem
    invoke home_find_col, rsi, rbx
    xor eax, eax
    EPILOG 192
.n_open:
    movsxd rbx, dword [r13+24]        ; NMITEMACTIVATE.iItem
    test rbx, rbx
    js .none
    mov eax, [r13+32]
    mov [rsp+96], eax
    mov eax, [r13+36]
    mov [rsp+100], eax
    invoke SendMessageW, [r13+NMH_FROM], 0x1012, 0, &rsp+96   ; LVM_HITTEST
    test dword [rsp+104], 8          ; LVHT_ONITEMSTATEICON
    jnz .none
    invoke home_find_act, rbx, FA_OPEN
    xor eax, eax
    EPILOG 192
.n_enter:
    invoke SendMessageW, [r13+NMH_FROM], LVM_GETNEXTITEM, -1, 2      ; LVNI_SELECTED
    movsxd rbx, eax
    test rbx, rbx
    js .none
    invoke home_find_act, rbx, FA_OPEN
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

; ---------------------------------------------------------------- file search
; home_find_x(hwnd): the clear button of the name field shows only while there is something to clear
home_find_x:
    PROLOG 64
    mov rsi, rcx
    invoke GetDlgItem, rsi, ID_FIND_EDIT
    invoke GetWindowTextLengthW, rax
    xor ebx, ebx
    test eax, eax
    jz .s
    mov ebx, 5
.s:
    invoke GetDlgItem, rsi, ID_FIND_CLR
    invoke ShowWindow, rax, rbx
    EPILOG 64

; home_find_reset(hwnd): all drives, every kind, cloud included, no extensions, no folder
home_find_reset:
    PROLOG 64
    mov rsi, rcx
    invoke GetDlgItem, rsi, ID_FIND_DRIVE
    invoke SendMessageW, rax, 0x014E, 0, 0            ; CB_SETCURSEL
    invoke GetDlgItem, rsi, ID_FIND_KIND
    invoke SendMessageW, rax, 0x014E, 0, 0
    invoke GetDlgItem, rsi, ID_FIND_CLOUD
    invoke SendMessageW, rax, 0x014E, 0, 0
    invoke GetDlgItem, rsi, ID_FIND_EXT
    invoke SetWindowTextW, rax, &s_empty
    invoke GetDlgItem, rsi, ID_FIND_PATH
    invoke SetWindowTextW, rax, &s_empty
    EPILOG 64

; home_find_browse(hwnd) -> eax 1 when a folder was chosen: its path becomes the folder filter
home_find_browse:
    PROLOG 1200
    mov rsi, rcx
    mov word [rsp+96], 0
    invoke browse_folder, rsi, &rsp+96
    test eax, eax
    jz .no
    cmp word [rsp+96], 0
    je .no
    invoke GetDlgItem, rsi, ID_FIND_PATH
    invoke SetWindowTextW, rax, &rsp+96
    mov eax, 1
    EPILOG 1200
.no:
    xor eax, eax
    EPILOG 1200

; home_find_init_il(hList): creates state image list with checkbox states (1=unchecked, 2=checked)
home_find_init_il:
    PROLOG 256
    mov r12, rcx                     ; r12 = hList
    cmp qword [hf_state_il], 0
    jne .set_il
    mov ecx, 16
    call scl
    movsxd rbx, eax                  ; rbx = size (e.g. 16 or scaled)
    invoke ImageList_Create, rbx, rbx, 0x0021, 3, 1  ; ILC_MASK | ILC_COLOR32
    test rax, rax
    jz .d
    mov [hf_state_il], rax

    invoke GetDC, 0
    mov r13, rax                     ; r13 = hdcScreen
    invoke CreateCompatibleDC, r13
    mov r14, rax                     ; r14 = hdcMem
    invoke CreateCompatibleBitmap, r13, rbx, rbx
    mov r15, rax                     ; r15 = hbm
    invoke SelectObject, r14, r15
    mov [rsp+128], rax               ; [rsp+128] = oldBm

    invoke CreateSolidBrush, 0x00FF00FF ; magenta mask brush
    mov rbp, rax

    ; RECT rc = { 0, 0, ebx, ebx }
    mov dword [rsp+96], 0
    mov dword [rsp+100], 0
    mov [rsp+104], ebx
    mov [rsp+108], ebx

    ; Image 0: dummy
    invoke FillRect, r14, &rsp+96, rbp
    invoke ImageList_AddMasked, [hf_state_il], r15, 0x00FF00FF

    ; Image 1: unchecked checkbox
    invoke FillRect, r14, &rsp+96, rbp
    invoke DrawFrameControl, r14, &rsp+96, 4, 0x4000 ; DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_FLAT
    invoke ImageList_AddMasked, [hf_state_il], r15, 0x00FF00FF

    ; Image 2: checked checkbox
    invoke FillRect, r14, &rsp+96, rbp
    invoke DrawFrameControl, r14, &rsp+96, 4, 0x4400 ; DFC_BUTTON, DFCS_BUTTONCHECK | DFCS_CHECKED | DFCS_FLAT
    invoke ImageList_AddMasked, [hf_state_il], r15, 0x00FF00FF

    invoke DeleteObject, rbp
    invoke SelectObject, r14, [rsp+128]
    invoke DeleteObject, r15
    invoke DeleteDC, r14
    invoke ReleaseDC, 0, r13

.set_il:
    invoke SendMessageW, r12, 0x1003, 2, [hf_state_il] ; LVM_SETIMAGELIST, LVSIL_STATE (2), himl
.d:
    EPILOG 256

; home_find_theme(hwnd): colours of the result list
home_find_theme:
    PROLOG 64
    invoke GetDlgItem, rcx, ID_FIND_LIST
    test rax, rax
    jz .d
    mov rbx, rax
    invoke SendMessageW, rbx, LVM_SETBKCOLOR, 0, #[th_bg]
    invoke SendMessageW, rbx, LVM_SETTEXTBKCOLOR, 0, #[th_bg]
    invoke SendMessageW, rbx, LVM_SETTEXTCOLOR, 0, #[th_fg]
.d:
    EPILOG 64

; home_find_drives(hwnd): the drive filter lists "all" and every listed tree; filled in as they appear
home_find_drives:
    PROLOG 1296
    invoke GetDlgItem, rcx, ID_FIND_DRIVE
    mov rbx, rax
    call find_volcount
    mov r12d, eax
    invoke SendMessageW, rbx, 0x0146, 0, 0            ; CB_GETCOUNT
    lea ecx, [r12+1]
    cmp eax, ecx
    je .d
    invoke SendMessageW, rbx, 0x0147, 0, 0            ; CB_GETCURSEL
    movsxd r13, eax
    invoke SendMessageW, rbx, 0x014B, 0, 0            ; CB_RESETCONTENT
    invoke SendMessageW, rbx, 0x0143, 0, &s_fd_all
    xor r14d, r14d
.l:
    cmp r14d, r12d
    jae .sel
    invoke find_volname, r14, &rsp+96, 590
    invoke SendMessageW, rbx, 0x0143, 0, &rsp+96
    inc r14d
    jmp .l
.sel:
    test r13, r13
    jns .s
    xor r13d, r13d
.s:
    invoke SendMessageW, rbx, 0x014E, r13, 0          ; CB_SETCURSEL
.d:
    EPILOG 1296

; home_find(hwnd): search for the text of the field, narrowed by the filters, sort and list what was found.
;   With the name and every filter empty the recent sessions are shown again. The first search starts
;   listing the drives; while that runs, a timer repeats the search so that the list fills up.
%define HF_TXT  96                   ; wchar[200] -> 496
%define HF_LBL  496                  ; wchar[100] -> 696
%define HF_ST   696                  ; state of the listing before the search
%define HF_EXT  704                  ; wchar[100]: extensions typed, turned into a list -> 904
%define HF_PTX  904                  ; wchar[120]: folder filter -> 1144
%define HF_FLT  1144                 ; filter for find_query2 (32) -> 1176
home_find:
    PROLOG 1184
    mov rsi, rcx
    cmp qword [hf_ids], 0
    jne .have
    invoke malloc, HF_CAP*4
    test rax, rax
    jz .ret
    mov [hf_ids], rax
.have:
    cmp qword [hf_chk], 0
    jne .have_chk
    invoke malloc, HF_CAP
    test rax, rax
    jz .ret
    mov [hf_chk], rax
.have_chk:
    invoke GetDlgItem, rsi, ID_FIND_EDIT
    invoke GetWindowTextW, rax, &rsp+HF_TXT, 199
    invoke GetDlgItem, rsi, ID_FIND_EXT
    invoke GetWindowTextW, rax, &rsp+HF_EXT, 97
    invoke GetDlgItem, rsi, ID_FIND_PATH
    invoke GetWindowTextW, rax, &rsp+HF_PTX, 119
    invoke GetDlgItem, rsi, ID_FIND_KIND
    invoke SendMessageW, rax, 0x0147, 0, 0            ; CB_GETCURSEL
    movsxd r14, eax
    cmp r14, 8
    jbe .kind
    xor r14d, r14d
.kind:
    shl r14, 4
    lea rax, [fk_tab]
    mov rcx, [rax+r14]
    mov [rsp+HF_FLT], rcx            ; kind
    mov rcx, [rax+r14+8]
    mov [rsp+HF_FLT+8], rcx          ; extensions of the kind
    lea rax, [rsp+HF_PTX]
    mov [rsp+HF_FLT+16], rax
    invoke GetDlgItem, rsi, ID_FIND_DRIVE
    invoke SendMessageW, rax, 0x0147, 0, 0
    test eax, eax
    jns .drv
    xor eax, eax
.drv:
    mov [rsp+HF_FLT+24], eax         ; drive: 0 all, n = the n-th listed tree
    invoke GetDlgItem, rsi, ID_FIND_CLOUD
    invoke SendMessageW, rax, 0x0147, 0, 0
    cmp eax, 2
    jbe .cld
    xor eax, eax
.cld:
    mov [rsp+HF_FLT+28], eax         ; cloud storage: 0 included, 1 left out, 2 only
    ; extensions typed: "pdf; *.docx , .txt" -> "pdf" "docx" "txt" "" (in place, never longer than the text)
    lea rdi, [rsp+HF_EXT]
    xor ecx, ecx                     ; read
    xor edx, edx                     ; write
.xs:
    movzx eax, word [rdi+rcx*2]
    test eax, eax
    jz .xend
    cmp eax, ';'
    je .xskip
    cmp eax, ','
    je .xskip
    cmp eax, ' '
    je .xskip
    cmp eax, '*'
    je .xskip
    cmp eax, '.'
    jne .xitem
.xskip:
    inc ecx
    jmp .xs
.xitem:
    movzx eax, word [rdi+rcx*2]
    test eax, eax
    jz .xclose
    cmp eax, ';'
    je .xclose
    cmp eax, ','
    je .xclose
    cmp eax, ' '
    je .xclose
    mov [rdi+rdx*2], ax
    inc ecx
    inc edx
    jmp .xitem
.xclose:
    mov word [rdi+rdx*2], 0
    inc edx
    test eax, eax
    jz .xend
    inc ecx
    jmp .xs
.xend:
    mov word [rdi+rdx*2], 0
    mov r15d, edx                    ; characters of the list
    test r15d, r15d
    jz .noext
    invoke CharLowerBuffW, rdi, r15
    mov [rsp+HF_FLT+8], rdi          ; typed extensions replace those of the kind
    cmp qword [rsp+HF_FLT], 0
    jne .noext
    mov qword [rsp+HF_FLT], 1        ; an extension means files
.noext:
    invoke GetDlgItem, rsi, ID_FIND_LIST
    mov r12, rax
    invoke GetDlgItem, rsi, ID_RECENT
    mov r13, rax
    ; nothing typed and no filter: back to the recent sessions
    movzx eax, word [rsp+HF_TXT]
    movzx ecx, word [rsp+HF_PTX]
    or eax, ecx
    or rax, [rsp+HF_FLT]
    or rax, [rsp+HF_FLT+8]
    or rax, [rsp+HF_FLT+24]
    jnz .q
    invoke KillTimer, rsi, FIND_TIMER
    mov qword [hf_n], 0
    invoke SendMessageW, r12, LVM_SETITEMCOUNT, 0, 0
    invoke ShowWindow, r12, 0
    invoke ShowWindow, r13, 5
    invoke GetDlgItem, rsi, ID_RDEL
    invoke ShowWindow, rax, 5
    invoke home_find_show_actions, rsi, 0
    invoke GetDlgItem, rsi, ID_RLABEL
    invoke SetWindowTextW, rax, &s_rlabel
    mov qword [hf_chk_cnt], 0
    EPILOG 1184
.q:
    call find_start
    ; the state from before the search decides whether to search again: a listing that ends while this
    ; search runs would otherwise leave results of a half-filled index on the screen
    mov rax, [fi_state]
    mov [rsp+HF_ST], rax
    invoke find_query2, &rsp+HF_TXT, [hf_ids], HF_CAP, &rsp+HF_FLT
    mov r14d, eax                    ; names found
    mov r15d, eax
    cmp r15d, HF_CAP
    jbe .c
    mov r15d, HF_CAP
.c:
    mov [hf_n], r15
    mov qword [hf_chk_cnt], 0
    cmp qword [hf_chk], 0
    jz .skip_memset
    invoke memset, [hf_chk], 0, r15
.skip_memset:
    invoke home_find_resort, rsi
    invoke SendMessageW, r12, LVM_SETITEMCOUNT, r15, 0
    invoke InvalidateRect, r12, 0, 1
    invoke ShowWindow, r13, 0
    invoke ShowWindow, r12, 5
    invoke GetDlgItem, rsi, ID_RDEL                  ; "clear history" belongs to the recent sessions
    invoke ShowWindow, rax, 0
    invoke home_find_show_actions, rsi, 5
    invoke home_find_drives, rsi
    lea rdi, [f_find_ready]
    cmp r14d, HF_CAP
    jbe .rdy
    lea rdi, [f_find_capped]
.rdy:
    cmp qword [rsp+HF_ST], 2
    je .lb
    lea rdi, [f_find_busy]
.lb:
    mov rbx, [fi_vis]
    invoke _snwprintf, &rsp+HF_LBL, 99, rdi, r14, rbx
    invoke GetDlgItem, rsi, ID_RLABEL
    invoke SetWindowTextW, rax, &rsp+HF_LBL
    cmp qword [rsp+HF_ST], 2
    je .kill
    invoke SetTimer, rsi, FIND_TIMER, 700, 0
    EPILOG 1184
.kill:
    invoke KillTimer, rsi, FIND_TIMER
.ret:
    EPILOG 1184

; home_find_resort(hwnd): put the results in the order of the sort column and mark that column's header
%define HR_HD   80                   ; HDITEMW (72) -> 152
%define HR_TX   160                  ; wchar[44]: header text with its arrow -> 248
home_find_resort:
    PROLOG 256
    mov rsi, rcx
    mov r12, [hf_key]
    cmp r12, 4
    jne .s
    cmp qword [hf_n], HF_DIRSORT
    jbe .s
    invoke set_status, &s_find_toomany
    xor r12d, r12d
.s:
    invoke find_sort, [hf_ids], [hf_n], r12, [hf_desc]
    invoke GetDlgItem, rsi, ID_FIND_LIST
    mov r13, rax
    invoke InvalidateRect, r13, 0, 1
    invoke SendMessageW, r13, 0x101F, 0, 0            ; LVM_GETHEADER
    test rax, rax
    jz .d
    mov r14, rax
    xor ebx, ebx
.h:
    cmp ebx, 5
    jae .d
    xor eax, eax
    mov ecx, 9
    lea rdi, [rsp+HR_HD]
.z:
    mov [rdi], rax
    add rdi, 8
    dec ecx
    jnz .z
    mov dword [rsp+HR_HD], 4         ; HDI_FORMAT
    invoke SendMessageW, r14, 0x120B, rbx, &rsp+HR_HD  ; HDM_GETITEMW
    mov eax, [rsp+HR_HD+28]
    and eax, ~0x600                  ; neither arrow
    cmp rbx, [hf_key]
    jne .w
    or eax, 0x400                    ; HDF_SORTUP
    cmp qword [hf_desc], 0
    je .w
    xor eax, 0x600                   ; HDF_SORTDOWN
.w:
    mov [rsp+HR_HD+28], eax
    ; the arrow is also written into the header text: the themed header does not draw the sort mark
    lea rax, [fcol_text]
    mov r15, [rax+rbx*8]
    lea r12, [s_empty]
    cmp rbx, [hf_key]
    jne .t
    lea r12, [s_arr_up]
    cmp qword [hf_desc], 0
    je .t
    lea r12, [s_arr_dn]
.t:
    invoke _snwprintf, &rsp+HR_TX, 40, &f_colhdr, r15, r12
    lea rax, [rsp+HR_TX]
    mov [rsp+HR_HD+8], rax
    mov dword [rsp+HR_HD], 6         ; HDI_TEXT | HDI_FORMAT
    invoke SendMessageW, r14, 0x120C, rbx, &rsp+HR_HD  ; HDM_SETITEMW
    inc ebx
    jmp .h
.d:
    EPILOG 256

; home_find_col(hwnd, column): a click on a column header - sort by it; a second click turns the order.
;   Dates and sizes start with the largest, names, types and folders with A.
home_find_col:
    PROLOG 192
    mov rsi, rcx
    mov ebx, edx
    cmp ebx, 4
    ja .d
    cmp rbx, [hf_key]
    jne .new
    xor qword [hf_desc], 1
    jmp .go
.new:
    mov [hf_key], rbx
    mov qword [hf_desc], 0
    cmp ebx, 1
    je .dn
    cmp ebx, 3
    jne .go
.dn:
    mov qword [hf_desc], 1
.go:
    ; the check marks and the selection belong to row numbers: after the sort they would sit on other files
    ; (and Delete would take those), so both are cleared
    mov qword [hf_chk_cnt], 0
    cmp qword [hf_chk], 0
    je .nochk
    invoke memset, [hf_chk], 0, [hf_n]
.nochk:
    invoke GetDlgItem, rsi, ID_FIND_LIST
    mov rdi, rax
    invoke memset, &rsp+96, 0, 88    ; LVITEMW: state 0 ...
    mov dword [rsp+96+16], 2         ; ... for LVIS_SELECTED
    invoke SendMessageW, rdi, 0x102B, -1, &rsp+96     ; LVM_SETITEMSTATE, every item
    invoke home_find_resort, rsi
    invoke home_find_update_chk_lbl, rsi
.d:
    EPILOG 192

; home_find_disp(NMLVDISPINFOW*): text of one cell of the result list
home_find_disp:
    PROLOG 160
    mov rbx, rcx
    test dword [rbx+DI_MASK], 8      ; LVIF_STATE
    jz .chk_text
    mov eax, [rbx+DI_ITEM]
    cmp rax, [hf_n]
    jae .chk_uncheck
    mov rcx, [hf_chk]
    test rcx, rcx
    jz .chk_uncheck
    cmp byte [rcx+rax], 0
    je .chk_uncheck
    mov dword [rbx+36], 0x2000       ; checked (state image 2)
    mov dword [rbx+40], 0xF000
    jmp .chk_text
.chk_uncheck:
    mov dword [rbx+36], 0x1000       ; unchecked (state image 1)
    mov dword [rbx+40], 0xF000
.chk_text:
    test dword [rbx+DI_MASK], LVIF_TEXT
    jz .d
    mov eax, [rbx+DI_ITEM]
    cmp rax, [hf_n]
    jae .d
    mov rcx, [hf_ids]
    mov esi, [rcx+rax*4]             ; id
    lea rdi, [hf_text]
    mov word [rdi], 0
    mov eax, [rbx+DI_SUB]
    cmp eax, 1
    je .date
    cmp eax, 2
    je .type
    cmp eax, 3
    je .size
    cmp eax, 4
    je .dir
    invoke find_name, rsi, rdi, 1000
    jmp .set
.date:
    invoke find_time, rsi
    test rax, rax
    jz .set
    mov [rsp+96], rax
    invoke FileTimeToLocalFileTime, &rsp+96, &rsp+104
    invoke FileTimeToSystemTime, &rsp+104, &rsp+112
    movzx r12d, word [rsp+112]       ; year
    movzx r13d, word [rsp+114]       ; month
    movzx r14d, word [rsp+118]       ; day
    movzx r15d, word [rsp+120]       ; hour
    movzx ebp, word [rsp+122]        ; minute
    invoke _snwprintf, rdi, 40, &f_find_date, r12, r13, r14, r15, rbp
    jmp .set
.type:
    invoke find_flags, rsi
    test eax, 1
    jz .tf
    lea rdi, [s_ft_dir]
    jmp .set
.tf:
    invoke find_name, rsi, &hf_tmp, 1000
    mov ecx, eax
    lea r8, [hf_tmp]
.td:
    test ecx, ecx
    jz .tn
    dec ecx
    cmp word [r8+rcx*2], '.'
    jne .td
    lea r12, [r8+rcx*2+2]
    cmp word [r12], 0
    je .tn
    invoke _snwprintf, rdi, 200, &f_ft_ext, r12
    jmp .set
.tn:
    lea rdi, [s_ft_file]
    jmp .set
.size:
    invoke find_flags, rsi
    test eax, 1
    jnz .set                         ; folders show no size
    invoke find_time, rsi
    test rax, rax
    jz .set                          ; listed without sizes
    invoke find_size, rsi
    add rax, 1023
    shr rax, 10
    mov r12, rax
    invoke fmt_size, rdi, r12
    invoke wcscat, rdi, &s_kb
    jmp .set
.dir:
    invoke find_parent, rsi
    mov r12d, eax
    invoke find_path, r12, rdi, 1090
.set:
    mov [rbx+DI_TEXT], rdi
.d:
    EPILOG 160

; home_find_act(row, action): do something with the found item of that row
;   FA_OPEN   open it with the program registered for its type (a folder opens in Explorer); when
;             there is none, ask which program to use
;   FA_OPENAS ask which program to use
;   FA_SHOW   open its folder in Explorer with the item selected
;   FA_BC     open it in a session of this application
;   FA_COPY   copy its path to the clipboard
%define FA_PATH 96                   ; wchar[800] -> 1696
%define FA_ARG  1696                 ; wchar[800] -> 3296
home_find_act:
    PROLOG 3312
    mov rsi, rcx
    mov r12d, edx
    cmp rsi, [hf_n]
    jae .d
    mov rax, [hf_ids]
    mov ebx, [rax+rsi*4]
    invoke find_path, rbx, &rsp+FA_PATH, 790
    test eax, eax
    jz .d
    mov r13d, eax
    cmp r12d, FA_OPENAS
    je .openas
    cmp r12d, FA_SHOW
    je .show
    cmp r12d, FA_BC
    je .bc
    cmp r12d, FA_COPY
    je .copy
    invoke ShellExecuteW, [g_hframe], &s_v_open, &rsp+FA_PATH, 0, 0, 1
    cmp rax, 32
    ja .d
    cmp rax, 31                      ; SE_ERR_NOASSOC: no program is registered for this type
    jne .fail
.openas:
    invoke ShellExecuteW, [g_hframe], &s_v_openas, &rsp+FA_PATH, 0, 0, 1
    cmp rax, 32
    ja .d
.fail:
    invoke set_status, &s_noapp
    EPILOG 3312
.show:
    invoke _snwprintf, &rsp+FA_ARG, 790, &f_select, &rsp+FA_PATH
    invoke ShellExecuteW, [g_hframe], &s_v_open, &s_explorer, &rsp+FA_ARG, 0, 1
    EPILOG 3312
.bc:
    ; a saved session or workspace opens as such; anything else starts a new session of the type that
    ; fits it (folder compare for a folder), with the item as the left side
    invoke wcsrchr, &rsp+FA_PATH, '.'
    test rax, rax
    jz .bcnew
    mov r14, rax
    invoke _wcsicmp, r14, &s_x_bcs
    test eax, eax
    jz .bcfile
    invoke _wcsicmp, r14, &s_x_bcw
    test eax, eax
    jnz .bcnew
.bcfile:
    invoke open_any_file, &rsp+FA_PATH
    EPILOG 3312
.bcnew:
    invoke GetFileAttributesW, &rsp+FA_PATH
    cmp eax, -1
    je .fail
    mov ebx, T_FOLDER
    test eax, 0x10
    jnz .bcgo
    invoke type_for_path, &rsp+FA_PATH
    mov ebx, eax
.bcgo:
    invoke add_session, rbx, &rsp+FA_PATH, 0, 0
    EPILOG 3312
.copy:
    lea r14, [r13*2+2]               ; bytes with the terminator
    invoke GlobalAlloc, 2, r14       ; GMEM_MOVEABLE
    test rax, rax
    jz .d
    mov r15, rax
    invoke GlobalLock, r15
    test rax, rax
    jz .d
    invoke memcpy, rax, &rsp+FA_PATH, r14
    invoke GlobalUnlock, r15
    invoke OpenClipboard, [g_hframe]
    test eax, eax
    jz .d
    call EmptyClipboard
    invoke SetClipboardData, 13, r15                  ; CF_UNICODETEXT
    call CloseClipboard
    invoke set_status, &s_copied
.d:
    EPILOG 3312

; home_find_menu(hwnd, list, lparam of WM_CONTEXTMENU): menu of actions for the row under the pointer
%define FN_PT   96                   ; POINT (screen), then RECT for the keyboard case -> 112
%define FN_HT   112                  ; LVHITTESTINFO (24) -> 136
%define FN_IT   136                  ; LVITEMW up to stateMask (24) -> 160
home_find_menu:
    PROLOG 176
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    cmp r12d, -1
    je .key
    movsx eax, r12w
    mov [rsp+FN_PT], eax
    mov rax, r12
    shr eax, 16
    movsx eax, ax
    mov [rsp+FN_PT+4], eax
    xor eax, eax
    mov [rsp+FN_HT+8], rax
    mov [rsp+FN_HT+16], rax
    mov rax, [rsp+FN_PT]
    mov [rsp+FN_HT], rax
    invoke ScreenToClient, rbx, &rsp+FN_HT
    invoke SendMessageW, rbx, 0x1012, 0, &rsp+FN_HT   ; LVM_HITTEST
    movsxd r13, eax
    test r13, r13
    js .cur
    invoke SendMessageW, rbx, 0x102C, r13, 2          ; LVM_GETITEMSTATE, r13, LVIS_SELECTED
    test eax, 2
    jnz .cur
    xor eax, eax
    mov [rsp+FN_IT], rax
    mov [rsp+FN_IT+8], rax
    mov [rsp+FN_IT+16], rax
    mov dword [rsp+FN_IT+12], 3      ; state: selected and focused
    mov dword [rsp+FN_IT+16], 3
    invoke SendMessageW, rbx, 0x102B, r13, &rsp+FN_IT  ; LVM_SETITEMSTATE
    jmp .cur
.key:
    invoke GetWindowRect, rbx, &rsp+FN_PT             ; the menu key: at the top left of the list
.cur:
    invoke SendMessageW, rbx, LVM_GETNEXTITEM, -1, 2  ; LVNI_SELECTED
    movsxd r14, eax
    test r14, r14
    js .d
    call CreatePopupMenu
    mov r15, rax
    invoke AppendMenuW, r15, MF_STRING, FA_OPEN, &s_fm_open
    invoke AppendMenuW, r15, MF_STRING, FA_OPENAS, &s_fm_openas
    invoke AppendMenuW, r15, MF_STRING, FA_SHOW, &s_fm_show
    invoke AppendMenuW, r15, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, r15, MF_STRING, FA_BC, &s_fm_bc
    invoke AppendMenuW, r15, MF_STRING, FA_COPY, &s_fm_copy
    invoke AppendMenuW, r15, MF_STRING, FA_SHARE, &s_fm_share
    invoke AppendMenuW, r15, MF_STRING, FA_EMAIL, &s_fm_email
    invoke AppendMenuW, r15, MF_STRING, FA_DEL, &s_fm_del
    invoke AppendMenuW, r15, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, r15, MF_STRING, FA_PROPS, &s_fm_props
    invoke AppendMenuW, r15, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, r15, MF_STRING, FA_SELALL, &s_fm_selall
    mov edi, [rsp+FN_PT]
    mov ebp, [rsp+FN_PT+4]
    invoke TrackPopupMenu, r15, TPM_RETURNCMD|TPM_RIGHTBUTTON, rdi, rbp, 0, rsi, 0
    mov r13d, eax
    invoke DestroyMenu, r15
    test r13d, r13d
    jz .d
    cmp r13d, FA_COPY
    je .act_copy
    cmp r13d, FA_SHARE
    je .act_share
    cmp r13d, FA_EMAIL
    je .act_email
    cmp r13d, FA_DEL
    je .act_del
    cmp r13d, FA_PROPS
    je .act_props
    cmp r13d, FA_SELALL
    je .act_selall
    invoke home_find_act, r14, r13
    jmp .d
.act_copy:
    invoke home_find_do_copy, rsi
    jmp .d
.act_share:
    invoke home_find_do_share, rsi
    jmp .d
.act_email:
    invoke home_find_do_email, rsi
    jmp .d
.act_del:
    invoke home_find_do_del, rsi
    jmp .d
.act_props:
    invoke home_find_do_props, rsi
    jmp .d
.act_selall:
    invoke home_find_selall, rsi
    jmp .d
.d:
    EPILOG 176

; home_find_open(list, index): open the found file of that row
home_find_open:
    PROLOG 1696
    mov rbx, rcx
    mov rsi, rdx
    invoke SendMessageW, rbx, 0x018A, rsi, 0          ; LB_GETTEXTLEN
    cmp eax, 790
    jae .d
    invoke SendMessageW, rbx, LB_GETTEXT, rsi, &rsp+96
    invoke open_any_file, &rsp+96
.d:
    EPILOG 1696

; home_find_update_chk_lbl(hwnd): count checked items and update ID_RLABEL
home_find_update_chk_lbl:
    PROLOG 256
    mov rsi, rcx
    mov r8, [hf_n]
    test r8, r8
    jz .d
    mov rcx, [hf_chk]
    test rcx, rcx
    jz .d
    xor edx, edx                     ; checked count
    xor eax, eax                     ; index
.chk_cnt_loop:
    cmp rax, r8
    jae .chk_cnt_done
    movzx r9d, byte [rcx+rax]
    add edx, r9d
    inc rax
    jmp .chk_cnt_loop
.chk_cnt_done:
    mov [hf_chk_cnt], rdx
    test edx, edx
    jz .chk_none_maybe_sel
    mov r12, rdx
    mov r13, [fi_vis]
    invoke _snwprintf, &rsp+96, 80, &f_find_ready_chk, [hf_n], r12, r13
    invoke GetDlgItem, rsi, ID_RLABEL
    invoke SetWindowTextW, rax, &rsp+96
    jmp .d
.chk_none_maybe_sel:
    invoke GetDlgItem, rsi, ID_FIND_LIST
    invoke SendMessageW, rax, 0x1032, 0, 0            ; LVM_GETSELECTEDCOUNT
    cmp eax, 1
    jle .chk_none
    movsxd r12, eax
    mov r13, [fi_vis]
    invoke _snwprintf, &rsp+96, 80, &f_find_ready_chk, [hf_n], r12, r13
    invoke GetDlgItem, rsi, ID_RLABEL
    invoke SetWindowTextW, rax, &rsp+96
    jmp .d
.chk_none:
    mov r13, [fi_vis]
    invoke _snwprintf, &rsp+96, 80, &f_find_ready, [hf_n], r13
    invoke GetDlgItem, rsi, ID_RLABEL
    invoke SetWindowTextW, rax, &rsp+96
.d:
    EPILOG 256

; home_find_show_actions(hwnd, show_flag)
home_find_show_actions:
    PROLOG 64
    mov rsi, rcx
    mov ebx, edx
    invoke GetDlgItem, rsi, ID_FIND_SELALL
    invoke ShowWindow, rax, rbx
    invoke GetDlgItem, rsi, ID_FIND_COPY
    invoke ShowWindow, rax, rbx
    invoke GetDlgItem, rsi, ID_FIND_SHARE
    invoke ShowWindow, rax, rbx
    invoke GetDlgItem, rsi, ID_FIND_EMAIL
    invoke ShowWindow, rax, rbx
    invoke GetDlgItem, rsi, ID_FIND_DEL
    invoke ShowWindow, rax, rbx
    invoke GetDlgItem, rsi, ID_FIND_PROPS
    invoke ShowWindow, rax, rbx
    EPILOG 64

; home_find_selall(hwnd)
home_find_selall:
    PROLOG 64
    mov rsi, rcx
    mov r8, [hf_n]
    test r8, r8
    jz .d
    mov rcx, [hf_chk]
    test rcx, rcx
    jz .d
    xor edx, edx
    mov rax, [hf_chk_cnt]
    cmp rax, r8
    je .do_set
    mov dl, 1
.do_set:
    mov rcx, [hf_chk]
    mov r8, [hf_n]
    call memset
    invoke GetDlgItem, rsi, ID_FIND_LIST
    invoke InvalidateRect, rax, 0, 0
    invoke home_find_update_chk_lbl, rsi
.d:
    EPILOG 64

; home_find_get_targets(hwnd, out_ids, max_count) -> eax = count
home_find_get_targets:
    PROLOG 128
    mov rsi, rcx
    mov rdi, rdx
    mov rbp, r8
    xor ebx, ebx                     ; count = 0
    cmp qword [hf_chk_cnt], 0
    jz .check_sel
    mov r8, [hf_n]
    xor ecx, ecx                     ; index
.chk_loop:
    cmp rcx, r8
    jae .done_chk
    cmp rbx, rbp
    jae .done_chk
    mov rax, [hf_chk]
    cmp byte [rax+rcx], 0
    jz .chk_next
    mov rax, [hf_ids]
    mov edx, [rax+rcx*4]
    mov [rdi+rbx*4], edx
    inc rbx
.chk_next:
    inc rcx
    jmp .chk_loop
.done_chk:
    test rbx, rbx
    jnz .ret_count

.check_sel:
    invoke GetDlgItem, rsi, ID_FIND_LIST
    mov r12, rax
    mov r13, -1                      ; item index iterator
.sel_loop:
    cmp rbx, rbp
    jae .done_sel
    invoke SendMessageW, r12, LVM_GETNEXTITEM, r13, 2 ; LVNI_SELECTED
    cmp eax, -1
    je .done_sel
    movsxd r13, eax
    cmp r13, [hf_n]
    jae .done_sel
    mov rax, [hf_ids]
    mov edx, [rax+r13*4]
    mov [rdi+rbx*4], edx
    inc rbx
    jmp .sel_loop
.done_sel:
    test rbx, rbx
    jnz .ret_count

.ret_none:
    xor ebx, ebx
.ret_count:
    mov eax, ebx
    EPILOG 128

; home_find_do_copy(hwnd)
home_find_do_copy:
    PROLOG 1600
    mov rsi, rcx
    invoke malloc, 40000
    test rax, rax
    jz .none
    mov r12, rax                     ; r12 = ids
    invoke home_find_get_targets, rsi, r12, 10000
    test eax, eax
    jz .free_ids
    mov r13d, eax                    ; r13d = count

    imul r14, r13, 1600
    add r14, 128
    invoke malloc, r14
    test rax, rax
    jz .free_ids
    mov r15, rax                     ; r15 = multi_sz_buf

    mov rdi, r15
    xor ebx, ebx
    mov dword [rsp+400], 0           ; paths that could be resolved
.cp_fill:
    cmp ebx, r13d
    jae .cp_fill_done
    mov edx, [r12+rbx*4]
    invoke find_path, rdx, rdi, 790
    test eax, eax
    jz .cp_fill_skip
    lea rdi, [rdi+rax*2+2]
    inc dword [rsp+400]
.cp_fill_skip:
    inc ebx
    jmp .cp_fill
.cp_fill_done:
    mov r13d, [rsp+400]              ; the list holds this many strings: the loops below must not look for more
    mov word [rdi], 0
    add rdi, 2
    sub rdi, r15                     ; rdi = total bytes

    mov qword [rsp+96], 0            ; hDrop
    mov qword [rsp+104], 0           ; hText

    ; Allocate CF_HDROP
    lea rbp, [rdi+20]
    invoke GlobalAlloc, 2, rbp       ; GMEM_MOVEABLE = 2
    test rax, rax
    jz .skip_alloc_drop
    mov [rsp+96], rax
    invoke GlobalLock, rax
    test rax, rax
    jz .skip_alloc_drop
    mov rbx, rax
    mov dword [rbx], 20              ; pFiles
    mov dword [rbx+4], 0             ; pt.x
    mov dword [rbx+8], 0             ; pt.y
    mov dword [rbx+12], 0            ; fNC
    mov dword [rbx+16], 1            ; fWide
    lea rax, [rbx+20]
    invoke memcpy, rax, r15, rdi
    invoke GlobalUnlock, [rsp+96]
.skip_alloc_drop:

    ; Allocate CF_UNICODETEXT
    lea rbp, [rdi + r13*2 + 8]
    invoke GlobalAlloc, 2, rbp
    test rax, rax
    jz .set_clip
    mov [rsp+104], rax
    invoke GlobalLock, rax
    test rax, rax
    jz .set_clip
    mov rbx, rax
    mov rdx, r15
    xor ecx, ecx
.txt_copy:
    cmp ecx, r13d
    jae .txt_done
    movzx eax, word [rdx]
    test eax, eax
    jz .txt_next_file
    mov word [rbx], ax
    add rdx, 2
    add rbx, 2
    jmp .txt_copy
.txt_next_file:
    inc ecx
    cmp ecx, r13d
    jae .txt_no_crlf
    mov word [rbx], 13               ; '\r'
    mov word [rbx+2], 10             ; '\n'
    add rbx, 4
.txt_no_crlf:
    add rdx, 2
    jmp .txt_copy
.txt_done:
    mov word [rbx], 0
    invoke GlobalUnlock, [rsp+104]

.set_clip:
    invoke OpenClipboard, [g_hframe]
    test eax, eax
    jz .free_msz
    call EmptyClipboard
    cmp qword [rsp+96], 0
    jz .skip_hdrop
    invoke SetClipboardData, 15, [rsp+96]
.skip_hdrop:
    cmp qword [rsp+104], 0
    jz .skip_htext
    invoke SetClipboardData, 13, [rsp+104]
.skip_htext:
    call CloseClipboard

    movsxd r13, r13d
    invoke _snwprintf, &rsp+112, 99, &s_status_copied, r13
    invoke set_status, &rsp+112

.free_msz:
    invoke free, r15
.free_ids_ok:
    invoke free, r12
    mov eax, r13d
    EPILOG 1600
.free_ids:
    invoke free, r12
.none:
    xor eax, eax
    EPILOG 1600

; home_find_do_share(hwnd): SNS (카카오톡, 문자 등) 전송
home_find_do_share:
    PROLOG 256
    mov rsi, rcx
    invoke home_find_do_copy, rsi
    test eax, eax
    jz .d

    invoke GetDlgItem, rsi, ID_FIND_SHARE
    mov rbx, rax
    invoke GetWindowRect, rbx, &rsp+96
    mov edi, [rsp+96]                ; left
    mov ebp, [rsp+108]               ; bottom

    call CreatePopupMenu
    mov r12, rax
    invoke AppendMenuW, r12, MF_STRING, 1, &s_sm_kakao
    invoke AppendMenuW, r12, MF_STRING, 2, &s_sm_sms
    invoke AppendMenuW, r12, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, r12, MF_STRING, 3, &s_sm_clip
    invoke AppendMenuW, r12, MF_STRING, 4, &s_sm_nearby

    invoke TrackPopupMenu, r12, TPM_RETURNCMD|TPM_RIGHTBUTTON, rdi, rbp, 0, rsi, 0
    mov r13d, eax
    invoke DestroyMenu, r12

    test r13d, r13d
    jz .d

    cmp r13d, 1
    je .sh_kakao
    cmp r13d, 2
    je .sh_sms
    cmp r13d, 3
    je .sh_clip
    cmp r13d, 4
    je .sh_nearby
    jmp .d

.sh_kakao:
    invoke FindWindowW, &s_kakao_wnd, 0
    test rax, rax
    jnz .act_k
    invoke FindWindowW, 0, &s_kakao_title
    test rax, rax
    jnz .act_k
    invoke ShellExecuteW, [g_hframe], &s_v_open, &s_kakao_exe, 0, 0, 1
    invoke set_status, &s_status_kakao
    jmp .d
.act_k:
    mov rbx, rax
    invoke ShowWindow, rbx, 9        ; SW_RESTORE
    invoke SetForegroundWindow, rbx
    invoke set_status, &s_status_kakao
    jmp .d

.sh_sms:
    invoke ShellExecuteW, [g_hframe], &s_v_open, &s_proto_sms, 0, 0, 1
    invoke set_status, &s_status_sms
    jmp .d

.sh_clip:
    invoke set_status, &s_status_sns_clip
    jmp .d

.sh_nearby:
    invoke ShellExecuteW, [g_hframe], &s_v_open, &s_proto_share, 0, 0, 1
    invoke set_status, &s_status_share
    jmp .d

.d:
    EPILOG 256

; home_find_do_props(hwnd): 속성 창 열기 (Alt+Enter)
home_find_do_props:
    PROLOG 2048
    mov rsi, rcx
    invoke malloc, 4000
    test rax, rax
    jz .d
    mov r12, rax
    invoke home_find_get_targets, rsi, r12, 1000
    test eax, eax
    jz .none_props
    mov r13d, eax

    mov edx, [r12]
    invoke find_path, rdx, &rsp+96, 790
    mov r14d, eax                    ; free() does not keep eax
    invoke free, r12
    test r14d, r14d
    jz .d

    invoke SHObjectProperties, [g_hframe], 2, &rsp+96, 0
    invoke set_status, &s_status_props
    jmp .d

.none_props:
    invoke free, r12
    invoke set_status, &s_status_none
.d:
    EPILOG 2048

; home_find_do_email(hwnd)
home_find_do_email:
    PROLOG 2048
    mov rsi, rcx
    invoke home_find_do_copy, rsi
    test eax, eax
    jz .none_email

    invoke ShellExecuteW, [g_hframe], &s_v_open, &f_mailto, 0, 0, 1
    invoke set_status, &s_status_email
    jmp .d

.none_email:
    invoke set_status, &s_status_none
.d:
    EPILOG 2048

; home_find_do_del(hwnd)
home_find_do_del:
    PROLOG 256
    mov rsi, rcx
    invoke malloc, 40000
    test rax, rax
    jz .d
    mov r12, rax
    invoke home_find_get_targets, rsi, r12, 10000
    test eax, eax
    jz .none_del
    mov r13d, eax

    imul r14, r13, 1600
    add r14, 64
    invoke malloc, r14
    test rax, rax
    jz .none_del
    mov r15, rax

    mov rdi, r15
    xor ebx, ebx
.del_fill:
    cmp ebx, r13d
    jae .del_filled
    mov edx, [r12+rbx*4]
    invoke find_path, rdx, rdi, 790
    test eax, eax
    jz .del_skip
    lea rdi, [rdi+rax*2+2]
.del_skip:
    inc ebx
    jmp .del_fill
.del_filled:
    mov word [rdi], 0

    invoke memset, &rsp+96, 0, 56
    mov rax, [g_hframe]
    mov [rsp+96], rax
    mov dword [rsp+104], 3           ; FO_DELETE
    mov [rsp+112], r15
    mov qword [rsp+120], 0
    mov word [rsp+128], 0x0040       ; FOF_ALLOWUNDO
    invoke SHFileOperationW, &rsp+96
    mov r14d, eax                    ; not 0: failed
    or r14d, [rsp+96+36]             ; fAnyOperationsAborted: cancelled, or not everything was deleted

    invoke free, r15
    invoke free, r12

    invoke home_find, rsi
    invoke home_find_update_chk_lbl, rsi
    test r14d, r14d
    jz .del_ok
    invoke set_status, &s_status_delfail
    jmp .d
.del_ok:
    movsxd r13, r13d
    invoke _snwprintf, &rsp+96, 80, &s_status_deleted, r13
    invoke set_status, &rsp+96
    jmp .d

.none_del:
    invoke free, r12
    invoke set_status, &s_status_none
.d:
    EPILOG 256

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
    lea rax, [s_nolabel]
    mov [rsp+4350], rax              ; label (a call does not keep r8)
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
    mov [rsp+4350], r8
.tdone:

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
