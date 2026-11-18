; conv.asm - TC-05 / IN-04: turn Word (.docx), RTF and PDF files into text before comparing, and run user conversion rules
;   [Convert] .ext=command  in settings.ini:  %1 = input file, %2 = output text file
%include "inc/macros.inc"
%include "inc/win.inc"

extern GetTempPathW, GetTickCount, CreateProcessW, WaitForSingleObject, GetExitCodeProcess, CloseHandle
extern GetPrivateProfileStringW, GetFileAttributesW, MultiByteToWideChar, strstr
extern malloc, calloc, free, memset, memcpy, wcslen, wcscat, wcsrchr, _wcsicmp, _snwprintf
extern zip_open, zip_extract, vt_free, inflate_buf, read_file_bytes, write_utf8, g_ini

global tl_convert, tl_isconv

%define XV_VE     0
%define XV_N      8
%define XV_STRUCT 48
%define XE_NAME   0
%define XE_IMPL   52
%define XE_BYTES  56

section .rdata
WSTR sec_convert, "Convert"
WSTR s_empty, ""
WSTR e_docx, ".docx"
WSTR e_docm, ".docm"
WSTR e_rtf, ".rtf"
WSTR e_pdf, ".pdf"
WSTR f_tmpname, "bcconv_%X.txt"
WSTR n_docxml, "word\document.xml"
WSTR t_wt, "w:t"
WSTR t_cwp, "/w:p"
WSTR t_tab, "w:tab"
WSTR t_br, "w:br"
WSTR t_cr, "w:cr"
k_flate: db "/FlateDecode", 0
k_endstream: db "endstream", 0
k_stream: db "stream", 0
k_image: db "/Image", 0
k_fontfile: db "/FontFile", 0
k_subtype: db "/Subtype", 0
k_type: db "/Type", 0
k_obj: db "obj", 0
k_length1: db "/Length1", 0
k_xref: db "/XRef", 0
k_objstm: db "/ObjStm", 0
k_meta: db "/Metadata", 0

section .text

; cv_tmp(out): "%TEMP%\bcconv_<tick>.txt"
cv_tmp:
    PROLOG 64
    mov rbx, rcx
    invoke GetTempPathW, 400, rbx
    invoke wcslen, rbx
    lea rsi, [rbx+rax*2]
    invoke GetTickCount
    invoke _snwprintf, rsi, 40, &f_tmpname, rax
    EPILOG 64

; cv_name_eq(q = after '<', name) -> eax 1 when the tag name equals `name`
cv_name_eq:
    xor eax, eax
.l:
    movzx r8d, word [rdx+rax*2]
    test r8d, r8d
    jz .end
    cmp r8w, [rcx+rax*2]
    jne .no
    inc rax
    jmp .l
.end:
    movzx r8d, word [rcx+rax*2]
    cmp r8d, '>'
    je .yes
    cmp r8d, ' '
    je .yes
    cmp r8d, '/'
    je .yes
    cmp r8d, 9
    je .yes
    cmp r8d, 13
    je .yes
    cmp r8d, 10
    je .yes
    test r8d, r8d
    jz .yes
.no:
    xor eax, eax
    ret
.yes:
    mov eax, 1
    ret

; cv_text(src after '>', dst) -> rax = src at '<' (or NUL), rdx = dst end ; entities decoded
cv_text:
.l:
    movzx eax, word [rcx]
    test eax, eax
    jz .d
    cmp eax, '<'
    je .d
    cmp eax, '&'
    jne .copy
    lea r8, [rcx+2]
    mov r9, [r8]
    mov r10, 'a'|('m'<<16)|('p'<<32)|(';'<<48)
    cmp r9, r10
    jne .e2
    mov eax, '&'
    lea rcx, [rcx+10]
    jmp .st
.e2:
    mov r11, r9
    shl r11, 16
    shr r11, 16
    mov r10, 'l'|('t'<<16)|(';'<<32)
    cmp r11, r10
    jne .e3
    mov eax, '<'
    lea rcx, [rcx+8]
    jmp .st
.e3:
    mov r10, 'g'|('t'<<16)|(';'<<32)
    cmp r11, r10
    jne .e4
    mov eax, '>'
    lea rcx, [rcx+8]
    jmp .st
.e4:
    mov r10, 'q'|('u'<<16)|('o'<<32)|('t'<<48)
    cmp r9, r10
    jne .e5
    cmp word [r8+8], ';'
    jne .e5
    mov eax, '"'
    lea rcx, [rcx+12]
    jmp .st
.e5:
    mov r10, 'a'|('p'<<16)|('o'<<32)|('s'<<48)
    cmp r9, r10
    jne .e6
    cmp word [r8+8], ';'
    jne .e6
    mov eax, 39
    lea rcx, [rcx+12]
    jmp .st
.e6:
    cmp word [r8], '#'
    jne .copy
    lea r8, [rcx+4]
    xor r9d, r9d
    mov r10d, 10
    cmp word [r8], 'x'
    jne .nd
    mov r10d, 16
    add r8, 2
.nd:
    movzx eax, word [r8]
    cmp eax, ';'
    je .ndone
    test eax, eax
    jz .bad
    cmp eax, '0'
    jb .bad
    cmp eax, '9'
    jbe .dg
    or eax, 0x20
    cmp eax, 'a'
    jb .bad
    cmp eax, 'f'
    ja .bad
    sub eax, 'a'-10
    jmp .acc
.dg:
    sub eax, '0'
.acc:
    imul r9d, r10d
    add r9d, eax
    add r8, 2
    jmp .nd
.bad:
    mov eax, '&'
    jmp .copy
.ndone:
    lea rcx, [r8+2]
    mov eax, r9d
    cmp eax, 0xFFFF
    jbe .st
    mov eax, '?'
    jmp .st
.copy:
    add rcx, 2
.st:
    mov [rdx], ax
    add rdx, 2
    jmp .l
.d:
    mov rax, rcx
    ret

; cv_wide(bytes, size, &outlen) -> malloc'd UTF-16 text (UTF-8 input, BOM skipped)
cv_wide:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    xor ebx, ebx
    cmp rdi, 3
    jb .go
    cmp byte [rsi], 0xEF
    jne .go
    cmp byte [rsi+1], 0xBB
    jne .go
    cmp byte [rsi+2], 0xBF
    jne .go
    mov ebx, 3
.go:
    lea rcx, [rdi*2+16]
    call malloc
    mov r13, rax
    test rax, rax
    jz .d
    lea r15, [rsi+rbx]
    mov rax, rdi
    sub rax, rbx
    lea r14, [rdi+4]
    invoke MultiByteToWideChar, 65001, 0, r15, rax, r13, r14
    test eax, eax
    jns .t
    xor eax, eax
.t:
    mov word [r13+rax*2], 0
    mov [r12], rax
.d:
    mov rax, r13
    EPILOG 64

; ---------------------------------------------------------------- Word
; cv_docx(path, out) -> eax 1
cv_docx:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    invoke memset, &rsp+96, 0, XV_STRUCT
    invoke zip_open, r12, &rsp+96
    test eax, eax
    jz .fail
    xor edi, edi
    xor ebx, ebx                      ; entry
.find:
    cmp rdi, [rsp+96+XV_N]
    jae .nofind
    mov rax, rdi
    imul rax, XE_BYTES
    add rax, [rsp+96+XV_VE]
    mov rbx, rax
    cmp dword [rbx+XE_IMPL], 0
    jne .n
    invoke _wcsicmp, [rbx+XE_NAME], &n_docxml
    test eax, eax
    jz .got
.n:
    inc rdi
    jmp .find
.nofind:
    invoke vt_free, &rsp+96
    jmp .fail
.got:
    invoke zip_extract, &rsp+96, rbx, r13
    mov r14d, eax
    invoke vt_free, &rsp+96
    test r14d, r14d
    jz .fail
    invoke read_file_bytes, r13, &rsp+160
    test rax, rax
    jz .fail
    mov r14, rax
    invoke cv_wide, r14, [rsp+160], &rsp+168
    mov r15, rax
    invoke free, r14
    test r15, r15
    jz .fail
    mov rcx, [rsp+168]
    lea rcx, [rcx*2+16]
    call malloc
    mov rbx, rax                      ; output text
    mov rdi, rax
    mov rsi, r15
.top:
    movzx eax, word [rsi]
    test eax, eax
    jz .done
    cmp eax, '<'
    je .tag
    add rsi, 2
    jmp .top
.tag:
    lea rcx, [rsi+2]
    lea rdx, [t_wt]
    call cv_name_eq
    test eax, eax
    jnz .wt
    lea rcx, [rsi+2]
    lea rdx, [t_cwp]
    call cv_name_eq
    test eax, eax
    jnz .nl
    lea rcx, [rsi+2]
    lea rdx, [t_br]
    call cv_name_eq
    test eax, eax
    jnz .nl
    lea rcx, [rsi+2]
    lea rdx, [t_cr]
    call cv_name_eq
    test eax, eax
    jnz .nl
    lea rcx, [rsi+2]
    lea rdx, [t_tab]
    call cv_name_eq
    test eax, eax
    jnz .tb
    add rsi, 2
    jmp .top
.nl:
    mov word [rdi], 10
    add rdi, 2
    add rsi, 2
    jmp .top
.tb:
    mov word [rdi], 9
    add rdi, 2
    add rsi, 2
    jmp .top
.wt:
    ; self-closing <w:t/> has no text
    lea rcx, [rsi+2]
.te:
    movzx eax, word [rcx]
    test eax, eax
    jz .done
    cmp eax, '>'
    je .tg
    add rcx, 2
    jmp .te
.tg:
    cmp word [rcx-2], '/'
    je .tskip
    add rcx, 2
    mov rdx, rdi
    call cv_text
    mov rsi, rax
    mov rdi, rdx
    jmp .top
.tskip:
    lea rsi, [rcx+2]
    jmp .top
.done:
    mov rax, rdi
    sub rax, rbx
    shr rax, 1
    invoke write_utf8, r13, rbx, rax, 0
    mov r14d, eax
    invoke free, rbx
    invoke free, r15
    mov eax, r14d
    EPILOG 192
.fail:
    xor eax, eax
    EPILOG 192

; ---------------------------------------------------------------- RTF
; cv_rtf(path, out) -> eax 1
cv_rtf:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    invoke read_file_bytes, r12, &rsp+96
    test rax, rax
    jz .fail
    mov r14, rax                      ; bytes
    mov r15, [rsp+96]                 ; size
    lea rcx, [r15*2+16]
    call malloc
    mov rbx, rax
    mov rdi, rax                      ; out
    xor esi, esi                      ; index
    xor ebp, ebp                      ; group depth
    mov qword [rsp+104], 0            ; skip depth (0 = none)
.l:
    cmp rsi, r15
    jae .done
    movzx eax, byte [r14+rsi]
    inc rsi
    cmp eax, '{'
    jne .nb
    inc ebp
    ; destination groups to skip: {\* ...} {\fonttbl ...} etc.
    cmp qword [rsp+104], 0
    jne .l
    cmp byte [r14+rsi], 0x5C
    jne .l
    movzx eax, byte [r14+rsi+1]
    cmp eax, '*'
    je .skipgrp
    cmp eax, 'f'
    jne .chk2
    cmp dword [r14+rsi+1], 'font'
    je .skipgrp
.chk2:
    cmp eax, 'c'
    jne .chk3
    cmp dword [r14+rsi+1], 'colo'
    je .skipgrp
.chk3:
    cmp eax, 's'
    jne .chk4
    cmp dword [r14+rsi+1], 'styl'
    je .skipgrp
.chk4:
    cmp eax, 'i'
    jne .chk5
    cmp dword [r14+rsi+1], 'info'
    je .skipgrp
.chk5:
    cmp eax, 'p'
    jne .l
    cmp dword [r14+rsi+1], 'pict'
    jne .l
.skipgrp:
    mov [rsp+104], rbp
    jmp .l
.nb:
    cmp eax, '}'
    jne .nc
    cmp qword [rsp+104], 0
    je .dec
    cmp rbp, [rsp+104]
    jne .dec
    mov qword [rsp+104], 0
.dec:
    test ebp, ebp
    jz .l
    dec ebp
    jmp .l
.nc:
    cmp qword [rsp+104], 0
    jne .l                            ; inside a skipped group
    cmp eax, 0x5C
    je .ctl
    cmp eax, 10
    je .l
    cmp eax, 13
    je .l
    ; plain character (ANSI)
    mov [rsp+112], al
    invoke MultiByteToWideChar, 0, 0, &rsp+112, 1, rdi, 1
    add rdi, 2
    jmp .l
.ctl:
    cmp rsi, r15
    jae .done
    movzx eax, byte [r14+rsi]
    cmp eax, 0x5C
    je .lit
    cmp eax, '{'
    je .lit
    cmp eax, '}'
    je .lit
    cmp eax, 39
    je .hex
    cmp eax, '~'
    je .nbsp
    ; control word: letters, optional number
    mov ecx, eax
    or ecx, 0x20
    cmp ecx, 'a'
    jb .sym
    cmp ecx, 'z'
    ja .sym
    ; word start in rsi
    lea r8, [r14+rsi]
    xor edx, edx
.wl:
    lea rax, [rsi+rdx]
    cmp rax, r15
    jae .we
    movzx ecx, byte [r14+rax]
    or ecx, 0x20
    cmp ecx, 'a'
    jb .we
    cmp ecx, 'z'
    ja .we
    inc edx
    jmp .wl
.we:
    add rsi, rdx                      ; after the letters
    ; number
    xor r9d, r9d                      ; value
    xor r10d, r10d                    ; negative
    xor r11d, r11d                    ; digits
    cmp rsi, r15
    jae .wdone
    cmp byte [r14+rsi], '-'
    jne .nd
    mov r10d, 1
    inc rsi
.nd:
    cmp rsi, r15
    jae .wdone
    movzx eax, byte [r14+rsi]
    sub eax, '0'
    cmp eax, 9
    ja .wdone
    imul r9d, r9d, 10
    add r9d, eax
    inc r11d
    inc rsi
    jmp .nd
.wdone:
    cmp rsi, r15
    jae .wnames
    cmp byte [r14+rsi], ' '
    jne .wnames
    inc rsi                           ; one delimiting space
.wnames:
    ; word compare: par / line -> newline, tab -> tab, u -> unicode
    cmp edx, 3
    jne .w4
    cmp word [r8], 'pa'
    jne .w4
    cmp byte [r8+2], 'r'
    je .nl
.w4:
    cmp edx, 4
    jne .w3
    cmp dword [r8], 'line'
    je .nl
.w3:
    cmp edx, 3
    jne .w1
    cmp word [r8], 'ta'
    jne .w1
    cmp byte [r8+2], 'b'
    je .tab
.w1:
    cmp edx, 1
    jne .l
    cmp byte [r8], 'u'
    jne .l
    test r11d, r11d
    jz .l
    test r10d, r10d
    jz .upos
    neg r9d
.upos:
    mov [rdi], r9w
    add rdi, 2
    cmp rsi, r15
    jae .l
    inc rsi                           ; the ANSI fallback character
    jmp .l
.sym:
    inc rsi
    jmp .l
.lit:
    inc rsi
    mov [rsp+112], al
    invoke MultiByteToWideChar, 0, 0, &rsp+112, 1, rdi, 1
    add rdi, 2
    jmp .l
.nbsp:
    inc rsi
    mov word [rdi], ' '
    add rdi, 2
    jmp .l
.hex:
    inc rsi
    xor ecx, ecx
    xor edx, edx
.hx:
    cmp edx, 2
    jae .hd
    cmp rsi, r15
    jae .hd
    movzx eax, byte [r14+rsi]
    cmp eax, '9'
    jbe .h09
    or eax, 0x20
    sub eax, 'a'-10
    jmp .hacc
.h09:
    sub eax, '0'
.hacc:
    shl ecx, 4
    or ecx, eax
    inc rsi
    inc edx
    jmp .hx
.hd:
    mov [rsp+112], cl
    invoke MultiByteToWideChar, 0, 0, &rsp+112, 1, rdi, 1
    add rdi, 2
    jmp .l
.nl:
    mov word [rdi], 10
    add rdi, 2
    jmp .l
.tab:
    mov word [rdi], 9
    add rdi, 2
    jmp .l
.done:
    invoke free, r14
    mov rax, rdi
    sub rax, rbx
    shr rax, 1
    invoke write_utf8, r13, rbx, rax, 0
    mov r14d, eax
    invoke free, rbx
    mov eax, r14d
    EPILOG 192
.fail:
    xor eax, eax
    EPILOG 192

; ---------------------------------------------------------------- PDF
; cv_pdf_content(data, len, out wchar*) -> rax = new out pointer ; text operators of one content stream
cv_pdf_content:
    PROLOG 64
    mov rsi, rcx
    mov r12, rdx                      ; end
    add r12, rsi
    mov rdi, r8
    xor ebx, ebx                      ; text since last newline
    xor ebp, ebp                      ; inside [ ] array
.l:
    cmp rsi, r12
    jae .d
    movzx eax, byte [rsi]
    inc rsi
    cmp eax, '('
    je .str
    cmp eax, '['
    jne .nb
    mov ebp, 1
    jmp .l
.nb:
    cmp eax, ']'
    jne .nn
    xor ebp, ebp
    jmp .l
.nn:
    ; operators that end a text line: ET T* Td TD ' "
    cmp eax, 'E'
    jne .nT
    cmp rsi, r12
    jae .nT
    cmp byte [rsi], 'T'
    jne .nT
    jmp .nlop
.nT:
    cmp eax, 'T'
    jne .nsq
    cmp rsi, r12
    jae .nsq
    movzx ecx, byte [rsi]
    cmp ecx, '*'
    je .nlop
    cmp ecx, 'd'
    je .nlop
    cmp ecx, 'D'
    je .nlop
.nsq:
    ; kerning inside TJ arrays: a big negative number is a word gap
    test ebp, ebp
    jz .l
    cmp eax, '-'
    jne .l
    xor ecx, ecx
.kn:
    cmp rsi, r12
    jae .kd
    movzx eax, byte [rsi]
    sub eax, '0'
    cmp eax, 9
    ja .kd
    imul ecx, ecx, 10
    add ecx, eax
    inc rsi
    jmp .kn
.kd:
    cmp ecx, 200
    jb .l
    test ebx, ebx
    jz .l
    mov word [rdi], ' '
    add rdi, 2
    jmp .l
.nlop:
    test ebx, ebx
    jz .l
    mov word [rdi], 10
    add rdi, 2
    xor ebx, ebx
    jmp .l
.str:
    mov r13d, 1                       ; nesting
.sl:
    cmp rsi, r12
    jae .d
    movzx eax, byte [rsi]
    inc rsi
    cmp eax, 0x5C
    jne .nbs
    cmp rsi, r12
    jae .d
    movzx eax, byte [rsi]
    inc rsi
    cmp eax, 'n'
    jne .e1
    mov eax, 10
    jmp .put
.e1:
    cmp eax, 'r'
    jne .e2
    mov eax, 13
    jmp .put
.e2:
    cmp eax, 't'
    jne .e3
    mov eax, 9
    jmp .put
.e3:
    cmp eax, '0'
    jb .put
    cmp eax, '7'
    ja .put
    sub eax, '0'
    mov ecx, 1
.oct:
    cmp ecx, 3
    jae .put
    cmp rsi, r12
    jae .put
    movzx edx, byte [rsi]
    sub edx, '0'
    cmp edx, 7
    ja .put
    shl eax, 3
    or eax, edx
    inc rsi
    inc ecx
    jmp .oct
.nbs:
    cmp eax, '('
    jne .nop
    inc r13d
    jmp .put
.nop:
    cmp eax, ')'
    jne .put
    dec r13d
    jz .sdone
.put:
    cmp eax, 32
    jae .ok
    cmp eax, 10
    je .ok
    jmp .sl
.ok:
    mov [rdi], ax
    add rdi, 2
    mov ebx, 1
    jmp .sl
.sdone:
    jmp .l
.d:
    mov rax, rdi
    EPILOG 64

; cv_find(hay, haylen, needle) -> rax pointer or 0 (bounded strstr on possibly non-terminated data)
cv_find:
    PROLOG 64
    mov rsi, rcx
    mov r12, rdx
    mov rdi, r8
    xor ebx, ebx
.nl:
    cmp byte [rdi+rbx], 0
    je .have
    inc ebx
    jmp .nl
.have:
    sub r12, rbx
    jb .no
    xor ecx, ecx
.s:
    cmp rcx, r12
    ja .no
    xor edx, edx
.c:
    cmp edx, ebx
    jae .hit
    lea r9, [rsi+rcx]
    mov al, [r9+rdx]
    cmp al, [rdi+rdx]
    jne .adv
    inc edx
    jmp .c
.adv:
    inc rcx
    jmp .s
.hit:
    lea rax, [rsi+rcx]
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; cv_pdf(path, out) -> eax 1
cv_pdf:
    PROLOG 256
    mov r12, rcx
    mov r13, rdx
    invoke read_file_bytes, r12, &rsp+96
    test rax, rax
    jz .fail
    mov r14, rax                      ; file bytes
    mov r15, [rsp+96]                 ; size
    cmp r15, 0x4000000
    ja .free
    ; output: generous (decompressed text is at most a few times the file)
    lea rcx, [r15*8+4096]
    call malloc
    mov rbx, rax
    mov rdi, rax
    mov [rsp+112], rax                ; output start
    lea rax, [r15*8+4096]
    add rax, rbx
    mov [rsp+120], rax                ; output end (bytes of wchar)
    ; scratch for inflated streams (16 MB)
    mov ecx, 0x1000000
    call malloc
    mov [rsp+128], rax
    xor esi, esi                      ; scan position
.next:
    mov rcx, r14
    add rcx, rsi
    mov rdx, r15
    sub rdx, rsi
    lea r8, [k_stream]
    call cv_find
    test rax, rax
    jz .done
    mov rbp, rax                      ; "stream" keyword
    mov rsi, rax
    sub rsi, r14
    add rsi, 6
    ; skip the EOL after the keyword
    cmp rsi, r15
    jae .done
    cmp byte [r14+rsi], 13
    jne .e1
    inc rsi
.e1:
    cmp rsi, r15
    jae .done
    cmp byte [r14+rsi], 10
    jne .e2
    inc rsi
.e2:
    mov [rsp+136], rsi                ; data start
    mov rcx, r14
    add rcx, rsi
    mov rdx, r15
    sub rdx, rsi
    lea r8, [k_endstream]
    call cv_find
    test rax, rax
    jz .done
    mov rcx, rax
    sub rcx, r14
    mov [rsp+144], rcx                ; data end (offset)
    mov rsi, rcx
    add rsi, 9                        ; continue after endstream
    mov [rsp+152], rsi
    ; the dictionary: up to 600 bytes before the keyword, after the last "obj"
    mov rdx, rbp
    sub rdx, r14
    mov r8, 600
    cmp rdx, r8
    jae .dl
    mov r8, rdx
.dl:
    mov rcx, rbp
    sub rcx, r8                       ; dict start
    mov [rsp+160], rcx
    mov [rsp+168], r8                 ; dict length
    ; reject images, font files and anything typed
    mov rcx, [rsp+160]
    mov rdx, [rsp+168]
    lea r8, [k_image]
    call cv_find
    test rax, rax
    jnz .skip
    mov rcx, [rsp+160]
    mov rdx, [rsp+168]
    lea r8, [k_length1]
    call cv_find
    test rax, rax
    jnz .skip
    mov rcx, [rsp+160]
    mov rdx, [rsp+168]
    lea r8, [k_xref]
    call cv_find
    test rax, rax
    jnz .skip
    mov rcx, [rsp+160]
    mov rdx, [rsp+168]
    lea r8, [k_objstm]
    call cv_find
    test rax, rax
    jnz .skip
    mov rcx, [rsp+160]
    mov rdx, [rsp+168]
    lea r8, [k_meta]
    call cv_find
    test rax, rax
    jnz .skip
    mov rcx, [rsp+160]
    mov rdx, [rsp+168]
    lea r8, [k_flate]
    call cv_find
    test rax, rax
    jnz .flate
    ; plain stream: accept only untyped dictionaries
    mov rcx, [rsp+160]
    mov rdx, [rsp+168]
    lea r8, [k_type]
    call cv_find
    test rax, rax
    jnz .skip
    mov rcx, r14
    add rcx, [rsp+136]
    mov rdx, [rsp+144]
    sub rdx, [rsp+136]
    mov r8, rdi
    call cv_pdf_content
    mov rdi, rax
    jmp .adv
.flate:
    mov rcx, r14
    add rcx, [rsp+136]
    add rcx, 2                        ; zlib header
    mov rdx, [rsp+144]
    sub rdx, [rsp+136]
    sub rdx, 2
    jbe .skip
    mov r8, [rsp+128]
    mov r9d, 0x1000000
    call inflate_buf
    test eax, eax
    jz .skip
    mov rcx, [rsp+128]
    ; rdx = bytes produced; at most one wide character comes out per byte: no more than fits (with room
    ; to spare) in what is left of the output - a stream can inflate to 16 MB
    mov rax, [rsp+120]
    sub rax, rdi
    shr rax, 2
    cmp rdx, rax
    jbe .fits
    mov rdx, rax
.fits:
    mov r8, rdi
    call cv_pdf_content
    mov rdi, rax
.adv:
    ; stop when the output buffer is nearly full
    mov rax, [rsp+120]
    sub rax, rdi
    cmp rax, 65536
    jb .done
.skip:
    mov rsi, [rsp+152]
    jmp .next
.done:
    invoke free, [rsp+128]
    mov rax, rdi
    sub rax, rbx
    shr rax, 1
    mov r15, rax
    xor r12d, r12d
    test rax, rax
    jz .out
    invoke write_utf8, r13, rbx, r15, 0
    mov r12d, eax
.out:
    invoke free, rbx
    invoke free, r14
    mov eax, r12d
    EPILOG 256
.free:
    invoke free, r14
.fail:
    xor eax, eax
    EPILOG 256

; ---------------------------------------------------------------- entry
; cv_rule(path, out) -> eax 1 when a [Convert] rule produced `out`
cv_rule:
    PROLOG 2704
    mov rsi, rcx
    mov rdi, rdx
    invoke wcsrchr, rsi, '.'
    test rax, rax
    jz .no
    invoke GetPrivateProfileStringW, &sec_convert, rax, &s_empty, &rsp+96, 1000, &g_ini
    cmp word [rsp+96], 0
    je .no
    lea r12, [rsp+96]
    lea r13, [rsp+2200]
    xor r14d, r14d
.e:
    movzx eax, word [r12]
    test eax, eax
    jz .run
    cmp eax, '%'
    jne .lit
    movzx ecx, word [r12+2]
    cmp ecx, '1'
    je .p1
    cmp ecx, '2'
    jne .lit
    mov r15, rdi
    jmp .sub
.p1:
    mov r15, rsi
.sub:
    add r12, 4
    mov word [r13+r14*2], '"'
    inc r14d
.sc:
    movzx eax, word [r15]
    test eax, eax
    jz .sq
    cmp r14d, 245
    jae .sq
    mov [r13+r14*2], ax
    inc r14d
    add r15, 2
    jmp .sc
.sq:
    mov word [r13+r14*2], '"'
    inc r14d
    jmp .e
.lit:
    cmp r14d, 245
    jae .run
    mov [r13+r14*2], ax
    inc r14d
    add r12, 2
    jmp .e
.run:
    mov word [r13+r14*2], 0
    invoke memset, &rsp+1200, 0, 104
    mov dword [rsp+1200], 104
    mov dword [rsp+1204+0], 0
    invoke CreateProcessW, 0, r13, 0, 0, 0, 0x08000000, 0, 0, &rsp+1200, &rsp+1320
    test eax, eax
    jz .no
    invoke WaitForSingleObject, [rsp+1320], 60000
    invoke CloseHandle, [rsp+1320]
    invoke CloseHandle, [rsp+1328]
    invoke GetFileAttributesW, rdi
    cmp eax, -1
    je .no
    mov eax, 1
    EPILOG 2704
.no:
    xor eax, eax
    EPILOG 2704

; tl_convert(path, out wchar[520]) -> eax 1 when `out` now holds a UTF-8 text rendition of `path`
tl_convert:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    invoke wcsrchr, rsi, '.'
    test rax, rax
    jz .no
    mov rbx, rax
    invoke cv_tmp, rdi
    invoke cv_rule, rsi, rdi
    test eax, eax
    jnz .yes
    invoke _wcsicmp, rbx, &e_docx
    test eax, eax
    jz .docx
    invoke _wcsicmp, rbx, &e_docm
    test eax, eax
    jz .docx
    invoke _wcsicmp, rbx, &e_rtf
    test eax, eax
    jz .rtf
    invoke _wcsicmp, rbx, &e_pdf
    test eax, eax
    jz .pdf
.no:
    xor eax, eax
    EPILOG 64
.docx:
    invoke cv_docx, rsi, rdi
    jmp .res
.rtf:
    invoke cv_rtf, rsi, rdi
    jmp .res
.pdf:
    invoke cv_pdf, rsi, rdi
.res:
    test eax, eax
    jz .no
.yes:
    mov eax, 1
    EPILOG 64

; tl_isconv(path) -> eax 1 when the file is read through a converter (so it must not be written back as text)
tl_isconv:
    PROLOG 1200
    mov rsi, rcx
    invoke wcsrchr, rsi, '.'
    test rax, rax
    jz .no
    mov rbx, rax
    invoke _wcsicmp, rbx, &e_docx
    test eax, eax
    jz .yes
    invoke _wcsicmp, rbx, &e_docm
    test eax, eax
    jz .yes
    invoke _wcsicmp, rbx, &e_rtf
    test eax, eax
    jz .yes
    invoke _wcsicmp, rbx, &e_pdf
    test eax, eax
    jz .yes
    invoke GetPrivateProfileStringW, &sec_convert, rbx, &s_empty, &rsp+96, 8, &g_ini
    cmp word [rsp+96], 0
    jne .yes
.no:
    xor eax, eax
    EPILOG 1200
.yes:
    mov eax, 1
    EPILOG 1200
