; rich.asm - RichEdit helpers shared by the text, merge and hex views
%include "inc/macros.inc"
%include "inc/win.inc"

extern SendMessageW, memset, malloc, free, wcslen
extern mk
extern g_mono, th_bg, th_fg

global rich_fg, rich_create, rich_set_text, rich_get_text, rich_bk, rich_theme, rich_len, rich_unsel

%define EM_EXLIMITTEXT      0x0435
%define EM_GETTEXTEX        0x045E
%define EM_GETTEXTLENGTHEX  0x045F
%define CFE_AUTOBACKCOLOR   0x04000000

section .rdata
WSTR cn_rich, "RICHEDIT50W"

section .text

; rich_create(parent, id) -> hwnd
rich_create:
    PROLOG 64
    mov rsi, rdx
    invoke mk, rcx, &cn_rich, 0, WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_NOHIDESEL|ES_WANTRETURN|WS_TABSTOP, rsi
    mov rbx, rax
    test rax, rax
    jz .d
    invoke SendMessageW, rbx, WM_SETFONT, [g_mono], 1
    invoke SendMessageW, rbx, EM_SETEVENTMASK, 0, 0x80004          ; ENM_SCROLL | ENM_SELCHANGE
    invoke SendMessageW, rbx, EM_EXLIMITTEXT, 0, 0x7FFFFFF0
    invoke SendMessageW, rbx, EM_SETMODIFY, 0, 0
.d:
    mov rax, rbx
    EPILOG 64

; rich_set_text(h, wide text): replace everything, clear undo/modify
rich_set_text:
    PROLOG 96
    mov rbx, rcx
    mov rsi, rdx
    mov dword [rsp+96-16], 0         ; flags = ST_DEFAULT
    mov dword [rsp+96-12], 1200      ; codepage = UTF-16
    invoke SendMessageW, rbx, EM_SETTEXTEX, &rsp+80, rsi
    invoke SendMessageW, rbx, EM_EMPTYUNDOBUFFER, 0, 0
    invoke SendMessageW, rbx, EM_SETMODIFY, 0, 0
    EPILOG 96

; rich_get_text(h, &len) -> malloc'd wide string ('' line breaks), *len = characters
rich_get_text:
    PROLOG 160
    mov rbx, rcx
    mov r12, rdx
    invoke SendMessageW, rbx, WM_GETTEXTLENGTH, 0, 0
    lea r13, [rax*2+64]              ; capacity in characters (generous: CRLF may count twice)
    lea rcx, [r13*2+8]
    call malloc
    mov rsi, rax
    test rax, rax
    jz .fail
    mov word [rsi], 0
    invoke memset, &rsp+104, 0, 32   ; GETTEXTEX
    lea rax, [r13*2]
    mov [rsp+104], eax               ; cb in bytes
    mov dword [rsp+108], 0           ; flags = GT_DEFAULT
    mov dword [rsp+112], 1200        ; codepage
    invoke SendMessageW, rbx, EM_GETTEXTEX, &rsp+104, rsi
    invoke wcslen, rsi
    mov [r12], rax
    mov rax, rsi
    EPILOG 160
.fail:
    mov qword [r12], 0
    xor eax, eax
    EPILOG 160

; rich_bk(h, cpMin, cpMax, color): background colour for a character range
rich_bk:
    PROLOG 224
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov [rsp+96], r12d               ; CHARRANGE
    mov [rsp+100], r13d
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, &rsp+96
    invoke memset, &rsp+104, 0, CF_SIZE
    mov dword [rsp+104], CF_SIZE
    mov dword [rsp+104+CF_MASK], CFM_BACKCOLOR
    mov [rsp+104+CF_BACKCOLOR], r14d
    invoke SendMessageW, rbx, EM_SETCHARFORMAT, SCF_SELECTION, &rsp+104
    EPILOG 224

; rich_fg(h, cpMin, cpMax, color): text colour for a character range
rich_fg:
    PROLOG 224
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov [rsp+96], r12d
    mov [rsp+100], r13d
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, &rsp+96
    invoke memset, &rsp+104, 0, CF_SIZE
    mov dword [rsp+104], CF_SIZE
    mov dword [rsp+104+CF_MASK], CFM_COLOR
    mov [rsp+104+CF_TEXTCOLOR], r14d
    invoke SendMessageW, rbx, EM_SETCHARFORMAT, SCF_SELECTION, &rsp+104
    EPILOG 224

; rich_theme(h): background + default text colour
; Recolouring is not an edit: SCF_ALL raises the modify flag, so it is put back afterwards.
rich_theme:
    PROLOG 224
    mov rbx, rcx
    invoke SendMessageW, rbx, EM_GETMODIFY, 0, 0
    mov r12, rax
    invoke SendMessageW, rbx, EM_SETBKGNDCOLOR, 0, #[th_bg]
    invoke memset, &rsp+104, 0, CF_SIZE
    mov dword [rsp+104], CF_SIZE
    mov dword [rsp+104+CF_MASK], CFM_COLOR
    mov eax, [th_fg]
    mov [rsp+104+CF_TEXTCOLOR], eax
    invoke SendMessageW, rbx, EM_SETCHARFORMAT, 0, &rsp+104          ; default format
    invoke SendMessageW, rbx, EM_SETCHARFORMAT, SCF_ALL, &rsp+104
    invoke SendMessageW, rbx, EM_SETMODIFY, r12, 0
    EPILOG 224

; rich_len(h) -> number of characters
rich_len:
    PROLOG 96
    invoke rich_get_text, rcx, &rsp+80
    mov rbx, [rsp+80]
    invoke free, rax
    mov rax, rbx
    EPILOG 96

; rich_unsel(h): clear the selection (colouring leaves the last range selected)
rich_unsel:
    PROLOG 64
    invoke SendMessageW, rcx, EM_SETSEL, 0, 0
    EPILOG 64
