; table_xl.inc - table compare result -> Excel workbook (.xlsx): the rows shown in the list (honours "차이만 표시")

section .rdata
WSTR s_tb_hstat, "상태"
WSTR s_tb_same, "같음"
WSTR s_tb_chg, "변경"
WSTR s_tb_lonly, "왼쪽만"
WSTR s_tb_ronly, "오른쪽만"
WSTR s_tb_arrow, " → "
WSTR s_tb_norun, "먼저 비교를 실행하세요."
WSTR s_b_xls, "엑셀 저장"
tb_stat: dq s_tb_same, s_tb_chg, s_tb_lonly, s_tb_ronly

section .text

%define BX_XL   96                   ; XL ctx (40)
%define BX_SB   136                  ; sb (24)
%define BX_W    160                  ; widths ptr
%define BX_PATH 168
%define BX_BUF  176                  ; wchar[32]

; tb_xlsx_path(tb, path) -> eax 1 when written
tb_xlsx_path:
    PROLOG 320
    mov rbx, rcx
    mov [rsp+BX_PATH], rdx
    mov rax, [rbx+TB_NCOLS]
    lea rcx, [rax*4+16]
    call malloc
    mov [rsp+BX_W], rax
    test rax, rax
    jz .fail
    mov dword [rax], 10
    mov dword [rax+4], 10
    mov rcx, [rbx+TB_NCOLS]
    xor edx, edx
.w:
    cmp rdx, rcx
    jae .wd
    mov dword [rax+rdx*4+8], 22
    inc rdx
    jmp .w
.wd:
    invoke xw_begin, &rsp+BX_XL
    invoke xw_cell, &rsp+BX_XL, &s_tb_hstat, XS_HEAD
    invoke xw_cell, &rsp+BX_XL, &s_col0, XS_HEAD
    xor r14d, r14d
.hc:
    cmp r14, [rbx+TB_NCOLS]
    jae .hd
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]            ; source column
    lea rsi, [rsp+BX_BUF]
    test dword [rbx+TB_FLAGS], 1
    jz .fb
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .fb
    invoke side_cell, &rbx+TB_SA, 0, r15
    cmp word [rax], 0
    je .fb
    mov rsi, rax
    jmp .hput
.fb:
    lea eax, [r15+1]
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_col, rax
.hput:
    invoke xw_cell, &rsp+BX_XL, rsi, XS_HEAD
    inc r14
    jmp .hc
.hd:
    invoke xw_endrow, &rsp+BX_XL
    xor r12d, r12d
.row:
    cmp r12, [rbx+TB_NVIS]
    jae .fin
    mov rcx, [rbx+TB_VIS]
    mov eax, [rcx+r12*4]
    imul rax, R_SIZE
    add rax, [rbx+TB_RES]
    mov r13, rax
    mov eax, [r13+R_ST]
    xor ebp, ebp                      ; row style
    xor ecx, ecx                      ; status text index
    cmp eax, 2
    jne .s3
    mov ebp, XS_RED
    mov ecx, 2
    jmp .sdone
.s3:
    cmp eax, 3
    jne .sc
    mov ebp, XS_GREEN
    mov ecx, 3
    jmp .sdone
.sc:
    cmp dword [r13+R_NCHG], 0
    je .sdone
    mov ecx, 1
.sdone:
    lea rdx, [tb_stat]
    mov rdx, [rdx+rcx*8]
    invoke xw_cell, &rsp+BX_XL, rdx, rbp
    ; row label like the list's first column
    mov eax, [r13+R_ST]
    mov r14d, [r13+R_A]
    inc r14d
    mov r15d, [r13+R_B]
    inc r15d
    cmp eax, 2
    je .la
    cmp eax, 3
    je .lb
    cmp r14d, r15d
    je .l1
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row2, r14, r15
    jmp .lput
.l1:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row1, r14
    jmp .lput
.la:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowa, r14
    jmp .lput
.lb:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowb, r15
.lput:
    invoke xw_cell, &rsp+BX_XL, &rsp+BX_BUF, rbp
    xor r14d, r14d                    ; displayed column
.col:
    cmp r14, [rbx+TB_NCOLS]
    jae .rowend
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]
    mov eax, [r13+R_ST]
    cmp eax, 2
    je .onlya
    cmp eax, 3
    je .onlyb
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    mov rsi, rax
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    mov rdi, rax
    invoke cell_differs, rbx, rsi, rdi
    test eax, eax
    jz .plain
    invoke sb_init, &rsp+BX_SB
    invoke sb_append, &rsp+BX_SB, rsi
    invoke sb_append, &rsp+BX_SB, &s_tb_arrow
    invoke sb_append, &rsp+BX_SB, rdi
    invoke xw_cell, &rsp+BX_XL, [rsp+BX_SB], XS_ORANGE
    invoke sb_free, &rsp+BX_SB
    jmp .cnext
.plain:
    invoke xw_cell, &rsp+BX_XL, rsi, XS_NORMAL
    jmp .cnext
.onlya:
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    invoke xw_cell, &rsp+BX_XL, rax, rbp
    jmp .cnext
.onlyb:
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    invoke xw_cell, &rsp+BX_XL, rax, rbp
.cnext:
    inc r14
    jmp .col
.rowend:
    invoke xw_endrow, &rsp+BX_XL
    inc r12
    jmp .row
.fin:
    mov rax, [rbx+TB_NCOLS]
    add eax, 2
    invoke xw_save, &rsp+BX_XL, [rsp+BX_PATH], [rsp+BX_W], rax
    mov ebx, eax
    invoke sb_free, &rsp+BX_XL
    invoke free, [rsp+BX_W]
    mov eax, ebx
    EPILOG 320
.fail:
    xor eax, eax
    EPILOG 320

; tb_export(hwnd, tb)
tb_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+TB_COLMAP], 0
    je .norun
    invoke ex_pick, rsi, &rsp+400, 0
    test eax, eax
    jz .d
    invoke tb_xlsx_path, rbx, &rsp+400
    invoke ex_done, rsi, rax
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &s_tb_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
