; text_patch.inc - text compare result -> unified diff file (IntelliJ "Create Patch")
;
;   The aligned row table is all that is needed: equal rows are context, the changed rows of a run give the
;   "-" lines (kinds 1, 2: the left text) followed by the "+" lines (kinds 1, 3: the right text).
;   Three context rows around a change; changes with six or fewer equal rows between them share a hunk.
;   The file is UTF-8 without a byte order mark, lines end with LF.
;   When a file does not end with a line break, its last line is followed by "\ No newline at end of file";
;   a last line that is equal but ends differently on the two sides is written as a "-" and a "+" line.

%define PT_CTX 3

section .rdata
WSTR s_b_patch, "패치 저장"
WSTR s_pt_ext, "patch"
WSTR s_pt_same, "차이가 없어 저장할 패치가 없습니다."
WSTR f_pt_hunk, "@@ -%u,%u +%u,%u @@"
WSTR s_b_pclip, "패치 복사"
WSTR s_pt_copied, "패치를 클립보드에 복사했습니다."
WSTR s_pt_noeol, "\ No newline at end of file"
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

; pt_noeol(rdi = output cursor): the marker behind a last line without a line break (leaf: rax, rdx)
pt_noeol:
    lea rdx, [s_pt_noeol]
    call pt_str
    mov word [rdi], 10
    add rdi, 2
    ret

; pt_last(rcx = TXT*, rdx = number of lines) -> eax index of the last line when the text does not end with a
;   line break, else -2 (no row has that line index) (leaf: rax, rcx, r8)
;   The stored text ends with its line break only as long as it is the text that was read from the file: once
;   a compare has taken it from the pane the break is gone, and TXT+24 still says that the file had one.
pt_last:
    mov eax, -2
    cmp dword [rcx+24], 0
    jne .e
    mov r8, [rcx+8]                  ; length
    test r8, r8
    jz .e
    mov rcx, [rcx]
    movzx ecx, word [rcx+r8*2-2]
    cmp ecx, 10
    je .e
    cmp ecx, 13
    je .e
    lea eax, [rdx-1]
.e:
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
%define PT_MA   320                  ; left line index that gets the "no newline" marker (-2: none)
%define PT_MB   328                  ; right line index

; text_patch_build(tv, &length) -> rax malloc'd UTF-16 text of the patch (0: out of memory), *length = characters
;   (the view must not be folded / unified)
text_patch_build:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov edx, 1                       ; the blank lines that the view ignores are changes all the same
    call tv_blank_kinds
    invoke text_patch_rows, rbx, rsi
    mov rsi, rax
    mov rcx, rbx
    xor edx, edx
    call tv_blank_kinds
    mov rax, rsi
    EPILOG 64

text_patch_rows:
    PROLOG 352
    mov rbx, rcx
    mov [rsp+PT_PATH], rdx           ; where the length goes
    lea rcx, [rbx+TV_TXA]
    mov rdx, [rbx+TV_NLA]
    call pt_last
    mov [rsp+PT_MA], eax
    lea rcx, [rbx+TV_TXB]
    mov rdx, [rbx+TV_NLB]
    call pt_last
    mov [rsp+PT_MB], eax
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
    add rax, [rbx+TV_NIGN]
    shl rax, 6
    add rcx, rax
    add rcx, 1500
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
    xor r8d, r8d                     ; is it the last line of a side that has no line break at its end?
    mov ecx, [r12+rax+4]
    cmp ecx, [rsp+PT_MA]
    sete r8b
    xor r9d, r9d
    mov ecx, [r12+rax+8]
    cmp ecx, [rsp+PT_MB]
    sete r9b
    cmp r8d, r9d
    jne .eqsplit
    mov ecx, ' '
    mov rdx, [r14+rsi*8]
    call pt_line
    test r8d, r8d
    jz .eqn
    call pt_noeol
.eqn:
    inc qword [rsp+PT_NA]
    inc qword [rsp+PT_NB]
    inc rsi
    jmp .body
.eqsplit:                            ; the same text, but only one side ends with a line break
    mov ecx, '-'
    mov rdx, [r14+rsi*8]
    call pt_line
    test r8d, r8d
    jz .es1
    call pt_noeol
.es1:
    mov ecx, '+'
    mov rdx, [r15+rsi*8]
    call pt_line
    test r9d, r9d
    jz .eqn
    call pt_noeol
    jmp .eqn
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
    mov rax, r9
    shl rax, 4
    mov eax, [r12+rax+4]
    cmp eax, [rsp+PT_MA]
    jne .mne
    call pt_noeol
.mne:
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
    mov rax, r9
    shl rax, 4
    mov eax, [r12+rax+8]
    cmp eax, [rsp+PT_MB]
    jne .pne
    call pt_noeol
.pne:
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
    mov rax, [rsp+PT_PATH]
    mov [rax], r8
    invoke free, r14
    mov rax, [rsp+PT_OUT]
    EPILOG 352
.fail1:
    invoke free, r14
.fail0:
    xor eax, eax
    EPILOG 352

; text_patch_path(tv, path) -> eax 1 when written
text_patch_path:
    PROLOG 96
    mov rsi, rdx
    invoke text_patch_build, rcx, &rsp+80
    mov rbx, rax
    test rax, rax
    jz .fail
    invoke write_utf8, rsi, rbx, [rsp+80], 0
    mov edi, eax
    invoke free, rbx
    mov eax, edi
    EPILOG 96
.fail:
    xor eax, eax
    EPILOG 96

; text_patch_clip(hwnd, tv): the same patch as text on the clipboard (IntelliJ "Copy as Patch to Clipboard")
text_patch_clip:
    PROLOG 128
    mov rsi, rcx
    mov rbx, rdx
    invoke tv_unfold, rsi, rbx
    cmp qword [rbx+TV_NROWS], 0
    je .norun
    mov rax, [rbx+TV_NBLK]
    add rax, [rbx+TV_NIGN]           ; ignored blank lines are in the patch
    jz .same
    invoke text_patch_build, rbx, &rsp+96
    mov r12, rax
    test rax, rax
    jz .fail
    mov rcx, [rsp+96]
    lea r14, [rcx*2+2]
    invoke GlobalAlloc, 2, r14                         ; GMEM_MOVEABLE
    mov r13, rax
    test rax, rax
    jz .failfree
    invoke GlobalLock, r13
    mov rdi, rax
    mov r14, [rsp+96]
    shl r14, 1
    invoke memcpy, rdi, r12, r14
    mov word [rdi+r14], 0
    invoke GlobalUnlock, r13
    invoke OpenClipboard, rsi
    test eax, eax
    jz .failfree
    invoke EmptyClipboard
    invoke SetClipboardData, 13, r13                   ; CF_UNICODETEXT: the clipboard owns the memory now
    invoke CloseClipboard
    invoke free, r12
    invoke set_status, &s_pt_copied
    EPILOG 128
.failfree:
    invoke free, r12
.fail:
    invoke ex_done, rsi, 0
    EPILOG 128
.norun:
    invoke msgbox, rsi, &s_tx_norun, MB_OK|MB_ICONINFO
    EPILOG 128
.same:
    invoke msgbox, rsi, &s_pt_same, MB_OK|MB_ICONINFO
    EPILOG 128

; text_patch(hwnd, tv): ask for a file and write the differences as a unified diff
text_patch:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    invoke tv_unfold, rsi, rbx            ; the patch is made from the full comparison
    cmp qword [rbx+TV_NROWS], 0
    je .norun
    mov rax, [rbx+TV_NBLK]
    add rax, [rbx+TV_NIGN]           ; ignored blank lines are in the patch
    jz .same
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
