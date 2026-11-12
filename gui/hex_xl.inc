; hex_xl.inc - hex compare result -> Excel workbook (.xlsx): the rows shown in the view (honours "차이 행만 표시")

%define HX_XL_MAXROWS 100000

section .rdata
WSTR s_hx_h0, "오프셋"
WSTR s_hx_h1, "왼쪽 (16진)"
WSTR s_hx_h2, "왼쪽 (ASCII)"
WSTR s_hx_h3, "오른쪽 (16진)"
WSTR s_hx_h4, "오른쪽 (ASCII)"
WSTR s_hx_h5, "다른 바이트 수"
WSTR s_hx_h6, "다른 위치(행 안의 바이트 번호)"
WSTR s_hx_norun, "먼저 비교를 실행하세요."
WSTR s_hx_cap, "행이 많아 처음 100,000행만 내보냈습니다."
WSTR s_b_xls, "엑셀 저장"
WSTR f_hx_u, "%u"
hx_widths: dd 12, 52, 20, 52, 20, 14, 30

section .text

; hx_fmt(rcx=hex buffer, rdx=ascii buffer, r8=data, r9d=available bytes, [5th]=positions to render)
; positions beyond the available bytes are shown as "--" like the view does
hx_fmt:
    push rbx
    push rsi
    mov r10d, [rsp+56]
    lea r11, [hexdigits]
    xor eax, eax
.l:
    cmp eax, r10d
    jae .d
    test eax, eax
    jz .ns
    mov word [rcx], ' '
    add rcx, 2
.ns:
    cmp eax, r9d
    jae .miss
    movzx ebx, byte [r8+rax]
    mov esi, ebx
    shr esi, 4
    movzx esi, word [r11+rsi*2]
    mov [rcx], si
    mov esi, ebx
    and esi, 15
    movzx esi, word [r11+rsi*2]
    mov [rcx+2], si
    add rcx, 4
    cmp ebx, 32
    jb .dot
    cmp ebx, 126
    ja .dot
    mov [rdx], bx
    jmp .an
.dot:
    mov word [rdx], '.'
.an:
    add rdx, 2
    inc eax
    jmp .l
.miss:
    mov word [rcx], '-'
    mov word [rcx+2], '-'
    add rcx, 4
    inc eax
    jmp .l
.d:
    mov word [rcx], 0
    mov word [rdx], 0
    pop rsi
    pop rbx
    ret

%define HXX_XL   96                  ; XL ctx (40)
%define HXX_HA   144                 ; wchar[56]
%define HXX_AA   256                 ; wchar[24]
%define HXX_HB   304                 ; wchar[56]
%define HXX_AB   416                 ; wchar[24]
%define HXX_POS  464                 ; wchar[56]
%define HXX_NUM  576                 ; wchar[16]
%define HXX_OFF  608                 ; wchar[16]
%define HXX_NA   640                 ; dword
%define HXX_NB   644                 ; dword
%define HXX_TOT  648                 ; dword

; hx_xlsx_path(hx, path) -> eax 1 when written
hx_xlsx_path:
    PROLOG 704
    mov rbx, rcx
    mov rdi, rdx
    invoke xw_begin, &rsp+HXX_XL
    invoke xw_cell, &rsp+HXX_XL, &s_hx_h0, XS_HEAD
    invoke xw_cell, &rsp+HXX_XL, &s_hx_h1, XS_HEAD
    invoke xw_cell, &rsp+HXX_XL, &s_hx_h2, XS_HEAD
    invoke xw_cell, &rsp+HXX_XL, &s_hx_h3, XS_HEAD
    invoke xw_cell, &rsp+HXX_XL, &s_hx_h4, XS_HEAD
    invoke xw_cell, &rsp+HXX_XL, &s_hx_h5, XS_HEAD
    invoke xw_cell, &rsp+HXX_XL, &s_hx_h6, XS_HEAD
    invoke xw_endrow, &rsp+HXX_XL
    xor r12d, r12d
.row:
    cmp r12, [rbx+HX_NV]
    jae .fin
    cmp r12, HX_XL_MAXROWS
    jae .fin
    mov rax, [rbx+HX_VROWS]
    mov r13d, [rax+r12*4]            ; row
    mov r14, r13
    shl r14, 4                       ; offset
    ; bytes available on each side (0..16)
    xor eax, eax
    mov rcx, [rbx+HX_SZA]
    cmp r14, rcx
    jae .na
    sub rcx, r14
    mov eax, 16
    cmp rcx, rax
    jae .na
    mov eax, ecx
.na:
    mov [rsp+HXX_NA], eax
    xor edx, edx
    mov rcx, [rbx+HX_SZB]
    cmp r14, rcx
    jae .nb
    sub rcx, r14
    mov edx, 16
    cmp rcx, rdx
    jae .nb
    mov edx, ecx
.nb:
    mov [rsp+HXX_NB], edx
    cmp edx, eax
    jae .tot
    mov edx, eax
.tot:
    mov [rsp+HXX_TOT], edx
    mov r8, [rbx+HX_BUFA]
    add r8, r14
    mov r9d, [rsp+HXX_NA]
    invoke hx_fmt, &rsp+HXX_HA, &rsp+HXX_AA, r8, r9, [rsp+HXX_TOT]
    mov r8, [rbx+HX_BUFB]
    add r8, r14
    mov r9d, [rsp+HXX_NB]
    invoke hx_fmt, &rsp+HXX_HB, &rsp+HXX_AB, r8, r9, [rsp+HXX_TOT]
    ; differing positions within the row
    lea rsi, [rsp+HXX_POS]
    xor r15d, r15d                   ; count
    xor ecx, ecx
    lea r11, [hexdigits]
.d:
    cmp ecx, [rsp+HXX_TOT]
    jae .dd
    cmp ecx, [rsp+HXX_NA]
    jae .isdiff
    cmp ecx, [rsp+HXX_NB]
    jae .isdiff
    mov rax, [rbx+HX_BUFA]
    add rax, r14
    movzx eax, byte [rax+rcx]
    mov rdx, [rbx+HX_BUFB]
    add rdx, r14
    cmp al, [rdx+rcx]
    je .nodiff
.isdiff:
    test r15d, r15d
    jz .first
    mov word [rsi], ' '
    add rsi, 2
.first:
    movzx eax, cl
    mov ax, [r11+rax*2]
    mov [rsi], ax
    add rsi, 2
    inc r15d
.nodiff:
    inc ecx
    jmp .d
.dd:
    mov word [rsi], 0
    invoke _snwprintf, &rsp+HXX_OFF, 16, &f_off, r14
    mov ebp, XS_MONO
    test r15d, r15d
    jz .style
    mov ebp, XS_MONOORANGE
.style:
    invoke xw_cell, &rsp+HXX_XL, &rsp+HXX_OFF, rbp
    invoke xw_cell, &rsp+HXX_XL, &rsp+HXX_HA, rbp
    invoke xw_cell, &rsp+HXX_XL, &rsp+HXX_AA, rbp
    invoke xw_cell, &rsp+HXX_XL, &rsp+HXX_HB, rbp
    invoke xw_cell, &rsp+HXX_XL, &rsp+HXX_AB, rbp
    mov word [rsp+HXX_NUM], 0
    test r15d, r15d
    jz .num
    invoke _snwprintf, &rsp+HXX_NUM, 16, &f_hx_u, r15
.num:
    invoke xw_numcell, &rsp+HXX_XL, &rsp+HXX_NUM, XS_NORMAL
    invoke xw_cell, &rsp+HXX_XL, &rsp+HXX_POS, XS_NORMAL
    invoke xw_endrow, &rsp+HXX_XL
    inc r12
    jmp .row
.fin:
    invoke xw_save, &rsp+HXX_XL, rdi, &hx_widths, 7
    mov ebx, eax
    invoke sb_free, &rsp+HXX_XL
    mov eax, ebx
    EPILOG 704

; hx_export(hwnd, hx)
hx_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+HX_LOADED], 0
    je .norun
    invoke ex_pick, rsi, &rsp+400, 0
    test eax, eax
    jz .d
    invoke hx_xlsx_path, rbx, &rsp+400
    mov edi, eax
    invoke ex_done, rsi, rdi
    test edi, edi
    jz .d
    cmp qword [rbx+HX_NV], HX_XL_MAXROWS
    jbe .d
    invoke msgbox, rsi, &s_hx_cap, MB_OK|MB_ICONINFO
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &s_hx_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
