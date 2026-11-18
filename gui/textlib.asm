; textlib.asm - text file I/O, line splitting, key normalisation and the LCS diff engine
%include "inc/macros.inc"
%include "inc/win.inc"

extern malloc, calloc, realloc, free, memcpy, memset, wcslen, wcscmp, wcsstr
extern MultiByteToWideChar, WideCharToMultiByte, CreateFileW, WriteFile, CloseHandle
extern safe_write2, tl_convert, tl_isconv, DeleteFileW
extern read_file_bytes, write_utf8, vec_push, lower_w

global g_tc_rules, text_load, text_save, text_load_raw, lines_split, compare_lines, hunk_free, keys_build, eol_keep, eol_restore
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
    mov dword [rsp+128], 0           ; TXT flags (+28): 1 read as Latin-1, 2 line ends of more than one kind
    mov eax, 0x7FFFFFF0              ; the conversion takes an int length: a larger file would be read in
    cmp rsi, rax                     ; part (size mod 4 GB) and a save would write that part back
    jbe .sizeok
    invoke free, rbx
    jmp .fail
.sizeok:
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
    lea rdx, [rbx+rdi]
    mov r8, rsi
    sub r8, rdi
    mov [rsp+104], rdx
    mov [rsp+112], r8
    mov r15d, CP_UTF8
    test r14d, r14d
    jnz .cp
    xor r15d, r15d                   ; the ANSI code page ...
    cmp qword [rsp+112], 0
    je .cp
    invoke MultiByteToWideChar, 0, MB_ERR_INVALID_CHARS, [rsp+104], [rsp+112], 0, 0
    test eax, eax
    jnz .cp
    ; ... unless the bytes are not valid there either (another code page, binary data): the conversion
    ; would replace them and a save would write the replacements. Latin-1 maps every byte to one character
    ; and back, so the file is shown oddly but saved byte for byte.
    mov r15d, 28591
    or dword [rsp+128], 1
.cp:
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
    ; line ends: CR LF (r9), LF (r10), bare CR (r11)
    xor ecx, ecx
    xor r9d, r9d
    xor r10d, r10d
    xor r11d, r11d
    mov rdx, [rsp+120]
.sl:
    cmp rcx, rdx
    jae .counted
    movzx eax, word [rbp+rcx*2]
    inc rcx
    cmp eax, 10
    je .lf
    cmp eax, 13
    jne .sl
    cmp rcx, rdx
    jae .cr
    cmp word [rbp+rcx*2], 10
    jne .cr
    inc rcx
    inc r9
    jmp .sl
.cr:
    inc r11
    jmp .sl
.lf:
    inc r10
    jmp .sl
.counted:
    mov r8d, 1                       ; the style for new lines: CR LF, unless LF is the more frequent one
    cmp r10, r9
    jbe .style
    xor r8d, r8d
.style:
    xor eax, eax                     ; kinds present
    test r9, r9
    setnz al
    xor ecx, ecx
    test r10, r10
    setnz cl
    add eax, ecx
    cmp eax, 2
    jae .mixed
    test r11, r11
    jz .sdone
.mixed:                              ; more than one kind (or bare CR): the view keeps the original to give
    or dword [rsp+128], 2            ; every line its own line end back when it saves (eol_restore)
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
    mov eax, [rsp+128]
    mov [r13+28], eax
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
    shr r10d, 8                      ; crlf flag: 0 LF, 1 CR LF, 2 the text holds its line ends as they are to be
    and r10d, 0xFF
.l:
    cmp rbx, r14
    jae .ld
    movzx eax, word [r13+rbx*2]
    inc rbx
    cmp r10d, 2
    je .put
    cmp eax, 13
    je .brk
    cmp eax, 10
    je .brk
.put:
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
    ; ANSI code page - or Latin-1 for a file that was read that way (flag 1 in bits 16..)
    mov dword [rsp+112], 0
    test r15d, 0x10000
    jz .acp
    mov dword [rsp+112], 28591
.acp:
    invoke WideCharToMultiByte, #[rsp+112], 0, rsi, rdi, 0, 0, 0, 0
    mov rbp, rax
    lea rcx, [rbp+4]
    call malloc
    mov r13, rax
    test rax, rax
    jz .convfail
    test rdi, rdi
    jz .ansiwrite
    mov dword [rsp+104], 0
    invoke WideCharToMultiByte, #[rsp+112], 0x400, rsi, rdi, r13, rbp, 0, &rsp+104   ; WC_NO_BEST_FIT_CHARS
    test eax, eax
    jz .ansifail
    cmp dword [rsp+104], 0           ; a character the code page does not have would be written as '?':
    jne .ansifail                    ; the save fails instead of destroying it
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

; ---------------------------------------------------------------- line ends of a file that mixes them
; A view shows and edits lines; the kind of line end is a property of the file (TXT+20). A file that holds
; more than one kind (TXT+28 bit 2) would be rewritten with one kind on save, so the view keeps the text as
; it was read (eol_keep) and, when it saves, eol_restore gives every line the line end it had: the lines
; of the new text are matched against the original ones with the compare engine; a new or changed line
; takes the line end of the line before it.

; eol_keep(TXT*, slot* {text, length}): keep a copy of the text when its line ends are mixed, else free the slot
eol_keep:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    invoke free, [rsi]
    mov qword [rsi], 0
    mov qword [rsi+8], 0
    test dword [rbx+28], 2
    jz .d
    mov rdi, [rbx+8]
    lea rcx, [rdi*2+8]
    call malloc
    test rax, rax
    jz .d
    mov [rsi], rax
    mov [rsi+8], rdi
    lea r8, [rdi*2+2]
    invoke memcpy, rax, [rbx], r8
.d:
    EPILOG 64

section .rdata
WSTR s_er_empty, ""
section .text
%define ER_LA    96                  ; LINES of the original -> 112
%define ER_LB    112                 ; LINES of the new text -> 128
%define ER_HV    128                 ; hunks -> 152
%define ER_CA    152                 ; copies that lines_split cuts up
%define ER_CB    160
%define ER_KA    168                 ; line end of every original line: 0 none, 1 LF, 2 CR LF, 3 CR
%define ER_KB    176                 ; and of every new line (0xFF: not known yet)
%define ER_OUT   184
%define ER_DEF   192
%define ER_TRAIL 200

; eol_restore(original, length, new text, length, crlf 0 / 1) -> rax malloc'd text with the line ends as they
;   are to be written (text_save with crlf = 2), rdx its length; rax 0 when it cannot be made
eol_restore:
    PROLOG 256
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    mov eax, [rsp+256+8+64+40]       ; 5th argument
    add eax, 1
    mov [rsp+ER_DEF], rax            ; 1 LF, 2 CR LF
    xor eax, eax
    mov [rsp+ER_LA], rax
    mov [rsp+ER_LB], rax
    mov [rsp+ER_HV], rax
    mov [rsp+ER_HV+8], rax
    mov [rsp+ER_HV+16], rax
    mov [rsp+ER_CA], rax
    mov [rsp+ER_CB], rax
    mov [rsp+ER_KA], rax
    mov [rsp+ER_KB], rax
    mov [rsp+ER_OUT], rax
    lea rcx, [r13*2+8]
    call malloc
    mov [rsp+ER_CA], rax
    test rax, rax
    jz .fail
    lea r8, [r13*2]
    invoke memcpy, rax, r12, r8
    mov rax, [rsp+ER_CA]
    mov word [rax+r13*2], 0
    lea rcx, [r15*2+8]
    call malloc
    mov [rsp+ER_CB], rax
    test rax, rax
    jz .fail
    lea r8, [r15*2]
    invoke memcpy, rax, r14, r8
    mov rax, [rsp+ER_CB]
    mov word [rax+r15*2], 0
    invoke lines_split, [rsp+ER_CA], r13, &rsp+ER_LA
    invoke lines_split, [rsp+ER_CB], r15, &rsp+ER_LB
    mov rdx, [rsp+ER_LA+8]
    add rdx, 16
    invoke calloc, 1, rdx
    mov [rsp+ER_KA], rax
    test rax, rax
    jz .fail
    mov rcx, [rsp+ER_LB+8]
    add rcx, 16
    call malloc
    mov [rsp+ER_KB], rax
    test rax, rax
    jz .fail
    mov r8, [rsp+ER_LB+8]
    add r8, 16
    invoke memset, rax, 0xFF, r8
    ; the line end of every original line
    mov rdi, [rsp+ER_KA]
    xor ecx, ecx                     ; index
    xor edx, edx                     ; line
.ka:
    cmp rcx, r13
    jae .kad
    movzx eax, word [r12+rcx*2]
    inc rcx
    cmp eax, 10
    je .kalf
    cmp eax, 13
    jne .ka
    cmp rcx, r13
    jae .kacr
    cmp word [r12+rcx*2], 10
    jne .kacr
    inc rcx
    mov byte [rdi+rdx], 2
    inc rdx
    jmp .ka
.kacr:
    mov byte [rdi+rdx], 3
    inc rdx
    jmp .ka
.kalf:
    mov byte [rdi+rdx], 1
    inc rdx
    jmp .ka
.kad:
    ; match the lines
    lea rax, [rsp+ER_HV]
    mov [rsp+32], rax
    lea rcx, [rsp+ER_LA]
    lea rdx, [rsp+ER_LB]
    xor r8d, r8d
    lea r9, [s_er_empty]
    call compare_lines
    mov rsi, [rsp+ER_HV]             ; hunks {type, a start, a length, b start, b length} (24 bytes)
    mov rbx, [rsp+ER_HV+8]
    mov rdi, [rsp+ER_KA]
    mov rbp, [rsp+ER_KB]
.h:
    test rbx, rbx
    jz .hd
    mov r8d, [rsi+4]
    mov r9d, [rsi+8]
    mov r10d, [rsi+12]
    mov r11d, [rsi+16]
    cmp r9d, r11d
    jbe .hm
    mov r9d, r11d                    ; pairs: the shorter side
.hm:
    test r9d, r9d
    jz .hn
    movzx eax, byte [rdi+r8]
    mov [rbp+r10], al
    inc r8d
    inc r10d
    dec r9d
    jmp .hm
.hn:
    add rsi, 24
    dec rbx
    jmp .h
.hd:
    ; lines without an original (or whose original was the last line and had none): as the line before
    mov rcx, [rsp+ER_LB+8]
    xor edx, edx
    mov r8, [rsp+ER_DEF]
.fl:
    cmp rdx, rcx
    jae .fld
    movzx eax, byte [rbp+rdx]
    cmp eax, 0xFF
    je .fset
    test eax, eax
    jnz .fkeep
.fset:
    mov [rbp+rdx], r8b
    jmp .fnext
.fkeep:
    mov r8d, eax
.fnext:
    inc rdx
    jmp .fl
.fld:
    mov qword [rsp+ER_TRAIL], 0      ; does the new text end with a line break?
    test r15, r15
    jz .out
    movzx eax, word [r14+r15*2-2]
    cmp eax, 10
    je .tr
    cmp eax, 13
    jne .out
.tr:
    mov qword [rsp+ER_TRAIL], 1
.out:
    mov rcx, [rsp+ER_LB+8]
    lea rcx, [r15+rcx*2+8]
    shl rcx, 1
    call malloc
    mov [rsp+ER_OUT], rax
    test rax, rax
    jz .fail
    mov rdi, rax
    mov rsi, [rsp+ER_LB]             ; line pointers
    mov rbx, [rsp+ER_LB+8]
    mov rbp, [rsp+ER_KB]
    xor edx, edx
.ol:
    cmp rdx, rbx
    jae .od
    mov rcx, [rsi+rdx*8]
.oc:
    movzx eax, word [rcx]
    test eax, eax
    jz .oe
    mov [rdi], ax
    add rdi, 2
    add rcx, 2
    jmp .oc
.oe:
    lea rax, [rdx+1]
    cmp rax, rbx
    jb .oeol
    cmp qword [rsp+ER_TRAIL], 0
    je .on
.oeol:
    movzx eax, byte [rbp+rdx]
    cmp eax, 1
    je .olf
    mov word [rdi], 13
    add rdi, 2
    cmp eax, 3
    je .on
.olf:
    mov word [rdi], 10
    add rdi, 2
.on:
    inc rdx
    jmp .ol
.od:
    mov word [rdi], 0
    sub rdi, [rsp+ER_OUT]
    shr rdi, 1
    mov r12, rdi                     ; length
    call .free
    mov rax, [rsp+ER_OUT]
    mov rdx, r12
    EPILOG 256
.fail:
    call .free
    invoke free, [rsp+ER_OUT]
    xor eax, eax
    EPILOG 256
.free:                               ; (the return address is on the stack: the locals are 8 higher)
    sub rsp, 40
    mov rcx, [rsp+48+ER_CA]
    call free
    mov rcx, [rsp+48+ER_CB]
    call free
    mov rcx, [rsp+48+ER_KA]
    call free
    mov rcx, [rsp+48+ER_KB]
    call free
    mov rcx, [rsp+48+ER_LA]
    call free
    mov rcx, [rsp+48+ER_LB]
    call free
    mov rcx, [rsp+48+ER_HV]
    call free
    add rsp, 40
    ret

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

; pat_char_match(ecx = kind, edx = ch) -> eax 1 if ch matches kind
pat_char_match:
    test edx, edx
    jz .no
    cmp ecx, '?'
    je .yes
    test ecx, 0x10000
    jz .lit
    mov eax, ecx
    and eax, 0xFF
    cmp eax, 'd'
    je .digit
    cmp eax, 'w'
    je .word
    cmp eax, 's'
    je .space
.lit:
    movzx ecx, cx
    cmp edx, ecx
    je .yes
    jmp .no
.digit:
    cmp edx, '0'
    jb .no
    cmp edx, '9'
    jbe .yes
    jmp .no
.word:
    cmp edx, '_'
    je .yes
    cmp edx, 0x80
    jae .yes
    cmp edx, '0'
    jb .no
    cmp edx, '9'
    jbe .yes
    mov eax, edx
    or eax, 0x20
    cmp eax, 'a'
    jb .no
    cmp eax, 'z'
    jbe .yes
    jmp .no
.space:
    cmp edx, ' '
    je .yes
    cmp edx, 9
    je .yes
    cmp edx, 0x3000
    je .yes
.no:
    xor eax, eax
    ret
.yes:
    mov eax, 1
    ret

; pat_match(rcx = pat, rdx = pat_end, r8 = src) -> rax = end_src (0 if no match)
pat_match:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
.loop:
    cmp rsi, rdi
    jae .matched
    movzx eax, word [rsi]
    add rsi, 2
    cmp eax, 0x5C
    jne .not_esc
    cmp rsi, rdi
    jae .gotatom
    movzx eax, word [rsi]
    add rsi, 2
    cmp eax, 'd'
    je .esc_cls
    cmp eax, 'w'
    je .esc_cls
    cmp eax, 's'
    je .esc_cls
    or eax, 0x20000
    jmp .gotatom
.esc_cls:
    or eax, 0x10000
    jmp .gotatom
.not_esc:
    cmp eax, '*'
    jne .gotatom
    mov r12d, '?'
    xor r13d, r13d
    jmp .quant
.gotatom:
    mov r12d, eax
    cmp rsi, rdi
    jae .single
    movzx eax, word [rsi]
    cmp eax, '+'
    je .qplus
    cmp eax, '*'
    je .qstar
.single:
    movzx edx, word [rbx]
    mov ecx, r12d
    call pat_char_match
    test eax, eax
    jz .fail
    add rbx, 2
    jmp .loop
.qplus:
    mov r13d, 1
    add rsi, 2
    jmp .quant
.qstar:
    xor r13d, r13d
    add rsi, 2
.quant:
    xor r14d, r14d
.qcnt:
    movzx edx, word [rbx+r14*2]
    mov ecx, r12d
    call pat_char_match
    test eax, eax
    jz .qtry
    inc r14
    jmp .qcnt
.qtry:
    cmp r14, r13
    jb .fail
    lea r8, [rbx+r14*2]
    invoke pat_match, rsi, rdi, r8
    test rax, rax
    jnz .done
    test r14, r14
    jz .fail
    dec r14
    jmp .qtry
.matched:
    mov rax, rbx
    jmp .done
.fail:
    xor eax, eax
.done:
    EPILOG 64

; keys_build(LINES*, flags, cprefix, hashes*) -> keys array; keys[n] holds the blob to free (or 0)
; rules_apply(src, dst, cap chars) : replace every "old" of g_tc_rules ("old=new;old=new") in src
rules_apply:
    PROLOG 128
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
    cmp r8, r14
    jbe .plain
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
    mov [rsp+96], r10
    mov [rsp+104], r11
    invoke pat_match, r14, r8, rsi
    mov r10, [rsp+96]
    mov r11, [rsp+104]
    cmp rax, rsi
    jbe .nextrule
    mov rbp, rax                     ; end of matched span in src
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
    mov rsi, rbp
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
    EPILOG 128

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
    mov qword [rsp+120], 0           ; the longest line
.tot:
    cmp rbp, rbx
    jae .alloc
    mov rax, [rsi]
    invoke wcslen, [rax+rbp*8]
    lea rdi, [rdi+rax+1]
    cmp rax, [rsp+120]
    jbe .totn
    mov [rsp+120], rax
.totn:
    inc rbp
    jmp .tot
.alloc:
    lea rcx, [rdi*2+8]
    test r12d, 32
    jz .blob
    mov rcx, rdi                     ; substitution rules may lengthen lines: up to 8 * length + 8 characters
    shl rcx, 4                       ; and a terminator each
    lea rcx, [rcx+rbx*2+16]
.blob:
    call malloc
    mov [r15+rbx*8], rax
    mov [rsp+104], rax               ; cursor
    mov qword [rsp+112], 0
    test r12d, 32
    jz .nodata
    mov rcx, [rsp+120]               ; room for the longest line after substitution (it was 4096 characters,
    shl rcx, 4                       ; and longer lines were compared by their first 4000 only)
    add rcx, 64
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
