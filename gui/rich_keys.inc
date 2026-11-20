; rich_keys.inc - line editing keys of the IntelliJ editor in every editable RichEdit pane
;   (text compare panes, the merge output, the editor)
;
;   Ctrl+D              duplicate the line of the caret, or the selection when there is one
;   Alt+Shift+Up/Down   move the line of the caret (or every line the selection touches) up / down
;   Ctrl+Shift+Up/Down  the same
;   Ctrl+Shift+J        join the line with the next one (or all lines of the selection): the line break and
;                       the blanks around it become one blank
;   Ctrl+Shift+U        toggle the case of the selection, or of the word at the caret
;   Ctrl+/              toggle the line comment of the line of the caret, or of every line the selection
;                       touches (the comment sign follows the file type; all lines commented: taken off)
;   Tab                 with a selection over more than one line: indent those lines
;   Shift+Tab           take one level of indent off the line of the caret / the selected lines
;   Ctrl+Shift+M        move the caret to the bracket that matches the one at the caret (or, away from a
;                       bracket, to the opening bracket around the caret)
;   Enter               the new line starts with the indent of the line it comes from; behind ( [ { one
;                       level more, and a closing partner at the caret goes to a line of its own below
;   Home                to the first character of the line that is not a blank; from there to the line start
;   Shift+Enter         start a new line below the line of the caret, wherever the caret is in it
;   Ctrl+Alt+Enter      start a new line above the line of the caret
;   Ctrl+Enter          split the line at the caret; the caret stays where it is
;   ( [ { " '           put the closing partner behind the caret (rich_char); typing that closing character
;                       steps over it, Backspace on the opening one takes both away
;
;   Every action is one replacement through EM_REPLACESEL, so the editor's own undo takes it back in one
;   step. A read-only pane (folded / unified view, the merge inputs) does not take the keys.

extern GetClassNameW, _wcsicmp, memcpy, IsCharLowerW, CharUpperBuffW, CharLowerBuffW
extern GetKeyState, GetPrivateProfileIntW, g_ini
global rich_char
extern GetParent, GetDlgCtrlID, GetDlgItem, GetWindowTextW, wcsrchr, wcsncmp, _snwprintf, wcsstr, wcscpy

global rich_key

%define EM_EXLINEFROMCHAR_ 0x0436
%define EM_GETTEXTRANGE_   0x044B

%define RK_CR   64                   ; CHARRANGE: the selection
%define RK_BLK  72                   ; dword x 5: first line, last line, start of the first, end of the last, lines
%define RK_RNG  96                   ; CHARRANGE: what is replaced
%define RK_SEL  104                  ; CHARRANGE: the selection afterwards
%define RK_T    112                  ; text read from the pane
%define RK_N    120                  ; text that goes in
%define RK_CLS  128                  ; wchar[32]
%define RK_A    192                  ; dword x 3: scratch
%define RK_B    196
%define RK_C    200

section .rdata
WSTR s_rk_editor, "Editor"
WSTR s_rk_autopair, "AutoPair"
WSTR s_rk_slash, "//"
WSTR s_rk_hash, "#"
WSTR s_rk_semi, ";"
WSTR s_rk_dash, "--"
WSTR s_rk_quote, "'"
WSTR s_rk_rem, "REM"
WSTR s_rk_pct, "%"
s_rk_tab: dw 9, 0
WSTR s_rk_sp4, "    "
WSTR f_rk_ps, "%s "
WSTR s_rk_e1, ".py.sh.rb.pl.yml.yaml.toml.conf.cfg.ps1.mk.r.tcl.cmake.properties."
WSTR s_rk_e2, ".asm.inc.ini.s.nasm."
WSTR s_rk_e3, ".sql.lua.hs."
WSTR s_rk_e4, ".vb.vbs.bas."
WSTR s_rk_e5, ".bat.cmd."
WSTR s_rk_e6, ".tex."
align 8
rk_cmt_tab:                          ; {extensions (between dots, lower case), comment sign}
    dq s_rk_e1, s_rk_hash
    dq s_rk_e2, s_rk_semi
    dq s_rk_e3, s_rk_dash
    dq s_rk_e4, s_rk_quote
    dq s_rk_e5, s_rk_rem
    dq s_rk_e6, s_rk_pct
    dq 0, 0
section .text

; rk_prefix(hwnd, out wchar[8]): the line comment sign for the file of that pane - by the extension of the
;   path in the path box of its view (the right pane: the right path), "//" when it is not in the table
rk_prefix:
    PROLOG 1216
    mov rbx, rcx
    mov rdi, rdx
    lea r12, [s_rk_slash]
    invoke GetDlgCtrlID, rbx
    mov esi, ID_EDIT_A
    cmp eax, 202                     ; the right pane of a text compare
    jne .left
    mov esi, ID_EDIT_B
.left:
    invoke GetParent, rbx
    invoke GetDlgItem, rax, rsi
    test rax, rax
    jz .d
    mov word [rsp+96], 0
    invoke GetWindowTextW, rax, &rsp+96, 519
    invoke wcsrchr, &rsp+96, '.'
    test rax, rax
    jz .d
    mov r13, rax
    invoke wcslen, r13
    cmp rax, 14
    ja .d
    mov word [r13+rax*2], '.'        ; ".ext." is looked for in the lists
    mov word [r13+rax*2+2], 0
    invoke CharLowerBuffW, r13, rax
    lea r14, [rk_cmt_tab]
.t:
    mov rcx, [r14]
    test rcx, rcx
    jz .d
    invoke wcsstr, rcx, r13
    test rax, rax
    jnz .hit
    add r14, 16
    jmp .t
.hit:
    mov r12, [r14+8]
.d:
    invoke wcscpy, rdi, r12
    EPILOG 1216

; rk_blank(rcx = line) -> eax 1 when the line (up to its CR or the end) holds blanks only (changes rcx, eax)
rk_blank:
    movzx eax, word [rcx]
    test eax, eax
    jz .y
    cmp eax, 13
    je .y
    cmp eax, ' '
    je .n
    cmp eax, 9
    je .n
    xor eax, eax
    ret
.n:
    add rcx, 2
    jmp rk_blank
.y:
    mov eax, 1
    ret

; rk_iscmt(text, sign) -> eax 1 when every line that is not blank starts with the sign (after its indent),
;   and there is such a line
rk_iscmt:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    invoke wcslen, rdi
    mov r12, rax
    xor r13d, r13d                   ; lines with the sign
.l:
    mov rcx, rsi
    call rk_blank
    test eax, eax
    jnz .next
.ind:
    movzx eax, word [rsi]
    cmp eax, ' '
    je .skip
    cmp eax, 9
    jne .cmp
.skip:
    add rsi, 2
    jmp .ind
.cmp:
    invoke wcsncmp, rsi, rdi, r12
    test eax, eax
    jnz .no
    inc r13d
.next:
    movzx eax, word [rsi]
    test eax, eax
    jz .end
    add rsi, 2
    cmp eax, 13
    jne .next
    jmp .l
.end:
    xor eax, eax
    test r13d, r13d
    setnz al
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; rk_add(text, prefix, all) -> rax new text, to free: the prefix in front of every line that is not blank
;   (all = 1: in front of every line when all of them are blank)
rk_add:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12d, r8d
    mov r13d, 1                      ; lines
    xor r14d, r14d                   ; one of them is not blank
    mov rbp, rsi
.c:
    mov rcx, rbp
    call rk_blank
    test eax, eax
    jnz .cn
    mov r14d, 1
.cn:
    movzx eax, word [rbp]
    test eax, eax
    jz .cd
    add rbp, 2
    cmp eax, 13
    jne .cn
    inc r13d
    jmp .c
.cd:
    test r14d, r14d                  ; "all" only counts when every line is blank
    jz .size
    xor r12d, r12d
.size:
    invoke wcslen, rdi
    mov r15, rax                     ; length of the prefix
    mov rcx, rbp
    sub rcx, rsi
    shr rcx, 1
    mov rax, r15
    imul rax, r13
    add rcx, rax
    lea rcx, [rcx*2+8]
    call malloc
    mov rbx, rax
    mov rbp, rax                     ; write
.l:
    test r12d, r12d
    jnz .pre
    mov rcx, rsi
    call rk_blank
    test eax, eax
    jnz .copy
.pre:
    lea r8, [r15*2]
    invoke memcpy, rbp, rdi, r8
    lea rbp, [rbp+r15*2]
.copy:
    movzx eax, word [rsi]
    test eax, eax
    jz .d
    mov [rbp], ax
    add rbp, 2
    add rsi, 2
    cmp eax, 13
    jne .copy
    jmp .l
.d:
    mov word [rbp], 0
    mov rax, rbx
    EPILOG 64

; rk_del(text, sign): in place - take the comment sign (and one blank behind it) off every line that starts
;   with it after its indent; sign = 0: take one level of indent off (a tab, or up to four blanks)
rk_del:
    PROLOG 64
    mov rsi, rcx                     ; read
    mov rbp, rcx                     ; write
    mov rdi, rdx
    xor r12d, r12d
    test rdi, rdi
    jz .l
    invoke wcslen, rdi
    mov r12, rax
.l:
    test rdi, rdi
    jnz .sign
    cmp word [rsi], 9                ; indent
    jne .sp
    add rsi, 2
    jmp .copy
.sp:
    xor ecx, ecx
.sp1:
    cmp ecx, 4
    jae .copy
    cmp word [rsi], ' '
    jne .copy
    add rsi, 2
    inc ecx
    jmp .sp1
.sign:
    movzx eax, word [rsi]            ; the indent stays
    cmp eax, ' '
    je .keep
    cmp eax, 9
    jne .at
.keep:
    mov [rbp], ax
    add rbp, 2
    add rsi, 2
    jmp .sign
.at:
    invoke wcsncmp, rsi, rdi, r12
    test eax, eax
    jnz .copy
    lea rsi, [rsi+r12*2]
    cmp word [rsi], ' '
    jne .copy
    add rsi, 2
.copy:
    movzx eax, word [rsi]
    mov [rbp], ax
    test eax, eax
    jz .d
    add rbp, 2
    add rsi, 2
    cmp eax, 13
    jne .copy
    jmp .l
.d:
    EPILOG 64

; rk_br(ecx = character) -> eax 0 no bracket, 1 an opening one, 2 a closing one; edx its partner (leaf)
rk_br:
    mov eax, 1
    mov edx, ')'
    cmp ecx, '('
    je .d
    mov edx, ']'
    cmp ecx, '['
    je .d
    mov edx, '}'
    cmp ecx, '{'
    je .d
    mov eax, 2
    mov edx, '('
    cmp ecx, ')'
    je .d
    mov edx, '['
    cmp ecx, ']'
    je .d
    mov edx, '{'
    cmp ecx, '}'
    je .d
    xor eax, eax
.d:
    ret

; rk_isword(ecx = character) -> eax 1 for a letter, digit, '_' or anything not ASCII (changes eax only)
rk_isword:
    mov eax, 1
    cmp ecx, 0x80
    jae .d
    cmp ecx, '_'
    je .d
    cmp ecx, '0'
    jb .n
    cmp ecx, '9'
    jbe .d
    cmp ecx, 'A'
    jb .n
    cmp ecx, 'Z'
    jbe .d
    cmp ecx, 'a'
    jb .n
    cmp ecx, 'z'
    jbe .d
.n:
    xor eax, eax
.d:
    ret

; rk_get(h, a, b) -> rax the characters a..b-1 (line breaks are CR), to free
rk_get:
    PROLOG 96
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov eax, r13d
    sub eax, r12d
    jae .len
    xor eax, eax
.len:
    lea rcx, [rax*2+8]
    call malloc
    mov rsi, rax
    mov word [rsi], 0
    cmp r13d, r12d
    jbe .d
    mov [rsp+64], r12d
    mov [rsp+68], r13d
    mov [rsp+72], rsi
    invoke SendMessageW, rbx, EM_GETTEXTRANGE_, 0, &rsp+64
.d:
    mov rax, rsi
    EPILOG 96

; rk_put(h, range*, text, selection*): replace the range (one undo step) and set the selection
rk_put:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12, r9
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, rsi
    invoke SendMessageW, rbx, EM_REPLACESEL, 1, rdi
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, r12
    invoke SendMessageW, rbx, EM_SCROLLCARET, 0, 0
    EPILOG 64

; rk_block(h, cpMin, cpMax, out dword[5]): the lines the selection touches
rk_block:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14, r9
    invoke SendMessageW, rbx, EM_EXLINEFROMCHAR_, 0, r12
    mov [r14], eax
    invoke SendMessageW, rbx, EM_EXLINEFROMCHAR_, 0, r13
    mov r15d, eax
    cmp r13d, r12d
    jbe .one
    cmp r15d, [r14]
    jbe .one
    invoke SendMessageW, rbx, EM_LINEINDEX, r15, 0
    cmp eax, r13d                    ; a selection that ends at the start of a line does not take that line in
    jne .one
    dec r15d
.one:
    mov [r14+4], r15d
    invoke SendMessageW, rbx, EM_LINEINDEX, #[r14], 0
    mov [r14+8], eax
    invoke SendMessageW, rbx, EM_LINEINDEX, r15, 0
    mov esi, eax
    invoke SendMessageW, rbx, EM_LINELENGTH, rsi, 0
    add eax, esi
    mov [r14+12], eax
    invoke SendMessageW, rbx, EM_GETLINECOUNT, 0, 0
    mov [r14+16], eax
    EPILOG 64

; rk_swap(text = first CR second, length of first, length of second) -> rax second CR first, to free
rk_swap:
    PROLOG 64
    mov rsi, rcx
    mov r12d, edx
    mov r13d, r8d
    lea eax, [r12+r13+2]
    lea rcx, [rax*2]
    call malloc
    mov rbx, rax
    lea rdx, [rsi+r12*2+2]
    lea r8, [r13*2]
    invoke memcpy, rbx, rdx, r8
    mov word [rbx+r13*2], 13
    lea rcx, [rbx+r13*2+2]
    lea r8, [r12*2]
    invoke memcpy, rcx, rsi, r8
    lea eax, [r12+r13+1]
    mov word [rbx+rax*2], 0
    mov rax, rbx
    EPILOG 64

; ---------------------------------------------------------------- paired brackets and quotes
; The closing characters this file typed itself are remembered as their distance from the end of the
; text (typing in front of one does not change it). Only such a character is typed over or taken away
; together with its opening one; a bracket the user typed is never skipped.
section .bss
rk_ap_h:    resq 1                   ; the pane the list belongs to
rk_ap_n:    resd 1
rk_ap_tail: resd 8
section .text

; rk_ap_find(rcx = pane, edx = distance from the end) -> eax index in the list, -1 when it is not there
rk_ap_find:
    mov eax, -1
    cmp [rk_ap_h], rcx
    jne .d
    lea r8, [rk_ap_tail]
    mov eax, [rk_ap_n]
.l:
    dec eax
    js .d
    cmp [r8+rax*4], edx
    jne .l
.d:
    ret

; rk_ctx(h, out dword[4]): caret (-1 with a selection), length of the text, character in front of the
;   caret (0 at the start), character at the caret (0 at the end)
rk_ctx:
    PROLOG 96
    mov rbx, rcx
    mov rsi, rdx
    invoke SendMessageW, rbx, EM_EXGETSEL, 0, &rsp+64
    mov r12d, [rsp+64]
    mov dword [rsi], -1
    cmp r12d, [rsp+68]
    jne .d
    mov [rsi], r12d
    mov dword [rsp+72], 0            ; GETTEXTLENGTHEX: characters, UTF-16
    mov dword [rsp+76], 1200
    invoke SendMessageW, rbx, 0x045F, &rsp+72, 0
    mov [rsi+4], eax
    xor eax, eax
    mov [rsi+8], rax
    mov edx, r12d
    test edx, edx
    jz .get
    dec edx
.get:
    lea r8d, [r12+1]
    cmp r8d, [rsi+4]
    jbe .rng
    mov r8d, [rsi+4]
.rng:
    invoke rk_get, rbx, rdx, r8
    mov rdi, rax
    movzx eax, word [rdi]
    test r12d, r12d
    jz .first
    mov [rsi+8], eax
    test eax, eax
    jz .free
    movzx eax, word [rdi+2]
.first:
    mov [rsi+12], eax
.free:
    invoke free, rdi
.d:
    EPILOG 96

; rk_pair_on() -> eax 0 when [Editor] AutoPair=0 switches the pairs off
rk_pair_on:
    PROLOG 64
    invoke GetPrivateProfileIntW, &s_rk_editor, &s_rk_autopair, 1, &g_ini
    EPILOG 64

; rk_pane(h) -> eax 1 for an editable RichEdit pane
rk_pane:
    PROLOG 128
    mov rbx, rcx
    xor esi, esi
    invoke GetClassNameW, rbx, &rsp+64, 32
    invoke _wcsicmp, &rsp+64, &cn_rich
    test eax, eax
    jnz .d
    invoke GetWindowLongPtrW, rbx, GWL_STYLE
    test eax, 0x800                  ; ES_READONLY
    jnz .d
    mov esi, 1
.d:
    mov eax, esi
    EPILOG 128

; rich_char(hwnd, character) -> eax 1 when the character was used up: called for WM_CHAR before the pane
;   sees it.  ( [ { and, away from a word, " ' put their closing partner behind the caret; typing a closing
;   character that was put there that way steps over it.
rich_char:
    PROLOG 128
    mov rbx, rcx
    mov r12d, edx
    xor r13d, r13d                   ; the closing partner; 0: the character is a closing bracket
    mov esi, 1                       ; 1 opening bracket, 2 closing bracket, 3 quote
    mov r13d, ')'
    cmp r12d, '('
    je .is
    mov r13d, ']'
    cmp r12d, '['
    je .is
    mov r13d, '}'
    cmp r12d, '{'
    je .is
    mov esi, 3
    mov r13d, r12d
    cmp r12d, '"'
    je .is
    cmp r12d, 39
    je .is
    mov esi, 2
    cmp r12d, ')'
    je .is
    cmp r12d, ']'
    je .is
    cmp r12d, '}'
    jne .no
.is:
    invoke rk_pane, rbx
    test eax, eax
    jz .no
    call rk_pair_on
    test eax, eax
    jz .no
    invoke rk_ctx, rbx, &rsp+64      ; +64 caret, +68 length, +72 before, +76 at
    mov r14d, [rsp+64]
    cmp r14d, -1
    je .no
    cmp esi, 1
    je .open
    cmp r12d, [rsp+76]               ; a closing character this file put there: step over it
    jne .notover
    mov edx, [rsp+68]
    sub edx, r14d
    mov rcx, rbx
    call rk_ap_find
    test eax, eax
    js .notover
    mov [rk_ap_n], eax
    lea eax, [r14+1]
    mov [rsp+80], eax
    mov [rsp+84], eax
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, &rsp+80
    invoke SendMessageW, rbx, EM_SCROLLCARET, 0, 0
    mov eax, 1
    EPILOG 128
.notover:
    cmp esi, 2
    je .no
    mov ecx, [rsp+72]                ; a quote behind a word, a quote or a backslash stands alone
    cmp ecx, r12d
    je .no
    cmp ecx, 92
    je .no
    call rk_isword
    test eax, eax
    jnz .no
.open:
    mov eax, [rsp+76]                ; only in front of a blank, a closing bracket, ; , or the line end
    test eax, eax
    jz .pair
    cmp eax, 13
    je .pair
    cmp eax, 10
    je .pair
    cmp eax, 32
    je .pair
    cmp eax, 9
    je .pair
    cmp eax, ')'
    je .pair
    cmp eax, ']'
    je .pair
    cmp eax, '}'
    je .pair
    cmp eax, ';'
    je .pair
    cmp eax, ','
    jne .no
.pair:
    mov [rsp+96], r12w
    mov [rsp+98], r13w
    mov word [rsp+100], 0
    mov [rsp+80], r14d
    mov [rsp+84], r14d
    lea eax, [r14+1]
    mov [rsp+88], eax
    mov [rsp+92], eax
    invoke rk_put, rbx, &rsp+80, &rsp+96, &rsp+88
    cmp [rk_ap_h], rbx               ; remember the closing character
    je .same
    mov [rk_ap_h], rbx
    mov dword [rk_ap_n], 0
.same:
    mov eax, [rk_ap_n]
    lea r8, [rk_ap_tail]
    cmp eax, 8
    jb .push
    mov rdx, [r8+4]                  ; the list is full: the oldest one goes
    mov [r8], rdx
    mov rdx, [r8+12]
    mov [r8+8], rdx
    mov rdx, [r8+20]
    mov [r8+16], rdx
    mov edx, [r8+28]
    mov [r8+24], edx
    mov eax, 7
.push:
    mov edx, [rsp+68]
    sub edx, r14d
    inc edx
    mov [r8+rax*4], edx
    inc eax
    mov [rk_ap_n], eax
    mov eax, 1
    EPILOG 128
.no:
    xor eax, eax
    EPILOG 128

; rk_pair_back(h) -> eax 1 when Backspace took an opening character away together with the closing
;   partner this file had put behind it
rk_pair_back:
    PROLOG 128
    mov rbx, rcx
    invoke rk_ctx, rbx, &rsp+64
    mov r14d, [rsp+64]
    cmp r14d, 1
    jl .no
    mov ecx, [rsp+72]
    mov eax, [rsp+76]
    cmp ecx, '('
    jne .b1
    cmp eax, ')'
    je .have
.b1:
    cmp ecx, '['
    jne .b2
    cmp eax, ']'
    je .have
.b2:
    cmp ecx, '{'
    jne .b3
    cmp eax, '}'
    je .have
.b3:
    cmp ecx, eax
    jne .no
    cmp ecx, '"'
    je .have
    cmp ecx, 39
    jne .no
.have:
    mov edx, [rsp+68]
    sub edx, r14d
    mov rcx, rbx
    call rk_ap_find
    test eax, eax
    js .no
    mov [rk_ap_n], eax
    call rk_pair_on
    test eax, eax
    jz .no
    lea eax, [r14-1]
    mov [rsp+80], eax
    mov [rsp+88], eax
    mov [rsp+92], eax
    lea eax, [r14+1]
    mov [rsp+84], eax
    mov word [rsp+96], 0
    invoke rk_put, rbx, &rsp+80, &rsp+96, &rsp+88
    mov eax, 1
    EPILOG 128
.no:
    xor eax, eax
    EPILOG 128

; rich_key(hwnd, virtual key, modifiers 1 Ctrl | 2 Shift | 4 Alt) -> eax 1 when the key was one of the
;   line editing keys and the window is an editable RichEdit pane (the key is used up then)
rich_key:
    PROLOG 208
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    xor esi, esi                     ; 1 duplicate, 2 join, 3 case, 4 up, 5 down, 6 comment, 7 indent, 8 unindent, 9 bracket
    test r13d, r13d                  ; 10 new line, 11 line below, 12 line above, 13 split
    jnz .k1
    mov esi, 7
    cmp r12d, 9                      ; VK_TAB
    je .is
    mov esi, 10
    cmp r12d, 13                     ; VK_RETURN
    je .is
    mov esi, 14                      ; 14 Backspace between a pair
    cmp r12d, 8                      ; VK_BACK
    je .is
    mov esi, 15                      ; 15 Home
    cmp r12d, 0x24                   ; VK_HOME
    je .is
    jmp .no
.k1:
    cmp r13d, 1
    jne .k2
    mov esi, 1
    cmp r12d, 'D'
    je .is
    mov esi, 6
    cmp r12d, 0xBF                   ; VK_OEM_2: the / key
    je .is
    cmp r12d, 0x6F                   ; VK_DIVIDE
    je .is
    cmp r12d, 13
    jne .no
    invoke GetKeyState, 0x12         ; VK_MENU: Ctrl+Alt+Enter, else Ctrl+Enter
    mov esi, 12
    test ax, ax
    js .is
    mov esi, 13
    jmp .is
.k2:
    cmp r13d, 2
    jne .k3
    mov esi, 8
    cmp r12d, 9
    je .is
    mov esi, 11
    cmp r12d, 13
    je .is
    jmp .no
.k3:
    cmp r13d, 3
    jne .k6
    mov esi, 2
    cmp r12d, 'J'
    je .is
    mov esi, 3
    cmp r12d, 'U'
    je .is
    mov esi, 9
    cmp r12d, 'M'
    je .is
    jmp .arrow
.k6:
    cmp r13d, 6
    jne .no
.arrow:
    mov esi, 4
    cmp r12d, 0x26                   ; VK_UP
    je .is
    mov esi, 5
    cmp r12d, 0x28                   ; VK_DOWN
    jne .no
.is:
    invoke GetClassNameW, rbx, &rsp+RK_CLS, 32
    invoke _wcsicmp, &rsp+RK_CLS, &cn_rich
    test eax, eax
    jnz .no
    invoke GetWindowLongPtrW, rbx, GWL_STYLE
    test eax, 0x800                  ; ES_READONLY
    jnz .no
    invoke SendMessageW, rbx, EM_EXGETSEL, 0, &rsp+RK_CR
    mov r14d, [rsp+RK_CR]
    mov r15d, [rsp+RK_CR+4]
    invoke rk_block, rbx, r14, r15, &rsp+RK_BLK
    xor eax, eax
    mov [rsp+RK_T], rax
    mov [rsp+RK_N], rax
    cmp esi, 1
    je .dup
    cmp esi, 2
    je .join
    cmp esi, 3
    je .case
    cmp esi, 4
    je .up
    cmp esi, 5
    je .down
    cmp esi, 6
    je .cmt
    cmp esi, 7
    je .indent
    cmp esi, 8
    je .unindent
    cmp esi, 9
    je .brace
    cmp esi, 15
    je .home
    cmp esi, 14
    jne .nl
    invoke rk_pair_back, rbx         ; ---- Backspace
    test eax, eax
    jz .pass
    jmp .done

    ; ---- Ctrl+D
.dup:
    cmp r14d, r15d
    jne .dupsel
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    mov edi, [rsp+RK_BLK+12]
    sub edi, [rsp+RK_BLK+8]          ; length of the line
    lea rcx, [rdi*2+8]
    call malloc
    mov [rsp+RK_N], rax
    mov word [rax], 13               ; a line break and the line again, behind the line
    lea rcx, [rax+2]
    lea r8, [rdi*2+2]
    invoke memcpy, rcx, [rsp+RK_T], r8
    mov eax, [rsp+RK_BLK+12]
    mov [rsp+RK_RNG], eax
    mov [rsp+RK_RNG+4], eax
    lea eax, [r14+rdi+1]             ; the caret keeps its column, on the copy
    mov [rsp+RK_SEL], eax
    mov [rsp+RK_SEL+4], eax
    jmp .put
.dupsel:
    invoke rk_get, rbx, r14, r15     ; the selection again, behind it; the copy is selected
    mov [rsp+RK_N], rax
    mov [rsp+RK_RNG], r15d
    mov [rsp+RK_RNG+4], r15d
    mov [rsp+RK_SEL], r15d
    lea eax, [r15+r15]
    sub eax, r14d
    mov [rsp+RK_SEL+4], eax
    jmp .put

    ; ---- move up: the line above goes behind the block
.up:
    mov eax, [rsp+RK_BLK]
    test eax, eax
    jz .done
    dec eax
    invoke SendMessageW, rbx, EM_LINEINDEX, rax, 0
    mov edi, eax                     ; start of the line above
    invoke rk_get, rbx, rdi, #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    mov eax, [rsp+RK_BLK+8]
    sub eax, edi
    dec eax
    mov [rsp+RK_A], eax              ; its length
    mov ecx, [rsp+RK_BLK+12]
    sub ecx, [rsp+RK_BLK+8]
    mov [rsp+RK_B], ecx
    invoke rk_swap, [rsp+RK_T], #[rsp+RK_A], #[rsp+RK_B]
    mov [rsp+RK_N], rax
    mov [rsp+RK_RNG], edi
    mov eax, [rsp+RK_BLK+12]
    mov [rsp+RK_RNG+4], eax
    mov eax, [rsp+RK_A]
    inc eax
    mov ecx, r14d
    sub ecx, eax
    mov [rsp+RK_SEL], ecx
    mov ecx, r15d
    sub ecx, eax
    mov [rsp+RK_SEL+4], ecx
    jmp .put

    ; ---- move down: the line below goes in front of the block
.down:
    mov eax, [rsp+RK_BLK+4]
    inc eax
    cmp eax, [rsp+RK_BLK+16]
    jae .done
    invoke SendMessageW, rbx, EM_LINEINDEX, rax, 0
    mov edi, eax
    invoke SendMessageW, rbx, EM_LINELENGTH, rdi, 0
    mov [rsp+RK_B], eax              ; length of the line below
    add edi, eax
    mov [rsp+RK_C], edi              ; its end
    invoke rk_get, rbx, #[rsp+RK_BLK+8], rdi
    mov [rsp+RK_T], rax
    mov eax, [rsp+RK_BLK+12]
    sub eax, [rsp+RK_BLK+8]
    mov [rsp+RK_A], eax
    invoke rk_swap, [rsp+RK_T], #[rsp+RK_A], #[rsp+RK_B]
    mov [rsp+RK_N], rax
    mov eax, [rsp+RK_BLK+8]
    mov [rsp+RK_RNG], eax
    mov eax, [rsp+RK_C]
    mov [rsp+RK_RNG+4], eax
    mov eax, [rsp+RK_B]
    inc eax
    lea ecx, [r14+rax]
    mov [rsp+RK_SEL], ecx
    lea ecx, [r15+rax]
    mov [rsp+RK_SEL+4], ecx
    jmp .put

    ; ---- Ctrl+Shift+J
.join:
    mov eax, [rsp+RK_BLK+4]          ; the last line that is taken in: the next one at least
    cmp eax, [rsp+RK_BLK]
    ja .jl
    mov eax, [rsp+RK_BLK]
    inc eax
.jl:
    cmp eax, [rsp+RK_BLK+16]
    jae .done
    invoke SendMessageW, rbx, EM_LINEINDEX, rax, 0
    mov edi, eax
    invoke SendMessageW, rbx, EM_LINELENGTH, rdi, 0
    add edi, eax
    mov [rsp+RK_C], edi
    invoke rk_get, rbx, #[rsp+RK_BLK+8], rdi
    mov [rsp+RK_N], rax
    mov rsi, rax                     ; read
    mov rdi, rax                     ; write (never ahead of the read position)
    mov dword [rsp+RK_A], -1         ; where the first two lines meet
.jc:
    movzx eax, word [rsi]
    test eax, eax
    jz .je
    cmp eax, 13
    je .jbrk
    mov [rdi], ax
    add rdi, 2
    add rsi, 2
    jmp .jc
.jbrk:
    cmp rdi, [rsp+RK_N]              ; the blanks in front of the line break go
    jbe .jt1
    movzx eax, word [rdi-2]
    cmp eax, ' '
    je .jstrip
    cmp eax, 9
    jne .jt1
.jstrip:
    sub rdi, 2
    jmp .jbrk
.jt1:
    add rsi, 2
.js:
    movzx eax, word [rsi]            ; and those behind it
    cmp eax, ' '
    je .jskip
    cmp eax, 9
    jne .jo
.jskip:
    add rsi, 2
    jmp .js
.jo:
    cmp dword [rsp+RK_A], -1
    jne .jb
    mov rax, rdi
    sub rax, [rsp+RK_N]
    shr rax, 1
    mov [rsp+RK_A], eax
.jb:
    cmp rdi, [rsp+RK_N]              ; one blank between two parts that both have text
    jbe .jc
    movzx eax, word [rsi]
    test eax, eax
    jz .jc
    cmp eax, 13
    je .jc
    mov word [rdi], ' '
    add rdi, 2
    jmp .jc
.je:
    mov word [rdi], 0
    mov eax, [rsp+RK_BLK+8]
    mov [rsp+RK_RNG], eax
    mov ecx, [rsp+RK_C]
    mov [rsp+RK_RNG+4], ecx
    add eax, [rsp+RK_A]
    mov [rsp+RK_SEL], eax
    mov [rsp+RK_SEL+4], eax
    jmp .put

    ; ---- Ctrl+Shift+U
.case:
    mov [rsp+RK_RNG], r14d
    mov [rsp+RK_RNG+4], r15d
    mov [rsp+RK_SEL], r14d
    mov [rsp+RK_SEL+4], r15d
    cmp r14d, r15d
    jne .cget
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]    ; no selection: the word at the caret
    mov [rsp+RK_T], rax
    mov rsi, rax
    mov edi, r14d
    sub edi, [rsp+RK_BLK+8]          ; the caret in the line
    mov edx, [rsp+RK_BLK+12]
    sub edx, [rsp+RK_BLK+8]          ; length of the line
    mov r8d, edi
.cw1:
    test r8d, r8d
    jz .cw2
    movzx ecx, word [rsi+r8*2-2]
    call rk_isword
    test eax, eax
    jz .cw2
    dec r8d
    jmp .cw1
.cw2:
    mov r9d, edi
.cw3:
    cmp r9d, edx
    jae .cw4
    movzx ecx, word [rsi+r9*2]
    call rk_isword
    test eax, eax
    jz .cw4
    inc r9d
    jmp .cw3
.cw4:
    cmp r8d, r9d
    jae .done
    mov eax, [rsp+RK_BLK+8]
    add r8d, eax
    add r9d, eax
    mov [rsp+RK_RNG], r8d
    mov [rsp+RK_RNG+4], r9d
.cget:
    invoke rk_get, rbx, #[rsp+RK_RNG], #[rsp+RK_RNG+4]
    mov [rsp+RK_N], rax
    mov edi, [rsp+RK_RNG+4]
    sub edi, [rsp+RK_RNG]
    xor esi, esi
.cl:
    cmp esi, edi                     ; a small letter anywhere: all capitals, else all small
    jae .lower
    mov rax, [rsp+RK_N]
    movzx ecx, word [rax+rsi*2]
    invoke IsCharLowerW, rcx
    test eax, eax
    jnz .upper
    inc esi
    jmp .cl
.upper:
    invoke CharUpperBuffW, [rsp+RK_N], rdi
    jmp .put
.lower:
    invoke CharLowerBuffW, [rsp+RK_N], rdi
    jmp .put

    ; ---- Ctrl+/
.cmt:
    invoke rk_prefix, rbx, &rsp+RK_CLS
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    mov dword [rsp+RK_C], 0          ; without a selection the caret goes on to the next line
    invoke rk_iscmt, [rsp+RK_T], &rsp+RK_CLS
    test eax, eax
    jnz .uncmt
    invoke _snwprintf, &rsp+RK_CLS+32, 12, &f_rk_ps, &rsp+RK_CLS
    mov word [rsp+RK_CLS+32+24], 0
    invoke rk_add, [rsp+RK_T], &rsp+RK_CLS+32, 1
    mov [rsp+RK_N], rax
    jmp .lines
.uncmt:
    invoke rk_del, [rsp+RK_T], &rsp+RK_CLS
    jmp .inplace

    ; ---- Tab over several lines
.indent:
    mov eax, [rsp+RK_BLK+4]
    cmp eax, [rsp+RK_BLK]
    jbe .pass                        ; one line: the key types a tab as before
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    lea rdi, [s_rk_sp4]              ; tabs when the lines use tabs, else four blanks
    mov rcx, rax
.it:
    cmp word [rcx], 9
    je .itab
.it1:
    movzx eax, word [rcx]
    test eax, eax
    jz .igo
    add rcx, 2
    cmp eax, 13
    jne .it1
    jmp .it
.itab:
    lea rdi, [s_rk_tab]
.igo:
    invoke rk_add, [rsp+RK_T], rdi, 0
    mov [rsp+RK_N], rax
    mov dword [rsp+RK_C], 1
    jmp .lines

    ; ---- Shift+Tab
.unindent:
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    mov dword [rsp+RK_C], 1          ; the caret stays in its line
    invoke rk_del, rax, 0
.inplace:
    mov rax, [rsp+RK_T]
    mov [rsp+RK_N], rax
    mov qword [rsp+RK_T], 0
.lines:                              ; the lines of the block are replaced by RK_N
    invoke wcslen, [rsp+RK_N]
    mov edi, eax
    mov eax, [rsp+RK_BLK+8]
    mov [rsp+RK_RNG], eax
    mov ecx, [rsp+RK_BLK+12]
    mov [rsp+RK_RNG+4], ecx
    sub ecx, eax
    cmp ecx, edi
    jne .lsel
    cmp dword [rsp+RK_C], 0          ; nothing to take off: nothing happens
    jne .done
.lsel:
    cmp r14d, r15d
    je .lcaret
    mov [rsp+RK_SEL], eax            ; a selection: the lines stay selected
    add eax, edi
    mov [rsp+RK_SEL+4], eax
    jmp .put
.lcaret:
    cmp dword [rsp+RK_C], 0
    jne .lstay
    add eax, edi                     ; comment: on to the start of the next line (the end on the last one)
    mov ecx, [rsp+RK_BLK+4]
    inc ecx
    cmp ecx, [rsp+RK_BLK+16]
    jae .lset
    inc eax
    jmp .lset
.lstay:
    sub ecx, edi                     ; characters taken off
    mov eax, r14d
    sub eax, ecx
    cmp eax, [rsp+RK_BLK+8]
    jge .lset
    mov eax, [rsp+RK_BLK+8]
.lset:
    mov [rsp+RK_SEL], eax
    mov [rsp+RK_SEL+4], eax
    jmp .put

    ; ---- Ctrl+Shift+M
.brace:
    mov dword [rsp+RK_A], 0          ; GETTEXTLENGTHEX: characters, UTF-16
    mov dword [rsp+RK_B], 1200
    invoke SendMessageW, rbx, 0x045F, &rsp+RK_A, 0
    mov edi, eax                     ; length of the text
    invoke rk_get, rbx, 0, rdi
    mov [rsp+RK_T], rax
    mov rsi, rax
    mov r8d, r14d                    ; a bracket at the caret, else the one in front of it
    cmp r8d, edi
    jae .bprev
    movzx ecx, word [rsi+r8*2]
    call rk_br
    test eax, eax
    jnz .bhave
.bprev:
    test r14d, r14d
    jz .baround
    lea r8d, [r14-1]
    cmp r8d, edi
    jae .baround
    movzx ecx, word [rsi+r8*2]
    call rk_br
    test eax, eax
    jz .baround
.bhave:
    movzx r10d, word [rsi+r8*2]      ; the bracket, edx its partner
    xor r9d, r9d                     ; brackets of the same kind opened on the way
    cmp eax, 2
    je .bback
.bfwd:
    inc r8d
    cmp r8d, edi
    jae .done
    movzx ecx, word [rsi+r8*2]
    cmp ecx, r10d
    jne .bf1
    inc r9d
    jmp .bfwd
.bf1:
    cmp ecx, edx
    jne .bfwd
    test r9d, r9d
    jz .bclose
    dec r9d
    jmp .bfwd
.bclose:
    inc r8d                          ; behind the closing bracket
    jmp .bset
.bback:
    test r8d, r8d
    jz .done
    dec r8d
    movzx ecx, word [rsi+r8*2]
    cmp ecx, r10d
    jne .bb1
    inc r9d
    jmp .bback
.bb1:
    cmp ecx, edx
    jne .bback
    test r9d, r9d
    jz .bset                         ; in front of the opening bracket
    dec r9d
    jmp .bback
.baround:                            ; no bracket at the caret: the opening one around it
    mov r8d, r14d
    cmp r8d, edi
    jbe .ba0
    mov r8d, edi
.ba0:
    xor r9d, r9d
.ba1:
    test r8d, r8d
    jz .done
    dec r8d
    movzx ecx, word [rsi+r8*2]
    call rk_br
    cmp eax, 2
    jne .ba2
    inc r9d
    jmp .ba1
.ba2:
    cmp eax, 1
    jne .ba1
    test r9d, r9d
    jz .bset
    dec r9d
    jmp .ba1
.bset:
    mov [rsp+RK_SEL], r8d
    mov [rsp+RK_SEL+4], r8d
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, &rsp+RK_SEL
    invoke SendMessageW, rbx, EM_SCROLLCARET, 0, 0
    jmp .done

    ; ---- Home
.home:
    cmp r14d, r15d
    jne .pass
    invoke rk_get, rbx, #[rsp+RK_BLK+8], #[rsp+RK_BLK+12]
    mov [rsp+RK_T], rax
    xor edi, edi                     ; blanks and tabs the line starts with
.hc:
    movzx ecx, word [rax+rdi*2]
    cmp ecx, 32
    je .hc1
    cmp ecx, 9
    jne .hce
.hc1:
    inc edi
    jmp .hc
.hce:
    test edi, edi                    ; no indent: the key does what it always did
    jz .pass
    mov eax, [rsp+RK_BLK+8]
    lea ecx, [rax+rdi]
    cmp r14d, ecx                    ; already at the first character: on to the line start
    je .hset
    mov eax, ecx
.hset:
    mov [rsp+RK_SEL], eax
    mov [rsp+RK_SEL+4], eax
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, &rsp+RK_SEL
    invoke SendMessageW, rbx, EM_SCROLLCARET, 0, 0
    jmp .done

    ; ---- Enter, Shift+Enter, Ctrl+Alt+Enter, Ctrl+Enter
.nl:
    invoke GetWindowLongPtrW, rbx, GWL_STYLE
    test eax, 4                      ; ES_MULTILINE
    jz .pass
    mov edi, [rsp+RK_BLK+8]          ; the indent is read from edi..r8d
    mov r8d, r14d                    ; Enter, Ctrl+Enter: only the blanks in front of the caret count
    cmp esi, 10
    je .nlget
    cmp esi, 13
    je .nlget
    mov r8d, [rsp+RK_BLK+12]
    cmp esi, 12
    je .nlget
    invoke SendMessageW, rbx, EM_LINEINDEX, #[rsp+RK_BLK+4], 0   ; Shift+Enter: the last line of the block
    mov edi, eax
    mov r8d, [rsp+RK_BLK+12]
.nlget:
    invoke rk_get, rbx, rdi, r8
    mov [rsp+RK_T], rax
    xor edi, edi                     ; blanks and tabs the line starts with
.nlc:
    movzx ecx, word [rax+rdi*2]
    cmp ecx, 32
    je .nlc1
    cmp ecx, 9
    jne .nlce
.nlc1:
    inc edi
    jmp .nlc
.nlce:
    xor eax, eax
    mov [rsp+RK_A], eax              ; characters of one more level (behind an opening bracket)
    mov [rsp+RK_C], eax              ; 1: the closing partner at the caret gets a line of its own
    cmp esi, 10
    jne .nlalloc
    invoke rk_ctx, rbx, &rsp+RK_CLS  ; caret, length, character in front, character at the caret
    cmp dword [rsp+RK_CLS], -1
    je .nlalloc
    mov ecx, [rsp+RK_CLS+8]
    mov edx, ')'
    cmp ecx, '('
    je .nlblk
    mov edx, ']'
    cmp ecx, '['
    je .nlblk
    mov edx, '}'
    cmp ecx, '{'
    jne .nlalloc
.nlblk:
    mov dword [rsp+RK_A], 4          ; four blanks; a tab when the line is indented with tabs
    mov dword [rsp+RK_B], 32
    test edi, edi
    jz .nlb1
    mov rax, [rsp+RK_T]
    cmp word [rax], 9
    jne .nlb1
    mov dword [rsp+RK_A], 1
    mov dword [rsp+RK_B], 9
.nlb1:
    cmp edx, [rsp+RK_CLS+12]
    jne .nlalloc
    mov dword [rsp+RK_C], 1
.nlalloc:
    lea rcx, [rdi*4+32]              ; at most: break, indent, level, break, indent
    call malloc
    mov [rsp+RK_N], rax
    cmp esi, 12
    je .nlup
    mov word [rax], 13               ; a line break, then the indent
    lea rcx, [rax+2]
    lea r8, [rdi*2]
    invoke memcpy, rcx, [rsp+RK_T], r8
    mov rax, [rsp+RK_N]
    lea rdx, [rax+rdi*2+2]
    mov ecx, [rsp+RK_A]
    mov r8d, [rsp+RK_B]
.nlu:
    test ecx, ecx
    jz .nlu1
    mov [rdx], r8w
    add rdx, 2
    dec ecx
    jmp .nlu
.nlu1:
    mov word [rdx], 0
    cmp dword [rsp+RK_C], 0
    je .nlz
    mov word [rdx], 13               ; the closing bracket: on the next line, at the indent of this one
    lea rcx, [rdx+2]
    lea r8, [rdi*2]
    mov word [rcx+r8], 0
    invoke memcpy, rcx, [rsp+RK_T], r8
.nlz:
    add edi, [rsp+RK_A]              ; the caret goes behind the indent and the level
    cmp esi, 11
    je .nlafter
    mov [rsp+RK_RNG], r14d           ; the selection is replaced
    mov [rsp+RK_RNG+4], r15d
    mov eax, r14d
    cmp esi, 13                      ; split: the caret stays in front of the break
    je .nlsel
    lea eax, [r14+rdi+1]
    jmp .nlsel
.nlafter:
    mov eax, [rsp+RK_BLK+12]         ; behind the line, whatever follows the caret stays on it
    mov [rsp+RK_RNG], eax
    mov [rsp+RK_RNG+4], eax
    lea eax, [rax+rdi+1]
    jmp .nlsel
.nlup:
    lea r8, [rdi*2]                  ; the indent, then a line break, in front of the line
    invoke memcpy, rax, [rsp+RK_T], r8
    mov rax, [rsp+RK_N]
    mov word [rax+rdi*2], 13
    mov word [rax+rdi*2+2], 0
    mov eax, [rsp+RK_BLK+8]
    mov [rsp+RK_RNG], eax
    mov [rsp+RK_RNG+4], eax
    add eax, edi
.nlsel:
    mov [rsp+RK_SEL], eax
    mov [rsp+RK_SEL+4], eax

.put:
    invoke rk_put, rbx, &rsp+RK_RNG, [rsp+RK_N], &rsp+RK_SEL
.done:
    invoke free, [rsp+RK_T]
    invoke free, [rsp+RK_N]
    mov eax, 1
    EPILOG 208
.pass:                               ; an editor pane, but the key keeps its usual meaning here
    invoke free, [rsp+RK_T]
    invoke free, [rsp+RK_N]
.no:
    xor eax, eax
    EPILOG 208
