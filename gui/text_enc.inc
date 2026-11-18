; text_enc.inc - Text Compare: choosing the encoding of files that are not Unicode
;
;   The loader recognises UTF-8 (with or without a byte order mark) and UTF-16 with a mark by itself. For
;   everything else it has to guess: the system's ANSI code page when the bytes are valid there, else byte for
;   byte (Latin-1). A guess can be wrong - a Shift-JIS file is often valid in the Korean code page too - so
;   the combo box in the toolbar names the code page for those files. It applies to both sides (a Unicode
;   file next to a legacy one keeps being read as Unicode) and is used again when the file is saved.
;   The choice goes to the loader through g_tl_cp (textlib.asm) and is kept in TXT+28 (bits 8..).

section .rdata
WSTR cn_enc_combo, "COMBOBOX"
WSTR s_enc_0, "인코딩: 자동"
WSTR s_enc_1, "한국어 (CP949)"
WSTR s_enc_2, "일본어 (Shift-JIS)"
WSTR s_enc_3, "중국어 간체 (GBK)"
WSTR s_enc_4, "중국어 번체 (Big5)"
WSTR s_enc_5, "서유럽 (1252)"
WSTR s_enc_6, "중앙유럽 (1250)"
WSTR s_enc_7, "키릴 (1251)"
WSTR s_enc_8, "UTF-16 LE (표식 없음)"
WSTR s_enc_9, "바이트 그대로 (Latin-1)"
WSTR s_enc_ask, "인코딩을 바꾸면 두 파일을 다시 읽습니다. 저장하지 않은 편집 내용이 사라집니다. 계속할까요?"
WSTR f_enc_cp, "CP%u"
align 8
enc_names_tab: dq s_enc_0, s_enc_1, s_enc_2, s_enc_3, s_enc_4, s_enc_5, s_enc_6, s_enc_7, s_enc_8, s_enc_9
enc_cp_tab: dd 0, 949, 932, 936, 950, 1252, 1250, 1251, 1200, 28591
align 8
section .text

%define ENC_N 10

; tv_enc_create(hwnd)
tv_enc_create:
    PROLOG 64
    mov rsi, rcx
    invoke mk, rsi, &cn_enc_combo, 0, 0x00210003, ID_CMB_ENC      ; CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP
    mov rbx, rax
    xor r12d, r12d
.a:
    lea rax, [enc_names_tab]
    mov r13, [rax+r12*8]
    invoke SendMessageW, rbx, 0x0143, 0, r13           ; CB_ADDSTRING
    inc r12d
    cmp r12d, ENC_N
    jb .a
    invoke SendMessageW, rbx, 0x014E, 0, 0             ; CB_SETCURSEL: automatic
    EPILOG 64

; tv_enc_get(hwnd) -> eax code page chosen in the combo box (0: automatic)
tv_enc_get:
    PROLOG 64
    invoke GetDlgItem, rcx, ID_CMB_ENC
    invoke SendMessageW, rax, 0x0147, 0, 0             ; CB_GETCURSEL
    cmp eax, ENC_N
    jb .ok
    xor eax, eax
    EPILOG 64
.ok:
    lea rcx, [enc_cp_tab]
    mov eax, [rcx+rax*4]
    EPILOG 64

; tv_enc_changed(hwnd, tv, sess): another encoding was chosen - read both files again and compare
tv_enc_changed:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    cmp dword [rbx+TV_LDA], 0        ; nothing loaded yet: the next compare uses the choice
    je .d
    invoke SendMessageW, [rbx+TV_HA], EM_GETMODIFY, 0, 0
    mov r13d, eax
    invoke SendMessageW, [rbx+TV_HB], EM_GETMODIFY, 0, 0
    or r13d, eax
    jz .go
    invoke msgbox, rsi, &s_enc_ask, MB_YESNO|MB_ICONWARNING
    cmp eax, IDYES
    je .go
    ; no: the box goes back to the encoding the files were read with
    invoke GetDlgItem, rsi, ID_CMB_ENC
    mov r13, rax
    xor r14d, r14d
.find:
    lea rcx, [enc_cp_tab]
    mov eax, [rcx+r14*4]
    cmp rax, [rbx+TV_CP]
    je .set
    inc r14d
    cmp r14d, ENC_N
    jb .find
    xor r14d, r14d
.set:
    invoke SendMessageW, r13, 0x014E, r14, 0           ; CB_SETCURSEL
    EPILOG 64
.go:
    ; forget the texts: text_side reads the files again (with the code page of the box)
    invoke free, [rbx+TV_TXA]
    mov qword [rbx+TV_TXA], 0
    mov dword [rbx+TV_LDA], 0
    invoke free, [rbx+TV_TXB]
    mov qword [rbx+TV_TXB], 0
    mov dword [rbx+TV_LDB], 0
    mov qword [rbx+TV_FOLDED], 0
    invoke SendMessageW, [rbx+TV_HA], EM_SETMODIFY, 0, 0
    invoke SendMessageW, [rbx+TV_HB], EM_SETMODIFY, 0, 0
    invoke text_compare, rsi, rbx, r12
.d:
    EPILOG 64
