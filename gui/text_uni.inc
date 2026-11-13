section .rdata
WSTR s_c_uni, "통합 보기"
WSTR s_c_word, "단어 강조"
section .text

; text_uni.inc - unified view (IntelliJ "Unified viewer")
;
;   One pane (A) shows every line once: equal lines, then for a change block all of its left lines followed
;   by all of its right lines. The aligned row table is turned into a unified one:
;       0 equal (aIdx, bIdx)   2 only left = deleted (aIdx)   3 only right = inserted (bIdx)
;       5 left line of a modified pair (aIdx)                 6 right line of a modified pair (bIdx)
;   Within a block the left lines (5, 2) come first and the right lines (6, 3) after them, so the j-th row 5 is
;   paired with the j-th row 6, a rows further down (a = number of left lines of the block).
;   The gutters keep numbering the A / B line of each row; folding works on the kind 0 rows as before.
;   Like folding it is display only: TV_TXA / TV_TXB keep the texts and the pane is read only.

; tv_unify_rows(tv) -> eax 1 when converted (0: out of memory, rows unchanged)
tv_unify_rows:
    PROLOG 96
    mov rbx, rcx
    mov r12, [rbx+TV_ROWS]
    mov r13, [rbx+TV_NROWS]
    lea rcx, [r13*2+2]
    shl rcx, 4
    call malloc
    mov r14, rax                     ; unified rows
    test r14, r14
    jz .nomem
    xor esi, esi                     ; aligned row
    xor edi, edi                     ; unified row count
.l:
    cmp rsi, r13
    jae .done
    mov rax, rsi
    shl rax, 4
    add rax, r12
    cmp dword [rax], 0
    jne .blk
    mov rcx, rdi                     ; equal row: as it is
    shl rcx, 4
    add rcx, r14
    mov rdx, [rax]
    mov [rcx], rdx
    mov rdx, [rax+8]
    mov [rcx+8], rdx
    inc edi
    inc rsi
    jmp .l
.blk:
    mov r15d, [rax+12]               ; block number
    mov r8, [rbx+TV_BLK]
    mov r9d, [r8+r15*8+4]            ; end (exclusive) in the aligned numbering
    mov r10d, edi                    ; first unified row of the block
    ; ---- left lines: pairs as kind 5, left-only rows as they are
    mov r11, rsi
.al:
    cmp r11, r9
    jae .ald
    mov rax, r11
    shl rax, 4
    add rax, r12
    mov ecx, [rax]
    cmp ecx, 1
    je .a5
    cmp ecx, 2
    jne .aln
    mov rcx, rdi
    shl rcx, 4
    add rcx, r14
    mov rdx, [rax]
    mov [rcx], rdx
    mov rdx, [rax+8]
    mov [rcx+8], rdx
    inc edi
    jmp .aln
.a5:
    mov rcx, rdi
    shl rcx, 4
    add rcx, r14
    mov dword [rcx], 5
    mov edx, [rax+4]
    mov [rcx+4], edx
    mov dword [rcx+8], -1
    mov edx, [rax+12]
    mov [rcx+12], edx
    inc edi
.aln:
    inc r11
    jmp .al
.ald:
    ; ---- right lines: pairs as kind 6, right-only rows as they are
    mov r11, rsi
.bl:
    cmp r11, r9
    jae .bld
    mov rax, r11
    shl rax, 4
    add rax, r12
    mov ecx, [rax]
    cmp ecx, 1
    je .b6
    cmp ecx, 3
    jne .bln
    mov rcx, rdi
    shl rcx, 4
    add rcx, r14
    mov rdx, [rax]
    mov [rcx], rdx
    mov rdx, [rax+8]
    mov [rcx+8], rdx
    inc edi
    jmp .bln
.b6:
    mov rcx, rdi
    shl rcx, 4
    add rcx, r14
    mov dword [rcx], 6
    mov dword [rcx+4], -1
    mov edx, [rax+8]
    mov [rcx+8], edx
    mov edx, [rax+12]
    mov [rcx+12], edx
    inc edi
.bln:
    inc r11
    jmp .bl
.bld:
    mov r8, [rbx+TV_BLK]             ; the block now covers the unified rows [first, edi)
    mov [r8+r15*8], r10d
    mov [r8+r15*8+4], edi
    mov rsi, r9
    jmp .l
.done:
    invoke free, r12
    mov [rbx+TV_ROWS], r14
    mov [rbx+TV_NROWS], rdi
    mov qword [rbx+TV_LASTP], 0      ; only the left pane exists
    mov eax, 1
    EPILOG 96
.nomem:
    xor eax, eax
    EPILOG 96

; tv_texts_uni(tv, LA, LB): TV_BUFA holds one line per row, TV_BUFB stays empty
tv_texts_uni:
    PROLOG 288
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov rbx, [r12+TV_ROWS]
    mov r15, [r12+TV_NROWS]
    mov rdi, [r12+TV_BUFA]
    mov rbp, [r12+TV_BUFB]
    xor esi, esi
.u:
    cmp rsi, r15
    jae .ud
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    mov ecx, [rax]
    cmp ecx, 4
    je .label
    cmp ecx, 3
    je .fromb
    cmp ecx, 6
    je .fromb
    mov ecx, [rax+4]                 ; left line
    mov rdx, [r13]
    mov rdx, [rdx+rcx*8]
    jmp .copy
.fromb:
    mov ecx, [rax+8]                 ; right line
    mov rdx, [r14]
    mov rdx, [rdx+rcx*8]
    jmp .copy
.label:
    invoke fold_label, &rsp+112, [rax+12]
    lea rdx, [rsp+112]
.copy:
    movzx r8d, word [rdx]
    test r8d, r8d
    jz .cd
    mov [rdi], r8w
    add rdi, 2
    add rdx, 2
    jmp .copy
.cd:
    lea r9, [rsi+1]
    cmp r9, r15
    jae .un
    mov word [rdi], 13
    add rdi, 2
.un:
    inc rsi
    jmp .u
.ud:
    mov word [rdi], 0
    mov word [rbp], 0
    mov rax, rdi
    sub rax, [r12+TV_BUFA]
    shr rax, 1
    mov [r12+TV_LENA], rax
    mov qword [r12+TV_LENB], 0
    EPILOG 288

%define CU_LENS 96                   ; base of the length array (after the n+1 offsets)
%define CU_A    104                  ; left lines of the current block
%define CU_Q    120                  ; quad {A text, A length, B text, B length}
; tv_color_uni(tv, LA, LB): backgrounds per kind, word level changes for the pairs
tv_color_uni:
    PROLOG 224
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov rbx, [r12+TV_ROWS]
    mov r15, [r12+TV_NROWS]
    lea rcx, [r15*8+8]
    shl rcx, 1
    call malloc
    mov rbp, rax                     ; offsets[n+1], lengths[n]
    test rbp, rbp
    jz .d
    lea rax, [rbp+r15*8+8]
    mov [rsp+CU_LENS], rax
    xor esi, esi
    xor edi, edi                     ; offset in pane A
.p1:
    cmp rsi, r15
    jae .p2
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    mov [rbp+rsi*8], rdi
    mov ecx, [rax]
    cmp ecx, 4
    je .p1l
    cmp ecx, 3
    je .p1b
    cmp ecx, 6
    je .p1b
    mov ecx, [rax+4]
    mov rdx, [r13]
    mov rdx, [rdx+rcx*8]
    jmp .p1len
.p1b:
    mov ecx, [rax+8]
    mov rdx, [r14]
    mov rdx, [rdx+rcx*8]
.p1len:
    invoke wcslen, rdx
    jmp .p1s
.p1l:
    invoke fold_label, &rsp+152, [rax+12]
.p1s:
    mov rcx, [rsp+CU_LENS]
    mov [rcx+rsi*8], rax
    lea rdi, [rdi+rax+1]
    inc rsi
    jmp .p1
.p2:
    xor esi, esi
.r:
    cmp rsi, r15
    jae .free
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    mov ecx, [rax]
    test ecx, ecx
    jz .rn
    mov rdx, [rsp+CU_LENS]
    mov r9, [rdx+rsi*8]              ; length
    mov r8, [rbp+rsi*8]              ; start
    add r9, r8                       ; end
    cmp ecx, 4
    je .gap
    cmp ecx, 2
    je .del
    cmp ecx, 3
    je .ins
    cmp ecx, 6
    je .rn                           ; the right line of a pair is painted together with its left line
    invoke rich_bk, [r12+TV_HA], r8, r9, #[th_tc_mod]       ; 5: modified
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    cmp dword [rax], 5
    jne .rn
    ; ---- word level changes between this left line and its right line, a rows further down
    mov edx, [rax+12]
    mov r8, [r12+TV_BLK]
    mov r9d, [r8+rdx*8]              ; first row of the block
    cmp r9, rsi
    jne .haveA
    mov r10d, [r8+rdx*8+4]           ; end of the block
    xor r11d, r11d
    mov rcx, r9
.cnt:
    cmp rcx, r10
    jae .cntd
    mov rax, rcx
    shl rax, 4
    add rax, rbx
    mov edx, [rax]
    cmp edx, 5
    je .cy
    cmp edx, 2
    jne .cnn
.cy:
    inc r11d
.cnn:
    inc rcx
    jmp .cnt
.cntd:
    mov [rsp+CU_A], r11
.haveA:
    mov rcx, rsi
    add rcx, [rsp+CU_A]              ; partner row
    cmp rcx, r15
    jae .rn
    mov rax, rcx
    shl rax, 4
    add rax, rbx
    mov edx, [rax+8]                 ; its right line
    mov r8, [r14]
    mov r8, [r8+rdx*8]
    mov [rsp+CU_Q+16], r8
    mov rdx, [rsp+CU_LENS]
    mov r8, [rdx+rcx*8]
    mov [rsp+CU_Q+24], r8
    mov r10, [rbp+rcx*8]             ; start of the partner line
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    mov edx, [rax+4]                 ; this left line
    mov r8, [r13]
    mov r8, [r8+rdx*8]
    mov [rsp+CU_Q], r8
    mov rdx, [rsp+CU_LENS]
    mov r8, [rdx+rsi*8]
    mov [rsp+CU_Q+8], r8
    mov [rsp+112], r10
    mov r9, [rsp+CU_Q+24]
    add r9, r10
    invoke rich_bk, [r12+TV_HA], r10, r9, #[th_tc_mod]      ; the partner line first, then the changes on both
    mov r10, [rsp+112]
    mov r8, [rbp+rsi*8]
    invoke tv_wordhl, r12, &rsp+CU_Q, r8, r10
    jmp .rn
.gap:
    invoke rich_bk, [r12+TV_HA], r8, r9, #[th_tc_gap]
    jmp .rn
.del:
    invoke rich_bk, [r12+TV_HA], r8, r9, #[th_tc_del]
    jmp .rn
.ins:
    invoke rich_bk, [r12+TV_HA], r8, r9, #[th_tc_ins]
.rn:
    inc rsi
    jmp .r
.free:
    invoke free, rbp
.d:
    EPILOG 224
