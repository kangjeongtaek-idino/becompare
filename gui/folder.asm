; folder.asm - Folder Compare session (FC-01..FC-07, SY-01..SY-05)
;   part 1: data model + background scan engine     part 2 (folder_ui.inc): view / actions
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"

extern FindFirstFileW, FindNextFileW, FindClose, CreateFileW, ReadFile, CloseHandle, CreateThread
extern Sleep
extern WaitForSingleObject, CopyFileW, RemoveDirectoryW, GetFileAttributesW
extern SHCreateDirectoryExW, SHFileOperationW, SetWindowTheme
extern CreateWindowExW, DefWindowProcW, SendMessageW, PostMessageW, MoveWindow, GetClientRect, GetWindowRect
extern GetDlgItem, GetWindowTextW, SetWindowTextW, SetWindowLongPtrW, GetWindowLongPtrW, FillRect
extern InvalidateRect, SetFocus, CreatePopupMenu, AppendMenuW, TrackPopupMenu, DestroyMenu
extern malloc, calloc, realloc, free, memset, memcmp, wcslen, wcscpy, wcscat, wcsrchr, _wcsicmp, _snwprintf, qsort
extern CreateDirectoryW, _wcsnicmp, EnableWindow, GetMessageW, TranslateMessage, DispatchMessageW, PostQuitMessage, DestroyWindow
extern th_type, th_diff
%define CB_ADDSTRING 0x0143
%define CB_SETCURSEL 0x014E
%define CB_GETCURSEL 0x0147
%define CBS_DROPDOWNLIST 3
%define CBN_SELCHANGE 1
extern InternetOpenW, InternetConnectW, FtpFindFirstFileW, InternetFindNextFileW, InternetCloseHandle, FtpGetFileW
extern vt_enum, vt_free, zip_open, snap_open, zip_extract, crc32_update, GetTempPathW
extern g_hinst, g_font, g_dark, th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_type, th_same
extern th_panelbrush, th_bgbrush
extern mk, reg_class, scl, set_status, msgbox, browse_save
extern lv_dark_header, theme_checks, mk_btn, mk_chk, mk_lbl, mk_edit, get_chk, ctl_color, pb_create, pb_layout, pb_load, pb_collect, pb_command
extern GetWindowTextLengthW, excl_match2, tip_add
extern vec_push, fmt_size, fmt_time, excl_match, sb_init, sb_append, sb_append_esc, sb_free, write_utf8
extern NormalizeString, GetNamedSecurityInfoW, SetNamedSecurityInfoW, LocalFree, SetFileAttributesW, SetFileTime, icon_set, g_copyr, g_copyl, g_mover, g_movel, g_del, g_sync, g_pause, g_report, g_snap, g_expand, g_collapse, GetLastError, GetCurrentProcessId, MoveFileW, input_box, wcsncpy, wcstoul, GetSystemTimeAsFileTime, set_status_part, str_dup, add_session, set_tab_title, type_for_path, ext_launch, g_policy

global folder_register, folder_create
extern g_batch, g_rep_path, g_exitcode, g_hframe, g_snap_path

; ---- statuses / entry layout
%define ST_SAME   0
%define ST_DIFF   1
%define ST_LONLY  2
%define ST_RONLY  3
%define ST_TYPE   4

%define E_NAME    0
%define E_NAMEOFF 8
%define E_DEPTH   12
%define E_FLAGS   16
%define E_STATUS  20
%define E_SIZEA   24
%define E_SIZEB   32
%define E_MTA     40
%define E_MTB     48
%define E_MASK    56
%define E_SUBEND  60
%define E_SIZE    64

%define EF_DIRA   1
%define EF_DIRB   2
%define EF_EXA    4
%define EF_EXB    8
%define EF_COLL   16
%define EF_LINK   32

; ---- per-session state
%define F_ENTS    0            ; vec {ptr,count,cap}
%define F_NENT    8
%define F_CAP     16
%define F_VIS     24
%define F_NVIS    32
%define F_THREAD  40
%define F_CANCEL  48
%define F_RUNNING 56
%define F_BUFA    64
%define F_BUFB    72
%define F_HVIEW   80
%define F_FMASK   88
%define F_OPT     96           ; bit0 size, bit1 mtime, bit2 content, bit3 follow links, bit4 backup
%define F_SYNCTHR 104
%define F_PAUSED  112
%define F_CNT     120          ; 5 qwords
%define F_PLAN    160          ; vec of OP {dd type, dd ent}
%define F_NPLAN   168
%define F_PCAP    176
%define F_SYNCERR 184
%define F_SYNCRUN 192
%define F_HLIST   200
%define F_SESS    208
%define F_ROOTA   216          ; wchar[520]
%define F_ROOTB   1256         ; wchar[520]
%define F_EXCL    2296         ; wchar[256]
%define F_KIND    2832
%define F_VTA     2848
%define F_VTB     2896
%define F_INET    3008
%define F_FTPC    3016
%define F_FTPP    3032
%define F_FMIN    5120
%define F_FMAX    5128
%define F_FCUT    5136
%define F_FDAYS   5144
%define F_CLOSING 5152
%define F_SCANERR 5160
%define F_ATTRX   5176
%define F_SIZE    5472
%define ID_LBL_ATTRX 168
%define ID_CHK_ASFILE 170
%define ID_CHK_LOOSE 174
%define ID_LBL_MASK 175
%define ID_EDIT_MASK 176
%define F_ALT 5184
%define F_MASKS 5208
%define EF_ALT 0x1000000
%define ID_BTN_STAMPLR 171
%define ID_BTN_STAMPRL 172
%define ID_CHK_ACL 173
%define ID_EDIT_ATTRX 169
%define EF_ATTRA  0x100
%define EF_ATTRB  0x10000
%define ID_LBL_FSZ   160
%define ID_EDIT_FMIN 161
%define ID_LBL_FTO   162
%define ID_EDIT_FMAX 163
%define ID_LBL_FDAY  164
%define ID_EDIT_FDAYS 165
%define VT_N_     8
%define VT_KIND_  40
%define HAS_CRC   0x80000000
%define ID_CHK_CRC  115
%define ID_CHK_ATTR 116
%define ID_BTN_SNAP 149
%define ID_BTN_MOVELR 166
%define ID_BTN_MOVERL 167
%define ID_BTN_EXCL 177

%define OP_COPYLR 1
%define OP_COPYRL 2
%define OP_DELR   3
%define OP_MKDIRR 5
%define OP_MKDIRL 6
%define OP_RMDIRR 7

%define MTIME_TOL 20000000
%define ID_CHK_SIZE   111
%define ID_CHK_MTIME  112
%define ID_CHK_CONT   113
%define ID_CHK_LINKS  114
%define ID_LBL_EXCL   120
%define ID_CHK_SAME   121
%define ID_CHK_DIFF   122
%define ID_CHK_LONLY  123
%define ID_CHK_RONLY  124
%define ID_EDIT_EXCL  131
%define ID_BTN_COPYLR 141
%define ID_BTN_COPYRL 142
%define ID_BTN_DEL    143
%define ID_BTN_SYNC   144
%define ID_BTN_REPORT 145
%define ID_BTN_EXPAND 146
%define ID_BTN_COLLAP 147
%define ID_BTN_PAUSE  148
%define ID_LIST       150
%define LVCFMT_RIGHT  1

section .rdata
WSTR cn_folder, "BCFolder"
WSTR cn_list, "SysListView32"
WSTR cn_combo, "COMBOBOX"
WSTR s_empty, ""
WSTR f_path, "%s\%s"
WSTR f_str, "%s"
WSTR f_star1, "%s\*"
WSTR f_star2, "%s\%s\*"
WSTR s_stop, "중지"
WSTR s_c_size, "크기"
WSTR s_c_mtime, "수정 시각"
WSTR s_c_cont, "내용(바이트)"
WSTR s_c_links, "링크 따라가기"
WSTR s_excl, "제외:"
WSTR s_fsz, "파일 크기(KB) 최소~최대:"
WSTR s_fto, "~"
WSTR s_fday, "최근 수정(일 이내):"
WSTR f_uint, "%u"
f_filt: dw 0x1F, "%", "u", ",", "%", "u", ",", "%", "u", 0
WSTR s_f_same, "같음"
WSTR s_f_diff, "다름"
WSTR s_f_lonly, "왼쪽만"
WSTR s_f_ronly, "오른쪽만"
WSTR s_b_copylr, "→ 복사"
WSTR s_b_copyrl, "← 복사"
WSTR s_b_movelr, "→ 이동"
WSTR s_b_moverl, "← 이동"
WSTR s_ren_title, "이름 바꾸기 (양쪽에 있으면 양쪽 모두 바뀝니다)"
WSTR s_ren_bad, "이름이 비었거나 사용할 수 없는 문자가 있습니다."
WSTR s_b_del, "삭제…"
WSTR s_b_sync, "동기화…"
WSTR s_b_report, "내보내기…"
WSTR s_tip_report, "비교 결과 내보내기 (Excel / Word / HTML 보고서)"
WSTR s_b_excl, "제외 선택…"
WSTR s_b_expand, "모두 펼침"
WSTR s_b_collap, "모두 접음"
WSTR s_b_pause, "일시중지"
WSTR s_b_resume, "계속"
WSTR h_name, "이름"
WSTR h_status, "상태"
WSTR h_dir, "방향"
WSTR s_ar_r, "→"
WSTR s_ar_l, "←"
WSTR s_ar_eq, "="
WSTR s_ar_ne, "≠"
WSTR h_sizea, "왼쪽 크기"
WSTR h_mta, "왼쪽 수정 시각"
WSTR h_sizeb, "오른쪽 크기"
WSTR h_mtb, "오른쪽 수정 시각"
WSTR st_same, "같음"
WSTR st_diff, "다름"
WSTR st_lonly, "왼쪽만"
WSTR st_ronly, "오른쪽만"
WSTR st_type, "형식 다름"
WSTR s_dark_theme, "DarkMode_Explorer"
WSTR s_dark_hdr, "DarkMode_Explorer"
WSTR s_light_theme, "Explorer"
WSTR s_scanning, "스캔 중… %llu 항목"
WSTR s_scan_done, "비교 완료 — 같음 %llu · 다름 %llu · 왼쪽만 %llu · 오른쪽만 %llu · 형식 다름 %llu"
WSTR s_scan_cancel, "취소됨"
WSTR s_title_f, "%s ↔ %s"
WSTR s_badpath, "두 경로 모두 존재하는 폴더여야 합니다."
WSTR s_norun, "먼저 비교를 실행하세요."
WSTR s_busy, "작업이 진행 중입니다."
WSTR s_mk_l, "왼쪽 항목 삭제 (휴지통)"
WSTR s_mk_r, "오른쪽 항목 삭제 (휴지통)"
WSTR s_mk_b, "양쪽 항목 삭제 (휴지통)"
WSTR s_sy1, "왼쪽 → 오른쪽 (새 항목·변경 항목 복사)"
WSTR s_sy2, "왼쪽 → 오른쪽 미러 (오른쪽에만 있는 항목 삭제)"
WSTR s_sy3, "양방향 (더 최신 파일로 맞춤)"
WSTR s_sy4, "덮어쓰기 전 백업(.bak)"
WSTR s_confirm_sync, "동기화 계획\n\n복사 %llu · 폴더 생성 %llu · 삭제 %llu 항목\n\n실행할까요?"
WSTR s_sync_prog, "동기화 중… %llu / %llu"
WSTR s_sync_done, "동기화 완료 (오류 %llu)"
WSTR s_nosel, "항목을 먼저 선택하세요."
WSTR s_bak, ".bak"
WSTR s_ftp_scheme, "ftp://"
WSTR s_anon, "anonymous"
WSTR s_anonpw, "becompare@asm"
WSTR s_agent, "BeCompareAsm"
WSTR f_ftp_root, "%s"
WSTR f_ftp_rel, "%s/%s"
WSTR s_ext_bcss, ".bcss"
WSTR s_tmpdir, "BeCompareAsm\"
WSTR f_tmpside, "%s%c"
WSTR f_tmpuniq, "%s%X_%X\%c"
WSTR f_snap_crc, `%s\t%X\t%llu\t%llu\t%X\n`
WSTR f_snap_nocrc, `%s\t%X\t%llu\t%llu\t\n`
WSTR s_snap_head, `BCSS1\n`
WSTR s_snap_saved, "스냅샷을 저장했습니다."
WSTR s_snap_nocontent, "스냅샷에는 파일 내용이 없어 열 수 없습니다."
WSTR s_b_stamplr, "→ 시각·속성 맞춤"
WSTR s_b_stamprl, "← 시각·속성 맞춤"
WSTR s_c_acl, "복사 시 NTFS 권한(ACL) 포함"
WSTR s_c_loose, "이름 느슨하게 맞춤(유니코드·확장자)"
WSTR s_l_mask, "마스크(*.txt=*.bak;..):"
WSTR s_c_asfile, "압축 파일을 하나의 파일로 비교(헥스)"
WSTR s_lbl_attrx, "제외 속성(R H S A L C O E):"
WSTR h_attr, "속성 (왼쪽 | 오른쪽)"
WSTR s_attr_letters, "RHSALCOE"
WSTR f_attrcell, "%s | %s"
WSTR s_done_short, "비교 완료"
WSTR s_scanerr, "일부 폴더를 읽지 못해(접근 거부·연결 끊김 등) 스캔이 불완전합니다. 동기화는 실행할 수 없습니다."
WSTR s_scanerr_short, "비교 완료 — 일부 폴더를 읽지 못함(동기화 불가)"
WSTR s_pol_blocked, "관리자 정책으로 차단된 기능입니다."
WSTR s_ro, "압축 파일·스냅샷은 읽기 전용이라 복사·삭제·동기화할 수 없습니다."
WSTR s_c_crc, "CRC32"
WSTR s_c_attr, "속성"
WSTR s_b_snap, "스냅샷 저장…"
WSTR s_html, ".html"
WSTR s_scanning_0, "스캔 중…"
WSTR f_opts, "%d%d%d%d%d%X%d%d|%s"
WSTR s_sync_none, "동기화할 항목이 없습니다."
WSTR s_report_fail, "보고서를 저장하지 못했습니다. 출력 경로와 쓰기 권한을 확인하세요."
WSTR s_report_ok, "보고서를 저장했습니다."
WSTR f_bak, "%s.bak"
WSTR s_title_rep, "BeCompare 폴더 보고서"
WSTR s_rep_head, `<!doctype html><html><head><meta charset="utf-8"><title>BeCompare 폴더 보고서</title><style>body{font-family:'Segoe UI',sans-serif;margin:24px}table{border-collapse:collapse}td,th{padding:3px 12px;border-bottom:1px solid #ddd;text-align:left}td.n{text-align:right}.s1{background:#ffd7d7}.s2{background:#d7f0d7}.s3{background:#d7e1ff}.s4{background:#ffe6be}.s0{color:#666}</style></head><body><h2>폴더 비교 보고서</h2><p>왼쪽: `
WSTR s_rep_mid, `<br>오른쪽: `
WSTR s_rep_tab, `</p><table><tr><th>경로</th><th>상태</th><th>왼쪽 크기</th><th>왼쪽 수정 시각</th><th>오른쪽 크기</th><th>오른쪽 수정 시각</th></tr>\n`
WSTR s_rep_foot, `</table></body></html>\n`
WSTR s_tr1, `<tr class="s`
WSTR s_tr2, `"><td>`
WSTR s_td, `</td><td>`
WSTR s_tdn, `</td><td class="n">`
WSTR s_trend, `</td></tr>\n`
WSTR f_one, "%d"
WSTR f_detail, "%s\%u.html"
WSTR s_a1, `<a href="`
WSTR s_a2, `.html">`
WSTR s_a3, `</a>`
WSTR s_files, "_files"
WSTR s_slash2, "/"
WSTR s_marker_open, "▼ "
WSTR s_marker_closed, "▶ "
WSTR s_sp2, "  "

align 8
st_text: dq st_same, st_diff, st_lonly, st_ronly, st_type
col_text: dq h_name, h_status, h_dir, h_sizea, h_mta, h_sizeb, h_mtb, h_attr
col_w:   dd 380, 90, 50, 110, 150, 110, 150, 140
col_fmt: dd 0, 0, 0, 1, 0, 1, 0, 0

section .text

; ================================================================= scan engine

; cmp_te(a, b): qsort comparator on entries {name*,...}
cmp_te:
    PROLOG 64
    invoke _wcsicmp, [rcx], [rdx]
    EPILOG 64

; free_te_vec(rcx = vec)
free_te_vec:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
.l:
    cmp rsi, [rbx+8]
    jae .d
    mov rax, rsi
    shl rax, 5
    add rax, [rbx]
    invoke free, [rax]
    inc rsi
    jmp .l
.d:
    invoke free, [rbx]
    mov qword [rbx], 0
    mov qword [rbx+8], 0
    EPILOG 64

; enum_dir(fold, side 0/1, rel, vec): list one directory into vec of TE{name*, attr dd, crc dd, size, mtime}
%define L_PAT 96
%define L_FD  2304
enum_dir:
    PROLOG 2896
    mov r12, rcx
    mov ebp, edx                     ; side
    mov r14, r8
    mov r15, r9
    lea r13, [r12+F_ROOTA]
    test ebp, ebp
    jz .rt
    lea r13, [r12+F_ROOTB]
.rt:
    cmp qword [r12+F_KIND+rbp*8], 1
    je .virt
    cmp qword [r12+F_KIND+rbp*8], 2
    je .ftp
    cmp word [r14], 0
    jne .rel
    invoke _snwprintf, &rsp+L_PAT, 1100, &f_star1, r13
    jmp .find
.rel:
    invoke _snwprintf, &rsp+L_PAT, 1100, &f_star2, r13, r14
.find:
    invoke FindFirstFileW, &rsp+L_PAT, &rsp+L_FD
    cmp rax, -1
    jne .found1
    mov dword [r12+F_SCANERR], 1     ; a directory that cannot be listed is an incomplete scan
    jmp .done
.found1:
    mov rbx, rax
.loop:
    lea rsi, [rsp+L_FD+FD_NAME]
    cmp word [rsi], '.'
    jne .nd
    cmp word [rsi+2], 0
    je .next
    cmp word [rsi+2], '.'
    jne .nd
    cmp word [rsi+4], 0
    je .next
.nd:
    invoke fold_excluded, r12, r14, rsi
    test eax, eax
    jnz .next
    ; FC-04: excluded attributes
    mov eax, [rsp+L_FD+FD_ATTR]
    call attr_compact
    test eax, [r12+F_ATTRX]
    jnz .next
    ; FC-03: size / modification date filters (files only)
    test dword [rsp+L_FD+FD_ATTR], 0x10
    jnz .keep
    mov edx, [rsp+L_FD+FD_SZHI]
    shl rdx, 32
    mov eax, [rsp+L_FD+FD_SZLO]
    or rdx, rax
    mov rax, [r12+F_FMIN]
    test rax, rax
    jz .nomin
    cmp rdx, rax
    jb .next
.nomin:
    mov rax, [r12+F_FMAX]
    test rax, rax
    jz .nomax
    cmp rdx, rax
    ja .next
.nomax:
    mov rax, [r12+F_FCUT]
    test rax, rax
    jz .keep
    cmp [rsp+L_FD+FD_MTIME], rax
    jb .next
.keep:
    invoke vec_push, r15, 32
    mov rdi, rax
    invoke str_dup, rsi
    mov [rdi], rax
    mov eax, [rsp+L_FD+FD_ATTR]
    mov [rdi+8], eax
    mov dword [rdi+12], 0
    mov edx, [rsp+L_FD+FD_SZHI]
    shl rdx, 32
    mov eax, [rsp+L_FD+FD_SZLO]
    or rdx, rax
    mov [rdi+16], rdx
    mov rax, [rsp+L_FD+FD_MTIME]
    mov [rdi+24], rax
.next:
    invoke FindNextFileW, rbx, &rsp+L_FD
    test eax, eax
    jnz .loop
    call GetLastError
    cmp eax, 18                      ; ERROR_NO_MORE_FILES
    je .endok
    mov dword [r12+F_SCANERR], 1
.endok:
    invoke FindClose, rbx
.done:
    mov rax, [r15+8]
    cmp rax, 1
    jbe .ret
    invoke qsort, [r15], rax, 32, &cmp_te
.ret:
    EPILOG 2896
.ftp:
    invoke ftp_enum, r12, rbp, r14, r15
    EPILOG 2896
.virt:
    lea rcx, [r12+F_VTA]
    test ebp, ebp
    jz .vt
    lea rcx, [r12+F_VTB]
.vt:
    invoke vt_enum, rcx, r14, r15
    xor esi, esi                     ; read index
    xor edi, edi                     ; write index
.vf:
    cmp rsi, [r15+8]
    jae .vdone
    mov rax, rsi
    shl rax, 5
    add rax, [r15]
    mov rbx, rax
    invoke fold_excluded, r12, r14, [rbx]
    test eax, eax
    jz .vkeep
    invoke free, [rbx]
    inc rsi
    jmp .vf
.vkeep:
    mov rax, rdi
    shl rax, 5
    add rax, [r15]
    cmp rax, rbx
    je .vsame
    mov r8, [rbx]
    mov [rax], r8
    mov r8, [rbx+8]
    mov [rax+8], r8
    mov r8, [rbx+16]
    mov [rax+16], r8
    mov r8, [rbx+24]
    mov [rax+24], r8
.vsame:
    inc rdi
    inc rsi
    jmp .vf
.vdone:
    mov [r15+8], rdi
    EPILOG 2896

; files_equal(fold, pathA, pathB) -> eax 1 if byte-identical
files_equal:
    PROLOG 96
    mov r12, rcx
    mov r13, r8
    invoke CreateFileW, rdx, GENERIC_READ, 3, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .no
    mov rbx, rax
    invoke CreateFileW, r13, GENERIC_READ, 3, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .closeA
    mov rsi, rax
.rd:
    cmp byte [r12+F_CANCEL], 0
    jne .diff
    invoke ReadFile, rbx, [r12+F_BUFA], 65536, &rsp+96, 0
    test eax, eax
    jz .diff
    mov r14d, [rsp+96]
    invoke ReadFile, rsi, [r12+F_BUFB], 65536, &rsp+100, 0
    test eax, eax
    jz .diff
    mov eax, [rsp+100]
    cmp eax, r14d
    jne .diff
    test r14d, r14d
    jz .eq
    invoke memcmp, [r12+F_BUFA], [r12+F_BUFB], r14
    test eax, eax
    jnz .diff
    jmp .rd
.eq:
    mov r15d, 1
    jmp .cl
.diff:
    xor r15d, r15d
.cl:
    invoke CloseHandle, rsi
    invoke CloseHandle, rbx
    mov eax, r15d
    EPILOG 96
.closeA:
    invoke CloseHandle, rbx
.no:
    xor eax, eax
    EPILOG 96

; cmp_files(fold, teA, teB, relname) -> eax status (SAME/DIFF) per the selected criteria
;   F_OPT bits: 1 size, 2 mtime, 4 content (bytes), 0x20 CRC-32, 0x40 attributes
cmp_files:
    PROLOG 4496
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    mov ebx, [r12+F_OPT]
    mov rax, [r13+16]
    cmp rax, [r14+16]
    jne .sizediff
    test ebx, 0x24
    jz .mt
    mov rax, [r12+F_KIND]
    or rax, [r12+F_KIND+8]
    jnz .virtual
    invoke _snwprintf, &rsp+96, 1100, &f_path, &r12+F_ROOTA, r15
    invoke alt_bpath, r12, r15
    invoke _snwprintf, &rsp+2296, 1100, &f_path, &r12+F_ROOTB, rax
    test ebx, 4
    jz .crc2
    invoke files_equal, r12, &rsp+96, &rsp+2296
    test eax, eax
    jz .diff
    jmp .attrs
.crc2:
    invoke file_crc, r12, &rsp+96
    mov rsi, rax
    invoke file_crc, r12, &rsp+2296
    cmp rsi, -1
    je .diff
    cmp rax, -1
    je .diff
    cmp rsi, rax
    jne .diff
    jmp .attrs
.virtual:
    invoke side_crc, r12, 0, r13, r15
    mov rsi, rax
    invoke side_crc, r12, 1, r14, r15
    cmp rsi, -1
    je .mt
    cmp rax, -1
    je .mt
    cmp rsi, rax
    jne .diff
    jmp .attrs
.mt:
    test ebx, 2
    jz .attrs
.mtcmp:
    mov rax, [r13+24]
    sub rax, [r14+24]
    jns .abs
    neg rax
.abs:
    mov rcx, MTIME_TOL
    cmp qword [r12+F_KIND], 2
    je .ftpt
    cmp qword [r12+F_KIND+8], 2
    jne .tl
.ftpt:
    mov rcx, 900000000               ; FTP listings only have minute resolution: 90 s
.tl:
    cmp rax, rcx
    ja .diff
    jmp .attrs
.sizediff:
    test ebx, 0x25
    jnz .diff
    test ebx, 2
    jnz .mtcmp
.attrs:
    test ebx, 0x40
    jz .same
    mov rax, [r12+F_KIND]
    or rax, [r12+F_KIND+8]
    jnz .same
    mov eax, [r13+8]
    xor eax, [r14+8]
    and eax, 0x27
    jnz .diff
.same:
    xor eax, eax
    EPILOG 4496
.diff:
    mov eax, ST_DIFF
    EPILOG 4496

; ent_side(ent, te, side 0/1): fill flags/size/mtime for one side (leaf)
ent_side:
    mov r9d, EF_EXA
    mov r10d, EF_DIRA
    test r8d, r8d
    jz .a
    mov r9d, EF_EXB
    mov r10d, EF_DIRB
.a:
    mov eax, [rdx+8]
    or [rcx+E_FLAGS], r9d
    push rax
    push rcx
    push rdx
    push r9
    push r10
    call attr_compact
    mov r11d, eax
    pop r10
    pop r9
    pop rdx
    pop rcx
    pop rax
    test r8d, r8d
    jnz .ab
    shl r11d, 8
    jmp .ao
.ab:
    shl r11d, 16
.ao:
    or [rcx+E_FLAGS], r11d
    test eax, FILE_ATTRIBUTE_DIRECTORY
    jz .f
    or [rcx+E_FLAGS], r10d
    test eax, FILE_ATTRIBUTE_REPARSE_POINT
    jz .f
    or dword [rcx+E_FLAGS], EF_LINK
.f:
    mov r11, [rdx+16]
    mov rax, [rdx+24]
    test r8d, r8d
    jnz .b
    mov [rcx+E_SIZEA], r11
    mov [rcx+E_MTA], rax
    ret
.b:
    mov [rcx+E_SIZEB], r11
    mov [rcx+E_MTB], rax
    ret

; attr_compact(eax = Win32 attributes) -> eax: bit0 R, 1 H, 2 S, 3 A, 4 L (reparse), 5 C (compressed), 6 O (offline), 7 E (encrypted)
attr_compact:
    mov edx, eax
    and edx, 1
    mov ecx, eax
    shr ecx, 1
    and ecx, 1
    shl ecx, 1
    or edx, ecx
    mov ecx, eax
    shr ecx, 2
    and ecx, 1
    shl ecx, 2
    or edx, ecx
    mov ecx, eax
    shr ecx, 5
    and ecx, 1
    shl ecx, 3
    or edx, ecx
    mov ecx, eax
    shr ecx, 10
    and ecx, 1
    shl ecx, 4
    or edx, ecx
    mov ecx, eax
    shr ecx, 11
    and ecx, 1
    shl ecx, 5
    or edx, ecx
    mov ecx, eax
    shr ecx, 12
    and ecx, 1
    shl ecx, 6
    or edx, ecx
    mov ecx, eax
    shr ecx, 14
    and ecx, 1
    shl ecx, 7
    or edx, ecx
    mov eax, edx
    ret

; scan_dir(fold, rel, depth): merge-walk one directory of both sides (depth first)
%define L_VA   96
%define L_VB   120
%define L_REL  144
%define L_K    2344
%define L_TA   2352
%define L_TB   2360
scan_dir:
    PROLOG 2384
    mov r12, rcx
    mov r13, rdx
    mov r14d, r8d
    xor eax, eax
    mov [rsp+L_VA], rax
    mov [rsp+L_VA+8], rax
    mov [rsp+L_VA+16], rax
    mov [rsp+L_VB], rax
    mov [rsp+L_VB+8], rax
    mov [rsp+L_VB+16], rax
    invoke enum_dir, r12, 0, r13, &rsp+L_VA
    invoke enum_dir, r12, 1, r13, &rsp+L_VB
    invoke pair_names, r12, r13, &rsp+L_VA, &rsp+L_VB
    xor ebx, ebx
    xor ebp, ebp
.loop:
    cmp byte [r12+F_CANCEL], 0
    jne .cleanup
    xor esi, esi
    xor edi, edi
    cmp rbx, [rsp+L_VA+8]
    jae .noa
    mov rsi, rbx
    shl rsi, 5
    add rsi, [rsp+L_VA]
.noa:
    cmp rbp, [rsp+L_VB+8]
    jae .nob
    mov rdi, rbp
    shl rdi, 5
    add rdi, [rsp+L_VB]
.nob:
    test rsi, rsi
    jnz .ha
    test rdi, rdi
    jz .cleanup
    mov r15d, 1
    jmp .have
.ha:
    test rdi, rdi
    jnz .both
    mov r15d, -1
    jmp .have
.both:
    invoke _wcsicmp, [rsi], [rdi]
    mov r15d, eax
.have:
    mov [rsp+L_TA], rsi
    mov [rsp+L_TB], rdi
    invoke vec_push, r12, E_SIZE
    mov rdi, rax
    invoke memset, rdi, 0, E_SIZE
    mov rax, [rsp+L_TA]
    cmp r15d, 0
    jle .nm                           ; A is current (A only or both)
    mov rax, [rsp+L_TB]
.nm:
    mov rsi, [rax]                    ; entry name
    cmp word [r13], 0
    je .norel
    invoke _snwprintf, &rsp+L_REL, 1100, &f_path, r13, rsi
    invoke wcslen, r13
    inc eax
    mov [rdi+E_NAMEOFF], eax
    jmp .dup
.norel:
    invoke _snwprintf, &rsp+L_REL, 1100, &f_str, rsi
    mov dword [rdi+E_NAMEOFF], 0
.dup:
    invoke str_dup, &rsp+L_REL
    mov [rdi+E_NAME], rax
    mov [rdi+E_DEPTH], r14d
    cmp qword [r12+F_ALT+8], 0
    je .noalt
    invoke alt_find, r12, [rdi+E_NAME]
    test rax, rax
    jz .noalt
    or dword [rdi+E_FLAGS], EF_ALT
.noalt:
    cmp r15d, 0
    jl .oa
    jg .ob
    invoke ent_side, rdi, [rsp+L_TA], 0
    invoke ent_side, rdi, [rsp+L_TB], 1
    mov eax, [rdi+E_FLAGS]
    mov ecx, eax
    and ecx, EF_DIRA|EF_DIRB
    jz .files
    cmp ecx, EF_DIRA|EF_DIRB
    je .adv
    mov dword [rdi+E_STATUS], ST_TYPE
    jmp .adv
.files:
    invoke cmp_files, r12, [rsp+L_TA], [rsp+L_TB], [rdi+E_NAME]
    mov [rdi+E_STATUS], eax
.adv:
    inc rbx
    inc rbp
    jmp .post
.oa:
    invoke ent_side, rdi, [rsp+L_TA], 0
    mov dword [rdi+E_STATUS], ST_LONLY
    inc rbx
    jmp .post
.ob:
    invoke ent_side, rdi, [rsp+L_TB], 1
    mov dword [rdi+E_STATUS], ST_RONLY
    inc rbp
.post:
    mov eax, [rdi+E_FLAGS]
    test eax, EF_DIRA|EF_DIRB
    jnz .nocount
    mov eax, [rdi+E_STATUS]
    inc qword [r12+F_CNT+rax*8]
.nocount:
    test dword [rdi+E_FLAGS], EF_DIRA|EF_DIRB
    jz .leaf
    test dword [rdi+E_FLAGS], EF_LINK
    jz .rec
    test dword [r12+F_OPT], 8
    jz .leaf
.rec:
    mov rax, [r12+F_NENT]
    dec rax
    mov [rsp+L_K], rax
    lea eax, [r14+1]
    invoke scan_dir, r12, [rdi+E_NAME], rax
    mov rax, [r12+F_ENTS]
    mov rcx, [rsp+L_K]
    shl rcx, 6
    add rax, rcx
    mov rdi, rax
    mov rcx, [rsp+L_K]
    inc rcx
    mov rdx, [r12+F_NENT]
    mov [rdi+E_SUBEND], edx
    xor r8d, r8d
    mov r9, [r12+F_ENTS]
.m:
    cmp rcx, rdx
    jae .md
    mov r10, rcx
    shl r10, 6
    or r8d, [r9+r10+E_MASK]
    inc rcx
    jmp .m
.md:
    mov eax, [rdi+E_FLAGS]
    and eax, EF_EXA|EF_EXB
    cmp eax, EF_EXA|EF_EXB
    jne .own
    mov eax, [rdi+E_STATUS]
    test eax, eax
    jnz .own
    test r8d, 0x1E
    jz .own
    mov dword [rdi+E_STATUS], ST_DIFF
.own:
    mov ecx, [rdi+E_STATUS]
    mov eax, 1
    shl eax, cl
    or eax, r8d
    mov [rdi+E_MASK], eax
    jmp .progress
.leaf:
    mov eax, [r12+F_NENT]
    mov [rdi+E_SUBEND], eax
    mov ecx, [rdi+E_STATUS]
    mov eax, 1
    shl eax, cl
    mov [rdi+E_MASK], eax
.progress:
    mov rax, [r12+F_NENT]
    test al, 0x7F
    jnz .loop
    invoke PostMessageW, [r12+F_HVIEW], BC_PROGRESS, rax, 0
    jmp .loop
.cleanup:
    invoke free_te_vec, &rsp+L_VA
    invoke free_te_vec, &rsp+L_VB
    EPILOG 2384

; scan_thread(fold)
scan_thread:
    PROLOG 64
    mov rbx, rcx
    invoke scan_dir, rbx, &s_empty, 0
    movzx esi, byte [rbx+F_CANCEL]
    mov qword [rbx+F_RUNNING], 0
    invoke PostMessageW, [rbx+F_HVIEW], BC_DONE, rsi, 0
    xor eax, eax
    EPILOG 64

%include "folder_ui.inc"
%include "folder_vfs.inc"
%include "ftp.inc"
%include "sync_dlg.inc"
%include "folder_rep.inc"
%include "folder_exp.inc"
%include "excl_dlg.inc"
%include "folder_pair.inc"
