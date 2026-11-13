; fmerge.asm - Folder Merge session (FM-01 .. FM-03): three folders (left, right, base) merged into an output folder
;   per file: automatic 3-way decision (same / left changed / right changed / added / deleted / conflict),
;   manual overrides, text-merge hand-off for conflicts, execution into the output folder
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"
%include "inc/export.inc"

extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem, EnableWindow
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetWindowTextW, SetWindowTextW, InvalidateRect, SetWindowTheme
extern FindFirstFileW, FindNextFileW, FindClose, CreateDirectoryW, CopyFileW, GetFileAttributesExW, GetFileAttributesW, SHCreateDirectoryExW, SetCursor, LoadCursorW
extern malloc, calloc, realloc, free, memset, memcpy, wcslen, wcscpy, wcscat, wcsrchr, wcsncpy, _wcsicmp, _wcsnicmp, _snwprintf, qsort
extern GetFullPathNameW, CreateFileW, ReadFile, CloseHandle
extern g_hinst, g_dark, th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_same, th_conf, th_type, th_panelbrush
extern mk, reg_class, scl, set_status, msgbox
extern mk_btn, mk_chk, mk_lbl, mk_edit, get_chk, ctl_color, theme_checks, lv_dark_header
extern pb_create, pb_layout, pb_load, pb_collect, pb_command
extern set_tab_title, str_dup, fmt_size, vec_push, read_file_bytes, crc32_update, browse_folder
extern sb_free, PostMessageW, g_batch, g_dx_path, g_exitcode, g_hframe, batch_done
extern sess_new, sess_open
extern icon_set, g_usel, g_user, g_base, g_del, g_edit, g_run

global fmerge_register, fmerge_create

; FM state (one per session)
%define FM_ENT    0                  ; vec of FE
%define FM_RAW    24                 ; vec of raw scan records
%define FM_VIS    48
%define FM_NVIS   56
%define FM_HLIST  64
%define FM_NCONF  72
%define FM_NAUTO  80
%define FM_TMP    88                 ; 3 x 64 wchar scratch for cell text (384 bytes)
%define FM_ROOT   472                ; 3 x 1040 bytes: left, right, base
%define FM_OUT    3592               ; 1040 bytes
%define FM_SESS   4632
%define FM_SIZE   4648

; FE (64 bytes)
%define FE_NAME   0
%define FE_FLAGS  8                  ; bit 0-2 exists on left/right/base, 4-6 is a folder there,
                                     ; 8-10 CRC known, 12-14 content could not be read
%define FE_ACT    12
%define FE_SZ     16                 ; 3 qwords
%define FE_CRC    40                 ; 3 dwords
%define FE_ST     52
%define FE_MAN    56
%define FE_BYTES  64

%define ACT_NONE  0
%define ACT_A     1
%define ACT_B     2
%define ACT_C     3
%define ACT_DEL   4
%define ACT_CONF  5

%define ID_LST    700
%define ID_CHKF   701
%define ID_LBLI   702
%define ID_BTN_RUNM 710
%define ID_BTN_TA 711
%define ID_BTN_TB 712
%define ID_BTN_TC 713
%define ID_BTN_TD 714
%define ID_BTN_OPENM 715
%define ID_BTN_DOC 716
%define ID_LBL_BASE 651
%define ID_LBL_OUT  652

section .rdata
WSTR cn_fm, "BCFMerge"
WSTR cn_listv, "SysListView32"
WSTR s_empty, ""
WSTR s_light_theme, "Explorer"
WSTR s_dark_theme, "DarkMode_Explorer"
WSTR s_dots, "..."
WSTR s_l_base, "Base(공통 조상):"
WSTR s_l_out, "출력 폴더:"
WSTR s_b_runm, "병합 실행"
WSTR s_b_ta, "← 왼쪽 채택"
WSTR s_b_tb, "오른쪽 채택 →"
WSTR s_b_tc, "Base 채택"
WSTR s_b_td, "삭제(제외)"
WSTR s_b_openm, "텍스트 병합 열기"
WSTR s_c_conf, "충돌만 보기"
WSTR h_name, "경로"
WSTR h_stat, "상태"
WSTR h_act, "동작"
WSTR h_sa, "왼쪽 크기"
WSTR h_sb, "오른쪽 크기"
WSTR h_sc, "Base 크기"
WSTR f_pat, "%s\%s*"
WSTR f_full, "%s\%s"
WSTR f_info, "항목 %d · 충돌 %d · 자동 해결 %d"
WSTR f_title, "병합 %s"
WSTR f_opts, "%d"
WSTR f_done, "병합 완료: 복사 %d · 병합 파일 %d · 제외 %d · 미해결 충돌 %d · 복사 실패 %d"
WSTR s_nopath, "왼쪽과 오른쪽 폴더를 입력하세요."
WSTR s_noout, "출력 폴더를 입력하세요."
WSTR s_outsame, "출력 폴더는 입력 폴더(왼쪽·오른쪽·Base)와 같거나 그 안에 있으면 안 됩니다. 다른 폴더를 지정하세요."
WSTR s_nocmp, "먼저 [비교]를 실행해 병합할 항목을 확인하세요."
WSTR s_out_used, "출력 폴더가 비어 있지 않습니다. 같은 이름의 파일은 덮어쓰고, 그 밖의 기존 파일은 그대로 남습니다. 계속할까요?"
WSTR f_nodir, `폴더를 찾을 수 없습니다:\n%s`
WSTR f_star, "%s\*"
WSTR s_nosel, "먼저 목록에서 항목을 선택하세요."
WSTR s_noopen, "텍스트 병합은 양쪽에 모두 있는 파일에서만 열 수 있습니다."
WSTR s_dash, "—"
WSTR s_slash, "\"
WSTR fst0, "같음"
WSTR fst1, "왼쪽 변경"
WSTR fst2, "오른쪽 변경"
WSTR fst3, "충돌"
WSTR fst4, "왼쪽만 추가"
WSTR fst5, "오른쪽만 추가"
WSTR fst6, "삭제됨"
WSTR fst7, "폴더"
WSTR fst8, "양쪽 삭제"
WSTR fst9, "읽을 수 없음"
WSTR ac0, "—"
WSTR ac1, "왼쪽 채택"
WSTR ac2, "오른쪽 채택"
WSTR ac3, "Base 채택"
WSTR ac4, "삭제(제외)"
WSTR ac5, "수동 해결 필요"

align 8
col_text: dq h_name, h_stat, h_act, h_sa, h_sb, h_sc
col_w:    dd 320, 110, 120, 110, 110, 110
col_fmt:  dd 0, 0, 0, 1, 1, 1
st_names: dq fst0, fst1, fst2, fst3, fst4, fst5, fst6, fst7, fst8, fst9
ac_names: dq ac0, ac1, ac2, ac3, ac4, ac5

section .text

fmerge_register:
    PROLOG 64
    invoke reg_class, &cn_fm, &fm_proc, 0
    EPILOG 64

fmerge_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_fm, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- scan
; fm_walk(fm, side, root, prefix, depth)
%define W_PAT 96
%define W_REL 2300
%define W_FD  4500
fm_walk:
    PROLOG 5200
    mov r12, rcx
    mov r13d, edx
    mov r14, r8
    mov r15, r9
    mov ebp, [rsp+5200+112]           ; depth
    cmp ebp, 32
    jae .d
    invoke _snwprintf, &rsp+W_PAT, 1090, &f_pat, r14, r15
    invoke FindFirstFileW, &rsp+W_PAT, &rsp+W_FD
    cmp rax, -1
    je .d
    mov rbx, rax
.next:
    lea rsi, [rsp+W_FD+FD_NAME]
    cmp word [rsi], '.'
    jne .real
    cmp word [rsi+2], 0
    je .skip
    cmp word [rsi+2], '.'
    jne .real
    cmp word [rsi+4], 0
    je .skip
.real:
    invoke _snwprintf, &rsp+W_REL, 1090, &f_rel, r15, rsi
    invoke vec_push, &r12+FM_RAW, 32
    mov rdi, rax
    invoke str_dup, &rsp+W_REL
    mov [rdi], rax
    mov eax, r13d
    test dword [rsp+W_FD+FD_ATTR], 0x10
    jz .nodir
    or eax, 0x10
.nodir:
    mov [rdi+8], eax
    mov edx, [rsp+W_FD+FD_SZHI]
    shl rdx, 32
    mov eax, [rsp+W_FD+FD_SZLO]
    or rdx, rax
    mov [rdi+16], rdx
    test dword [rsp+W_FD+FD_ATTR], 0x10
    jz .skip
    test dword [rsp+W_FD+FD_ATTR], 0x400
    jnz .skip
    invoke wcscat, &rsp+W_REL, &s_slash
    lea eax, [rbp+1]
    invoke fm_walk, r12, r13, r14, &rsp+W_REL, rax
.skip:
    invoke FindNextFileW, rbx, &rsp+W_FD
    test eax, eax
    jnz .next
    invoke FindClose, rbx
.d:
    EPILOG 5200

section .rdata
WSTR f_rel, "%s%s"
section .text

; raw record compare for qsort: by name, case-insensitive
fm_rawcmp:
    sub rsp, 40
    mov rcx, [rcx]
    mov rdx, [rdx]
    call _wcsicmp
    add rsp, 40
    ret

; fm_clear(fm): drop entries and the visible list
fm_clear:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
.l:
    cmp rsi, [rbx+FM_ENT+8]
    jae .d
    mov rax, rsi
    shl rax, 6
    add rax, [rbx+FM_ENT]
    invoke free, [rax+FE_NAME]
    inc rsi
    jmp .l
.d:
    invoke free, [rbx+FM_ENT]
    xor eax, eax
    mov [rbx+FM_ENT], rax
    mov [rbx+FM_ENT+8], rax
    mov [rbx+FM_ENT+16], rax
    invoke free, [rbx+FM_VIS]
    mov qword [rbx+FM_VIS], 0
    mov qword [rbx+FM_NVIS], 0
    EPILOG 64

; fm_scan(fm)
fm_scan:
    PROLOG 128
    mov rbx, rcx
    invoke fm_clear, rbx
    xor eax, eax
    mov [rbx+FM_RAW], rax
    mov [rbx+FM_RAW+8], rax
    mov [rbx+FM_RAW+16], rax
    xor esi, esi
.sides:
    cmp esi, 3
    jae .sorted
    imul eax, esi, 1040
    lea rdi, [rbx+FM_ROOT+rax]
    cmp word [rdi], 0
    je .nx
    invoke GetFileAttributesW, rdi
    cmp eax, -1
    je .nx
    test eax, 0x10
    jz .nx
    invoke fm_walk, rbx, rsi, rdi, &s_empty, 0
.nx:
    inc esi
    jmp .sides
.sorted:
    mov rax, [rbx+FM_RAW+8]
    test rax, rax
    jz .done
    invoke qsort, [rbx+FM_RAW], rax, 32, &fm_rawcmp
    xor esi, esi                      ; raw index
.grp:
    cmp rsi, [rbx+FM_RAW+8]
    jae .freeraw
    mov r12, rsi
    shl r12, 5
    add r12, [rbx+FM_RAW]
    invoke vec_push, &rbx+FM_ENT, FE_BYTES
    mov r13, rax
    invoke memset, r13, 0, FE_BYTES
    mov rax, [r12]
    mov [r13+FE_NAME], rax
    xor r14d, r14d                    ; flags
.mem:
    ; add this record to the entry
    mov eax, [r12+8]
    mov ecx, eax
    and ecx, 3
    mov edx, 1
    shl edx, cl
    or r14d, edx
    test eax, 0x10
    jz .nd
    mov edx, 0x10
    shl edx, cl
    or r14d, edx
.nd:
    mov rax, [r12+16]
    mov [r13+FE_SZ+rcx*8], rax
    inc rsi
    cmp rsi, [rbx+FM_RAW+8]
    jae .memdone
    mov r15, rsi
    shl r15, 5
    add r15, [rbx+FM_RAW]
    invoke _wcsicmp, [r13+FE_NAME], [r15]
    test eax, eax
    jnz .memdone
    invoke free, [r15]
    mov r12, r15
    jmp .mem
.memdone:
    mov [r13+FE_FLAGS], r14d
    jmp .grp
.freeraw:
    invoke free, [rbx+FM_RAW]
    xor eax, eax
    mov [rbx+FM_RAW], rax
    mov [rbx+FM_RAW+8], rax
.done:
    EPILOG 128

; ---------------------------------------------------------------- compare / decide
; fm_crc(fm, e, side) -> eax : CRC32 of the file, read in blocks (any size); cached in the entry.
; A file that cannot be opened or read sets the "unreadable" flag instead.
fm_crc:
    PROLOG 2400
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    mov eax, 0x100
    mov ecx, r12d
    shl eax, cl
    test [rsi+FE_FLAGS], eax
    jnz .have
    imul eax, r12d, 1040
    lea rdi, [rbx+FM_ROOT+rax]
    invoke _snwprintf, &rsp+96, 1090, &f_full, rdi, [rsi+FE_NAME]
    xor r13d, r13d                    ; crc
    xor r15d, r15d                    ; block buffer
    invoke CreateFileW, &rsp+96, GENERIC_READ, 3, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .bad
    mov r14, rax
    invoke malloc, 262144
    mov r15, rax
    test rax, rax
    jz .badclose
.rd:
    mov dword [rsp+2300], 0
    invoke ReadFile, r14, r15, 262144, &rsp+2300, 0
    test eax, eax
    jz .badclose
    mov r8d, [rsp+2300]
    test r8d, r8d
    jz .eof
    mov ecx, r13d
    mov rdx, r15
    call crc32_update
    mov r13d, eax
    jmp .rd
.badclose:
    invoke CloseHandle, r14
.bad:
    xor r13d, r13d
    mov eax, 0x1000
    mov ecx, r12d
    shl eax, cl
    or [rsi+FE_FLAGS], eax
    jmp .store
.eof:
    invoke CloseHandle, r14
.store:
    invoke free, r15
    mov [rsi+FE_CRC+r12*4], r13d
    mov eax, 0x100
    mov ecx, r12d
    shl eax, cl
    or [rsi+FE_FLAGS], eax
.have:
    mov eax, [rsi+FE_CRC+r12*4]
    EPILOG 2400

; fm_eq(fm, e, s1, s2) -> eax 1 when the two sides hold identical content
fm_eq:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    mov r13d, r9d
    mov rax, [rsi+FE_SZ+r12*8]
    cmp rax, [rsi+FE_SZ+r13*8]
    jne .no
    test rax, rax
    jz .yes
    invoke fm_crc, rbx, rsi, r12
    mov r14d, eax
    invoke fm_crc, rbx, rsi, r13
    cmp eax, r14d
    jne .no
    mov eax, 0x1000                   ; unknown content is never "the same"
    mov ecx, r12d
    shl eax, cl
    mov edx, 0x1000
    mov ecx, r13d
    shl edx, cl
    or eax, edx
    test [rsi+FE_FLAGS], eax
    jnz .no
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; fm_decide(fm, e): set FE_ACT / FE_ST
fm_decide:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, [rsi+FE_FLAGS]
    mov r13d, r12d
    and r13d, 7                       ; existence
    mov r14d, r12d
    shr r14d, 4
    and r14d, 7                       ; directory bits
    test r14d, r14d
    jz .files
    cmp r14d, r13d
    jne .conflict                     ; folder on one side, file on another
    test r13d, 3
    jz .nothing
    test r13d, 4
    jz .dirkeep
    mov eax, r13d
    and eax, 3
    cmp eax, 3
    je .dirkeep
    ; in Base but gone on one side: removed. Files the other side added below it
    ; still create their folders when they are copied.
    mov dword [rsi+FE_ST], 6
    mov dword [rsi+FE_ACT], ACT_DEL
    jmp .done
.dirkeep:
    mov dword [rsi+FE_ST], 7
    mov dword [rsi+FE_ACT], ACT_A
    test r13d, 1
    jnz .done
    mov dword [rsi+FE_ACT], ACT_B
    jmp .done
.files:
    test r13d, 4
    jnz .withbase
    cmp r13d, 3
    je .ab
    mov dword [rsi+FE_ACT], ACT_A
    mov dword [rsi+FE_ST], 4
    cmp r13d, 1
    je .done
    mov dword [rsi+FE_ACT], ACT_B
    mov dword [rsi+FE_ST], 5
    jmp .done
.ab:
    invoke fm_eq, rbx, rsi, 0, 1
    test eax, eax
    jz .conflict
.same:
    mov dword [rsi+FE_ACT], ACT_A
    mov dword [rsi+FE_ST], 0
    jmp .done
.withbase:
    cmp r13d, 7
    je .abc
    cmp r13d, 5
    je .ac
    cmp r13d, 6
    je .bc
.nothing:
    mov dword [rsi+FE_ACT], ACT_NONE
    mov dword [rsi+FE_ST], 8
    jmp .done
.ac:
    invoke fm_eq, rbx, rsi, 0, 2
    jmp .delchk
.bc:
    invoke fm_eq, rbx, rsi, 1, 2
.delchk:
    test eax, eax
    jz .conflict
    mov dword [rsi+FE_ACT], ACT_DEL
    mov dword [rsi+FE_ST], 6
    jmp .done
.abc:
    invoke fm_eq, rbx, rsi, 0, 1
    test eax, eax
    jnz .same
    invoke fm_eq, rbx, rsi, 0, 2
    test eax, eax
    jz .notac
    mov dword [rsi+FE_ACT], ACT_B
    mov dword [rsi+FE_ST], 2
    jmp .done
.notac:
    invoke fm_eq, rbx, rsi, 1, 2
    test eax, eax
    jz .conflict
    mov dword [rsi+FE_ACT], ACT_A
    mov dword [rsi+FE_ST], 1
    jmp .done
.conflict:
    mov dword [rsi+FE_ACT], ACT_CONF
    mov dword [rsi+FE_ST], 3
.done:
    test dword [rsi+FE_FLAGS], 0x7000
    jz .ret
    mov dword [rsi+FE_ACT], ACT_CONF
    mov dword [rsi+FE_ST], 9
.ret:
    EPILOG 64

; fm_vis(fm, onlyconf): visible list, conflict / automatic counters
fm_vis:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    invoke free, [rbx+FM_VIS]
    mov rcx, [rbx+FM_ENT+8]
    lea rcx, [rcx*4+8]
    call malloc
    mov [rbx+FM_VIS], rax
    mov rsi, rax
    xor edi, edi
    xor r13d, r13d                    ; conflicts
    xor r14d, r14d                    ; automatic
    xor ecx, ecx
.l:
    cmp rcx, [rbx+FM_ENT+8]
    jae .d
    mov rax, rcx
    shl rax, 6
    add rax, [rbx+FM_ENT]
    mov edx, [rax+FE_ACT]
    cmp dword [rax+FE_MAN], 0
    je .noman
    mov edx, [rax+FE_MAN]
.noman:
    cmp edx, ACT_CONF
    jne .auto
    inc r13d
    jmp .take
.auto:
    inc r14d
    test r12d, r12d
    jnz .skip
.take:
    mov [rsi+rdi*4], ecx
    inc edi
.skip:
    inc rcx
    jmp .l
.d:
    mov [rbx+FM_NVIS], rdi
    mov [rbx+FM_NCONF], r13
    mov [rbx+FM_NAUTO], r14
    EPILOG 64

; fm_refresh(hwnd, fm)
fm_refresh:
    PROLOG 256
    mov rsi, rcx
    mov rbx, rdx
    invoke get_chk, rsi, ID_CHKF
    invoke fm_vis, rbx, rax
    invoke SendMessageW, [rbx+FM_HLIST], LVM_SETITEMCOUNT, [rbx+FM_NVIS], 0
    invoke InvalidateRect, [rbx+FM_HLIST], 0, 1
    mov r12, [rbx+FM_ENT+8]
    mov r13, [rbx+FM_NCONF]
    mov r14, [rbx+FM_NAUTO]
    invoke _snwprintf, &rsp+96, 100, &f_info, r12, r13, r14
    invoke GetDlgItem, rsi, ID_LBLI
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 256

; fm_norm(rcx = path buffer wchar[520]): full path without a trailing backslash (drive roots keep it)
fm_norm:
    PROLOG 1200
    mov rbx, rcx
    cmp word [rbx], 0
    je .d
    lea rdi, [rsp+96]
    invoke GetFullPathNameW, rbx, 519, rdi, 0
    test eax, eax
    jz .d
    cmp eax, 519
    jae .d
    mov ecx, eax
.s:
    cmp ecx, 3
    jbe .cp
    cmp word [rdi+rcx*2-2], 0x5C
    jne .cp
    dec ecx
    mov word [rdi+rcx*2], 0
    jmp .s
.cp:
    invoke wcscpy, rbx, rdi
.d:
    EPILOG 1200

; fm_under(path, root) -> eax 1 when path is root itself or lies inside it (both normalized)
fm_under:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    cmp word [rsi], 0
    je .no
    cmp word [rdi], 0
    je .no
    invoke wcslen, rdi
    mov rbx, rax
    invoke _wcsnicmp, rsi, rdi, rbx
    test eax, eax
    jnz .no
    movzx eax, word [rsi+rbx*2]
    test eax, eax
    jz .yes
    cmp eax, 0x5C
    je .yes
    cmp word [rdi+rbx*2-2], 0x5C      ; root is a drive root such as C:\
    jne .no
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; fm_outcheck(hwnd, fm) -> eax 1 when the output folder is set and is not one of the inputs
fm_outcheck:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke fm_norm, &rbx+FM_OUT
    cmp word [rbx+FM_OUT], 0
    je .noout
    xor edi, edi
.chk:
    cmp edi, 3
    jae .ok
    imul eax, edi, 1040
    lea rdx, [rbx+FM_ROOT+rax]
    lea rcx, [rbx+FM_OUT]
    call fm_under
    test eax, eax
    jnz .same
    inc edi
    jmp .chk
.ok:
    mov eax, 1
    EPILOG 64
.noout:
    invoke msgbox, rsi, &s_noout, MB_OK|MB_ICONINFORMATION
    xor eax, eax
    EPILOG 64
.same:
    invoke msgbox, rsi, &s_outsame, MB_OK|MB_ICONWARNING
    xor eax, eax
    EPILOG 64

; fm_used(rcx = folder) -> eax 1 when it already holds something
fm_used:
    PROLOG 2912
    mov r13, rcx
    invoke _snwprintf, &rsp+96, 1090, &f_star, r13
    invoke FindFirstFileW, &rsp+96, &rsp+2300
    cmp rax, -1
    je .no
    mov rbx, rax
    xor r12d, r12d
.n:
    lea rsi, [rsp+2300+FD_NAME]
    cmp word [rsi], '.'
    jne .hit
    cmp word [rsi+2], 0
    je .skip
    cmp word [rsi+2], '.'
    jne .hit
    cmp word [rsi+4], 0
    je .skip
.hit:
    mov r12d, 1
    jmp .close
.skip:
    invoke FindNextFileW, rbx, &rsp+2300
    test eax, eax
    jnz .n
.close:
    invoke FindClose, rbx
    mov eax, r12d
    EPILOG 2912
.no:
    xor eax, eax
    EPILOG 2912

; fm_compare(hwnd, fm, sess)
fm_compare:
    PROLOG 1648
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov [rbx+FM_SESS], r12
    xor edi, edi
.get:
    cmp edi, 3
    jae .out
    mov eax, ID_EDIT_A
    cmp edi, 2
    jb .id
    mov eax, ID_EDIT_C
    jmp .go
.id:
    add eax, edi
.go:
    invoke GetDlgItem, rsi, rax
    imul ecx, edi, 1040
    lea rdx, [rbx+FM_ROOT+rcx]
    invoke GetWindowTextW, rax, rdx, 519
    imul ecx, edi, 1040
    lea rcx, [rbx+FM_ROOT+rcx]
    call fm_norm
    inc edi
    jmp .get
.out:
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke GetWindowTextW, rax, &rbx+FM_OUT, 519
    cmp word [rbx+FM_ROOT], 0
    je .nopath
    cmp word [rbx+FM_ROOT+1040], 0
    je .nopath
    ; every folder that was entered must exist: a mistyped one would read as "everything deleted"
    xor edi, edi
.exist:
    cmp edi, 3
    jae .scan
    imul eax, edi, 1040
    lea r13, [rbx+FM_ROOT+rax]
    inc edi
    cmp word [r13], 0
    je .exist
    invoke GetFileAttributesW, r13
    cmp eax, -1
    je .nodir
    test eax, 0x10
    jnz .exist
.nodir:
    invoke fm_clear, rbx
    invoke fm_refresh, rsi, rbx
    lea rdi, [rsp+96]
    invoke _snwprintf, rdi, 759, &f_nodir, r13
    mov word [rdi+1518], 0
    invoke msgbox, rsi, rdi, MB_OK|MB_ICONWARNING
    EPILOG 1648
.scan:
    invoke LoadCursorW, 0, 32514      ; IDC_WAIT
    invoke SetCursor, rax
    mov r14, rax
    invoke fm_scan, rbx
    xor edi, edi
.dec:
    cmp rdi, [rbx+FM_ENT+8]
    jae .refresh
    mov rax, rdi
    shl rax, 6
    add rax, [rbx+FM_ENT]
    invoke fm_decide, rbx, rax
    inc rdi
    jmp .dec
.refresh:
    invoke SetCursor, r14
    invoke fm_refresh, rsi, rbx
    invoke wcsrchr, &rbx+FM_ROOT, 0x5C
    lea r13, [rbx+FM_ROOT]
    test rax, rax
    jz .t
    cmp word [rax+2], 0
    je .t
    lea r13, [rax+2]
.t:
    invoke _snwprintf, &rsp+96, 50, &f_title, r13
    mov word [rsp+96+100], 0
    invoke set_tab_title, r12, &rsp+96
    EPILOG 1648
.nopath:
    invoke msgbox, rsi, &s_nopath, MB_OK|MB_ICONINFORMATION
    EPILOG 1648

; ---------------------------------------------------------------- actions
; fm_entry(fm, visible index) -> FE*
fm_entry:
    mov rax, [rcx+FM_VIS]
    mov eax, [rax+rdx*4]
    shl rax, 6
    add rax, [rcx+FM_ENT]
    ret

; fm_setact(hwnd, fm, act): manual action for every selected row
fm_setact:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    invoke SendMessageW, [rbx+FM_HLIST], LVM_GETSELECTEDCOUNT, 0, 0
    test eax, eax
    jz .nosel
    mov rdi, -1
.n:
    invoke SendMessageW, [rbx+FM_HLIST], LVM_GETNEXTITEM, rdi, LVNI_SELECTED
    movsxd rdi, eax
    test rdi, rdi
    js .done
    cmp rdi, [rbx+FM_NVIS]
    jae .done
    mov rcx, rbx
    mov rdx, rdi
    call fm_entry
    ; the chosen side must exist
    mov ecx, [rax+FE_FLAGS]
    cmp r12d, ACT_A
    jne .cb
    test ecx, 1
    jz .n
.cb:
    cmp r12d, ACT_B
    jne .cc
    test ecx, 2
    jz .n
.cc:
    cmp r12d, ACT_C
    jne .set
    test ecx, 4
    jz .n
.set:
    mov [rax+FE_MAN], r12d
    jmp .n
.done:
    invoke fm_refresh, rsi, rbx
    EPILOG 64
.nosel:
    invoke msgbox, rsi, &s_nosel, MB_OK|MB_ICONINFORMATION
    EPILOG 64

; fm_path(fm, e, side, out): full path of an entry on one side
fm_path:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r9
    imul eax, r8d, 1040
    lea r12, [rsi+FM_ROOT+rax]
    invoke _snwprintf, rbx, 1090, &f_full, r12, [rdi+FE_NAME]
    EPILOG 64

; fm_openmerge(hwnd, fm): 3-way text merge for the first selected conflict
fm_openmerge:
    PROLOG 9008
    mov rsi, rcx
    mov rbx, rdx
    invoke SendMessageW, [rbx+FM_HLIST], LVM_GETNEXTITEM, -1, LVNI_SELECTED
    movsxd rdi, eax
    test rdi, rdi
    js .nosel
    cmp rdi, [rbx+FM_NVIS]
    jae .nosel
    mov rcx, rbx
    mov rdx, rdi
    call fm_entry
    mov r12, rax
    mov eax, [r12+FE_FLAGS]
    and eax, 0x73
    cmp eax, 3
    jne .noopen
    invoke fm_outcheck, rsi, rbx
    test eax, eax
    jz .d
    invoke fm_path, rbx, r12, 0, &rsp+96
    invoke fm_path, rbx, r12, 1, &rsp+2300
    mov word [rsp+4500], 0
    test dword [r12+FE_FLAGS], 4
    jz .nobase
    invoke fm_path, rbx, r12, 2, &rsp+4500
.nobase:
    invoke _snwprintf, &rsp+6700, 1090, &f_full, &rbx+FM_OUT, [r12+FE_NAME]
    ; make sure the folder of the output file exists
    invoke wcsrchr, &rsp+6700, 0x5C
    test rax, rax
    jz .mk
    mov word [rax], 0
    invoke SHCreateDirectoryExW, 0, &rsp+6700, 0
    invoke _snwprintf, &rsp+6700, 1090, &f_full, &rbx+FM_OUT, [r12+FE_NAME]
.mk:
    invoke sess_new, T_MERGE, &rsp+96, &rsp+2300, 0
    test rax, rax
    jz .d
    mov r13, rax
    lea rcx, [r13+SS_PATHC]
    invoke wcsncpy, rcx, &rsp+4500, 519
    lea rcx, [r13+SS_PATHO]
    invoke wcsncpy, rcx, &rsp+6700, 519
    invoke sess_open, r13
    jmp .d
.nosel:
    invoke msgbox, rsi, &s_nosel, MB_OK|MB_ICONINFORMATION
    jmp .d
.noopen:
    invoke msgbox, rsi, &s_noopen, MB_OK|MB_ICONINFORMATION
.d:
    EPILOG 9008

; fm_mtime(path) -> rax last-write FILETIME (0 when missing)
fm_mtime:
    PROLOG 128
    invoke GetFileAttributesExW, rcx, 0, &rsp+96
    xor ecx, ecx
    test eax, eax
    jz .d
    mov rcx, [rsp+96+20]
.d:
    mov rax, rcx
    EPILOG 128

; fm_run(hwnd, fm): write the merge result into the output folder
fm_run:
    PROLOG 4608
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+FM_ENT+8], 0
    je .nocmp
    invoke fm_outcheck, rsi, rbx
    test eax, eax
    jz .ret
    invoke fm_used, &rbx+FM_OUT
    test eax, eax
    jz .go
    invoke msgbox, rsi, &s_out_used, MB_YESNO|MB_ICONWARNING
    cmp eax, IDYES
    jne .ret
.go:
    invoke LoadCursorW, 0, 32514      ; IDC_WAIT
    invoke SetCursor, rax
    mov [rsp+4576], rax
    xor r12d, r12d                    ; copied
    xor r13d, r13d                    ; merged (already written)
    xor r14d, r14d                    ; excluded
    xor r15d, r15d                    ; unresolved
    mov dword [rsp+4512], 0           ; copy failures
    invoke SHCreateDirectoryExW, 0, &rbx+FM_OUT, 0
    xor edi, edi
.e:
    cmp rdi, [rbx+FM_ENT+8]
    jae .summary
    mov rbp, rdi
    shl rbp, 6
    add rbp, [rbx+FM_ENT]
    inc edi
    mov eax, [rbp+FE_ACT]
    cmp dword [rbp+FE_MAN], 0
    je .act
    mov eax, [rbp+FE_MAN]
.act:
    mov [rsp+4500], eax
    invoke _snwprintf, &rsp+2300, 1090, &f_full, &rbx+FM_OUT, [rbp+FE_NAME]
    mov eax, [rsp+4500]
    cmp eax, ACT_NONE
    je .e
    cmp eax, ACT_DEL
    je .excl
    cmp eax, ACT_CONF
    je .conf
    ; copy from side (act-1)
    lea r8d, [rax-1]
    mov [rsp+4504], r8d
    invoke fm_path, rbx, rbp, r8, &rsp+96
    ; folders
    mov r8d, [rsp+4504]
    mov eax, 0x10
    mov ecx, r8d
    shl eax, cl
    test [rbp+FE_FLAGS], eax
    jz .file
    invoke SHCreateDirectoryExW, 0, &rsp+2300, 0
    jmp .e
.file:
    invoke wcsrchr, &rsp+2300, 0x5C
    test rax, rax
    jz .cp
    mov word [rax], 0
    invoke SHCreateDirectoryExW, 0, &rsp+2300, 0
    invoke _snwprintf, &rsp+2300, 1090, &f_full, &rbx+FM_OUT, [rbp+FE_NAME]
.cp:
    invoke CopyFileW, &rsp+96, &rsp+2300, 0
    test eax, eax
    jnz .copied
    inc dword [rsp+4512]
    jmp .e
.copied:
    inc r12d
    jmp .e
.excl:
    test dword [rbp+FE_FLAGS], 0x70   ; folders are not counted as excluded files
    jnz .e
    inc r14d
    jmp .e
.conf:
    test dword [rbp+FE_FLAGS], 0x70
    jnz .unres
    ; a conflict counts as merged only when the output was written after both inputs last changed
    invoke fm_mtime, &rsp+2300
    mov [rsp+4560], rax
    test rax, rax
    jz .unres
    mov qword [rsp+4568], 0
    test dword [rbp+FE_FLAGS], 1
    jz .nol
    invoke fm_path, rbx, rbp, 0, &rsp+96
    invoke fm_mtime, &rsp+96
    cmp rax, [rsp+4568]
    jbe .nol
    mov [rsp+4568], rax
.nol:
    test dword [rbp+FE_FLAGS], 2
    jz .nor
    invoke fm_path, rbx, rbp, 1, &rsp+96
    invoke fm_mtime, &rsp+96
    cmp rax, [rsp+4568]
    jbe .nor
    mov [rsp+4568], rax
.nor:
    mov rax, [rsp+4560]
    cmp rax, [rsp+4568]
    jb .unres
    inc r13d
    jmp .e
.unres:
    inc r15d
    jmp .e
.summary:
    mov eax, [rsp+4512]
    mov [rsp+4572], eax
    invoke SetCursor, [rsp+4576]
    invoke _snwprintf, &rsp+96, 120, &f_done, r12, r13, r14, r15, [rsp+4572]
    mov word [rsp+96+240], 0
    invoke set_status, &rsp+96
    mov r8d, MB_OK|MB_ICONINFORMATION ; unresolved conflicts or failed copies: a warning
    mov eax, [rsp+4572]
    or eax, r15d
    jz .box
    mov r8d, MB_OK|MB_ICONWARNING
.box:
    invoke msgbox, rsi, &rsp+96, r8
.ret:
    EPILOG 4608
.nocmp:
    invoke msgbox, rsi, &s_nocmp, MB_OK|MB_ICONINFORMATION
    EPILOG 4608

; ---------------------------------------------------------------- view
%macro PLACE 2
    mov ecx, %2
    call scl
    mov ebx, eax
    ; wrap to the next toolbar row when the control would leave the client area
    cmp r12d, r14d
    je %%fit
    lea eax, [r12+rbx]
    cmp eax, [rsp+104]
    jbe %%fit
    mov r12d, r14d
    lea r13d, [r13+r15]
    add r13d, r14d
%%fit:
    MVID rsi, %1, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
%endmacro

fm_layout:
    PROLOG 224
    mov rsi, rcx
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, 4
    call scl
    mov r14d, eax
    mov ecx, 26
    call scl
    mov r15d, eax
    mov edx, [rsp+104]
    invoke pb_layout, rsi, rdx, r14
    mov r13d, eax
    ; row 2: base and output
    mov eax, r14d
    lea eax, [rax*2+r14]
    mov edx, [rsp+104]
    sub edx, eax
    jns .h
    xor edx, edx
.h:
    shr edx, 1
    mov [rsp+112], edx
    mov ecx, 30
    call scl
    mov [rsp+116], eax
    mov ecx, 110
    call scl
    mov [rsp+120], eax
    mov r12d, r14d
    mov ebx, [rsp+120]
    MVID rsi, ID_LBL_BASE, r12, r13, rbx, r15
    add r12d, [rsp+120]
    mov ebx, [rsp+112]
    sub ebx, [rsp+120]
    sub ebx, [rsp+116]
    MVID rsi, ID_EDIT_C, r12, r13, rbx, r15
    add r12d, ebx
    mov ebx, [rsp+116]
    MVID rsi, ID_BROWSE_C, r12, r13, rbx, r15
    mov r12d, r14d
    add r12d, [rsp+112]
    add r12d, r14d
    mov ebx, [rsp+120]
    MVID rsi, ID_LBL_OUT, r12, r13, rbx, r15
    add r12d, [rsp+120]
    mov ebx, [rsp+112]
    sub ebx, [rsp+120]
    sub ebx, [rsp+116]
    MVID rsi, ID_EDIT_O, r12, r13, rbx, r15
    add r12d, ebx
    mov ebx, [rsp+116]
    MVID rsi, ID_BROWSE_O, r12, r13, rbx, r15
    ; row 3: buttons
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_COMPARE, 96
    PLACE ID_BTN_TA, 56
    PLACE ID_BTN_TB, 56
    PLACE ID_BTN_TC, 40
    PLACE ID_BTN_TD, 40
    PLACE ID_BTN_OPENM, 40
    PLACE ID_BTN_RUNM, 40
    PLACE ID_BTN_DOC, 90
    ; row 4
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_CHKF, 130
    PLACE ID_LBLI, 500
    lea r13d, [r13+r15]
    add r13d, r14d
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .l
    xor ebp, ebp
.l:
    mov ebx, [rsp+104]
    MVID rsi, ID_LST, 0, r13, rbx, rbp
    EPILOG 224

fm_theme:
    PROLOG 64
    mov rbx, rcx
    mov rdi, rdx
    mov rsi, [rbx+FM_HLIST]
    invoke SendMessageW, rsi, LVM_SETBKCOLOR, 0, #[th_bg]
    invoke SendMessageW, rsi, LVM_SETTEXTCOLOR, 0, #[th_fg]
    invoke SendMessageW, rsi, LVM_SETTEXTBKCOLOR, 0, #[th_bg]
    lea rdx, [s_light_theme]
    cmp qword [g_dark], 0
    je .t
    lea rdx, [s_dark_theme]
.t:
    invoke SetWindowTheme, rsi, rdx, 0
    invoke InvalidateRect, rsi, 0, 1
    invoke theme_checks, rdi
    EPILOG 64

; fm_cell(fm, e, col, out ptr slot) -> text pointer
fm_cell:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    cmp r12d, 1
    jb .name
    je .stat
    cmp r12d, 2
    je .act
    ; sizes
    lea eax, [r12-3]
    mov r13d, eax                     ; side
    mov ecx, r13d
    mov eax, 1
    shl eax, cl
    test [rsi+FE_FLAGS], eax
    jz .dash
    mov eax, 0x10
    mov ecx, r13d
    shl eax, cl
    test [rsi+FE_FLAGS], eax
    jnz .dash
    imul eax, r13d, 128
    lea rdi, [rbx+FM_TMP+rax]
    invoke fmt_size, rdi, [rsi+FE_SZ+r13*8]
    mov rax, rdi
    EPILOG 64
.dash:
    lea rax, [s_dash]
    EPILOG 64
.name:
    mov rax, [rsi+FE_NAME]
    EPILOG 64
.stat:
    mov eax, [rsi+FE_ST]
    lea rcx, [st_names]
    mov rax, [rcx+rax*8]
    EPILOG 64
.act:
    mov eax, [rsi+FE_ACT]
    cmp dword [rsi+FE_MAN], 0
    je .an
    mov eax, [rsi+FE_MAN]
.an:
    lea rcx, [ac_names]
    mov rax, [rcx+rax*8]
    EPILOG 64

fm_proc:
    PROLOG 320
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_ERASEBKGND
    je .erase
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    test rax, rax
    jz .def
    mov r14, rax
    mov r15, [r14+SS_DATA]
    test r15, r15
    jz .def
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_COMMAND
    je .command
    cmp edi, WM_NOTIFY
    je .notify
    cmp edi, BC_RUN
    je .run
    cmp edi, BC_COLLECT
    je .collect
    cmp edi, BC_THEME
    je .theme
    cmp edi, WM_CTLCOLOREDIT
    je .cedit
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLORBTN
    je .cstatic
    cmp edi, WM_SETFOCUS
    je .focus
    cmp edi, WM_DESTROY
    je .destroy
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 320

.create:
    mov r14, [r13]
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, r14
    invoke calloc, 1, FM_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    invoke pb_create, rsi
    invoke mk_lbl, rsi, &s_l_base, ID_LBL_BASE
    invoke mk_edit, rsi, ID_EDIT_C
    invoke mk_btn, rsi, &s_dots, ID_BROWSE_C
    invoke mk_lbl, rsi, &s_l_out, ID_LBL_OUT
    invoke mk_edit, rsi, ID_EDIT_O
    invoke mk_btn, rsi, &s_dots, ID_BROWSE_O
    invoke mk_btn, rsi, &s_b_ta, ID_BTN_TA
    invoke mk_btn, rsi, &s_b_tb, ID_BTN_TB
    invoke mk_btn, rsi, &s_b_tc, ID_BTN_TC
    invoke mk_btn, rsi, &s_b_td, ID_BTN_TD
    invoke mk_btn, rsi, &s_b_openm, ID_BTN_OPENM
    invoke mk_btn, rsi, &s_b_runm, ID_BTN_RUNM
    invoke mk_btn, rsi, &s_b_doc, ID_BTN_DOC
    invoke icon_set, rsi, ID_BTN_TA, &g_usel, &s_b_ta
    invoke icon_set, rsi, ID_BTN_TB, &g_user, &s_b_tb
    invoke icon_set, rsi, ID_BTN_TC, &g_base, &s_b_tc
    invoke icon_set, rsi, ID_BTN_TD, &g_del, &s_b_td
    invoke icon_set, rsi, ID_BTN_OPENM, &g_edit, &s_b_openm
    invoke icon_set, rsi, ID_BTN_RUNM, &g_run, &s_b_runm
    xor r9d, r9d
    cmp word [r14+SS_OPTS], '1'
    sete r9b
    invoke mk_chk, rsi, &s_c_conf, ID_CHKF, r9
    invoke mk_lbl, rsi, &s_empty, ID_LBLI
    invoke tips_apply, rsi, &tt_fmerge
    invoke mk, rsi, &cn_listv, 0, LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER, ID_LST
    mov [r15+FM_HLIST], rax
    mov rbx, rax
    invoke SendMessageW, rbx, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER
    invoke lv_dark_header, rbx
    xor ebp, ebp
.col:
    cmp ebp, 6
    jae .coldone
    xor eax, eax
    mov [rsp+96], rax
    mov [rsp+104], rax
    mov [rsp+112], rax
    mov [rsp+120], rax
    mov [rsp+128], rax
    mov dword [rsp+96], LVCF_FMT|LVCF_WIDTH|LVCF_TEXT
    lea rax, [col_fmt]
    mov eax, [rax+rbp*4]
    mov [rsp+100], eax
    lea rax, [col_w]
    mov ecx, [rax+rbp*4]
    call scl
    mov [rsp+104], eax
    lea rax, [col_text]
    mov rax, [rax+rbp*8]
    mov [rsp+112], rax
    invoke SendMessageW, rbx, LVM_INSERTCOLUMNW, rbp, &rsp+96
    inc ebp
    jmp .col
.coldone:
    invoke pb_load, rsi, r14
    invoke GetDlgItem, rsi, ID_EDIT_C
    invoke SetWindowTextW, rax, &r14+SS_PATHC
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke SetWindowTextW, rax, &r14+SS_PATHO
    invoke fm_theme, r15, rsi
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320

.size:
    invoke fm_layout, rsi
    jmp .zero
.run:
    invoke fm_compare, rsi, r15, r14
    test qword [g_batch], 4           ; unattended: the first compare ends the run
    jz .zero
    cmp qword [g_exitcode], 2         ; a folder could not be read: no report of an empty compare
    je .bdx_ok
    test qword [g_batch], 0x20        ; /docx file: export it first
    jz .bdx_ok
    and qword [g_batch], ~0x20
    invoke fm_docx_path, rsi, r15, &g_dx_path
    test eax, eax
    jnz .bdx_ok
    mov qword [g_exitcode], 2
.bdx_ok:
    xor eax, eax                      ; exit code: 0 no conflict, 1 conflicts left, 2 error
    mov rcx, [r15+FM_ENT+8]
    mov rdx, [r15+FM_ENT]
.bdx_c:
    test rcx, rcx
    jz .bdx_d
    cmp dword [rdx+FE_ACT], ACT_CONF
    jne .bdx_n
    mov eax, 1
.bdx_n:
    add rdx, 64
    dec rcx
    jmp .bdx_c
.bdx_d:
    invoke batch_done, rax
    jmp .zero
.doc:
    invoke fm_export, rsi, r15
    jmp .zero
.collect:
    invoke pb_collect, rsi, r14
    invoke GetDlgItem, rsi, ID_EDIT_C
    invoke GetWindowTextW, rax, &r14+SS_PATHC, 519
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke GetWindowTextW, rax, &r14+SS_PATHO, 519
    invoke get_chk, rsi, ID_CHKF
    invoke _snwprintf, &r14+SS_OPTS, 1023, &f_opts, rax
    jmp .zero
.theme:
    invoke fm_theme, r15, rsi
    jmp .zero

.command:
    movzx ebx, r12w
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_BROWSE_B
    je .browse
    cmp ebx, ID_SWAP
    je .browse
    cmp ebx, ID_BROWSE_C
    je .bc
    cmp ebx, ID_BROWSE_O
    je .bo
    cmp ebx, ID_CHKF
    je .filter
    cmp ebx, ID_BTN_TA
    je .ta
    cmp ebx, ID_BTN_TB
    je .tb
    cmp ebx, ID_BTN_TC
    je .tc
    cmp ebx, ID_BTN_TD
    je .td
    cmp ebx, ID_BTN_OPENM
    je .openm
    cmp ebx, ID_BTN_RUNM
    je .runm
    cmp ebx, ID_BTN_DOC
    je .doc
    jmp .def
.browse:
    invoke pb_command, rsi, rbx, 1
    jmp .zero
.bc:
    lea rdi, [rsp+96]
    invoke browse_folder, rsi, rdi
    test eax, eax
    jz .zero
    invoke GetDlgItem, rsi, ID_EDIT_C
    invoke SetWindowTextW, rax, &rsp+96
    jmp .zero
.bo:
    lea rdi, [rsp+96]
    invoke browse_folder, rsi, rdi
    test eax, eax
    jz .zero
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke SetWindowTextW, rax, &rsp+96
    jmp .zero
.filter:
    invoke fm_refresh, rsi, r15
    jmp .zero
.ta:
    invoke fm_setact, rsi, r15, ACT_A
    jmp .zero
.tb:
    invoke fm_setact, rsi, r15, ACT_B
    jmp .zero
.tc:
    invoke fm_setact, rsi, r15, ACT_C
    jmp .zero
.td:
    invoke fm_setact, rsi, r15, ACT_DEL
    jmp .zero
.openm:
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke GetWindowTextW, rax, &r15+FM_OUT, 519
    invoke fm_openmerge, rsi, r15
    jmp .zero
.runm:
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke GetWindowTextW, rax, &r15+FM_OUT, 519
    invoke fm_run, rsi, r15
    jmp .zero

.notify:
    mov rax, [r13+NMH_FROM]
    cmp rax, [r15+FM_HLIST]
    jne .def
    mov eax, [r13+NMH_CODE]
    cmp eax, LVN_GETDISPINFOW
    je .disp
    cmp eax, NM_CUSTOMDRAW
    je .cdraw
    cmp eax, NM_DBLCLK
    je .openm
    jmp .def
.disp:
    test dword [r13+DI_MASK], LVIF_TEXT
    jz .zero
    mov eax, [r13+DI_ITEM]
    cmp rax, [r15+FM_NVIS]
    jae .zero
    mov rcx, r15
    mov rdx, rax
    call fm_entry
    mov rdx, rax
    mov rcx, r15
    mov r8d, [r13+DI_SUB]
    call fm_cell
    mov [r13+DI_TEXT], rax
    jmp .zero
.cdraw:
    mov eax, [r13+CD_STAGE]
    cmp eax, CDDS_PREPAINT
    jne .cd2
    mov eax, CDRF_NOTIFYITEMDRAW
    EPILOG 320
.cd2:
    cmp eax, CDDS_ITEMPREPAINT
    jne .zero
    mov rax, [r13+CD_ITEMSPEC]
    cmp rax, [r15+FM_NVIS]
    jae .zero
    mov rcx, r15
    mov rdx, rax
    call fm_entry
    mov ecx, [rax+FE_ST]
    mov edx, [th_bg]
    mov r8d, [th_fg]
    cmp ecx, 9
    je .cconf
    cmp ecx, 3
    jne .c1
.cconf:
    mov edx, [th_conf]
    jmp .cset
.c1:
    cmp ecx, 1
    jne .c2
    mov edx, [th_lonly]
    jmp .cset
.c2:
    cmp ecx, 4
    jne .c3
    mov edx, [th_lonly]
    jmp .cset
.c3:
    cmp ecx, 2
    je .c4
    cmp ecx, 5
    jne .c5
.c4:
    mov edx, [th_ronly]
    jmp .cset
.c5:
    cmp ecx, 6
    jne .c6
    mov edx, [th_diff]
    jmp .cset
.c6:
    mov r8d, [th_same]
.cset:
    mov [r13+CD_CLRTEXTBK], edx
    mov [r13+CD_CLRTEXT], r8d
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+FM_HLIST]
    jmp .zero
.destroy:
    invoke fm_clear, r15
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320

section .rdata
WSTR s_c_conf_dummy, ""
%include "fmerge_doc.inc"

; ---------------------------------------------------------------- tooltips of this view
extern tips_apply
section .rdata
WSTR tt_fmerge_0, "공통 조상(Base) 폴더 경로"
WSTR tt_fmerge_1, "Base 폴더 찾아보기"
WSTR tt_fmerge_2, "병합 결과를 쓸 출력 폴더 경로"
WSTR tt_fmerge_3, "출력 폴더 찾아보기"
WSTR tt_fmerge_4, "병합 계획을 워드 파일(.docx)로 저장합니다"
WSTR tt_fmerge_5, "충돌 항목만 표시합니다"
align 8
tt_fmerge:
    dq ID_EDIT_C, tt_fmerge_0
    dq ID_BROWSE_C, tt_fmerge_1
    dq ID_EDIT_O, tt_fmerge_2
    dq ID_BROWSE_O, tt_fmerge_3
    dq ID_BTN_DOC, tt_fmerge_4
    dq ID_CHKF, tt_fmerge_5
    dq 0, 0
section .text
