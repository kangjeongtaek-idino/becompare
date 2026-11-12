; textlib.asm - text file I/O, line splitting, key normalisation and the LCS diff engine
%include "inc/macros.inc"
%include "inc/win.inc"

extern malloc, calloc, realloc, free, memcpy, memset, wcslen, wcscmp, wcsstr
extern MultiByteToWideChar, WideCharToMultiByte, CreateFileW, WriteFile, CloseHandle
extern safe_write2, tl_convert, tl_isconv, DeleteFileW
extern read_file_bytes, write_utf8, vec_push, lower_w

global g_tc_rules, text_load, text_save, text_load_raw, lines_split, compare_lines, hunk_free, keys_build
; TXT { wchar* text; qword len; dd enc; dd crlf; dd eol }
; LINES { wchar** ptrs; qword count }
; hunk: { dd type(0 equal,1 change); dd aStart; dd aLen; dd bStart; dd bLen; dd pad } = 24 bytes

%define LCS_LIMIT 30000000

section .text

; ---------------------------------------------------------------- loading
; text_load(path, TXT*) -> eax 1 ok
; text_load(path, TXT*) -> eax 1 ok: Word / RTF / PDF and [Convert] rules are rendered to text first
text_load:
    PROLOG 1200
    mov rsi, rcx
    mov rdi, rdx
    invoke tl_convert, rsi, &rsp+96
    test eax, eax
    jz .plain
    invoke text_load_raw, &rsp+96, rdi
    mov ebx, eax
    invoke DeleteFileW, &rsp+96
    mov eax, ebx
    EPILOG 1200
.plain:
    invoke text_load_raw, rsi, rdi
    EPILOG 1200

text_load_raw:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    invoke read_file_bytes, r12, &rsp+96
    test rax, rax
    jz .fail
    mov rbx, rax                     ; bytes
    mov rsi, [rsp+96]                ; size
    xor edi, edi                     ; start offset
    xor r14d, r14d                   ; enc
    cmp rsi, 3
    jb .bom16
    cmp byte [rbx], 0xEF
    jne .bom16
    cmp byte [rbx+1], 0xBB
    jne .bom16
    cmp byte [rbx+2], 0xBF
    jne .bom16
    mov r14d, 2
    mov edi, 3
    jmp .conv
.bom16:
    cmp rsi, 2
    jb .probe
    cmp byte [rbx], 0xFF
    jne .probe
    cmp byte [rbx+1], 0xFE
    jne .probe
    mov r14d, 3
    mov edi, 2
    jmp .conv
.probe:
    mov r14d, 1
    test rsi, rsi
    jz .conv
    invoke MultiByteToWideChar, CP_UTF8, MB_ERR_INVALID_CHARS, rbx, rsi, 0, 0
    test eax, eax
    jnz .conv
    xor r14d, r14d                   ; not valid UTF-8 -> ANSI code page
.conv:
    cmp r14d, 3
    je .utf16
    ; multibyte -> wide
    mov ecx, CP_UTF8
    test r14d, r14d
    jnz .cp
    xor ecx, ecx
.cp:
    mov r15d, ecx
    lea rdx, [rbx+rdi]
    mov r8, rsi
    sub r8, rdi
    mov [rsp+104], rdx
    mov [rsp+112], r8
    invoke MultiByteToWideChar, r15, 0, [rsp+104], [rsp+112], 0, 0
    mov [rsp+120], rax               ; wide length
    lea rcx, [rax*2+4]
    call malloc
    mov rbp, rax
    test rax, rax
    jz .fail2
    mov rax, [rsp+120]
    test rax, rax
    jz .term
    invoke MultiByteToWideChar, r15, 0, [rsp+104], [rsp+112], rbp, [rsp+120]
.term:
    mov rax, [rsp+120]
    mov word [rbp+rax*2], 0
    jmp .scan
.utf16:
    mov rax, rsi
    sub rax, rdi
    shr rax, 1
    mov [rsp+120], rax
    lea rcx, [rax*2+4]
    call malloc
    mov rbp, rax
    test rax, rax
    jz .fail2
    mov rax, [rsp+120]
    lea r8, [rax*2]
    lea rdx, [rbx+rdi]
    invoke memcpy, rbp, rdx, r8
    mov rax, [rsp+120]
    mov word [rbp+rax*2], 0
.scan:
    ; CRLF style: look at the first line break
    mov r8d, 1                       ; default CRLF
    xor ecx, ecx
    mov rdx, [rsp+120]
.sl:
    cmp rcx, rdx
    jae .sdone
    movzx eax, word [rbp+rcx*2]
    cmp eax, 10
    je .lf
    cmp eax, 13
    je .cr
    inc rcx
    jmp .sl
.lf:
    xor r8d, r8d                     ; bare LF
    jmp .sdone
.cr:
    mov r8d, 1
.sdone:
    xor r9d, r9d                     ; ends with a line break?
    test rdx, rdx
    jz .st
    movzx eax, word [rbp+rdx*2-2]
    cmp eax, 10
    je .e1
    cmp eax, 13
    jne .st
.e1:
    mov r9d, 1
.st:
    mov [r13], rbp
    mov rax, [rsp+120]
    mov [r13+8], rax
    mov [r13+16], r14d
    mov [r13+20], r8d
    mov [r13+24], r9d
    invoke free, rbx
    mov eax, 1
    EPILOG 160
.fail2:
    invoke free, rbx
.fail:
    xor eax, eax
    EPILOG 160

; ---------------------------------------------------------------- saving
; wr2(path, p1, n1, p2, n2): create file, write two blocks
wr2:
    jmp safe_write2

; text_save(path, text, len, enc | crlf<<8) -> eax 1 ok
;   text uses \r, \n or \r\n as line breaks; output uses the chosen EOL
; text_save(path, text, len, enc | crlf<<8) -> eax 1 ok; refuses files that are only readable through a converter
text_save:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r12, r8
    mov r13, r9
    invoke tl_isconv, rbx
    test eax, eax
    jnz .refuse
    invoke text_save_raw, rbx, rsi, r12, r13
    EPILOG 64
.refuse:
    xor eax, eax
    EPILOG 64

text_save_raw:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15d, r9d
    lea rcx, [r14*2+8]
    add rcx, r14
    add rcx, r14
    call malloc                      ; wide output buffer (len*3+4 chars at most)
    mov rsi, rax
    test rax, rax
    jz .fail
    xor ebx, ebx                     ; input index
    xor edi, edi                     ; output length
    mov r10d, r15d
    shr r10d, 8                      ; crlf flag
.l:
    cmp rbx, r14
    jae .ld
    movzx eax, word [r13+rbx*2]
    inc rbx
    cmp eax, 13
    je .brk
    cmp eax, 10
    je .brk
    mov [rsi+rdi*2], ax
    inc rdi
    jmp .l
.brk:
    cmp eax, 13
    jne .emit
    cmp rbx, r14
    jae .emit
    cmp word [r13+rbx*2], 10
    jne .emit
    inc rbx
.emit:
    test r10d, r10d
    jz .lf
    mov word [rsi+rdi*2], 13
    inc rdi
.lf:
    mov word [rsi+rdi*2], 10
    inc rdi
    jmp .l
.ld:
    mov word [rsi+rdi*2], 0
    movzx eax, r15b                  ; encoding
    cmp eax, 3
    je .u16
    cmp eax, 1
    je .u8
    cmp eax, 2
    je .u8bom
    ; ANSI code page
    invoke WideCharToMultiByte, CP_ACP, 0, rsi, rdi, 0, 0, 0, 0
    mov rbp, rax
    lea rcx, [rbp+4]
    call malloc
    mov r13, rax
    test rax, rax
    jz .convfail
    test rdi, rdi
    jz .ansiwrite
    invoke WideCharToMultiByte, CP_ACP, 0, rsi, rdi, r13, rbp, 0, 0
    test eax, eax
    jz .ansifail
.ansiwrite:
    invoke wr2, r12, 0, 0, r13, rbp
    mov ebx, eax
    invoke free, r13
    jmp .done
 .ansifail:
    invoke free, r13
.convfail:
    xor ebx, ebx
    jmp .done
.u8:
    invoke write_utf8, r12, rsi, rdi, 0
    mov ebx, eax
    jmp .done
.u8bom:
    invoke write_utf8, r12, rsi, rdi, 1
    mov ebx, eax
    jmp .done
.u16:
    mov word [rsp+96], 0xFEFF
    lea rax, [rdi*2]
    invoke wr2, r12, &rsp+96, 2, rsi, rax
    mov ebx, eax
.done:
    invoke free, rsi
    mov eax, ebx
    EPILOG 192
.fail:
    xor eax, eax
    EPILOG 192

; ---------------------------------------------------------------- lines
; lines_split(text (modified in place), len, LINES*) : \r\n, \r or \n separate lines
lines_split:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    xor ecx, ecx
    xor eax, eax
.cnt:
    cmp rcx, rdi
    jae .cd
    movzx edx, word [rsi+rcx*2]
    cmp edx, 10
    je .cn
    cmp edx, 13
    jne .cx
.cn:
    inc rax
.cx:
    inc rcx
    jmp .cnt
.cd:
    lea rcx, [rax*8+16]
    call malloc
    mov rbx, rax
    mov [r12], rbx
    xor r8d, r8d                     ; line count
    xor ecx, ecx                     ; index
    xor r9d, r9d                     ; start index of the current line
    test rdi, rdi
    jz .fin
.l:
    cmp rcx, rdi
    jae .last
    movzx edx, word [rsi+rcx*2]
    cmp edx, 10
    je .brk
    cmp edx, 13
    je .brk
    inc rcx
    jmp .l
.brk:
    lea rax, [rsi+r9*2]
    mov [rbx+r8*8], rax
    inc r8
    cmp edx, 13
    jne .term
    lea r10, [rcx+1]
    cmp r10, rdi
    jae .term
    cmp word [rsi+r10*2], 10
    jne .term
    mov word [rsi+rcx*2], 0
    inc rcx                          ; skip the LF of a CRLF pair
    jmp .after
.term:
    mov word [rsi+rcx*2], 0
.after:
    inc rcx
    mov r9, rcx
    jmp .l
.last:
    cmp r9, rdi
    jae .fin                         ; text ended with a line break: no extra line
    lea rax, [rsi+r9*2]
    mov [rbx+r8*8], rax
    inc r8
.fin:
    mov [r12+8], r8
    EPILOG 64

; ---------------------------------------------------------------- keys
; key_copy(src, dst, flags, cprefix) -> eax = length written (dst terminated)
;   flags: 1 ignore case, 2 ignore all white space, 4 trim, 8 cut at comment prefix
key_copy:
    PROLOG 96
    mov rsi, rcx
    mov rdi, rdx
    mov r12d, r8d
    mov r13, r9
    invoke wcslen, rsi
    lea r14, [rsi+rax*2]             ; end
    test r12d, 8
    jz .nocomment
    test r13, r13
    jz .nocomment
    cmp word [r13], 0
    je .nocomment
    invoke wcsstr, rsi, r13
    test rax, rax
    jz .nocomment
    mov r14, rax
.nocomment:
    test r12d, 4
    jz .copy
.ltrim:
    cmp rsi, r14
    jae .copy
    movzx eax, word [rsi]
    cmp eax, ' '
    je .lt
    cmp eax, 9
    jne .rtrim
.lt:
    add rsi, 2
    jmp .ltrim
.rtrim:
    cmp r14, rsi
    jbe .copy
    movzx eax, word [r14-2]
    cmp eax, ' '
    je .rt
    cmp eax, 9
    jne .copy
.rt:
    sub r14, 2
    jmp .rtrim
.copy:
    mov rbx, rdi
    xor r15d, r15d                   ; inside an identifier (masked comparison)
.l:
    cmp rsi, r14
    jae .done
    movzx ecx, word [rsi]
    add rsi, 2
    test r12d, 2
    jz .case
    cmp ecx, ' '
    je .l
    cmp ecx, 9
    je .l
    cmp ecx, 0x3000
    je .l
.case:
    test r12d, 16
    jz .nomask
    cmp ecx, '_'
    je .isid
    cmp ecx, 0x80
    jae .isid
    mov eax, ecx
    or eax, 0x20
    cmp eax, 'a'
    jb .noid
    cmp eax, 'z'
    jbe .isid
.noid:
    cmp ecx, '0'
    jb .idreset
    cmp ecx, '9'
    ja .idreset
    test r15d, r15d
    jnz .l                           ; digits inside an identifier
    jmp .nomask
.idreset:
    xor r15d, r15d
    jmp .nomask
.isid:
    test r15d, r15d
    jnz .l
    mov r15d, 1
    mov ecx, 1                       ; one placeholder for the whole identifier
    jmp .st
.nomask:
    test r12d, 1
    jz .st
    call lower_w
    mov ecx, eax
.st:
    mov [rdi], cx
    add rdi, 2
    jmp .l
.done:
    mov word [rdi], 0
    mov rax, rdi
    sub rax, rbx
    shr rax, 1
    EPILOG 96

; keys_build(LINES*, flags, cprefix, hashes*) -> keys array; keys[n] holds the blob to free (or 0)
; rules_apply(src, dst, cap chars) : replace every "old" of g_tc_rules ("old=new;old=new") in src
rules_apply:
    PROLOG 96
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    xor r13d, r13d                   ; written chars
.l:
    movzx eax, word [rsi]
    test eax, eax
    jz .d
    cmp r13, r12
    jae .d
    mov r14, [g_tc_rules]
.rule:
    cmp word [r14], 0
    je .plain
    ; one rule: old up to '=', new up to ';'
    mov r8, r14
.ol:
    movzx eax, word [r8]
    test eax, eax
    jz .plain
    cmp eax, '='
    je .gotold
    add r8, 2
    jmp .ol
.gotold:
    mov r9, r8
    sub r9, r14
    shr r9, 1                        ; old length
    lea r10, [r8+2]                  ; new text
    mov r11, r10
.nl:
    movzx eax, word [r11]
    test eax, eax
    jz .gotnew
    cmp eax, ';'
    je .gotnew
    add r11, 2
    jmp .nl
.gotnew:
    mov r15, r11
    sub r15, r10
    shr r15, 1                       ; new length
    ; compare old with src
    test r9, r9
    jz .nextrule
    xor ecx, ecx
.cmp:
    cmp rcx, r9
    jae .match
    movzx eax, word [r14+rcx*2]
    cmp ax, [rsi+rcx*2]
    jne .nextrule
    inc rcx
    jmp .cmp
.match:
    lea rax, [r13+r15]
    cmp rax, r12
    ja .plain
    xor ecx, ecx
.cn:
    cmp rcx, r15
    jae .cnd
    movzx eax, word [r10+rcx*2]
    mov [rdi+r13*2], ax
    inc r13
    inc rcx
    jmp .cn
.cnd:
    lea rsi, [rsi+r9*2]
    jmp .l
.nextrule:
    movzx eax, word [r11]
    test eax, eax
    jz .plain
    lea r14, [r11+2]
    jmp .rule
.plain:
    movzx eax, word [rsi]
    mov [rdi+r13*2], ax
    inc r13
    add rsi, 2
    jmp .l
.copyrest:
    movzx eax, word [rsi]
    test eax, eax
    jz .d
    mov [rdi+r13*2], ax
    inc r13
    add rsi, 2
    jmp .copyrest
.d:
    mov word [rdi+r13*2], 0
    mov rax, r13
    EPILOG 96

keys_build:
    PROLOG 128
    mov qword [rsp+112], 0
    mov rsi, rcx
    mov r12d, edx
    mov r13, r8
    mov r14, r9
    mov rbx, [rsi+8]                 ; n
    lea rcx, [rbx*8+16]
    call malloc
    mov r15, rax
    mov qword [r15+rbx*8], 0
    xor ebp, ebp
    ; blob for normalised copies (only when needed)
    test r12d, r12d
    jz .nodata
    ; total chars
    xor edi, edi
    xor ebp, ebp
.tot:
    cmp rbp, rbx
    jae .alloc
    mov rax, [rsi]
    invoke wcslen, [rax+rbp*8]
    lea rdi, [rdi+rax+1]
    inc rbp
    jmp .tot
.alloc:
    lea rcx, [rdi*2+8]
    test r12d, 32
    jz .blob
    mov rcx, rdi                     ; substitution rules may lengthen lines
    shl rcx, 4
    add rcx, 16
.blob:
    call malloc
    mov [r15+rbx*8], rax
    mov [rsp+104], rax               ; cursor
    mov qword [rsp+112], 0
    test r12d, 32
    jz .nodata
    mov ecx, 4096*2
    call malloc
    mov [rsp+112], rax               ; scratch line for the substituted text
.nodata:
    xor ebp, ebp
.l:
    cmp rbp, rbx
    jae .done
    mov rax, [rsi]
    mov rdi, [rax+rbp*8]             ; original line
    test r12d, r12d
    jnz .norm
    mov [r15+rbp*8], rdi
    mov rcx, rdi
    jmp .hash
.norm:
    mov rdx, [rsp+104]
    mov [r15+rbp*8], rdx
    test r12d, 32
    jz .nosub
    invoke wcslen, rdi
    lea r8, [rax*8+8]
    cmp r8, 4000
    jbe .capok
    mov r8d, 4000
.capok:
    mov [rsp+120], r8
    invoke rules_apply, rdi, [rsp+112], [rsp+120]
    mov rdi, [rsp+112]
    mov rdx, [rsp+104]
.nosub:
    invoke key_copy, rdi, rdx, r12, r13
    mov rcx, [rsp+104]
    lea rdx, [rcx+rax*2+2]
    mov [rsp+104], rdx
.hash:
    mov rcx, [r15+rbp*8]
    mov eax, 2166136261
.h:
    movzx edx, word [rcx]
    test edx, edx
    jz .hd
    xor eax, edx
    imul eax, eax, 16777619
    add rcx, 2
    jmp .h
.hd:
    mov [r14+rbp*4], eax
    inc rbp
    jmp .l
.done:
    invoke free, [rsp+112]
    mov rax, r15
    EPILOG 128

; ---------------------------------------------------------------- diff
; DIFFIN: 0 keysA, 8 hashA, 16 nA, 24 keysB, 32 hashB, 40 nB, 48 vec* hunks

; keq(din, i, j) -> eax 1 if key A[i] == key B[j]
keq:
    PROLOG 64
    mov rax, [rcx+8]
    mov eax, [rax+rdx*4]
    mov rsi, [rcx+32]
    cmp eax, [rsi+r8*4]
    jne .no
    mov rax, [rcx]
    mov r10, [rax+rdx*8]
    mov rax, [rcx+24]
    mov r11, [rax+r8*8]
    invoke wcscmp, r10, r11
    test eax, eax
    jnz .no
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; run_flush(run*{kind,a,la,b,lb}, vec*)
run_flush:
    PROLOG 64
    mov rsi, rcx
    cmp dword [rsi], 0
    je .d
    invoke vec_push, rdx, 24
    xor ecx, ecx
    cmp dword [rsi], 1
    je .eq
    mov ecx, 1
.eq:
    mov [rax], ecx
    mov ecx, [rsi+4]
    mov [rax+4], ecx
    mov ecx, [rsi+8]
    mov [rax+8], ecx
    mov ecx, [rsi+12]
    mov [rax+12], ecx
    mov ecx, [rsi+16]
    mov [rax+16], ecx
    mov dword [rax+20], 0
    mov dword [rsi], 0
.d:
    EPILOG 64

%include "myers.inc"

%macro RUNSET 1
    cmp dword [rsp+L_RUN], %1
    je %%ok
    invoke run_flush, &rsp+L_RUN, [r12+48]
    mov dword [rsp+L_RUN], %1
    lea eax, [r13+rbx]
    mov [rsp+L_RUN+4], eax
    mov dword [rsp+L_RUN+8], 0
    lea eax, [r13+rbp]
    mov [rsp+L_RUN+12], eax
    mov dword [rsp+L_RUN+16], 0
%%ok:
%endmacro

%define L_N1   96
%define L_M1   104
%define L_S    112
%define L_RUN  120                  ; 20 bytes -> 140
%define L_TAB  144
%define L_W    152
%define L_OPS  160
%define L_OPN  168
%define L_OPI  176
%define L_PIECE 184
%define MY_PIECES 128                ; pieces of MY_DMAX edits before the rest is reported as one block

; diff_run(DIFFIN*): fills the hunk vector
diff_run:
    PROLOG 192
    mov r12, rcx
    xor r13d, r13d                   ; common prefix p
.pre:
    cmp r13, [r12+16]
    jae .pd
    cmp r13, [r12+40]
    jae .pd
    invoke keq, r12, r13, r13
    test eax, eax
    jz .pd
    inc r13
    jmp .pre
.pd:
    xor r14d, r14d                   ; common suffix s
.suf:
    mov rax, [r12+16]
    sub rax, r13
    cmp r14, rax
    jae .sd
    mov rax, [r12+40]
    sub rax, r13
    cmp r14, rax
    jae .sd
    mov rdx, [r12+16]
    sub rdx, r14
    dec rdx
    mov r8, [r12+40]
    sub r8, r14
    dec r8
    invoke keq, r12, rdx, r8
    test eax, eax
    jz .sd
    inc r14
    jmp .suf
.sd:
    mov [rsp+L_S], r14
    mov rax, [r12+16]
    sub rax, r13
    sub rax, r14
    mov [rsp+L_N1], rax
    mov rax, [r12+40]
    sub rax, r13
    sub rax, r14
    mov [rsp+L_M1], rax
    mov dword [rsp+L_RUN], 0
    ; leading equal run
    test r13, r13
    jz .middle
    mov dword [rsp+L_RUN], 1
    mov dword [rsp+L_RUN+4], 0
    mov [rsp+L_RUN+8], r13d
    mov dword [rsp+L_RUN+12], 0
    mov [rsp+L_RUN+16], r13d
    invoke run_flush, &rsp+L_RUN, [r12+48]
.middle:
    mov rax, [rsp+L_N1]
    mov rcx, [rsp+L_M1]
    or rax, rcx
    jz .tail
    cmp qword [rsp+L_N1], 0
    je .whole
    cmp qword [rsp+L_M1], 0
    je .whole
    mov rax, [rsp+L_N1]
    inc rax
    mov rcx, [rsp+L_M1]
    inc rcx
    mov [rsp+L_W], rcx
    mul rcx
    test rdx, rdx
    jnz .whole
    cmp rax, LCS_LIMIT
    ja .whole
    lea rcx, [rax*4]
    call malloc
    test rax, rax
    jz .whole
    mov [rsp+L_TAB], rax
    ; ---- fill the table (suffix form)
    mov rsi, [rsp+L_TAB]
    mov rdi, [rsp+L_W]
    mov rbx, [rsp+L_N1]              ; i
.irow:
    mov rbp, [rsp+L_M1]              ; j
.jcol:
    mov rax, rbx
    imul rax, rdi
    add rax, rbp
    cmp rbx, [rsp+L_N1]
    je .zero
    cmp rbp, [rsp+L_M1]
    je .zero
    lea rdx, [r13+rbx]
    lea r8, [r13+rbp]
    mov rcx, [r12+8]
    mov ecx, [rcx+rdx*4]
    mov r9, [r12+32]
    cmp ecx, [r9+r8*4]
    jne .neq
    invoke keq, r12, rdx, r8
    mov ecx, eax
    mov rax, rbx
    imul rax, rdi
    add rax, rbp
    test ecx, ecx
    jz .neq
    lea rdx, [rax+rdi+1]
    mov ecx, [rsi+rdx*4]
    inc ecx
    mov [rsi+rax*4], ecx
    jmp .jn
.neq:
    lea rdx, [rax+rdi]
    mov ecx, [rsi+rdx*4]
    mov r8d, [rsi+rax*4+4]
    cmp ecx, r8d
    jae .mx
    mov ecx, r8d
.mx:
    mov [rsi+rax*4], ecx
    jmp .jn
.zero:
    mov dword [rsi+rax*4], 0
.jn:
    test rbp, rbp
    jz .rowdone
    dec rbp
    jmp .jcol
.rowdone:
    test rbx, rbx
    jz .walk0
    dec rbx
    jmp .irow
    ; ---- walk the table forward, grouping runs
.walk0:
    xor ebx, ebx
    xor ebp, ebp
.walk:
    cmp rbx, [rsp+L_N1]
    jb .ihas
    cmp rbp, [rsp+L_M1]
    jae .wend
    jmp .ins
.ihas:
    cmp rbp, [rsp+L_M1]
    jae .del
    lea rdx, [r13+rbx]
    lea r8, [r13+rbp]
    invoke keq, r12, rdx, r8
    test eax, eax
    jnz .com
    mov rax, rbx
    inc rax
    imul rax, rdi
    add rax, rbp
    mov ecx, [rsi+rax*4]             ; L[i+1][j]
    mov rax, rbx
    imul rax, rdi
    add rax, rbp
    inc rax
    mov edx, [rsi+rax*4]             ; L[i][j+1]
    cmp ecx, edx
    jb .ins
.del:
    RUNSET 2
    inc dword [rsp+L_RUN+8]
    inc rbx
    jmp .walk
.ins:
    RUNSET 2
    inc dword [rsp+L_RUN+16]
    inc rbp
    jmp .walk
.com:
    RUNSET 1
    inc dword [rsp+L_RUN+8]
    inc dword [rsp+L_RUN+16]
    inc rbx
    inc rbp
    jmp .walk
.wend:
    invoke run_flush, &rsp+L_RUN, [r12+48]
    invoke free, [rsp+L_TAB]
    jmp .tail
.whole:
    xor ebx, ebx                     ; i
    xor ebp, ebp                     ; j
    mov dword [rsp+L_PIECE], 0
.piece:
    ; Myers over what is left; a result that stops short (more than MY_DMAX edits) is continued from there
    mov rdi, [rsp+L_N1]
    sub rdi, rbx
    jz .rest
    mov r14, [rsp+L_M1]
    sub r14, rbp
    jz .rest
    cmp dword [rsp+L_PIECE], MY_PIECES
    jae .rest
    inc dword [rsp+L_PIECE]
    lea rsi, [r13+rbx]
    lea r15, [r13+rbp]
    invoke myers_ops, r12, rsi, rdi, r14, r15
    test rax, rax
    jz .rest
    mov [rsp+L_OPS], rax
    mov [rsp+L_OPI], rdx
.rp:
    mov rax, [rsp+L_OPI]
    test rax, rax
    jz .rpend
    dec rax
    mov [rsp+L_OPI], rax
    mov rcx, [rsp+L_OPS]
    movzx eax, byte [rcx+rax]
    cmp eax, 0
    je .rcom
    cmp eax, 1
    je .rdel
    RUNSET 2
    inc dword [rsp+L_RUN+16]
    inc rbp
    jmp .rp
.rdel:
    RUNSET 2
    inc dword [rsp+L_RUN+8]
    inc rbx
    jmp .rp
.rcom:
    RUNSET 1
    inc dword [rsp+L_RUN+8]
    inc dword [rsp+L_RUN+16]
    inc rbx
    inc rbp
    jmp .rp
.rpend:
    invoke free, [rsp+L_OPS]
    jmp .piece
.rest:
    ; what was not compared (one side used up, no memory, or too many pieces) is one changed block
    mov rdi, [rsp+L_N1]
    sub rdi, rbx
    mov r14, [rsp+L_M1]
    sub r14, rbp
    mov rax, rdi
    or rax, r14
    jz .restd
    RUNSET 2
    add [rsp+L_RUN+8], edi
    add [rsp+L_RUN+16], r14d
.restd:
    invoke run_flush, &rsp+L_RUN, [r12+48]
.tail:
    mov rax, [rsp+L_S]
    test rax, rax
    jz .done
    mov dword [rsp+L_RUN], 1
    mov rcx, [r12+16]
    sub rcx, rax
    mov [rsp+L_RUN+4], ecx
    mov [rsp+L_RUN+8], eax
    mov rcx, [r12+40]
    sub rcx, rax
    mov [rsp+L_RUN+12], ecx
    mov [rsp+L_RUN+16], eax
    invoke run_flush, &rsp+L_RUN, [r12+48]
.done:
    EPILOG 192

; compare_lines(LINES* A, LINES* B, flags, cprefix, vec* hunks)  (flags in low dword; cprefix as 4th)
;   5th argument: hunks vector pointer on the stack
compare_lines:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    mov r14d, r8d
    mov r15, r9
    mov rbp, [rsp+192+112]           ; hunks vec*
    mov rbx, [r12+8]                 ; nA
    mov rsi, [r13+8]                 ; nB
    lea rcx, [rbx*4+8]
    call malloc
    mov [rsp+96], rax                ; hashA
    lea rcx, [rsi*4+8]
    call malloc
    mov [rsp+104], rax               ; hashB
    invoke keys_build, r12, r14, r15, [rsp+96]
    mov [rsp+112], rax               ; keysA
    invoke keys_build, r13, r14, r15, [rsp+104]
    mov [rsp+120], rax               ; keysB
    ; DIFFIN at [rsp+128..184]
    mov rax, [rsp+112]
    mov [rsp+128], rax
    mov rax, [rsp+96]
    mov [rsp+136], rax
    mov [rsp+144], rbx
    mov rax, [rsp+120]
    mov [rsp+152], rax
    mov rax, [rsp+104]
    mov [rsp+160], rax
    mov [rsp+168], rsi
    mov [rsp+176], rbp
    invoke diff_run, &rsp+128
    ; free temporaries
    mov rax, [rsp+112]
    invoke free, [rax+rbx*8]
    invoke free, [rsp+112]
    mov rax, [rsp+120]
    invoke free, [rax+rsi*8]
    invoke free, [rsp+120]
    invoke free, [rsp+96]
    invoke free, [rsp+104]
    EPILOG 192

; hunk_free(vec*)
hunk_free:
    PROLOG 64
    mov rbx, rcx
    invoke free, [rbx]
    mov qword [rbx], 0
    mov qword [rbx+8], 0
    mov qword [rbx+16], 0
    EPILOG 64

section .bss
alignb 8
g_tc_rules: resq 1
section .text
