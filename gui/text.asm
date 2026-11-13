; text.asm - Text Compare session (TC-01 .. TC-03, TC-06 partial)
;   aligned side-by-side RichEdit panes, intra-line highlight, inline edit, block copy, save
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"
%include "inc/export.inc"

extern PostMessageW, wcsstr, ShowWindow
extern g_batch, g_xl_path, g_exitcode, g_hframe, batch_done
extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetFocus, GetWindowTextW, SetWindowTextW
extern InvalidateRect, SetWindowTheme
extern malloc, calloc, free, memcpy, memset, wcslen, wcscpy, wcscmp, wcsrchr, _snwprintf
extern DrawTextW, SelectObject, SetBkMode, SetTextColor, FrameRect, CreateSolidBrush, DeleteObject, BeginPaint, EndPaint
extern g_font, g_iconfont, th_same, th_conf
extern th_tc_mod, th_tc_modin, th_tc_ins, th_tc_del, th_tc_modb, th_tc_insb, th_tc_delb, th_tc_gap
extern g_hinst, g_mono, th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_hl, th_panelbrush
extern save_error
extern mk, reg_class, scl, set_status, msgbox
extern mk_btn, mk_chk, mk_lbl, mk_edit, get_chk, ctl_color, theme_checks
extern pb_create, pb_layout, pb_load, pb_collect, pb_command
extern rich_create, rich_set_text, rich_get_text, rich_bk, rich_theme, rich_len, rich_unsel, rich_quiet, rich_batch
extern sb_init, sb_free, text_load, text_save, lines_split, compare_lines, hunk_free
extern g_tc_rules, icon_set, g_prev, g_next, g_copyr, g_copyl, set_tab_title, vec_push, rich_theme
extern _wcsicmp, rich_get_text, g_dark, set_status_part

global text_register, text_create, edit_create, tv_anchor_add, tv_cmp_anch

; ---- state
%define TV_HA     0
%define TV_HB     8
%define TV_ROWS   16
%define TV_NROWS  24
%define TV_BLK    32
%define TV_NBLK   40
%define TV_GUARD  48
%define TV_LDA    56
%define TV_LDB    60
%define TV_BUFA   64
%define TV_BUFB   72
%define TV_LENA   80
%define TV_LENB   88
%define TV_TXA    96            ; TXT (32 bytes)
%define TV_TXB    128
%define TV_LASTP  160           ; last focused pane (0 left / 1 right)
%define TV_CUR    168           ; current block index
%define TV_PATHA  176           ; wchar[520]
%define TV_PATHB  1216
%define TV_NLA    2256
%define TV_NLB    2264
%define TV_GT0    2272
%define TV_GT1    2296
%define TV_HGL    2320
%define TV_HGR    2328
%define TV_HMM    2336
%define TV_FORCE  2344
%define TV_ANC    2368
%define TV_NANC   2880
%define TV_BM     2896
%define TV_NBM    3152
%define TV_PEND   3408           ; a redraw request (WM_APP+50) is already in the queue
%define TV_FOLD   3440          ; fold option read at the last compare
%define TV_FOLDED 3448          ; the displayed rows are folded
%define TV_OPTS   3472          ; compare options of the last compare (engine flags)
%define TV_CMT    3480          ; comment prefix of the last compare, wchar[64]
%define TV_EXPN   3616          ; expanded fold fragments: count
%define TV_EXPK   3624          ; dword[64]: first hidden row of each expanded fragment
%define TV_UNI    3888          ; unified view: option read at the last compare
%define TV_SIZE   3896
%define E_HR      40
%define ID_ED_FIND   303
%define ID_ED_NEXT   304
%define ID_ED_PREV   305
%define ID_ED_REPL   306
%define ID_ED_RONE   307
%define ID_ED_RALL   308
%define ID_ED_CASE   309
%define ID_ED_LFIND  312
%define ID_CHK_IDS   216
%define ID_CHK_FOLD  217
%define ID_CHK_UNI   218
%define ID_EDIT_RULES 232
%define ID_LBL_RULES 233
%define TV_RULES  3168
%define ID_ED_LREPL  313
%define ID_GUL    263
%define ID_GUR    264
%define ID_MINI   265

%define ID_RA       201
%define ID_RB       202
%define ID_CHK_CASE 211
%define ID_CHK_SPC  212
%define ID_CHK_TRIM 213
%define ID_CHK_CMT  214
%define ID_CHK_SYN  215
%define ID_BTN_ALN  247
%define ID_BTN_ALC  248
%define ID_BTN_XLS  249
%define MAX_ANC     64
%define ID_EDIT_CMT 231
%define ID_BTN_PREV 241
%define ID_BTN_NEXT 242
%define ID_BTN_CLR  243
%define ID_BTN_CRL  244
%define ID_BTN_SVL  245
%define ID_BTN_SVR  246
%define ID_LBL_INFO 250
%define FILLER      0x200B

section .rdata
WSTR cn_text, "BCText"
WSTR s_empty, ""
WSTR s_c_case, "대소문자 무시"
WSTR s_c_ids, "식별자 이름 무시"
WSTR s_l_rules, "대체 규칙(old=new;..):"
WSTR s_c_spc, "공백 무시"
WSTR s_c_trim, "앞뒤 공백 무시"
WSTR s_c_cmt, "주석 무시:"
WSTR s_prefix, "//"
WSTR s_b_prev, "▲ 이전 차이"
WSTR s_b_next, "▼ 다음 차이"
WSTR s_b_clr, "→ 가져오기"
WSTR s_b_crl, "← 가져오기"
WSTR s_b_svl, "왼쪽 저장"
WSTR s_b_svr, "오른쪽 저장"
WSTR s_info0, "차이 없음"
WSTR f_info, "차이 %llu개 · 현재 %llu/%llu · 줄 %llu ↔ %llu · %s · %s"
WSTR en_ansi, "ANSI"
WSTR en_utf8, "UTF-8"
WSTR en_utf8b, "UTF-8 BOM"
WSTR en_u16, "UTF-16"
WSTR eol_lf, "LF"
WSTR f_enceol, "%s · %s"
WSTR eol_crlf, "CRLF"
WSTR f_same, "두 파일이 같습니다 · 줄 %llu ↔ %llu · %s · %s"
WSTR f_title, "%s ↔ %s"
WSTR f_opts, "%d%d%d%d%d|%s"
WSTR s_cantread, "파일을 읽을 수 없습니다."
WSTR s_nopath, "저장할 파일 경로가 없습니다."
WSTR s_saved, "저장했습니다."
WSTR s_nodiff, "이동할 차이가 없습니다."
WSTR s_nocur, "가져올 차이 블록이 없습니다. 먼저 차이 위에 커서를 놓으세요."
WSTR s_dark_theme, "DarkMode_Explorer"
WSTR s_light_theme, "Explorer"

section .rdata
align 8
enc_names: dq en_ansi, en_utf8, en_utf8b, en_u16
eol_names: dq eol_lf, eol_crlf
section .text

text_register:
    PROLOG 64
    invoke reg_class, &cn_text, &tv_proc, 0
    invoke reg_class, &cn_edit, &ed_proc, 0
    invoke reg_class, &cn_gutter, &gt_proc, 0
    invoke reg_class, &cn_mini, &mm_proc, 0
    EPILOG 64

text_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_text, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- filler handling
; strip_fillers(text, len) -> new length: drops rows that are exactly one FILLER char
strip_fillers:
    PROLOG 64
    mov rsi, rcx                     ; text
    mov rdi, rdx                     ; len
    xor r8d, r8d                     ; read index
    xor r9d, r9d                     ; write index
.l:
    cmp r8, rdi
    jae .d
    mov r10, r8                      ; line start
.f:
    cmp r8, rdi
    jae .e
    cmp word [rsi+r8*2], 13
    je .e
    inc r8
    jmp .f
.e:
    ; line is [r10, r8); r8 is at the '\r' or end
    mov rax, r8
    sub rax, r10
    cmp rax, 1
    jne .keep
    cmp word [rsi+r10*2], FILLER
    jne .keep
    cmp r8, rdi                      ; filler row: skip it and its break
    jae .skipend
    inc r8
    jmp .l
.skipend:
    ; a filler as the very last row: also drop the break written before it
    test r9, r9
    jz .d
    dec r9
    jmp .d
.keep:
    mov r11, r10
.cp:
    cmp r11, r8
    jae .brk
    movzx eax, word [rsi+r11*2]
    mov [rsi+r9*2], ax
    inc r9
    inc r11
    jmp .cp
.brk:
    cmp r8, rdi
    jae .d
    mov word [rsi+r9*2], 13
    inc r9
    inc r8
    jmp .l
.d:
    mov word [rsi+r9*2], 0
    mov rax, r9
    EPILOG 64

; ---------------------------------------------------------------- build aligned rows
; sum_len(LINES*) -> total characters + number of lines
sum_len:
    PROLOG 64
    mov rsi, rcx
    xor ebx, ebx
    xor edi, edi
.l:
    cmp rdi, [rsi+8]
    jae .d
    mov rax, [rsi]
    invoke wcslen, [rax+rdi*8]
    lea rbx, [rbx+rax+1]
    inc rdi
    jmp .l
.d:
    mov rax, rbx
    EPILOG 64

%macro ADDROW 4
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    mov dword [rax], %1
    mov dword [rax+4], %2
    mov dword [rax+8], %3
    mov dword [rax+12], %4
    inc rsi
%endmacro


%include "text_ui.inc"
%include "text_xl.inc"
%include "text_word.inc"
%include "text_fold.inc"
%include "text_uni.inc"
%include "text_gutter.inc"
%include "text_syn.inc"
%include "text_align.inc"
%include "edit_ui.inc"
