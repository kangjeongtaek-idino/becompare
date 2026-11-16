; text_patch.inc - text compare result -> unified diff file (IntelliJ "Create Patch")
;
;   The aligned row table is all that is needed: equal rows are context, the changed rows of a run give the
;   "-" lines (kinds 1, 2: the left text) followed by the "+" lines (kinds 1, 3: the right text).
;   Three context rows around a change; changes with six or fewer equal rows between them share a hunk.
;   The file is UTF-8 without a byte order mark, lines end with LF.

%define PT_CTX 3

section .rdata
WSTR s_b_patch, "패치 저장"
WSTR s_pt_ext, "patch"
WSTR s_pt_same, "차이가 없어 저장할 패치가 없습니다."
WSTR f_pt_hunk, "@@ -%u,%u +%u,%u @@"
WSTR s_pt_a, "--- "
WSTR s_pt_b, "+++ "
pt_flt: dw __utf16__("패치 파일 (*.patch;*.diff)"), 0, __utf16__("*.patch;*.diff"), 0, 0
align 8

section .text

; pt_line(ecx = prefix, rdx = text ending at CR or 0, rdi = output cursor): one line of the patch (leaf: rax, rdx)
pt_line:
    mov [rdi], cx
    add rdi, 2
.l:
    movzx eax, word [rdx]
    test eax, eax
    jz .e
    cmp eax, 13
    je .e
    mov [rdi], ax
    add rdi, 2
    add rdx, 2
    jmp .l
.e:
    mov word [rdi], 10
    add rdi, 2
    ret

; pt_str(rdx = zero terminated text, rdi = output cursor) (leaf: rax, rdx)
pt_str:
    movzx eax, word [rdx]
    test eax, eax
    jz .e
    mov [rdi], ax
    add rdi, 2
    add rdx, 2
    jmp pt_str
.e:
    ret

%define PT_NA   96                   ; left lines before the cursor row
%define PT_NB   104                  ; right lines before the cursor row
%define PT_S    112                  ; hunk: first row
%define PT_E    120                  ; hunk: end row (exclusive)
%define PT_AC   128                  ; hunk: left line count
%define PT_BC   136                  ; hunk: right line count
%define PT_OUT  144                  ; output buffer
%define PT_J    152                  ; end of the run of changed rows
%define PT_T1   160
%define PT_T2   168
%define PT_PATH 176
%define PT_FMT  192                  ; wchar[64] -> 320

; text_patch_path(tv, path) -> eax 1 when written (the view must not be folded / unified)
text_patch_path:
    PROLOG 352
    mov rbx, rcx
    mov [rsp+PT_PATH], rdx
    mov r12, [rbx+TV_ROWS]
    mov r13, [rbx+TV_NROWS]
    lea rcx, [r13*8+8]
    shl rcx, 1
    call malloc
    mov r14, rax                     ; start of the left line of every row
    test rax, rax
    jz .fail0
    lea r15, [r14+r13*8+8]           ; start of the right line of every row
    mov rdx, [rbx+TV_BUFA]
    xor ecx, ecx
.fa:
    cmp rcx, r13
    jae .fad
    mov [r14+rcx*8], rdx
.fa1:
    movzx eax, word [rdx]
    test eax, eax
    jz .fa2
    add rdx, 2
    cmp eax, 13
    jne .fa1
.fa2:
    inc rcx
    jmp .fa
.fad:
    mov rdx, [rbx+TV_BUFB]
    xor ecx, ecx
.fb:
    cmp rcx, r13
    jae .fbd
    mov [r15+rcx*8], rdx
.fb1:
    movzx eax, word [rdx]
    test eax, eax
    jz .fb2
    add rdx, 2
    cmp eax, 13
    jne .fb1
.fb2:
    inc rcx
    jmp .fb
.fbd:
    ; every line at most once from each side, a prefix and a line end per row, a header per hunk, the paths
    mov rcx, [rbx+TV_LENA]
    add rcx, [rbx+TV_LENB]
    lea rcx, [rcx+r13*4]
    mov rax, [rbx+TV_NBLK]
    shl rax, 6
    add rcx, rax
    add rcx, 1400
    shl rcx, 1
    call malloc
    mov [rsp+PT_OUT], rax
    test rax, rax
    jz .fail1
    mov rdi, rax
    lea rdx, [s_pt_a]
    call pt_str
    lea rdx, [rbx+TV_PATHA]
    call pt_str
    mov word [rdi], 10
    add rdi, 2
    lea rdx, [s_pt_b]
    call pt_str
    lea rdx, [rbx+TV_PATHB]
    call pt_str
    mov word [rdi], 10
    add rdi, 2
    xor esi, esi                     ; cursor row: the rows before it are counted in PT_NA / PT_NB
    mov qword [rsp+PT_NA], 0
    mov qword [rsp+PT_NB], 0
.scan:
    mov rcx, rsi                     ; the next changed row
.find:
    cmp rcx, r13
    jae .finish
    mov rax, rcx
    shl rax, 4
    cmp dword [r12+rax], 0
    jne .found
    inc rcx
    jmp .find
.found:
    lea rax, [rcx-PT_CTX]            ; the hunk starts PT_CTX rows above it, not above the cursor
    cmp rax, rsi
    jge .sok
    mov rax, rsi
.sok:
    mov [rsp+PT_S], rax
.adv:                                ; the equal rows in between belong to no hunk
    cmp rsi, [rsp+PT_S]
    jae .advd
    inc qword [rsp+PT_NA]
    inc qword [rsp+PT_NB]
    inc rsi
    jmp .adv
.advd:
    mov rdx, rcx
.e1:                                 ; over a run of changed rows
    cmp rdx, r13
    jae .eend
    mov rax, rdx
    shl rax, 4
    cmp dword [r12+rax], 0
    je .e2
    inc rdx
    jmp .e1
.e2:
    mov r8, rdx                      ; the equal rows behind it
.e3:
    cmp r8, r13
    jae .etail
    mov rax, r8
    shl rax, 4
    cmp dword [r12+rax], 0
    jne .e4
    inc r8
    mov rax, r8
    sub rax, rdx
    cmp rax, PT_CTX*2
    jbe .e3
.etail:                              ; far enough to the next change (or none): PT_CTX rows of context end the hunk
    lea rax, [rdx+PT_CTX]
    cmp rax, r13
    jbe .ehave
    mov rax, r13
    jmp .ehave
.e4:
    mov rdx, r8                      ; the next change is close: same hunk
    jmp .e1
.eend:
    mov rax, r13
.ehave:
    mov [rsp+PT_E], rax
    xor r8d, r8d                     ; left lines of the hunk
    xor r9d, r9d                     ; right lines
    mov rcx, [rsp+PT_S]
.cnt:
    cmp rcx, [rsp+PT_E]
    jae .cntd
    mov rax, rcx
    shl rax, 4
    mov eax, [r12+rax]
    cmp eax, 3
    je .onlyb
    inc r8
    cmp eax, 2
    je .cn
.onlyb:
    inc r9
.cn:
    inc rcx
    jmp .cnt
.cntd:
    mov [rsp+PT_AC], r8
    mov [rsp+PT_BC], r9
    mov r10, [rsp+PT_NA]             ; first line number; with no lines the one before the hunk
    test r8, r8
    jz .a0
    inc r10
.a0:
    mov r11, [rsp+PT_NB]
    test r9, r9
    jz .b0
    inc r11
.b0:
    mov [rsp+PT_T1], r10
    mov [rsp+PT_T2], r11
    invoke _snwprintf, &rsp+PT_FMT, 60, &f_pt_hunk, [rsp+PT_T1], [rsp+PT_AC], [rsp+PT_T2], [rsp+PT_BC]
    lea rdx, [rsp+PT_FMT]
    call pt_str
    mov word [rdi], 10
    add rdi, 2
.body:
    cmp rsi, [rsp+PT_E]
    jae .scan
    mov rax, rsi
    shl rax, 4
    cmp dword [r12+rax], 0
    jne .chg
    mov ecx, ' '
    mov rdx, [r14+rsi*8]
    call pt_line
    inc qword [rsp+PT_NA]
    inc qword [rsp+PT_NB]
    inc rsi
    jmp .body
.chg:
    mov r8, rsi                      ; the run of changed rows [rsi, r8)
.c1:
    cmp r8, [rsp+PT_E]
    jae .c2
    mov rax, r8
    shl rax, 4
    cmp dword [r12+rax], 0
    je .c2
    inc r8
    jmp .c1
.c2:
    mov [rsp+PT_J], r8
    mov r9, rsi
.mi:                                 ; its left lines
    cmp r9, [rsp+PT_J]
    jae .mid
    mov rax, r9
    shl rax, 4
    cmp dword [r12+rax], 3
    je .min
    mov ecx, '-'
    mov rdx, [r14+r9*8]
    call pt_line
    inc qword [rsp+PT_NA]
.min:
    inc r9
    jmp .mi
.mid:
    mov r9, rsi
.pl:                                 ; then its right lines
    cmp r9, [rsp+PT_J]
    jae .pld
    mov rax, r9
    shl rax, 4
    cmp dword [r12+rax], 2
    je .pln
    mov ecx, '+'
    mov rdx, [r15+r9*8]
    call pt_line
    inc qword [rsp+PT_NB]
.pln:
    inc r9
    jmp .pl
.pld:
    mov rsi, [rsp+PT_J]
    jmp .body
.finish:
    mov r8, rdi
    sub r8, [rsp+PT_OUT]
    shr r8, 1
    mov [rsp+PT_T1], r8
    invoke write_utf8, [rsp+PT_PATH], [rsp+PT_OUT], [rsp+PT_T1], 0
    mov ebx, eax
    invoke free, [rsp+PT_OUT]
    invoke free, r14
    mov eax, ebx
    EPILOG 352
.fail1:
    invoke free, r14
.fail0:
    xor eax, eax
    EPILOG 352

; text_patch(hwnd, tv): ask for a file and write the differences as a unified diff
text_patch:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    invoke tv_unfold, rsi, rbx            ; the patch is made from the full comparison
    cmp qword [rbx+TV_NROWS], 0
    je .norun
    cmp qword [rbx+TV_NBLK], 0
    je .same
    mov word [rsp+128], 0
    invoke browse_save_as, rsi, &rsp+128, &pt_flt, &s_pt_ext
    test eax, eax
    jz .d
    invoke text_patch_path, rbx, &rsp+128
    mov edi, eax
    invoke ex_done, rsi, rdi
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &s_tx_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
.same:
    invoke msgbox, rsi, &s_pt_same, MB_OK|MB_ICONINFO
    EPILOG 1200
