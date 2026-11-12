; table.asm - Table (CSV / TSV) Compare session (TB-01 .. TB-03)
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"
%include "inc/export.inc"

extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetWindowTextW, SetWindowTextW
extern InvalidateRect, SetWindowTheme
extern malloc, calloc, realloc, free, memset, wcslen, wcscpy, wcscmp, _wcsicmp, wcsrchr, _snwprintf, qsort, wcstod
extern g_hinst, g_dark, th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_hl, th_panelbrush
extern mk, reg_class, scl, set_status, msgbox
extern lv_dark_header, mk_btn, mk_chk, mk_lbl, mk_edit, get_chk, ctl_color, theme_checks
extern pb_create, pb_layout, pb_load, pb_collect, pb_command
extern sb_init, sb_append, sb_free, PostMessageW, g_batch, g_xl_path, g_exitcode, g_hframe
extern text_load, vec_push, lower_w, set_tab_title, malloc

global table_register, table_create

; SIDE (80 bytes): +0 cells vec, +24 rowoff vec(dword), +48 nrows, +56 text, +64 keys, +72 maxcols
%define SD_CELLS  0
%define SD_ROWOFF 24
%define SD_NROWS  48
%define SD_TEXT   56
%define SD_KEYS   64
%define SD_MAXC   72
%define SD_SIZE   80

; RES (24 bytes): +0 aRow, +4 bRow, +8 status, +12 nchg, +16 key
%define R_A       0
%define R_B       4
%define R_ST      8
%define R_NCHG    12
%define R_KEY     16
%define R_SIZE    24

; view state
%define TB_SA     0
%define TB_SB     80
%define TB_RES    160           ; vec
%define TB_VIS    184
%define TB_NVIS   192
%define TB_HLIST  200
%define TB_NCOLS  208           ; displayed data columns
%define TB_COLMAP 216           ; ptr dword[]
%define TB_FLAGS  224           ; bit0 header, bit1 case, bit2 trim, bit3 only differences, bit4 sort by key
%define TB_KEYM   232           ; 0 row number, N column N, -1 all
%define TB_IGN    240           ; ignored columns bitmask
%define TB_TOL    248           ; double
%define TB_HASTOL 256
%define TB_CNT    264           ; rows changed, cells changed, only A, only B, same
%define TB_PATHA  312
%define TB_PATHB  1352
%define TB_KEYMASK 2400
%define TB_SIZE   2416

%define ID_CHK_HDR   311
%define ID_CHK_CASE  312
%define ID_CHK_TRIM  313
%define ID_CHK_DIFF  314
%define ID_CHK_SORT  315
%define ID_BTN_XLS   316
%define ID_LBL_KEY   320
%define ID_EDIT_KEY  331
%define ID_LBL_TOL   321
%define ID_EDIT_TOL  332
%define ID_LBL_IGN   322
%define ID_EDIT_IGN  333
%define ID_LBL_INFO  350
%define ID_TLIST     360
%define LVCFMT_LEFT  0

section .rdata
WSTR cn_table, "BCTable"
WSTR cn_list, "SysListView32"
WSTR s_empty, ""
WSTR s_c_hdr, "머리글 행"
WSTR s_c_case, "대소문자 무시"
WSTR s_c_trim, "앞뒤 공백 무시"
WSTR s_c_diff, "차이만 표시"
WSTR s_c_sort, "키 순 정렬"
WSTR s_l_key, "키 열(0=행, 2,3=복합, all):"
WSTR s_l_tol, "허용 오차:"
WSTR s_l_ign, "무시할 열(예 3,5):"
WSTR s_key0, "0"
WSTR s_col0, "행"
WSTR f_col, "열 %d"
WSTR f_row1, "%d"
WSTR f_row2, "%d / %d"
WSTR f_rowa, "-%d"
WSTR f_rowb, "+%d"
WSTR f_cellchg, "%s → %s"
WSTR f_info, "행 A:%llu B:%llu · 다른 행 %llu · 다른 셀 %llu · A만 %llu · B만 %llu"
WSTR f_title, "%s ↔ %s"
WSTR f_opts, "%d%d%d%d%d|%s|%s|%s"
WSTR s_cantread, "파일을 읽을 수 없습니다."
WSTR s_all, "all"
WSTR s_dark_theme, "DarkMode_Explorer"
WSTR s_light_theme, "Explorer"

section .bss
alignb 8
g_qkeys:  resq 1
g_qcase:  resq 1

section .text

table_register:
    PROLOG 64
    invoke reg_class, &cn_table, &tb_proc, 0
    EPILOG 64

table_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_table, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- CSV parsing
; csv_detect(text) -> eax delimiter (tab, semicolon or comma) from the first line
csv_detect:
    xor r8d, r8d                     ; commas
    xor r9d, r9d                     ; tabs
    xor r10d, r10d                   ; semicolons
    xor r11d, r11d                   ; inside quotes
.l:
    movzx eax, word [rcx]
    test eax, eax
    jz .d
    cmp eax, 10
    je .d
    cmp eax, 13
    je .d
    cmp eax, '"'
    jne .nq
    xor r11d, 1
    jmp .n
.nq:
    test r11d, r11d
    jnz .n
    cmp eax, ','
    jne .t
    inc r8d
    jmp .n
.t:
    cmp eax, 9
    jne .s
    inc r9d
    jmp .n
.s:
    cmp eax, ';'
    jne .n
    inc r10d
.n:
    add rcx, 2
    jmp .l
.d:
    mov eax, ','
    cmp r9d, r8d
    jbe .c1
    mov eax, 9
    mov r8d, r9d
.c1:
    cmp r10d, r8d
    jbe .c2
    mov eax, ';'
.c2:
    ret

; csv_parse(text (modified in place), delim, SIDE*)
csv_parse:
    PROLOG 64
    mov rsi, rcx
    mov r12d, edx
    mov rbx, r8
    mov rdi, rsi                     ; write pointer
.rowstart:
    cmp word [rsi], 0
    je .done
    invoke vec_push, &rbx+SD_ROWOFF, 4
    mov rcx, [rbx+SD_CELLS+8]
    mov [rax], ecx
    inc qword [rbx+SD_NROWS]
    xor r14d, r14d                   ; columns in this row
.field:
    invoke vec_push, &rbx+SD_CELLS, 8
    mov [rax], rdi
    inc r14d
    movzx eax, word [rsi]
    cmp eax, '"'
    jne .plain
    add rsi, 2
.q:
    movzx eax, word [rsi]
    test eax, eax
    jz .fend
    add rsi, 2
    cmp eax, '"'
    jne .qc
    cmp word [rsi], '"'
    jne .junk
    add rsi, 2
    mov word [rdi], '"'
    add rdi, 2
    jmp .q
.qc:
    mov [rdi], ax
    add rdi, 2
    jmp .q
.junk:
    movzx eax, word [rsi]
    test eax, eax
    jz .fend
    cmp eax, r12d
    je .fend
    cmp eax, 10
    je .fend
    cmp eax, 13
    je .fend
    add rsi, 2
    jmp .junk
.plain:
    movzx eax, word [rsi]
    test eax, eax
    jz .fend
    cmp eax, r12d
    je .fend
    cmp eax, 10
    je .fend
    cmp eax, 13
    je .fend
    mov [rdi], ax
    add rdi, 2
    add rsi, 2
    jmp .plain
.fend:
    movzx r13d, word [rsi]           ; terminator, read before the NUL overwrites it
    mov word [rdi], 0
    add rdi, 2
    test r13d, r13d
    jz .rowend_eof
    add rsi, 2
    cmp r13d, r12d
    je .field
    cmp r13d, 13
    jne .rowend
    cmp word [rsi], 10
    jne .rowend
    add rsi, 2
.rowend:
    cmp r14d, [rbx+SD_MAXC]
    jbe .rs
    mov [rbx+SD_MAXC], r14d
.rs:
    jmp .rowstart
.rowend_eof:
    cmp r14d, [rbx+SD_MAXC]
    jbe .done
    mov [rbx+SD_MAXC], r14d
.done:
    ; sentinel row offset
    invoke vec_push, &rbx+SD_ROWOFF, 4
    mov rcx, [rbx+SD_CELLS+8]
    mov [rax], ecx
    EPILOG 64

; side_free(SIDE*)
side_free:
    PROLOG 64
    mov rbx, rcx
    invoke free, [rbx+SD_CELLS]
    invoke free, [rbx+SD_ROWOFF]
    invoke free, [rbx+SD_TEXT]
    invoke free, [rbx+SD_KEYS]
    invoke memset, rbx, 0, SD_SIZE
    EPILOG 64

; side_ncols(SIDE*, row) -> eax
side_ncols:
    mov rax, [rcx+SD_ROWOFF]
    mov r8d, [rax+rdx*4+4]
    sub r8d, [rax+rdx*4]
    mov eax, r8d
    ret

; side_cell(SIDE*, row, col) -> pointer ("" when the row is shorter)
side_cell:
    mov rax, [rcx+SD_ROWOFF]
    mov r9d, [rax+rdx*4+4]
    mov r10d, [rax+rdx*4]
    sub r9d, r10d
    cmp r8d, r9d
    jae .empty
    add r10d, r8d
    mov rax, [rcx+SD_CELLS]
    mov rax, [rax+r10*8]
    ret
.empty:
    lea rax, [s_empty]
    ret

; ---------------------------------------------------------------- cell comparison
; str_eq(a, b, flags) -> eax 1 equal.  flags: 2 ignore case, 4 trim
str_eq:
    mov r10, rcx
    mov r11, rdx
    test r8d, 4
    jz .cmp
    ; trim leading
.l1:
    movzx eax, word [r10]
    cmp eax, ' '
    je .a1
    cmp eax, 9
    jne .l2
.a1:
    add r10, 2
    jmp .l1
.l2:
    movzx eax, word [r11]
    cmp eax, ' '
    je .a2
    cmp eax, 9
    jne .cmp
.a2:
    add r11, 2
    jmp .l2
.cmp:
    movzx eax, word [r10]
    movzx edx, word [r11]
    test r8d, 2
    jz .cmpc
    cmp eax, 'A'
    jb .c1
    cmp eax, 'Z'
    ja .c1
    add eax, 32
.c1:
    cmp edx, 'A'
    jb .cmpc
    cmp edx, 'Z'
    ja .cmpc
    add edx, 32
.cmpc:
    cmp eax, edx
    jne .ne
    test eax, eax
    jz .eq
    add r10, 2
    add r11, 2
    jmp .cmp
.eq:
    mov eax, 1
    ret
.ne:
    ; with trim, trailing spaces on either side may remain: accept "x " vs "x"
    test r8d, 4
    jz .no
    test eax, eax
    jz .rest_b
    test edx, edx
    jz .rest_a
.no:
    xor eax, eax
    ret
.rest_a:
    ; b ended: the rest of a must be white space
.ra:
    movzx eax, word [r10]
    test eax, eax
    jz .yes
    cmp eax, ' '
    je .rna
    cmp eax, 9
    jne .no
.rna:
    add r10, 2
    jmp .ra
.rest_b:
.rb:
    movzx eax, word [r11]
    test eax, eax
    jz .yes
    cmp eax, ' '
    je .rnb
    cmp eax, 9
    jne .no
.rnb:
    add r11, 2
    jmp .rb
.yes:
    mov eax, 1
    ret

; num_eq(a, b, tb) -> eax 1 if both strings are numbers within the tolerance
num_eq:
    PROLOG 96
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    invoke wcstod, rsi, &rsp+80
    mov rax, [rsp+80]
    cmp rax, rsi
    je .no
    cmp word [rax], 0
    jne .no
    movsd [rsp+64], xmm0
    invoke wcstod, rdi, &rsp+72
    mov rax, [rsp+72]
    cmp rax, rdi
    je .no
    cmp word [rax], 0
    jne .no
    movsd xmm1, [rsp+64]
    subsd xmm1, xmm0
    movq rax, xmm1
    btr rax, 63
    movq xmm1, rax
    ucomisd xmm1, [rbx+TB_TOL]
    jp .no
    ja .no
    mov eax, 1
    EPILOG 96
.no:
    xor eax, eax
    EPILOG 96

; cell_differs(tb, a, b) -> eax 1 if the two cell strings differ under the current options
cell_differs:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r8d, [rbx+TB_FLAGS]
    mov rcx, rsi
    mov rdx, rdi
    call str_eq
    test eax, eax
    jnz .same
    cmp qword [rbx+TB_HASTOL], 0
    je .diff
    invoke num_eq, rsi, rdi, rbx
    test eax, eax
    jnz .same
    invoke date_eq, rsi, rdi, rbx
    test eax, eax
    jnz .same
.diff:
    mov eax, 1
    EPILOG 64
.same:
    xor eax, eax
    EPILOG 64

; row_diff(tb, aRow, bRow) -> eax = number of differing (non-ignored) cells
row_diff:
    PROLOG 96
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    lea rcx, [rbx+TB_SA]
    invoke side_ncols, rcx, r12
    mov r14d, eax
    lea rcx, [rbx+TB_SB]
    invoke side_ncols, rcx, r13
    cmp eax, r14d
    jbe .mx
    mov r14d, eax
.mx:
    xor r15d, r15d                   ; count
    xor esi, esi                     ; column
.c:
    cmp esi, r14d
    jae .d
    cmp esi, 64
    jae .chk
    mov rax, [rbx+TB_IGN]
    bt rax, rsi
    jc .n
.chk:
    invoke side_cell, &rbx+TB_SA, r12, rsi
    mov rdi, rax
    invoke side_cell, &rbx+TB_SB, r13, rsi
    invoke cell_differs, rbx, rdi, rax
    add r15d, eax
.n:
    inc esi
    jmp .c
.d:
    mov eax, r15d
    EPILOG 96

; ---------------------------------------------------------------- keys & matching
; build_keys(tb, SIDE*)
build_keys:
    PROLOG 192
    mov rbx, rcx
    mov rsi, rdx
    invoke free, [rsi+SD_KEYS]
    mov rax, [rsi+SD_NROWS]
    lea rcx, [rax*8+16]
    call malloc
    mov [rsi+SD_KEYS], rax
    mov rdi, rax
    xor r12d, r12d
.r:
    cmp r12, [rsi+SD_NROWS]
    jae .d
    mov rax, [rbx+TB_KEYM]
    cmp rax, -1
    je .all
    cmp rax, -2
    je .all
    lea r8, [rax-1]
    invoke side_cell, rsi, r12, r8
    mov [rdi+r12*8], rax
    jmp .n
.all:
    ; concatenate all cells with a unit separator
    xor r13d, r13d                   ; total chars
    xor r14d, r14d                   ; column
    invoke side_ncols, rsi, r12
    mov r15d, eax
.len:
    cmp r14d, r15d
    jae .alloc
    cmp qword [rbx+TB_KEYM], -2
    jne .lu
    cmp r14d, 63
    jae .lskip
    bt qword [rbx+TB_KEYMASK], r14
    jnc .lskip
.lu:
    invoke side_cell, rsi, r12, r14
    invoke wcslen, rax
    lea r13, [r13+rax+1]
.lskip:
    inc r14d
    jmp .len
.alloc:
    lea rcx, [r13*2+8]
    call malloc
    mov [rdi+r12*8], rax
    mov word [rax], 0
    mov r13, rax                     ; write pointer
    xor r14d, r14d
.cp:
    cmp r14d, r15d
    jae .n
    cmp qword [rbx+TB_KEYM], -2
    jne .cu
    cmp r14d, 63
    jae .cskip
    bt qword [rbx+TB_KEYMASK], r14
    jnc .cskip
.cu:
    invoke side_cell, rsi, r12, r14
    mov rdx, rax
.cc:
    movzx eax, word [rdx]
    test eax, eax
    jz .sep
    mov [r13], ax
    add r13, 2
    add rdx, 2
    jmp .cc
.sep:
    mov word [r13], 0x1F
    add r13, 2
.cskip:
    inc r14d
    jmp .cp
.n:
    cmp qword [rbx+TB_KEYM], 0
    jge .nx
    ; terminate the concatenation
    mov rax, [rdi+r12*8]
    ; find end (rows with zero cells keep the empty string)
    cmp r15d, 0
    je .nx
    mov word [r13], 0
.nx:
    inc r12
    jmp .r
.d:
    EPILOG 192

; cmp_idx(a*, b*): order B row indices by key
cmp_idx:
    mov eax, [rcx]
    mov r8, [g_qkeys]
    mov rcx, [r8+rax*8]
    mov eax, [rdx]
    mov rdx, [r8+rax*8]
    cmp qword [g_qcase], 0
    jne .ci
    jmp wcscmp
.ci:
    jmp _wcsicmp

; cmp_res(a*, b*): final ordering by key, then by row numbers
cmp_res:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov rcx, [rsi+R_KEY]
    mov rdx, [rdi+R_KEY]
    cmp qword [g_qcase], 0
    jne .ci
    call wcscmp
    jmp .got
.ci:
    call _wcsicmp
.got:
    test eax, eax
    jnz .d
    mov eax, [rsi+R_A]
    cmp eax, [rdi+R_A]
    je .b
    jb .lt
    mov eax, 1
    jmp .d
.lt:
    mov eax, -1
    jmp .d
.b:
    mov eax, [rsi+R_B]
    cmp eax, [rdi+R_B]
    je .z
    jb .lt
    mov eax, 1
    jmp .d
.z:
    xor eax, eax
.d:
    EPILOG 64

; res_add(tb, aRow, bRow, status, key) -> pointer
res_add:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov r15, [rsp+64+8+64+8+32]      ; key (5th argument)
    invoke vec_push, &rbx+TB_RES, R_SIZE
    mov [rax+R_A], r12d
    mov [rax+R_B], r13d
    mov [rax+R_ST], r14d
    mov dword [rax+R_NCHG], 0
    mov [rax+R_KEY], r15
    EPILOG 64

; tb_match(tb): fill the result vector
tb_match:
    PROLOG 192
    mov rbx, rcx
    xor eax, eax
    mov [rbx+TB_RES], rax
    mov [rbx+TB_RES+8], rax
    mov [rbx+TB_RES+16], rax
    xor r15d, r15d                   ; first data row (1 when a header exists)
    test dword [rbx+TB_FLAGS], 1
    jz .nohdr
    mov r15d, 1
    ; header rows compare positionally
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .nohdr
    cmp qword [rbx+TB_SB+SD_NROWS], 0
    je .nohdr
    invoke res_add, rbx, 0, 0, 0, &s_empty
.nohdr:
    mov rax, [rbx+TB_KEYM]
    test rax, rax
    jnz .keyed
    ; ---- by row number
    mov r12, [rbx+TB_SA+SD_NROWS]
    cmp r12, [rbx+TB_SB+SD_NROWS]
    jae .mxr
    mov r12, [rbx+TB_SB+SD_NROWS]
.mxr:
    mov r13d, r15d
    cmp r15d, 0
    je .rn
    ; the header row already added (if both sides had one)
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .rn0
    cmp qword [rbx+TB_SB+SD_NROWS], 0
    je .rn0
    jmp .rn
.rn0:
    xor r13d, r13d
.rn:
    cmp r13, r12
    jae .classify
    mov esi, r13d                    ; aRow
    mov edi, r13d                    ; bRow
    xor r14d, r14d                   ; status 0 (provisional)
    cmp r13, [rbx+TB_SA+SD_NROWS]
    jb .haveA
    mov esi, -1
    mov r14d, 3                      ; only in B
    jmp .addrow
.haveA:
    cmp r13, [rbx+TB_SB+SD_NROWS]
    jb .addrow
    mov edi, -1
    mov r14d, 2                      ; only in A
.addrow:
    invoke res_add, rbx, rsi, rdi, r14, &s_empty
    inc r13d
    jmp .rn
.keyed:
    invoke build_keys, rbx, &rbx+TB_SA
    invoke build_keys, rbx, &rbx+TB_SB
    mov eax, [rbx+TB_FLAGS]
    shr eax, 1
    and eax, 1
    mov [g_qcase], rax
    mov rax, [rbx+TB_SB+SD_KEYS]
    mov [g_qkeys], rax
    ; sorted index of the B data rows
    mov r12, [rbx+TB_SB+SD_NROWS]
    sub r12, r15
    jbe .noB
    lea rcx, [r12*4+8]
    call malloc
    mov r13, rax                     ; idxB
    xor ecx, ecx
.fillidx:
    cmp rcx, r12
    jae .sortidx
    lea eax, [rcx+r15]
    mov [r13+rcx*4], eax
    inc rcx
    jmp .fillidx
.sortidx:
    invoke qsort, r13, r12, 4, &cmp_idx
    jmp .used
.noB:
    xor r12d, r12d
    xor r13d, r13d
.used:
    mov rax, [rbx+TB_SB+SD_NROWS]
    lea rcx, [rax+8]
    mov edx, 1
    invoke calloc, rcx, rdx
    mov r14, rax                     ; used[] flags
    ; A rows
    mov esi, r15d
.ar:
    cmp rsi, [rbx+TB_SA+SD_NROWS]
    jae .brest
    mov rax, [rbx+TB_SA+SD_KEYS]
    mov rdi, [rax+rsi*8]             ; key of A row
    ; lower bound in idxB
    xor ecx, ecx                     ; lo
    mov rdx, r12                     ; hi
.lb:
    cmp rcx, rdx
    jae .scan
    lea rax, [rcx+rdx]
    shr rax, 1
    mov r8d, [r13+rax*4]
    mov r9, [g_qkeys]
    mov r10, [r9+r8*8]
    mov [rsp+96], rcx
    mov [rsp+104], rdx
    mov [rsp+112], rax
    cmp qword [g_qcase], 0
    jne .lbci
    invoke wcscmp, r10, rdi
    jmp .lbgot
.lbci:
    invoke _wcsicmp, r10, rdi
.lbgot:
    mov rcx, [rsp+96]
    mov rdx, [rsp+104]
    mov r8, [rsp+112]
    test eax, eax
    jns .lbge                        ; key[mid] >= key
    lea rcx, [r8+1]
    jmp .lb
.lbge:
    mov rdx, r8
    jmp .lb
.scan:
    mov [rsp+96], rcx                ; position in idxB
.sl:
    mov rcx, [rsp+96]
    cmp rcx, r12
    jae .nomatch
    mov r8d, [r13+rcx*4]
    mov r9, [g_qkeys]
    mov r10, [r9+r8*8]
    cmp qword [g_qcase], 0
    jne .sci
    invoke wcscmp, r10, rdi
    jmp .sgot
.sci:
    invoke _wcsicmp, r10, rdi
.sgot:
    test eax, eax
    jnz .nomatch                     ; ran past the equal keys
    mov rcx, [rsp+96]
    mov r8d, [r13+rcx*4]
    cmp byte [r14+r8], 0
    je .match
    inc qword [rsp+96]
    jmp .sl
.match:
    mov byte [r14+r8], 1
    invoke res_add, rbx, rsi, r8, 0, rdi
    jmp .anext
.nomatch:
    invoke res_add, rbx, rsi, -1, 2, rdi
.anext:
    inc esi
    jmp .ar
.brest:
    mov esi, r15d
.br:
    cmp rsi, [rbx+TB_SB+SD_NROWS]
    jae .keydone
    cmp byte [r14+rsi], 0
    jne .bn
    mov rax, [rbx+TB_SB+SD_KEYS]
    mov rdi, [rax+rsi*8]
    invoke res_add, rbx, -1, rsi, 3, rdi
.bn:
    inc esi
    jmp .br
.keydone:
    invoke free, r14
    invoke free, r13
    ; sort by key (data rows only; a header row stays on top)
    test dword [rbx+TB_FLAGS], 0x10
    jz .classify
    mov rax, [rbx+TB_RES]
    mov rcx, [rbx+TB_RES+8]
    xor edx, edx
    test dword [rbx+TB_FLAGS], 1
    jz .dosort
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .dosort
    cmp qword [rbx+TB_SB+SD_NROWS], 0
    je .dosort
    add rax, R_SIZE
    dec rcx
.dosort:
    cmp rcx, 1
    jbe .classify
    mov r12, rcx
    invoke qsort, rax, r12, R_SIZE, &cmp_res
.classify:
    ; status of matched rows and the summary counters
    xor eax, eax
    mov [rbx+TB_CNT], rax
    mov [rbx+TB_CNT+8], rax
    mov [rbx+TB_CNT+16], rax
    mov [rbx+TB_CNT+24], rax
    mov [rbx+TB_CNT+32], rax
    xor r12d, r12d
.cl:
    cmp r12, [rbx+TB_RES+8]
    jae .done
    mov rax, r12
    imul rax, R_SIZE
    add rax, [rbx+TB_RES]
    mov r13, rax
    mov eax, [r13+R_ST]
    cmp eax, 2
    je .oa
    cmp eax, 3
    je .ob
    mov edx, [r13+R_A]
    mov r8d, [r13+R_B]
    invoke row_diff, rbx, rdx, r8
    mov [r13+R_NCHG], eax
    test eax, eax
    jz .same
    mov dword [r13+R_ST], 1
    inc qword [rbx+TB_CNT]
    add [rbx+TB_CNT+8], rax
    jmp .cn
.same:
    mov dword [r13+R_ST], 0
    inc qword [rbx+TB_CNT+32]
    jmp .cn
.oa:
    inc qword [rbx+TB_CNT+16]
    jmp .cn
.ob:
    inc qword [rbx+TB_CNT+24]
.cn:
    inc r12
    jmp .cl
.done:
    EPILOG 192

; ---------------------------------------------------------------- parse the control values
; parse_cols(wide string) -> rax bitmask of listed column numbers (1..64)
parse_cols:
    xor eax, eax
    xor r8d, r8d                     ; current number
.l:
    movzx edx, word [rcx]
    add rcx, 2
    cmp edx, '0'
    jb .sep
    cmp edx, '9'
    ja .sep
    imul r8d, r8d, 10
    sub edx, '0'
    add r8d, edx
    jmp .l
.sep:
    test r8d, r8d
    jz .z
    cmp r8d, 64
    ja .z
    lea r9d, [r8d-1]
    bts rax, r9
.z:
    xor r8d, r8d
    test edx, edx
    jnz .l
    ret

; tb_read_ui(hwnd, tb): flags, key mode, ignore mask and tolerance from the controls
tb_read_ui:
    PROLOG 224
    mov rsi, rcx
    mov rbx, rdx
    xor edi, edi
    invoke get_chk, rsi, ID_CHK_HDR
    or edi, eax
    invoke get_chk, rsi, ID_CHK_CASE
    shl eax, 1
    or edi, eax
    invoke get_chk, rsi, ID_CHK_TRIM
    shl eax, 2
    or edi, eax
    invoke get_chk, rsi, ID_CHK_DIFF
    shl eax, 3
    or edi, eax
    invoke get_chk, rsi, ID_CHK_SORT
    shl eax, 4
    or edi, eax
    mov [rbx+TB_FLAGS], edi
    ; key
    invoke GetDlgItem, rsi, ID_EDIT_KEY
    invoke GetWindowTextW, rax, &rsp+96, 20
    mov qword [rbx+TB_KEYM], 0
    movzx eax, word [rsp+96]
    cmp eax, 'a'
    je .all
    cmp eax, 'A'
    je .all
    xor ecx, ecx
    lea rdx, [rsp+96]
.k:
    movzx eax, word [rdx]
    cmp eax, '0'
    jb .kd
    cmp eax, '9'
    ja .kd
    imul ecx, ecx, 10
    sub eax, '0'
    add ecx, eax
    add rdx, 2
    jmp .k
.kd:
    cmp eax, ','
    je .mask
    mov [rbx+TB_KEYM], rcx
    jmp .ign
.mask:
    invoke parse_cols, &rsp+96
    mov [rbx+TB_KEYMASK], rax
    mov qword [rbx+TB_KEYM], -2
    jmp .ign
.all:
    mov qword [rbx+TB_KEYM], -1
.ign:
    invoke GetDlgItem, rsi, ID_EDIT_IGN
    invoke GetWindowTextW, rax, &rsp+96, 60
    invoke parse_cols, &rsp+96
    mov [rbx+TB_IGN], rax
    ; tolerance
    mov qword [rbx+TB_HASTOL], 0
    invoke GetDlgItem, rsi, ID_EDIT_TOL
    invoke GetWindowTextW, rax, &rsp+96, 30
    cmp word [rsp+96], 0
    je .d
    invoke wcstod, &rsp+96, &rsp+200
    mov rax, [rsp+200]
    lea rcx, [rsp+96]
    cmp rax, rcx
    je .d
    movsd [rbx+TB_TOL], xmm0
    mov qword [rbx+TB_HASTOL], 1
.d:
    EPILOG 224

; ---------------------------------------------------------------- visible rows, columns, cells
; tb_rebuild(tb): visible row list (hides identical rows when "only differences")
tb_rebuild:
    PROLOG 64
    mov rbx, rcx
    mov rax, [rbx+TB_RES+8]
    lea rdx, [rax*4+8]
    invoke realloc, [rbx+TB_VIS], rdx
    mov [rbx+TB_VIS], rax
    xor ecx, ecx
    xor edx, edx                     ; count
    mov r8, [rbx+TB_RES]
.l:
    cmp rcx, [rbx+TB_RES+8]
    jae .d
    test rcx, rcx
    jnz .hdrok
    test dword [rbx+TB_FLAGS], 1             ; an identical header row is shown as the column titles only
    jz .hdrok
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .hdrok
    cmp qword [rbx+TB_SB+SD_NROWS], 0
    je .hdrok
    cmp dword [r8+R_ST], 0
    je .n
.hdrok:
    test dword [rbx+TB_FLAGS], 8
    jz .add
    mov r9, rcx
    imul r9, R_SIZE
    cmp dword [r8+r9+R_ST], 0
    je .n
.add:
    mov [rax+rdx*4], ecx
    inc rdx
.n:
    inc rcx
    jmp .l
.d:
    mov [rbx+TB_NVIS], rdx
    EPILOG 64

; tb_columns(hwnd, tb): rebuild the list view columns from both files
tb_columns:
    PROLOG 224
    mov rsi, rcx
    mov rbx, rdx
    mov r12, [rbx+TB_HLIST]
.del:
    invoke SendMessageW, r12, LVM_DELETECOLUMN, 0, 0
    test eax, eax
    jnz .del
    ; displayed source columns
    invoke free, [rbx+TB_COLMAP]
    mov eax, [rbx+TB_SA+SD_MAXC]
    cmp eax, [rbx+TB_SB+SD_MAXC]
    jae .mx
    mov eax, [rbx+TB_SB+SD_MAXC]
.mx:
    mov r13d, eax                    ; max source columns
    lea rcx, [r13*4+8]
    call malloc
    mov [rbx+TB_COLMAP], rax
    mov rdi, rax
    xor r14d, r14d                   ; displayed count
    xor r15d, r15d                   ; source column
.map:
    cmp r15d, r13d
    jae .mapped
    cmp r15d, 64
    jae .keep
    mov rax, [rbx+TB_IGN]
    bt rax, r15
    jc .skip
.keep:
    mov [rdi+r14*4], r15d
    inc r14d
.skip:
    inc r15d
    jmp .map
.mapped:
    mov [rbx+TB_NCOLS], r14
    ; first column: row number
    xor eax, eax
    mov [rsp+96], rax
    mov [rsp+104], rax
    mov [rsp+112], rax
    mov [rsp+120], rax
    mov [rsp+128], rax
    mov dword [rsp+96], LVCF_FMT|LVCF_WIDTH|LVCF_TEXT
    mov ecx, 70
    call scl
    mov [rsp+104], eax
    lea rax, [s_col0]
    mov [rsp+112], rax
    invoke SendMessageW, r12, LVM_INSERTCOLUMNW, 0, &rsp+96
    xor r15d, r15d
.col:
    cmp r15, r14
    jae .d
    ; header text
    mov eax, [rdi+r15*4]
    mov esi, eax                     ; source column
    lea rax, [rsp+160]
    mov word [rax], 0
    test dword [rbx+TB_FLAGS], 1
    jz .fallback
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .fallback
    invoke side_cell, &rbx+TB_SA, 0, rsi
    cmp word [rax], 0
    je .fallback
    invoke wcscpy, &rsp+160, rax
    jmp .ins
.fallback:
    lea eax, [rsi+1]
    invoke _snwprintf, &rsp+160, 30, &f_col, rax
.ins:
    xor eax, eax
    mov [rsp+96], rax
    mov [rsp+104], rax
    mov [rsp+112], rax
    mov dword [rsp+96], LVCF_FMT|LVCF_WIDTH|LVCF_TEXT
    mov ecx, 130
    call scl
    mov [rsp+104], eax
    lea rax, [rsp+160]
    mov [rsp+112], rax
    lea rax, [r15+1]
    invoke SendMessageW, r12, LVM_INSERTCOLUMNW, rax, &rsp+96
    inc r15
    jmp .col
.d:
    EPILOG 224

; tb_cell(tb, resrow, sub, buf) -> eax 1 if this cell is a changed cell
tb_cell:
    PROLOG 192
    mov rbx, rcx
    mov rax, rdx
    imul rax, R_SIZE
    add rax, [rbx+TB_RES]
    mov rdi, rax                     ; RES entry
    mov rsi, r9                      ; output buffer
    mov word [rsi], 0
    test r8d, r8d
    jnz .data
    ; row label
    mov eax, [rdi+R_ST]
    cmp eax, 2
    je .la
    cmp eax, 3
    je .lb
    mov r12d, [rdi+R_A]
    inc r12d
    mov r13d, [rdi+R_B]
    inc r13d
    cmp r12d, r13d
    je .l1
    invoke _snwprintf, rsi, 30, &f_row2, r12, r13
    xor eax, eax
    EPILOG 192
.l1:
    invoke _snwprintf, rsi, 30, &f_row1, r12
    xor eax, eax
    EPILOG 192
.la:
    mov r12d, [rdi+R_A]
    inc r12d
    invoke _snwprintf, rsi, 30, &f_rowa, r12
    xor eax, eax
    EPILOG 192
.lb:
    mov r12d, [rdi+R_B]
    inc r12d
    invoke _snwprintf, rsi, 30, &f_rowb, r12
    xor eax, eax
    EPILOG 192
.data:
    dec r8d
    cmp r8, [rbx+TB_NCOLS]
    jae .none
    mov rax, [rbx+TB_COLMAP]
    mov r12d, [rax+r8*4]             ; source column
    mov eax, [rdi+R_ST]
    cmp eax, 2
    je .onlya
    cmp eax, 3
    je .onlyb
    invoke side_cell, &rbx+TB_SA, #[rdi+R_A], r12
    mov r13, rax
    invoke side_cell, &rbx+TB_SB, #[rdi+R_B], r12
    mov r14, rax
    invoke cell_differs, rbx, r13, r14
    test eax, eax
    jz .same
    invoke _snwprintf, rsi, 250, &f_cellchg, r13, r14
    mov eax, 1
    EPILOG 192
.same:
    invoke wcscpy, rsi, r13
    xor eax, eax
    EPILOG 192
.onlya:
    mov edx, [rdi+R_A]
    invoke side_cell, &rbx+TB_SA, rdx, r12
    invoke wcscpy, rsi, rax
    xor eax, eax
    EPILOG 192
.onlyb:
    mov edx, [rdi+R_B]
    invoke side_cell, &rbx+TB_SB, rdx, r12
    invoke wcscpy, rsi, rax
.none:
    xor eax, eax
    EPILOG 192

; ---------------------------------------------------------------- compare
; tb_info(hwnd, tb)
tb_info:
    PROLOG 320
    mov rsi, rcx
    mov rbx, rdx
    invoke _snwprintf, &rsp+96, 140, &f_info, [rbx+TB_SA+SD_NROWS], [rbx+TB_SB+SD_NROWS], [rbx+TB_CNT], [rbx+TB_CNT+8], [rbx+TB_CNT+16], [rbx+TB_CNT+24]
    invoke GetDlgItem, rsi, ID_LBL_INFO
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 320

; tb_load_side(path, SIDE*) -> eax
tb_load_side:
    PROLOG 192
    mov r12, rcx
    mov rbx, rdx
    invoke side_free, rbx
    invoke wcsrchr, r12, '.'
    test rax, rax
    jz .csv
    mov r13, rax
    invoke _wcsicmp, r13, &xl_ext
    test eax, eax
    jz .xlsx
    invoke _wcsicmp, r13, &xl_extm
    test eax, eax
    jnz .csv
.xlsx:
    invoke xlsx_load, r12, rbx
    EPILOG 192
.csv:
    invoke text_load, r12, &rsp+96             ; TXT at [rsp+96]: text, len, enc, crlf, eol
    test eax, eax
    jz .fail
    mov r13, [rsp+96]                          ; wide text (private to us, we parse in place)
    mov [rbx+SD_TEXT], r13
    invoke csv_detect, r13
    invoke csv_parse, r13, rax, rbx
    mov eax, 1
    EPILOG 192
.fail:
    xor eax, eax
    EPILOG 192

; tb_compare(hwnd, tb, sess)
tb_compare:
    PROLOG 256
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke tb_read_ui, rsi, rbx
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rbx+TB_PATHA, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, &rbx+TB_PATHB, 519
    invoke tb_load_side, &rbx+TB_PATHA, &rbx+TB_SA
    test eax, eax
    jz .err
    invoke tb_load_side, &rbx+TB_PATHB, &rbx+TB_SB
    test eax, eax
    jz .err
    invoke free, [rbx+TB_RES]
    invoke tb_match, rbx
    invoke tb_columns, rsi, rbx
    invoke tb_rebuild, rbx
    invoke SendMessageW, [rbx+TB_HLIST], LVM_SETITEMCOUNT, [rbx+TB_NVIS], 0
    invoke InvalidateRect, [rbx+TB_HLIST], 0, 1
    invoke tb_info, rsi, rbx
    ; tab title
    invoke wcsrchr, &rbx+TB_PATHA, 0x5C
    lea r13, [rbx+TB_PATHA]
    test rax, rax
    jz .ta
    lea r13, [rax+2]
.ta:
    invoke wcsrchr, &rbx+TB_PATHB, 0x5C
    lea r14, [rbx+TB_PATHB]
    test rax, rax
    jz .tb
    lea r14, [rax+2]
.tb:
    invoke _snwprintf, &rsp+96, 60, &f_title, r13, r14
    invoke set_tab_title, r12, &rsp+96
    test qword [g_batch], 0x10        ; /xlsx file: export the first compare and close
    jz .okb
    and qword [g_batch], ~0x10
    invoke tb_xlsx_path, rbx, &g_xl_path
    test eax, eax
    jnz .okx
    mov qword [g_exitcode], 2
.okx:
    test qword [g_batch], 4
    jz .okb
    invoke PostMessageW, [g_hframe], WM_CLOSE, 0, 0
.okb:
    EPILOG 256
.err:
    invoke msgbox, rsi, &s_cantread, MB_OK|MB_ICONWARNING
    EPILOG 256

; tb_refresh(hwnd, tb): only the visible list (filter change)
tb_refresh:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke tb_read_ui, rsi, rbx
    invoke tb_rebuild, rbx
    invoke SendMessageW, [rbx+TB_HLIST], LVM_SETITEMCOUNT, [rbx+TB_NVIS], 0
    invoke InvalidateRect, [rbx+TB_HLIST], 0, 1
    EPILOG 64

; ---------------------------------------------------------------- options string, theme, layout
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

tb_layout:
    PROLOG 192
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
    mov r12d, r14d
    PLACE ID_COMPARE, 96
    PLACE ID_CHK_HDR, 90
    PLACE ID_CHK_CASE, 110
    PLACE ID_CHK_TRIM, 120
    PLACE ID_CHK_DIFF, 100
    PLACE ID_CHK_SORT, 90
    PLACE ID_BTN_XLS, 90
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_LBL_KEY, 190
    PLACE ID_EDIT_KEY, 50
    PLACE ID_LBL_TOL, 80
    PLACE ID_EDIT_TOL, 60
    PLACE ID_LBL_IGN, 130
    PLACE ID_EDIT_IGN, 90
    PLACE ID_LBL_INFO, 560
    lea r13d, [r13+r15]
    add r13d, r14d
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .h
    xor ebp, ebp
.h:
    mov ebx, [rsp+104]
    MVID rsi, ID_TLIST, 0, r13, rbx, rbp
    EPILOG 192

tb_collect:
    PROLOG 640
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    invoke pb_collect, rsi, rdi
    invoke tb_read_ui, rsi, rbx
    mov eax, [rbx+TB_FLAGS]
    mov ecx, eax
    and ecx, 1
    mov r12d, ecx
    mov ecx, eax
    shr ecx, 1
    and ecx, 1
    mov r13d, ecx
    mov ecx, eax
    shr ecx, 2
    and ecx, 1
    mov r14d, ecx
    mov ecx, eax
    shr ecx, 3
    and ecx, 1
    mov r15d, ecx
    shr eax, 4
    and eax, 1
    mov [rsp+96], rax
    invoke GetDlgItem, rsi, ID_EDIT_KEY
    invoke GetWindowTextW, rax, &rsp+112, 20
    invoke GetDlgItem, rsi, ID_EDIT_TOL
    invoke GetWindowTextW, rax, &rsp+160, 30
    invoke GetDlgItem, rsi, ID_EDIT_IGN
    invoke GetWindowTextW, rax, &rsp+240, 60
    ; "%d%d%d%d%d|key|tol|ign"  (5 flag digits then three fields)
    invoke _snwprintf, &rdi+SS_OPTS, 1023, &f_opts, r12, r13, r14, r15, [rsp+96], &rsp+112, &rsp+160, &rsp+240
    EPILOG 640

; tb_opts_apply(hwnd, sess): restore controls from "FCTDS|key|tol|ign"
tb_opts_apply:
    PROLOG 64
    mov rsi, rcx
    lea rbx, [rdx+SS_OPTS]
    cmp word [rbx], 0
    je .d
    movzx r8d, word [rbx]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_HDR, r8
    movzx r8d, word [rbx+2]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_CASE, r8
    movzx r8d, word [rbx+4]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_TRIM, r8
    movzx r8d, word [rbx+6]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_DIFF, r8
    movzx r8d, word [rbx+8]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_SORT, r8
    ; fields after "|": key, tol, ign separated by '|'
    lea rdi, [rbx+12]                ; first field start
    xor r12d, r12d                   ; field number
.f:
    mov r13, rdi
.s:
    movzx eax, word [rdi]
    test eax, eax
    jz .end
    cmp eax, '|'
    je .end
    add rdi, 2
    jmp .s
.end:
    movzx r14d, word [rdi]
    mov word [rdi], 0
    mov eax, ID_EDIT_KEY
    cmp r12d, 1
    jne .nt
    mov eax, ID_EDIT_TOL
.nt:
    cmp r12d, 2
    jne .go
    mov eax, ID_EDIT_IGN
.go:
    invoke GetDlgItem, rsi, rax
    invoke SetWindowTextW, rax, r13
    test r14d, r14d
    jz .d
    mov word [rdi], '|'
    add rdi, 2
    inc r12d
    cmp r12d, 3
    jb .f
.d:
    EPILOG 64

; set_chk_id(parent, id, value)
set_chk_id:
    PROLOG 64
    mov rbx, r8
    invoke GetDlgItem, rcx, rdx
    invoke SendMessageW, rax, BM_SETCHECK, rbx, 0
    EPILOG 64

tb_theme:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, [rbx+TB_HLIST]
    invoke SendMessageW, rdi, LVM_SETBKCOLOR, 0, #[th_bg]
    invoke SendMessageW, rdi, LVM_SETTEXTCOLOR, 0, #[th_fg]
    invoke SendMessageW, rdi, LVM_SETTEXTBKCOLOR, 0, #[th_bg]
    lea rdx, [s_light_theme]
    cmp qword [g_dark], 0
    je .t
    lea rdx, [s_dark_theme]
.t:
    invoke SetWindowTheme, rdi, rdx, 0
    invoke theme_checks, rsi
    invoke InvalidateRect, rdi, 0, 1
    EPILOG 64

; ---------------------------------------------------------------- window procedure
tb_proc:
    PROLOG 640
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
    EPILOG 640

.create:
    mov r14, [r13]
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, r14
    invoke calloc, 1, TB_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    invoke pb_create, rsi
    invoke mk_chk, rsi, &s_c_hdr, ID_CHK_HDR, 1
    invoke mk_chk, rsi, &s_c_case, ID_CHK_CASE, 0
    invoke mk_chk, rsi, &s_c_trim, ID_CHK_TRIM, 0
    invoke mk_chk, rsi, &s_c_diff, ID_CHK_DIFF, 0
    invoke mk_chk, rsi, &s_c_sort, ID_CHK_SORT, 1
    invoke mk_btn, rsi, &s_b_xls, ID_BTN_XLS
    invoke mk_lbl, rsi, &s_l_key, ID_LBL_KEY
    invoke mk_edit, rsi, ID_EDIT_KEY
    invoke GetDlgItem, rsi, ID_EDIT_KEY
    invoke SetWindowTextW, rax, &s_key0
    invoke mk_lbl, rsi, &s_l_tol, ID_LBL_TOL
    invoke mk_edit, rsi, ID_EDIT_TOL
    invoke mk_lbl, rsi, &s_l_ign, ID_LBL_IGN
    invoke mk_edit, rsi, ID_EDIT_IGN
    invoke mk_lbl, rsi, &s_empty, ID_LBL_INFO
    invoke mk, rsi, &cn_list, 0, LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER, ID_TLIST
    mov [r15+TB_HLIST], rax
    invoke SendMessageW, rax, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES
    invoke lv_dark_header, [r15+TB_HLIST]
    invoke pb_load, rsi, r14
    invoke tb_opts_apply, rsi, r14
    invoke tb_theme, rsi, r15
    xor eax, eax
    EPILOG 640

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 640

.size:
    invoke tb_layout, rsi
    jmp .zero
.run:
    invoke tb_compare, rsi, r15, r14
    jmp .zero
.collect:
    invoke tb_collect, rsi, r14, r15
    jmp .zero
.theme:
    invoke tb_theme, rsi, r15
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
    cmp ebx, ID_CHK_DIFF
    je .refresh
    cmp ebx, ID_BTN_XLS
    je .xls
    cmp ebx, ID_CHK_HDR
    jb .def
    cmp ebx, ID_CHK_SORT
    ja .def
    cmp qword [r15+TB_SA+SD_NROWS], 0
    je .zero
    jmp .run
.browse:
    invoke pb_command, rsi, rbx, 0
    jmp .zero
.xls:
    invoke tb_export, rsi, r15
    jmp .zero
.refresh:
    invoke tb_refresh, rsi, r15
    jmp .zero

.notify:
    mov rax, [r13+NMH_FROM]
    cmp rax, [r15+TB_HLIST]
    jne .def
    mov eax, [r13+NMH_CODE]
    cmp eax, LVN_GETDISPINFOW
    je .disp
    cmp eax, NM_CUSTOMDRAW
    je .cdraw
    jmp .def
.disp:
    test dword [r13+DI_MASK], LVIF_TEXT
    jz .zero
    mov eax, [r13+DI_ITEM]
    cmp rax, [r15+TB_NVIS]
    jae .zero
    mov rcx, [r15+TB_VIS]
    mov edx, [rcx+rax*4]
    mov r8d, [r13+DI_SUB]
    mov r9, [r13+DI_TEXT]
    invoke tb_cell, r15, rdx, r8, r9
    jmp .zero
.cdraw:
    mov eax, [r13+CD_STAGE]
    cmp eax, CDDS_PREPAINT
    jne .cd2
    mov eax, CDRF_NOTIFYITEMDRAW
    EPILOG 640
.cd2:
    cmp eax, CDDS_ITEMPREPAINT
    jne .cd3
    mov rax, [r13+CD_ITEMSPEC]
    cmp rax, [r15+TB_NVIS]
    jae .zero
    mov rcx, [r15+TB_VIS]
    mov eax, [rcx+rax*4]
    imul rax, R_SIZE
    add rax, [r15+TB_RES]
    mov ecx, [rax+R_ST]
    mov edx, [th_bg]
    cmp ecx, 2
    jne .r3
    mov edx, [th_lonly]
    jmp .rset
.r3:
    cmp ecx, 3
    jne .rset
    mov edx, [th_ronly]
.rset:
    mov [r13+CD_CLRTEXTBK], edx
    mov eax, [th_fg]
    mov [r13+CD_CLRTEXT], eax
    mov eax, CDRF_NOTIFYSUBITEMDRAW
    EPILOG 640
.cd3:
    cmp eax, CDDS_ITEMPREPAINT|CDDS_SUBITEM
    jne .zero
    mov rax, [r13+CD_ITEMSPEC]
    cmp rax, [r15+TB_NVIS]
    jae .zero
    mov rcx, [r15+TB_VIS]
    mov edx, [rcx+rax*4]
    mov r8d, [r13+CD_SUBITEM]
    invoke tb_cell, r15, rdx, r8, &rsp+96
    test eax, eax
    jz .cdnorm
    mov eax, [th_diff]
    mov [r13+CD_CLRTEXTBK], eax
    jmp .zero
.cdnorm:
    ; restore the row colour for unchanged cells
    mov rax, [r13+CD_ITEMSPEC]
    mov rcx, [r15+TB_VIS]
    mov eax, [rcx+rax*4]
    imul rax, R_SIZE
    add rax, [r15+TB_RES]
    mov ecx, [rax+R_ST]
    mov edx, [th_bg]
    cmp ecx, 2
    jne .n3
    mov edx, [th_lonly]
.n3:
    cmp ecx, 3
    jne .nset
    mov edx, [th_ronly]
.nset:
    mov [r13+CD_CLRTEXTBK], edx
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 640
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 640
.focus:
    invoke SetFocus, [r15+TB_HLIST]
    jmp .zero
.destroy:
    invoke side_free, &r15+TB_SA
    invoke side_free, &r15+TB_SB
    invoke free, [r15+TB_RES]
    invoke free, [r15+TB_VIS]
    invoke free, [r15+TB_COLMAP]
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 640
%include "xlsx.inc"
%include "table_date.inc"
%include "table_xl.inc"
