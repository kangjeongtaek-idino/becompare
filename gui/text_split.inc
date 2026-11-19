; text_split.inc - Text Compare: "split changes" (IntelliJ "Highlight split changes")
;
;   The line compare gives one hunk for a run of changed lines, and tv_build pairs its lines by position: the
;   first left line with the first right line and so on, the rest of the longer side after them. A line that
;   was inserted in front of two edited ones therefore shows as three "modified" rows of unrelated text.
;   With the option on, the lines inside such a hunk are matched by what they have in common and the hunk is
;   cut into smaller ones: runs of lines that belong together (modified pairs) and the lines between them
;   (inserted, deleted, or replaced by something else). Each piece is a difference of its own.
;
;   Two lines belong together when at least half of the tokens of the longer one are in the other (a token is
;   a word or one other character; blanks do not count, the case does not when it is ignored) and they share
;   a word - punctuation alone does not pair "foo();" with "bar();". The pairs are chosen in order (no
;   crossing) so that their number and likeness is the largest. A hunk with more than SP_MAX line pairs to
;   look at is left as it is.

extern qsort

%define SP_MAX  1000000              ; left lines x right lines of one hunk

%define SP_A0   96                   ; dword: first left line of the hunk
%define SP_N    100                  ; dword: left lines
%define SP_B0   104
%define SP_M    108
%define SP_LA   112                  ; line pointers of the left side, at the first line of the hunk
%define SP_LB   120
%define SP_TOK  128                  ; dword[]: the tokens of all lines (sorted per line)
%define SP_IDX  136                  ; {dword offset, dword count}[n + m]
%define SP_W    144                  ; byte[n * m]: likeness of a pair, 0 = they do not belong together
%define SP_DP   152                  ; dword[(n + 1) * (m + 1)]: best score of the first i / j lines
%define SP_MT   160                  ; dword[n]: the right line a left line is paired with, -1 none
%define SP_NV   168                  ; the hunk vector that is being written
%define SP_M1   176                  ; m + 1
%define SP_UA   184                  ; dword x 2: scratch

section .rdata
WSTR s_c_split, "변경 나누기"
section .bss
alignb 8
sp_fl: resq 1                        ; compare options (bit 0: ignore case)
section .text

; sp_tok(line, out dword[], flags) -> eax token count: a word is its hash with bit 31 set, another character
;   is itself, blanks are left out
sp_tok:
    push rbx
    push rsi
    push rdi
    push r12
    mov rsi, rcx
    mov rdi, rdx
    mov r12d, r8d
    xor ebx, ebx
.l:
    movzx ecx, word [rsi]
    test ecx, ecx
    jz .d
    call wh_class                    ; (changes eax only)
    cmp eax, 2
    je .skip
    cmp eax, 1
    je .word
    mov [rdi+rbx*4], ecx
    inc ebx
.skip:
    add rsi, 2
    jmp .l
.word:
    mov edx, 2166136261
.w:
    movzx ecx, word [rsi]
    test ecx, ecx
    jz .we
    call wh_class
    cmp eax, 1
    jne .we
    test r12d, 1
    jz .nc
    cmp ecx, 'A'
    jb .nc
    cmp ecx, 'Z'
    ja .nc
    or ecx, 0x20
.nc:
    xor edx, ecx
    imul edx, edx, 16777619
    add rsi, 2
    jmp .w
.we:
    or edx, 0x80000000
    mov [rdi+rbx*4], edx
    inc ebx
    jmp .l
.d:
    mov eax, ebx
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

; sp_cmp(a*, b*): order of two tokens for qsort (unsigned: the words come last)
sp_cmp:
    mov eax, [rcx]
    mov ecx, [rdx]
    xor edx, edx
    cmp eax, ecx
    seta dl
    sbb eax, eax                     ; -1 when below
    or eax, edx
    ret

; sp_sim(tokens A, count A, tokens B, count B) -> eax 0 when the lines do not belong together, else 1..255
sp_sim:
    push rbx
    push rsi
    mov eax, edx
    or eax, r9d
    jnz .some
    mov eax, 255                     ; two lines of blanks
    jmp .d
.some:
    test edx, edx
    jz .no
    test r9d, r9d
    jz .no
    xor r10d, r10d                   ; index A
    xor r11d, r11d                   ; index B
    xor ebx, ebx                     ; tokens in both
    xor esi, esi                     ; words in both
.l:
    cmp r10d, edx
    jae .ld
    cmp r11d, r9d
    jae .ld
    mov eax, [rcx+r10*4]
    cmp eax, [r8+r11*4]
    jb .ia
    ja .ib
    inc ebx
    shr eax, 31
    add esi, eax
    inc r10d
    inc r11d
    jmp .l
.ia:
    inc r10d
    jmp .l
.ib:
    inc r11d
    jmp .l
.ld:
    mov r10d, edx                    ; the longer line
    cmp r10d, r9d
    jae .mx
    mov r10d, r9d
.mx:
    lea eax, [rbx*2]
    cmp eax, r10d
    jb .no
    test esi, esi
    jnz .yes
    mov eax, [rcx+rdx*4-4]           ; no word in common: fine only when neither line has a word
    or eax, [r8+r9*4-4]
    js .no
.yes:
    imul eax, ebx, 254
    xor edx, edx
    div r10d
    inc eax
    jmp .d
.no:
    xor eax, eax
.d:
    pop rsi
    pop rbx
    ret

; sp_emit(frame rcx, a, aLen, b, bLen on the stack): append a changed hunk
%macro SP_EMIT 4
    invoke vec_push, [rsp+SP_NV], 24
    mov dword [rax], 1
    mov ecx, %1
    mov [rax+4], ecx
    mov ecx, %2
    mov [rax+8], ecx
    mov ecx, %3
    mov [rax+12], ecx
    mov ecx, %4
    mov [rax+16], ecx
    mov dword [rax+20], 0
%endmacro

; sp_one(LA, LB, hunk*, new vec*): the pieces of one changed hunk
sp_one:
    PROLOG 224
    mov rax, [rcx]
    mov r10d, [r8+4]
    lea rax, [rax+r10*8]
    mov [rsp+SP_LA], rax
    mov [rsp+SP_A0], r10d
    mov eax, [r8+8]
    mov [rsp+SP_N], eax
    mov rax, [rdx]
    mov r10d, [r8+12]
    lea rax, [rax+r10*8]
    mov [rsp+SP_LB], rax
    mov [rsp+SP_B0], r10d
    mov eax, [r8+16]
    mov [rsp+SP_M], eax
    inc eax
    mov [rsp+SP_M1], rax
    mov [rsp+SP_NV], r9
    ; room for the tokens: there are no more of them than characters
    xor r12d, r12d
    xor ebx, ebx
.lena:
    cmp ebx, [rsp+SP_N]
    jae .lenb0
    mov rax, [rsp+SP_LA]
    invoke wcslen, [rax+rbx*8]
    add r12, rax
    inc ebx
    jmp .lena
.lenb0:
    xor ebx, ebx
.lenb:
    cmp ebx, [rsp+SP_M]
    jae .lend
    mov rax, [rsp+SP_LB]
    invoke wcslen, [rax+rbx*8]
    add r12, rax
    inc ebx
    jmp .lenb
.lend:
    lea rcx, [r12*4+16]
    call malloc
    mov [rsp+SP_TOK], rax
    mov ecx, [rsp+SP_N]
    add ecx, [rsp+SP_M]
    lea rcx, [rcx*8+16]
    call malloc
    mov [rsp+SP_IDX], rax
    ; the tokens of every line, sorted
    mov r13, [rsp+SP_TOK]
    xor ebx, ebx
.tk:
    mov eax, [rsp+SP_N]
    add eax, [rsp+SP_M]
    cmp ebx, eax
    jae .tkd
    cmp ebx, [rsp+SP_N]
    jae .tkb
    mov rax, [rsp+SP_LA]
    mov rcx, [rax+rbx*8]
    jmp .tkc
.tkb:
    mov eax, ebx
    sub eax, [rsp+SP_N]
    mov rdx, [rsp+SP_LB]
    mov rcx, [rdx+rax*8]
.tkc:
    mov rdx, r13
    mov r8d, [sp_fl]
    call sp_tok
    mov r14d, eax
    mov rdx, [rsp+SP_IDX]
    mov rax, r13
    sub rax, [rsp+SP_TOK]
    shr rax, 2
    mov [rdx+rbx*8], eax
    mov [rdx+rbx*8+4], r14d
    cmp r14d, 2
    jb .tkn
    invoke qsort, r13, r14, 4, &sp_cmp
.tkn:
    lea r13, [r13+r14*4]
    inc ebx
    jmp .tk
.tkd:
    mov eax, [rsp+SP_N]
    mov ecx, [rsp+SP_M]
    imul rax, rcx
    lea rcx, [rax+16]
    call malloc
    mov [rsp+SP_W], rax
    mov ecx, [rsp+SP_N]
    inc ecx
    imul rcx, [rsp+SP_M1]
    mov edx, 4
    call calloc                      ; row 0 and column 0 stay 0
    mov [rsp+SP_DP], rax
    mov ecx, [rsp+SP_N]
    lea rcx, [rcx*4+16]
    call malloc
    mov [rsp+SP_MT], rax
    mov r8d, [rsp+SP_N]
    shl r8, 2
    invoke memset, rax, 0xFF, r8
    ; likeness of every pair and the best score up to it
    xor r12d, r12d                   ; left line
.fi:
    cmp r12d, [rsp+SP_N]
    jae .fid
    mov eax, [rsp+SP_M]
    imul rax, r12
    add rax, [rsp+SP_W]
    mov r15, rax                     ; likeness row
    lea rax, [r12+1]
    imul rax, [rsp+SP_M1]
    shl rax, 2
    add rax, [rsp+SP_DP]
    mov r14, rax                     ; score row i + 1
    xor r13d, r13d                   ; right line
.fj:
    cmp r13d, [rsp+SP_M]
    jae .fjd
    mov rbp, [rsp+SP_IDX]
    mov rsi, [rsp+SP_TOK]
    mov eax, [rbp+r12*8]
    lea rcx, [rsi+rax*4]
    mov edx, [rbp+r12*8+4]
    mov eax, [rsp+SP_N]
    add eax, r13d
    mov r9d, [rbp+rax*8+4]
    mov eax, [rbp+rax*8]
    lea r8, [rsi+rax*4]
    call sp_sim
    mov [r15+r13], al
    mov rdx, [rsp+SP_M1]
    shl rdx, 2
    mov rcx, r14
    sub rcx, rdx                     ; score row i
    mov r8d, [rcx+r13*4+4]           ; without the left line
    mov r9d, [r14+r13*4]             ; without the right line
    cmp r8d, r9d
    jae .m1
    mov r8d, r9d
.m1:
    test eax, eax
    jz .m2
    add eax, 256                     ; a pair: one more, and the more alike the better
    add eax, [rcx+r13*4]
    cmp eax, r8d
    jbe .m2
    mov r8d, eax
.m2:
    mov [r14+r13*4+4], r8d
    inc r13d
    jmp .fj
.fjd:
    inc r12d
    jmp .fi
.fid:
    ; walk back: which pairs make the best score
    mov r12d, [rsp+SP_N]
    mov r13d, [rsp+SP_M]
.bt:
    test r12d, r12d
    jz .btd
    test r13d, r13d
    jz .btd
    mov rax, r12
    imul rax, [rsp+SP_M1]
    add rax, r13
    mov rcx, [rsp+SP_DP]
    lea rcx, [rcx+rax*4]             ; score of i, j
    mov edx, [rcx]
    mov rax, [rsp+SP_M1]
    shl rax, 2
    mov r8, rcx
    sub r8, rax                      ; score of i - 1, j
    lea eax, [r12-1]
    mov r9d, [rsp+SP_M]
    imul rax, r9
    add rax, r13
    mov r9, [rsp+SP_W]
    movzx eax, byte [r9+rax-1]
    test eax, eax
    jz .bt2
    add eax, 256
    add eax, [r8-4]
    cmp eax, edx
    jne .bt2
    mov rax, [rsp+SP_MT]
    lea ecx, [r13-1]
    mov [rax+r12*4-4], ecx
    dec r12d
    dec r13d
    jmp .bt
.bt2:
    cmp edx, [r8]
    jne .bt3
    dec r12d
    jmp .bt
.bt3:
    dec r13d
    jmp .bt
.btd:
    ; the pieces: the lines before a run of pairs, then the run
    xor r12d, r12d                   ; left line
    xor r13d, r13d                   ; right line
    mov r14, [rsp+SP_MT]
    mov r15d, [rsp+SP_N]
    mov ebp, [rsp+SP_M]
.em:
    mov esi, r12d
.eu:
    cmp r12d, r15d
    jae .eu1
    cmp dword [r14+r12*4], -1
    jne .eu1
    inc r12d
    jmp .eu
.eu1:
    mov edi, ebp                     ; the right line of the next pair (or the end)
    cmp r12d, r15d
    jae .eu2
    mov edi, [r14+r12*4]
.eu2:
    mov eax, r12d
    sub eax, esi
    mov ecx, edi
    sub ecx, r13d
    mov [rsp+SP_UA], eax
    mov [rsp+SP_UA+4], ecx
    or eax, ecx
    jz .eu3
    mov ebx, [rsp+SP_A0]
    add ebx, esi
    mov esi, [rsp+SP_B0]
    add esi, r13d
    SP_EMIT ebx, [rsp+SP_UA], esi, [rsp+SP_UA+4]
.eu3:
    mov r13d, edi
    cmp r12d, r15d
    jae .emd
    mov esi, r12d
    mov edi, r13d
.er:
    inc r12d
    inc r13d
    cmp r12d, r15d
    jae .er1
    cmp [r14+r12*4], r13d
    je .er
.er1:
    mov eax, r12d
    sub eax, esi
    mov [rsp+SP_UA], eax
    add esi, [rsp+SP_A0]
    add edi, [rsp+SP_B0]
    SP_EMIT esi, [rsp+SP_UA], edi, [rsp+SP_UA]
    jmp .em
.emd:
    invoke free, [rsp+SP_TOK]
    invoke free, [rsp+SP_IDX]
    invoke free, [rsp+SP_W]
    invoke free, [rsp+SP_DP]
    invoke free, [rsp+SP_MT]
    EPILOG 224

; tv_split_hunks(tv, LA, LB, hunk vec): cut the changed hunks into the pieces that belong together
tv_split_hunks:
    PROLOG 96
    mov r12, rdx
    mov r13, r8
    mov r14, r9
    mov rax, [rcx+TV_OPTS]
    mov [sp_fl], rax
    xor eax, eax
    mov [rsp+64], rax
    mov [rsp+72], rax
    mov [rsp+80], rax
    xor ebx, ebx
.l:
    cmp rbx, [r14+8]
    jae .d
    lea rsi, [rbx+rbx*2]
    shl rsi, 3
    add rsi, [r14]
    cmp dword [rsi], 0
    je .copy
    mov eax, [rsi+8]
    mov ecx, [rsi+16]
    test eax, eax
    jz .copy
    test ecx, ecx
    jz .copy
    mov edx, eax
    or edx, ecx
    cmp edx, 1                       ; one line against one: nothing to cut
    je .copy
    imul rax, rcx
    cmp rax, SP_MAX
    ja .copy
    invoke sp_one, r12, r13, rsi, &rsp+64
    jmp .n
.copy:
    invoke vec_push, &rsp+64, 24
    movdqu xmm0, [rsi]
    movdqu [rax], xmm0
    mov rcx, [rsi+16]
    mov [rax+16], rcx
.n:
    inc rbx
    jmp .l
.d:
    invoke free, [r14]
    mov rax, [rsp+64]
    mov [r14], rax
    mov rax, [rsp+72]
    mov [r14+8], rax
    mov rax, [rsp+80]
    mov [r14+16], rax
    EPILOG 96
