; text_fold.inc - collapse unchanged fragments (IntelliJ "Collapse unchanged fragments")
;
;   tv_build creates one row per line of the aligned view. When folding is on, every run of equal rows
;   keeps FOLD_CTX rows next to a change and the rest (when at least FOLD_MIN rows) is replaced by ONE
;   placeholder row, kind 4, whose +12 field holds the number of hidden rows. Everything that works on rows
;   (gutters, overview map, navigation, colouring) simply sees the shorter table; blocks are remapped.
;
;   The folded view is display only: TV_TXA / TV_TXB keep the full texts, the panes are read only, and every
;   action that reads or writes pane text (block copy, save, replace, manual alignment) first calls
;   tv_unfold, which unchecks the option and compares again.

%define FOLD_CTX 3
%define FOLD_MIN 4

section .rdata
WSTR f_fold_lbl, "⋯ 변경 없는 줄 %u개 접힘 (⋯ 클릭: 펼치기)"
WSTR s_c_fold, "변경 없는 구간 접기"

section .text

; fold_isexp(rbx = tv, edx = first hidden row) -> eax 1 when the user expanded that fragment (leaf: eax, ecx only)
fold_isexp:
    mov ecx, [rbx+TV_EXPN]
.l:
    test ecx, ecx
    jz .no
    dec ecx
    cmp [rbx+TV_EXPK+rcx*4], edx
    jne .l
    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

; fold_label(buf (>= 40 wchar), n) -> eax length
fold_label:
    PROLOG 64
    mov rbx, rcx
    mov esi, edx
    invoke _snwprintf, rbx, 39, &f_fold_lbl, rsi
    mov word [rbx+78], 0
    invoke wcslen, rbx
    EPILOG 64

%define FR_HS  96
%define FR_HE  104
%define FR_HID 112

; tv_fold_rows(tv) -> eax 1 when the rows were folded (0: out of memory, rows unchanged)
tv_fold_rows:
    PROLOG 128
    mov rbx, rcx
    mov r12, [rbx+TV_ROWS]
    mov r13, [rbx+TV_NROWS]
    lea rcx, [r13+2]
    shl rcx, 4
    call malloc
    mov r14, rax                     ; new rows
    lea rcx, [r13*4+8]
    call malloc
    mov r15, rax                     ; old row -> new row (-1: hidden)
    test r14, r14
    jz .nomem
    test r15, r15
    jz .nomem
    xor esi, esi                     ; old row
    xor edi, edi                     ; new row count
.i:
    cmp rsi, r13
    jae .done
    mov rax, rsi
    shl rax, 4
    add rax, r12
    cmp dword [rax], 0
    je .eq
    mov [r15+rsi*4], edi             ; a changed row is always kept
    mov rcx, rdi
    shl rcx, 4
    add rcx, r14
    mov rdx, [rax]
    mov [rcx], rdx
    mov rdx, [rax+8]
    mov [rcx+8], rdx
    inc edi
    inc rsi
    jmp .i
.eq:
    mov r8, rsi
.e:
    inc r8
    cmp r8, r13
    jae .ed
    mov rax, r8
    shl rax, 4
    add rax, r12
    cmp dword [rax], 0
    je .e
.ed:                                 ; the equal run is [rsi, r8)
    xor r9d, r9d
    test rsi, rsi
    jz .nl
    mov r9d, FOLD_CTX                ; context after the previous change
.nl:
    xor r10d, r10d
    cmp r8, r13
    jae .nt
    mov r10d, FOLD_CTX               ; context before the next change
.nt:
    lea r11, [rsi+r9]                ; first hidden row
    mov rcx, r8
    sub rcx, r10                     ; end of the hidden rows
    xor eax, eax
    cmp rcx, r11                     ; signed: a short run gives end < start
    jle .setup
    mov rax, rcx
    sub rax, r11
    cmp rax, FOLD_MIN
    jae .setup
    xor eax, eax
.setup:
    test rax, rax
    jnz .have
    mov r11, r8                      ; nothing hidden
    mov rcx, r8
.have:
    mov r9, rax                      ; rows that would be hidden
    mov r10, rcx                     ; end of the hidden rows
    mov edx, r11d
    call fold_isexp                  ; a fragment the user expanded stays open
    mov rcx, r10
    test eax, eax
    jz .hide
    xor r9d, r9d
    mov r11, r8
    mov rcx, r8
.hide:
    mov rax, r9
    mov [rsp+FR_HS], r11
    mov [rsp+FR_HE], rcx
    mov [rsp+FR_HID], rax
    mov r9, rsi
.j:
    cmp r9, r8
    jae .jd
    cmp r9, [rsp+FR_HS]
    jne .copy
    cmp qword [rsp+FR_HID], 0
    je .copy
    mov rcx, rdi                     ; placeholder row
    shl rcx, 4
    add rcx, r14
    mov dword [rcx], 4
    mov eax, [rsp+FR_HS]
    mov [rcx+4], eax                 ; key of the fragment (first hidden row), for tv_expand
    mov dword [rcx+8], -1
    mov eax, [rsp+FR_HID]
    mov [rcx+12], eax
    inc edi
    mov r10, r9
.hk:
    cmp r10, [rsp+FR_HE]
    jae .hkd
    mov dword [r15+r10*4], -1
    inc r10
    jmp .hk
.hkd:
    mov r9, [rsp+FR_HE]
    jmp .j
.copy:
    mov rax, r9
    shl rax, 4
    add rax, r12
    mov [r15+r9*4], edi
    mov rcx, rdi
    shl rcx, 4
    add rcx, r14
    mov rdx, [rax]
    mov [rcx], rdx
    mov rdx, [rax+8]
    mov [rcx+8], rdx
    inc edi
    inc r9
    jmp .j
.jd:
    mov rsi, r8
    jmp .i
.done:
    ; blocks: first / end(exclusive) rows move to the new numbering
    mov r8, [rbx+TV_BLK]
    xor ecx, ecx
.b:
    cmp rcx, [rbx+TV_NBLK]
    jae .bd
    mov eax, [r8+rcx*8]
    mov eax, [r15+rax*4]
    mov edx, [r8+rcx*8+4]
    mov edx, [r15+rdx*4-4]
    inc edx
    mov [r8+rcx*8], eax
    mov [r8+rcx*8+4], edx
    inc rcx
    jmp .b
.bd:
    invoke free, r12
    mov [rbx+TV_ROWS], r14
    mov [rbx+TV_NROWS], rdi
    invoke free, r15
    mov eax, 1
    EPILOG 128
.nomem:
    invoke free, r14
    invoke free, r15
    xor eax, eax
    EPILOG 128

; tv_unfold(hwnd, tv): leave the folded view (uncheck the option and compare again), keeping the current block
tv_unfold:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+TV_FOLDED], 0
    je .d
    mov r12, [rbx+TV_CUR]
    invoke GetDlgItem, rsi, ID_CHK_FOLD
    invoke SendMessageW, rax, BM_SETCHECK, 0, 0
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    invoke text_compare, rsi, rbx, rax
    cmp r12, -1
    je .d
    cmp r12, [rbx+TV_NBLK]
    jae .d
    invoke tv_goto, rsi, rbx, r12
.d:
    EPILOG 64

; tv_expand(hwnd, tv, row*): open the folded fragment of a placeholder row, keeping the scroll position
tv_expand:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov eax, [r8+4]                  ; key
    mov ecx, [rbx+TV_EXPN]
    cmp ecx, 64
    jae .d
    mov [rbx+TV_EXPK+rcx*4], eax
    inc ecx
    mov [rbx+TV_EXPN], ecx
    invoke SendMessageW, [rbx+TV_HA], EM_GETFIRSTVISIBLELINE, 0, 0
    mov r12d, eax                    ; first visible row
    mov r13, [rbx+TV_CUR]
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    invoke text_compare, rsi, rbx, rax
    invoke SendMessageW, [rbx+TV_HA], EM_GETFIRSTVISIBLELINE, 0, 0
    sub r12d, eax
    movsxd r14, r12d
    mov qword [rbx+TV_GUARD], 1
    invoke SendMessageW, [rbx+TV_HA], EM_LINESCROLL, 0, r14
    invoke SendMessageW, [rbx+TV_HB], EM_LINESCROLL, 0, r14
    mov qword [rbx+TV_GUARD], 0
    cmp r13, -1
    je .d
    cmp r13, [rbx+TV_NBLK]
    jae .d
    mov [rbx+TV_CUR], r13            ; the current block stays the current block
    invoke tv_info, rsi, rbx
    invoke tv_redraw_aux, rbx
.d:
    EPILOG 64
