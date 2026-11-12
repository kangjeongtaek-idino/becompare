; merge.asm - Text Merge session (TM-01 .. TM-03): 3-way (diff3) with a base file, 2-way without one
;   top: left | base | right (read only, changed lines coloured)   bottom: editable merge output
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"
%include "inc/export.inc"

extern PostMessageW, g_batch, g_dx_path, g_exitcode, g_hframe
extern icon_set, g_prev, g_next, g_usel, g_user, g_both, g_save
extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetWindowTextW, SetWindowTextW
extern InvalidateRect
extern malloc, calloc, free, memcpy, memset, wcslen, wcscmp, wcsrchr, _snwprintf
extern g_hinst, th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_conf, th_panelbrush
extern save_error
extern tv_anchor_add, tv_cmp_anch
extern mk, reg_class, scl, set_status, msgbox
extern mk_btn, mk_chk, mk_lbl, mk_edit, get_chk, ctl_color, theme_checks
extern pb_create, pb_layout, pb_load, pb_collect, pb_command
extern rich_create, rich_set_text, rich_get_text, rich_bk, rich_theme, rich_unsel
extern text_load, text_save, lines_split, compare_lines, hunk_free
extern sb_init, sb_append, sb_free, vec_push
extern set_tab_title
extern browse_open, browse_save

global merge_register, merge_create

; ---- state
%define MG_HL     0
%define MG_HO     8
%define MG_HR     16
%define MG_HM     24
%define MG_ENC    32
%define MG_CRLF   36
%define MG_EOL    40
%define MG_NCONF  48
%define MG_NAUTO  56
%define MG_SCTEXT 64
%define MG_SCL    72            ; LINES {ptrs, count}
%define MG_CONF   88            ; vec of {start,mid,end} (12 bytes)
%define MG_THREE  112
%define MG_TXL    120
%define MG_TXR    152
%define MG_TXO    184
%define MG_ANCB   216           ; TM-06: block laid out like a text view (TV_NANC / TV_ANC) so tv_anchor_add / tv_cmp_anch can be shared
%define MG_PRIS   224           ; pristine generated output, to detect hand edits before regenerating
%define MG_SIZE   256
%define MG_ANCSZ  2960          ; must exceed text.asm TV_NANC (2880) + 4

%define ID_RL       601
%define ID_RO       602
%define ID_RR       603
%define ID_RM       610
%define ID_BTN_PREV 641
%define ID_BTN_NEXT 642
%define ID_BTN_USEL 643
%define ID_BTN_USER 644
%define ID_BTN_BOTH 645
%define ID_BTN_SAVE 646
%define ID_BTN_ALLL 647
%define ID_BTN_ALLR 648
%define ID_LBL_INFO 650
%define ID_LBL_BASE 651
%define ID_LBL_OUT  652
%define ID_BTN_ALN  653
%define ID_BTN_ALC  654
%define ID_BTN_DOC  655

section .rdata
WSTR cn_merge, "BCMerge"
WSTR s_empty, ""
WSTR s_cr, `\r`
WSTR s_mk_l, "<<<<<<< 왼쪽"
WSTR s_mk_m, "======="
WSTR s_mk_r, ">>>>>>> 오른쪽"
WSTR s_dots, "…"
WSTR s_merge, "병합 (F5)"
WSTR s_b_prev, "▲ 이전 충돌"
WSTR s_b_next, "▼ 다음 충돌"
WSTR s_b_usel, "왼쪽 채택"
WSTR s_b_user, "오른쪽 채택"
WSTR s_b_both, "둘 다 채택"
WSTR s_b_save, "병합 결과 저장"
WSTR s_b_alll, "전체 왼쪽 우선"
WSTR s_b_allr, "전체 오른쪽 우선"
WSTR s_l_base, "기준(선택):"
WSTR s_l_out, "출력:"
WSTR f_info, "%s · 충돌 %llu개 · 자동 병합 %llu곳"
WSTR s_mode3, "3-way 병합"
WSTR s_mode2, "2-way 병합"
WSTR f_title, "병합 %s"
WSTR s_b_aln, "정렬 지정"
WSTR s_b_alc, "정렬 해제"
WSTR s_mg_hint, "정렬 지정: 먼저 병합(F5)을 실행하고, 위쪽 왼쪽·오른쪽 창에서 맞추려는 줄(여러 줄 선택 가능)을 각각 선택한 뒤 누르세요."
WSTR s_mg_3way, "정렬 지정은 기준 파일이 없는 2-way 병합에서만 사용할 수 있습니다."
WSTR s_mg_conf, "정렬 지정: 기존 정렬 지점과 순서가 맞지 않아 추가할 수 없습니다."
WSTR s_mg_ok, "정렬 지점을 추가하고 다시 병합했습니다."
WSTR s_mg_edited, "출력 창을 직접 수정했습니다. 정렬을 적용하면 출력이 다시 만들어져 수정 내용이 사라집니다. 계속할까요?"
WSTR s_cantread, "파일을 읽을 수 없습니다."
WSTR s_noconf, "충돌이 없습니다."
WSTR s_nocur, "채택할 충돌이 없습니다."
WSTR s_noout, "출력 파일 경로를 입력하세요."
WSTR s_saved, "병합 결과를 저장했습니다."
WSTR f_leftconf, "해결하지 않은 충돌이 %llu개 남아 있습니다. 그래도 저장할까요?"

section .text

merge_register:
    PROLOG 64
    invoke reg_class, &cn_merge, &mg_proc, 0
    EPILOG 64

merge_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_merge, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- building blocks
; emit_range(sb, LINES*, s, e): lines s..e-1, each followed by a CR
emit_range:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12, r9
.l:
    cmp rdi, r12
    jae .d
    mov rax, [rsi]
    invoke sb_append, rbx, [rax+rdi*8]
    invoke sb_append, rbx, &s_cr
    inc rdi
    jmp .l
.d:
    EPILOG 64

; seg_eq(kx*, xs, xe, ky*, ys, ye) -> eax 1 when the two line segments are identical
seg_eq:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov r13, r9
    mov r14, [rsp+64+112]            ; ys (5th argument)
    mov r15, [rsp+64+120]            ; ye (6th argument)
    sub r12, rdi                     ; length x
    sub r15, r14                     ; length y
    cmp r12, r15
    jne .no
    xor ebx, ebx
.l:
    cmp rbx, r12
    jae .yes
    lea rax, [rdi+rbx]
    mov rcx, [rsi+rax*8]
    lea rax, [r14+rbx]
    mov rdx, [r13+rax*8]
    call wcscmp
    test eax, eax
    jnz .no
    inc rbx
    jmp .l
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; conflict(sb, LL, ls, le, LR, rs, re): marker block with both sides  (7 args: pass via a frame block)
; block layout at rcx: 0 sb, 8 LL, 16 ls, 24 le, 32 LR, 40 rs, 48 re
conflict:
    PROLOG 64
    mov rbx, rcx
    invoke sb_append, [rbx], &s_mk_l
    invoke sb_append, [rbx], &s_cr
    invoke emit_range, [rbx], [rbx+8], [rbx+16], [rbx+24]
    invoke sb_append, [rbx], &s_mk_m
    invoke sb_append, [rbx], &s_cr
    invoke emit_range, [rbx], [rbx+32], [rbx+40], [rbx+48]
    invoke sb_append, [rbx], &s_mk_r
    invoke sb_append, [rbx], &s_cr
    EPILOG 64

; match_build(hv*, dword* match, n): match[baseLine] = other line, from the equal hunks
match_build:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    xor ecx, ecx
.init:
    cmp rcx, r12
    jae .h
    mov dword [rdi+rcx*4], -1
    inc rcx
    jmp .init
.h:
    xor ebx, ebx                     ; hunk index
.hl:
    cmp rbx, [rsi+8]
    jae .d
    lea rax, [rbx+rbx*2]
    shl rax, 3
    add rax, [rsi]
    cmp dword [rax], 0
    jne .hn
    mov ecx, [rax+4]                 ; aStart
    mov edx, [rax+12]                ; bStart
    mov r8d, [rax+8]                 ; length
.t:
    test r8d, r8d
    jz .hn
    mov [rdi+rcx*4], edx
    inc ecx
    inc edx
    dec r8d
    jmp .t
.hn:
    inc rbx
    jmp .hl
.d:
    EPILOG 64

; color_lines(hrich, LINES*, hv*, side 0=a 1=b, color): paint the lines of every change hunk
color_lines:
    PROLOG 96
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15d, r9d
    mov rax, [rsp+96+112]            ; color (5th argument)
    mov [rsp+96-8], eax
    ; prefix offsets of every line
    mov rax, [r13+8]
    lea rcx, [rax*8+16]
    call malloc
    mov rbp, rax                     ; offs[]
    xor esi, esi
    xor edi, edi                     ; running offset
.o:
    cmp rsi, [r13+8]
    jae .odone
    mov [rbp+rsi*8], rdi
    mov rax, [r13]
    invoke wcslen, [rax+rsi*8]
    lea rdi, [rdi+rax+1]
    inc rsi
    jmp .o
.odone:
    mov [rbp+rsi*8], rdi
    xor ebx, ebx
.h:
    cmp rbx, [r14+8]
    jae .done
    lea rax, [rbx+rbx*2]
    shl rax, 3
    add rax, [r14]
    cmp dword [rax], 1
    jne .hn
    mov ecx, [rax+4]                 ; a start
    mov edx, [rax+8]                 ; a len
    test r15d, r15d
    jz .have
    mov ecx, [rax+12]                ; b start
    mov edx, [rax+16]                ; b len
.have:
    test edx, edx
    jz .hn
    mov rsi, [rbp+rcx*8]             ; first char
    add ecx, edx
    mov rdi, [rbp+rcx*8]
    dec rdi                          ; exclude the last separator
    invoke rich_bk, r12, rsi, rdi, #[rsp+88]
.hn:
    inc rbx
    jmp .h
.done:
    invoke free, rbp
    EPILOG 96

; pane_text(hrich, LINES*): joined text, no trailing CR
pane_text:
    PROLOG 96
    mov r12, rcx
    mov rsi, rdx
    invoke sb_init, &rsp+64
    invoke emit_range, &rsp+64, rsi, 0, [rsi+8]
    mov rax, [rsp+64+8]
    test rax, rax
    jz .set
    dec rax
    mov [rsp+64+8], rax
    mov rcx, [rsp+64]
    mov word [rcx+rax*2], 0
.set:
    mov rax, [rsp+64]
    test rax, rax
    jnz .have
    lea rax, [s_empty]
.have:
    invoke rich_set_text, r12, rax
    invoke sb_free, &rsp+64
    EPILOG 96

; ---------------------------------------------------------------- scanning the output for conflicts
; has7(line, ch) -> eax 1 if the line starts with 7 copies of ch
has7:
    xor eax, eax
    xor r8d, r8d
.l:
    cmp r8d, 7
    jae .y
    cmp word [rcx+r8*2], dx
    jne .n
    inc r8d
    jmp .l
.y:
    mov eax, 1
.n:
    ret

mg_unscan:
    PROLOG 64
    mov rbx, rcx
    invoke free, [rbx+MG_SCTEXT]
    invoke free, [rbx+MG_SCL]
    invoke free, [rbx+MG_CONF]
    xor eax, eax
    mov [rbx+MG_SCTEXT], rax
    mov [rbx+MG_SCL], rax
    mov [rbx+MG_SCL+8], rax
    mov [rbx+MG_CONF], rax
    mov [rbx+MG_CONF+8], rax
    mov [rbx+MG_CONF+16], rax
    EPILOG 64

; mg_scan(mg): read the output pane, find <<<<<<< / ======= / >>>>>>> blocks
mg_scan:
    PROLOG 128
    mov rbx, rcx
    invoke mg_unscan, rbx
    invoke rich_get_text, [rbx+MG_HM], &rsp+96
    mov [rbx+MG_SCTEXT], rax
    test rax, rax
    jz .d
    invoke lines_split, rax, [rsp+96], &rbx+MG_SCL
    xor r12d, r12d                   ; line
    xor r13d, r13d                   ; state
.l:
    cmp r12, [rbx+MG_SCL+8]
    jae .d
    mov rax, [rbx+MG_SCL]
    mov r14, [rax+r12*8]
    test r13d, r13d
    jnz .st1
    invoke has7, r14, '<'
    test eax, eax
    jz .n
    mov r15d, r12d                   ; start
    mov r13d, 1
    jmp .n
.st1:
    cmp r13d, 1
    jne .st2
    invoke has7, r14, '='
    test eax, eax
    jz .n
    mov ebp, r12d                    ; mid
    mov r13d, 2
    jmp .n
.st2:
    invoke has7, r14, '>'
    test eax, eax
    jz .n
    invoke vec_push, &rbx+MG_CONF, 12
    mov [rax], r15d
    mov [rax+4], ebp
    mov [rax+8], r12d
    xor r13d, r13d
.n:
    inc r12
    jmp .l
.d:
    EPILOG 128

; mg_colorout(mg): colour conflict blocks in the output pane
mg_colorout:
    PROLOG 128
    mov rbx, rcx
    invoke SendMessageW, [rbx+MG_HM], EM_EXGETSEL, 0, &rsp+112
    invoke mg_scan, rbx
    mov r8, [rbx+MG_CONF+8]
    mov [rbx+MG_NCONF], r8
    ; character offsets of the lines
    mov rax, [rbx+MG_SCL+8]
    lea rcx, [rax*8+16]
    call malloc
    mov rbp, rax
    xor esi, esi
    xor edi, edi
.o:
    cmp rsi, [rbx+MG_SCL+8]
    jae .odone
    mov [rbp+rsi*8], rdi
    mov rax, [rbx+MG_SCL]
    invoke wcslen, [rax+rsi*8]
    lea rdi, [rdi+rax+1]
    inc rsi
    jmp .o
.odone:
    mov [rbp+rsi*8], rdi
    invoke SendMessageW, [rbx+MG_HM], WM_SETREDRAW, 0, 0
    xor r12d, r12d
.c:
    cmp r12, [rbx+MG_CONF+8]
    jae .redraw
    lea rax, [r12+r12*2]
    mov rcx, [rbx+MG_CONF]
    lea r13, [rcx+rax*4]             ; triple
    mov esi, [r13]                   ; start line
    mov edi, [r13+4]                 ; mid line
    mov r14d, [r13+8]                ; end line
    ; marker lines
    mov rdx, [rbp+rsi*8]
    mov r8, [rbp+rsi*8+8]
    dec r8
    invoke rich_bk, [rbx+MG_HM], rdx, r8, #[th_conf]
    mov rdx, [rbp+rdi*8]
    mov r8, [rbp+rdi*8+8]
    dec r8
    invoke rich_bk, [rbx+MG_HM], rdx, r8, #[th_conf]
    mov rdx, [rbp+r14*8]
    mov r8, [rbp+r14*8+8]
    dec r8
    invoke rich_bk, [rbx+MG_HM], rdx, r8, #[th_conf]
    ; left part: start+1 .. mid-1
    lea rax, [rsi+1]
    cmp eax, edi
    jae .rightpart
    mov rdx, [rbp+rax*8]
    mov r8, [rbp+rdi*8]
    dec r8
    invoke rich_bk, [rbx+MG_HM], rdx, r8, #[th_lonly]
.rightpart:
    lea rax, [rdi+1]
    cmp eax, r14d
    jae .nc
    mov rdx, [rbp+rax*8]
    mov r8, [rbp+r14*8]
    dec r8
    invoke rich_bk, [rbx+MG_HM], rdx, r8, #[th_ronly]
.nc:
    inc r12
    jmp .c
.redraw:
    invoke SendMessageW, [rbx+MG_HM], EM_EXSETSEL, 0, &rsp+112
    invoke SendMessageW, [rbx+MG_HM], WM_SETREDRAW, 1, 0
    invoke InvalidateRect, [rbx+MG_HM], 0, 1
    invoke free, rbp
    EPILOG 128

; mg_info(hwnd, mg)
mg_info:
    PROLOG 320
    mov rsi, rcx
    mov rbx, rdx
    lea rdi, [s_mode2]
    cmp qword [rbx+MG_THREE], 0
    je .m
    lea rdi, [s_mode3]
.m:
    invoke _snwprintf, &rsp+96, 120, &f_info, rdi, [rbx+MG_NCONF], [rbx+MG_NAUTO]
    invoke GetDlgItem, rsi, ID_LBL_INFO
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 320

; ---------------------------------------------------------------- the merge itself
%define M_LL   96
%define M_LO   112
%define M_LR   128
%define M_H1   144
%define M_H2   168
%define M_CP   192
%define M_MO   208
%define M_MR   216
%define M_DL   224
%define M_DO   232
%define M_DR   240
%define M_SB   248
%define M_BLK  272               ; conflict block (56 bytes)
%define M_TT   336
%define M_LJ   464
%define M_RJ   472

; dup_lines(TXT*, LINES*) -> rax private text copy (to free later)
dup_lines:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov rax, [rsi+8]
    lea rcx, [rax*2+8]
    call malloc
    mov rbx, rax
    mov rdx, [rsi+8]
    lea r8, [rdx*2+2]
    invoke memcpy, rbx, [rsi], r8
    invoke lines_split, rbx, [rsi+8], rdi
    mov rax, rbx
    EPILOG 64

; mg_merge(hwnd, mg, sess)
mg_merge:
    PROLOG 512
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke mg_free_texts, rbx
    ; ---- paths
    invoke pb_collect, rsi, r12
    invoke GetDlgItem, rsi, ID_EDIT_C
    invoke GetWindowTextW, rax, &r12+SS_PATHC, 519
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke GetWindowTextW, rax, &r12+SS_PATHO, 519
    invoke text_load, &r12+SS_PATHA, &rbx+MG_TXL
    test eax, eax
    jz .err
    invoke text_load, &r12+SS_PATHB, &rbx+MG_TXR
    test eax, eax
    jz .err
    xor r13d, r13d                   ; three-way?
    cmp word [r12+SS_PATHC], 0
    je .two
    invoke text_load, &r12+SS_PATHC, &rbx+MG_TXO
    test eax, eax
    jz .err
    mov r13d, 1
.two:
    mov [rbx+MG_THREE], r13
    mov eax, [rbx+MG_TXL+16]
    mov [rbx+MG_ENC], eax
    mov eax, [rbx+MG_TXL+20]
    mov [rbx+MG_CRLF], eax
    mov eax, [rbx+MG_TXL+24]
    mov [rbx+MG_EOL], eax
    mov word [rsp+M_CP], 0
    invoke dup_lines, &rbx+MG_TXL, &rsp+M_LL
    mov [rsp+M_DL], rax
    invoke dup_lines, &rbx+MG_TXR, &rsp+M_LR
    mov [rsp+M_DR], rax
    xor eax, eax
    mov [rsp+M_DO], rax
    mov [rsp+M_MO], rax               ; freed at the end even when 2-way leaves them unused
    mov [rsp+M_MR], rax
    mov [rsp+M_LO], rax
    mov [rsp+M_LO+8], rax
    test r13d, r13d
    jz .nobase
    invoke dup_lines, &rbx+MG_TXO, &rsp+M_LO
    mov [rsp+M_DO], rax
.nobase:
    xor eax, eax
    mov [rsp+M_H1], rax
    mov [rsp+M_H1+8], rax
    mov [rsp+M_H1+16], rax
    mov [rsp+M_H2], rax
    mov [rsp+M_H2+8], rax
    mov [rsp+M_H2+16], rax
    mov qword [rbx+MG_NAUTO], 0
    invoke sb_init, &rsp+M_SB
    mov rax, [rsp+M_SB]
    ; block pointer for conflict(): sb at +0, LL +8 ...
    lea rax, [rsp+M_SB]
    mov [rsp+M_BLK], rax
    lea rax, [rsp+M_LL]
    mov [rsp+M_BLK+8], rax
    lea rax, [rsp+M_LR]
    mov [rsp+M_BLK+32], rax
    test r13d, r13d
    jnz .three
    ; ---------------- 2-way: every differing hunk is a conflict
    invoke tv_cmp_anch, [rbx+MG_ANCB], &rsp+M_LL, &rsp+M_LR, 0, &rsp+M_CP, &rsp+M_H1
    xor r14d, r14d
.h2:
    cmp r14, [rsp+M_H1+8]
    jae .built
    lea rax, [r14+r14*2]
    shl rax, 3
    add rax, [rsp+M_H1]
    mov r15, rax
    cmp dword [r15], 0
    jne .c2
    mov r13d, [r15+4]
    mov edi, [r15+8]
    add edi, r13d
    invoke emit_range, &rsp+M_SB, &rsp+M_LL, r13, rdi
    jmp .n2
.c2:
    mov eax, [r15+4]
    mov [rsp+M_BLK+16], rax
    add eax, [r15+8]
    mov [rsp+M_BLK+24], rax
    mov eax, [r15+12]
    mov [rsp+M_BLK+40], rax
    add eax, [r15+16]
    mov [rsp+M_BLK+48], rax
    invoke conflict, &rsp+M_BLK
.n2:
    inc r14
    jmp .h2
    ; ---------------- 3-way
.three:
    invoke compare_lines, &rsp+M_LO, &rsp+M_LL, 0, &rsp+M_CP, &rsp+M_H1
    invoke compare_lines, &rsp+M_LO, &rsp+M_LR, 0, &rsp+M_CP, &rsp+M_H2
    mov rax, [rsp+M_LO+8]
    lea rcx, [rax*4+8]
    call malloc
    mov [rsp+M_MO], rax
    mov rax, [rsp+M_LO+8]
    lea rcx, [rax*4+8]
    call malloc
    mov [rsp+M_MR], rax
    invoke match_build, &rsp+M_H1, [rsp+M_MO], [rsp+M_LO+8]
    invoke match_build, &rsp+M_H2, [rsp+M_MR], [rsp+M_LO+8]
    xor r13d, r13d                   ; oi
    xor r14d, r14d                   ; li
    xor r15d, r15d                   ; ri
.loop:
    ; next stable base line j >= oi
    mov rcx, r13
.find:
    cmp rcx, [rsp+M_LO+8]
    jae .end
    mov rax, [rsp+M_MO]
    mov edx, [rax+rcx*4]
    cmp edx, -1
    je .nj
    mov rax, [rsp+M_MR]
    mov r8d, [rax+rcx*4]
    cmp r8d, -1
    jne .have
.nj:
    inc rcx
    jmp .find
.end:
    mov rcx, [rsp+M_LO+8]            ; j = nO
    mov rdx, [rsp+M_LL+8]            ; lj = nL
    mov r8, [rsp+M_LR+8]             ; rj = nR
.have:
    mov rbp, rcx                     ; j
    mov [rsp+M_LJ], rdx              ; lj
    mov [rsp+M_RJ], r8               ; rj
    ; region segments: O[r13,rbp) L[r14,lj) R[r15,rj)
    invoke seg_eq, [rsp+M_LO], r13, rbp, [rsp+M_LL], r14, [rsp+M_LJ]
    mov [rsp+M_BLK+16], eax          ; eqOL
    invoke seg_eq, [rsp+M_LO], r13, rbp, [rsp+M_LR], r15, [rsp+M_RJ]
    mov [rsp+M_BLK+20], eax          ; eqOR
    invoke seg_eq, [rsp+M_LL], r14, [rsp+M_LJ], [rsp+M_LR], r15, [rsp+M_RJ]
    mov [rsp+M_BLK+24], eax          ; eqLR
    cmp dword [rsp+M_BLK+16], 0
    je .notol
    cmp dword [rsp+M_BLK+20], 0
    jne .emitnone
    inc qword [rbx+MG_NAUTO]
    invoke emit_range, &rsp+M_SB, &rsp+M_LR, r15, [rsp+M_RJ]
    jmp .after
.emitnone:
    invoke emit_range, &rsp+M_SB, &rsp+M_LO, r13, rbp
    jmp .after
.notol:
    cmp dword [rsp+M_BLK+20], 0
    je .notor
    inc qword [rbx+MG_NAUTO]
    invoke emit_range, &rsp+M_SB, &rsp+M_LL, r14, [rsp+M_LJ]
    jmp .after
.notor:
    cmp dword [rsp+M_BLK+24], 0
    je .conf3
    inc qword [rbx+MG_NAUTO]
    invoke emit_range, &rsp+M_SB, &rsp+M_LL, r14, [rsp+M_LJ]
    jmp .after
.conf3:
    mov [rsp+M_BLK+16], r14
    mov rax, [rsp+M_LJ]
    mov [rsp+M_BLK+24], rax
    mov [rsp+M_BLK+40], r15
    mov rax, [rsp+M_RJ]
    mov [rsp+M_BLK+48], rax
    invoke conflict, &rsp+M_BLK
.after:
    cmp rbp, [rsp+M_LO+8]
    jae .built
    lea rax, [rbp+1]
    invoke emit_range, &rsp+M_SB, &rsp+M_LO, rbp, rax    ; the stable line itself
    lea r13, [rbp+1]
    mov rax, [rsp+M_LJ]
    lea r14, [rax+1]
    mov rax, [rsp+M_RJ]
    lea r15, [rax+1]
    jmp .loop
.built:
    ; drop the last CR so no extra empty line appears
    mov rax, [rsp+M_SB+8]
    test rax, rax
    jz .set
    dec rax
    mov [rsp+M_SB+8], rax
    mov rcx, [rsp+M_SB]
    mov word [rcx+rax*2], 0
.set:
    mov rax, [rsp+M_SB]
    test rax, rax
    jnz .have2
    lea rax, [s_empty]
.have2:
    mov r13, rax
    invoke rich_set_text, [rbx+MG_HM], r13
    invoke SendMessageW, [rbx+MG_HM], EM_SETMODIFY, 1, 0
    invoke rich_theme, [rbx+MG_HM]
    invoke free, [rbx+MG_PRIS]
    invoke wcslen, r13
    lea r14, [rax*2+2]
    mov rcx, r14
    call malloc
    mov [rbx+MG_PRIS], rax
    invoke memcpy, rax, r13, r14
    invoke sb_free, &rsp+M_SB
    ; top panes
    invoke rich_theme, [rbx+MG_HL]
    invoke rich_theme, [rbx+MG_HO]
    invoke rich_theme, [rbx+MG_HR]
    invoke pane_text, [rbx+MG_HL], &rsp+M_LL
    invoke pane_text, [rbx+MG_HR], &rsp+M_LR
    cmp qword [rbx+MG_THREE], 0
    je .color2
    invoke pane_text, [rbx+MG_HO], &rsp+M_LO
    invoke color_lines, [rbx+MG_HL], &rsp+M_LL, &rsp+M_H1, 1, [th_lonly]
    invoke color_lines, [rbx+MG_HO], &rsp+M_LO, &rsp+M_H1, 0, [th_diff]
    invoke color_lines, [rbx+MG_HO], &rsp+M_LO, &rsp+M_H2, 0, [th_diff]
    invoke color_lines, [rbx+MG_HR], &rsp+M_LR, &rsp+M_H2, 1, [th_ronly]
    jmp .colored
.color2:
    invoke rich_set_text, [rbx+MG_HO], &s_empty
    invoke color_lines, [rbx+MG_HL], &rsp+M_LL, &rsp+M_H1, 0, [th_lonly]
    invoke color_lines, [rbx+MG_HR], &rsp+M_LR, &rsp+M_H1, 1, [th_ronly]
.colored:
    invoke rich_unsel, [rbx+MG_HL]
    invoke rich_unsel, [rbx+MG_HO]
    invoke rich_unsel, [rbx+MG_HR]
    invoke mg_colorout, rbx
    invoke mg_info, rsi, rbx
    ; tab title
    invoke wcsrchr, &r12+SS_PATHA, 0x5C
    lea r13, [r12+SS_PATHA]
    test rax, rax
    jz .t1
    lea r13, [rax+2]
.t1:
    invoke _snwprintf, &rsp+M_TT, 60, &f_title, r13
    invoke set_tab_title, r12, &rsp+M_TT
    ; free temporaries
    invoke free, [rsp+M_DL]
    invoke free, [rsp+M_DR]
    invoke free, [rsp+M_DO]
    invoke free, [rsp+M_LL]
    invoke free, [rsp+M_LR]
    invoke free, [rsp+M_LO]
    invoke free, [rsp+M_MO]
    invoke free, [rsp+M_MR]
    invoke hunk_free, &rsp+M_H1
    invoke hunk_free, &rsp+M_H2
    mov eax, 1
    EPILOG 512
.err:
    invoke msgbox, rsi, &s_cantread, MB_OK|MB_ICONWARNING
    xor eax, eax
    EPILOG 512

%include "merge_ui.inc"
%include "merge_doc.inc"
