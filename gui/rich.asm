; rich.asm - RichEdit helpers shared by the text, merge and hex views
%include "inc/macros.inc"
%include "inc/win.inc"

extern SendMessageW, GetWindowLongPtrW, memset, malloc, free, wcslen
extern mk
extern g_mono, th_bg, th_fg

global rich_fg, rich_create, rich_set_text, rich_get_text, rich_bk, rich_theme, rich_len, rich_unsel, rich_quiet, rich_batch

%define EM_EXLIMITTEXT      0x0435
%define EM_GETTEXTEX        0x045E
%define EM_GETTEXTLENGTHEX  0x045F
%define CFE_AUTOBACKCOLOR   0x04000000
%define EM_GETOLEINTERFACE  0x043C
; Text Object Model (tom.h): slots of the methods used, in pointers from the start of the vtable
%define TD_FREEZE    18              ; ITextDocument
%define TD_UNFREEZE  19
%define TD_UNDO      22
%define TD_RANGE     24
%define TR_GETFONT   18              ; ITextRange
%define TR_SETRANGE  28
%define TF_SETBACK   19              ; ITextFont
%define TF_SETFORE   25
%define TOM_SUSPEND  -9999995        ; tomSuspend / tomResume for ITextDocument::Undo
%define TOM_RESUME   -9999994
%define RB_N         4               ; controls that can be in a batch at the same time
%define RB_H         0               ; batch entry: control, ITextDocument*, ITextRange*, ITextFont*
%define RB_DOC       8
%define RB_RANGE     16
%define RB_FONT      24
%define RB_RO        32              ; the control was read-only and is made so again at the end
%define RB_SIZE      40
%define EM_SETREADONLY_ 0x00CF
%define RICH_EVENTS         0x80004             ; ENM_SCROLL | ENM_SELCHANGE

section .rdata
WSTR cn_rich, "RICHEDIT50W"
align 8
iid_textdoc: dd 0x8CC497C0           ; IID_ITextDocument {8CC497C0-A1DF-11CE-8098-00AA0047BE5D}
             dw 0xA1DF, 0x11CE
             db 0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D

section .bss
alignb 8
rb_tab: resb RB_N*RB_SIZE

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
    invoke SendMessageW, rbx, EM_SETEVENTMASK, 0, RICH_EVENTS
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

; ---------------------------------------------------------------- batch colouring
; Colouring through the selection (EM_EXSETSEL + EM_SETCHARFORMAT) costs time in proportion to the size of the
; document for every span: 20,000 spans in 200,000 lines took over 4 seconds, and each one was an undo step.
; Inside rich_batch(h, 1) .. rich_batch(h, 0), rich_bk and rich_fg go through a text range of the control's
; Text Object Model instead: the selection stays where it is, the display is frozen and undo is suspended
; (the same 20,000 spans: about 0.01 seconds). Outside a batch they work through the selection as before.

; rb_find(rcx = control, 0 for a free entry) -> rax entry or 0
rb_find:
    lea rax, [rb_tab]
    mov edx, RB_N
.l:
    cmp [rax+RB_H], rcx
    je .d
    add rax, RB_SIZE
    dec edx
    jnz .l
    xor eax, eax
.d:
    ret

; rb_close(r12 = document, r13 = range or 0, r14 = font or 0): undo everything rich_batch(h, 1) did
rb_close:
    PROLOG 64
    test r14, r14
    jz .nf
    mov rcx, r14
    mov rax, [r14]
    call [rax+16]                    ; Release
.nf:
    test r13, r13
    jz .nr
    mov rcx, r13
    mov rax, [r13]
    call [rax+16]
.nr:
    mov rcx, r12
    lea rdx, [rsp+48]
    mov rax, [r12]
    call [rax+TD_UNFREEZE*8]
    mov rcx, r12
    mov edx, TOM_RESUME
    lea r8, [rsp+48]
    mov rax, [r12]
    call [rax+TD_UNDO*8]
    mov rcx, r12
    mov rax, [r12]
    call [rax+16]
    EPILOG 64

; rich_batch(h, on): on = 1 starts a batch for the control, on = 0 ends it.
; When the Text Object Model is not available nothing is registered and colouring uses the selection.
; A read-only control refuses format changes through a text range (E_ACCESSDENIED), so it is made
; writable for the length of the batch.
rich_batch:
    PROLOG 96
    mov rbx, rcx
    test edx, edx
    jz .end
    test rbx, rbx
    jz .d
    call rb_find                     ; already in a batch
    test rax, rax
    jnz .d
    xor ecx, ecx
    call rb_find                     ; free entry
    test rax, rax
    jz .d
    mov rsi, rax
    xor r15d, r15d
    invoke GetWindowLongPtrW, rbx, -16           ; GWL_STYLE
    test eax, 0x0800                 ; ES_READONLY
    jz .rw
    mov r15d, 1
    invoke SendMessageW, rbx, EM_SETREADONLY_, 0, 0
.rw:
    mov qword [rsp+80], 0
    invoke SendMessageW, rbx, EM_GETOLEINTERFACE, 0, &rsp+80
    mov rdi, [rsp+80]                ; IRichEditOle*
    test rdi, rdi
    jz .fail
    mov qword [rsp+72], 0
    mov rcx, rdi
    lea rdx, [iid_textdoc]
    lea r8, [rsp+72]
    mov rax, [rdi]
    call [rax]                       ; QueryInterface
    mov r12d, eax
    mov rcx, rdi
    mov rax, [rdi]
    call [rax+16]                    ; Release
    test r12d, r12d
    js .fail
    mov r12, [rsp+72]                ; ITextDocument*
    test r12, r12
    jz .fail
    xor r13d, r13d
    xor r14d, r14d
    mov rcx, r12
    mov edx, TOM_SUSPEND
    lea r8, [rsp+64]
    mov rax, [r12]
    call [rax+TD_UNDO*8]
    mov rcx, r12
    lea rdx, [rsp+64]
    mov rax, [r12]
    call [rax+TD_FREEZE*8]
    mov qword [rsp+72], 0
    mov rcx, r12
    xor edx, edx
    xor r8d, r8d
    lea r9, [rsp+72]
    mov rax, [r12]
    call [rax+TD_RANGE*8]
    mov r13, [rsp+72]                ; ITextRange*
    test r13, r13
    jz .undo
    mov qword [rsp+72], 0
    mov rcx, r13
    lea rdx, [rsp+72]
    mov rax, [r13]
    call [rax+TR_GETFONT*8]
    mov r14, [rsp+72]                ; ITextFont* of the range: follows it when the range moves
    test r14, r14
    jz .undo
    mov [rsi+RB_H], rbx
    mov [rsi+RB_DOC], r12
    mov [rsi+RB_RANGE], r13
    mov [rsi+RB_FONT], r14
    mov [rsi+RB_RO], r15
    EPILOG 96
.undo:
    call rb_close
.fail:
    test r15d, r15d
    jz .d
    invoke SendMessageW, rbx, EM_SETREADONLY_, 1, 0
    EPILOG 96
.end:
    test rbx, rbx
    jz .d
    call rb_find
    test rax, rax
    jz .d
    mov r12, [rax+RB_DOC]
    mov r13, [rax+RB_RANGE]
    mov r14, [rax+RB_FONT]
    mov r15, [rax+RB_RO]
    mov qword [rax+RB_H], 0
    call rb_close
    test r15, r15
    jz .d
    invoke SendMessageW, rbx, EM_SETREADONLY_, 1, 0
.d:
    EPILOG 96

; rb_set(rcx = h, edx = cpMin, r8d = cpMax, r9d = colour, method slot in eax) -> eax 1 when done in a batch
rb_set:
    PROLOG 64
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov r15d, eax
    test rcx, rcx
    jz .no
    call rb_find
    test rax, rax
    jz .no
    mov rsi, rax
    mov rdi, [rsi+RB_RANGE]
    mov rcx, rdi
    mov edx, r12d
    mov r8d, r13d
    mov rax, [rdi]
    call [rax+TR_SETRANGE*8]
    mov rdi, [rsi+RB_FONT]
    mov rcx, rdi
    mov edx, r14d
    mov rax, [rdi]
    call [rax+r15*8]
    test eax, eax
    js .no                           ; refused: the caller colours through the selection
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; rich_bk(h, cpMin, cpMax, color): background colour for a character range
rich_bk:
    PROLOG 224
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov eax, TF_SETBACK
    call rb_set
    test eax, eax
    jnz .d
    mov [rsp+96], r12d               ; CHARRANGE
    mov [rsp+100], r13d
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, &rsp+96
    invoke memset, &rsp+104, 0, CF_SIZE
    mov dword [rsp+104], CF_SIZE
    mov dword [rsp+104+CF_MASK], CFM_BACKCOLOR
    mov [rsp+104+CF_BACKCOLOR], r14d
    invoke SendMessageW, rbx, EM_SETCHARFORMAT, SCF_SELECTION, &rsp+104
.d:
    EPILOG 224

; rich_fg(h, cpMin, cpMax, color): text colour for a character range
rich_fg:
    PROLOG 224
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov eax, TF_SETFORE
    call rb_set
    test eax, eax
    jnz .d
    mov [rsp+96], r12d
    mov [rsp+100], r13d
    invoke SendMessageW, rbx, EM_EXSETSEL, 0, &rsp+96
    invoke memset, &rsp+104, 0, CF_SIZE
    mov dword [rsp+104], CF_SIZE
    mov dword [rsp+104+CF_MASK], CFM_COLOR
    mov [rsp+104+CF_TEXTCOLOR], r14d
    invoke SendMessageW, rbx, EM_SETCHARFORMAT, SCF_SELECTION, &rsp+104
.d:
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

; rich_quiet(h, on): on = 1 stops the scroll / selection notifications, on = 0 restores them.
; Colouring moves the selection once per span and each move scrolls the control: with notifications on, every
; one of them reached the parent (which then scrolled the other pane and posted itself a redraw request).
rich_quiet:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
    test edx, edx
    jnz .set
    mov esi, RICH_EVENTS
.set:
    invoke SendMessageW, rbx, EM_SETEVENTMASK, 0, rsi
    EPILOG 64

; rich_unsel(h): clear the selection (colouring leaves the last range selected)
rich_unsel:
    PROLOG 64
    invoke SendMessageW, rcx, EM_SETSEL, 0, 0
    EPILOG 64
