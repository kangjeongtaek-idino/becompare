; export_xml.asm - XML builders on top of export.asm's xlsx_save / docx_save
;   xlsx: xw_begin / xw_cell / xw_numcell / xw_endrow / xw_save      (see inc/export.inc)
;   docx: dw_begin / dw_para / dw_tbl_begin / dw_row_begin / dw_cell / dw_row_end / dw_tbl_end / dw_save
;   The caller adds rows; xw_save / dw_save wrap them into the sheet / document XML and hand it to export.asm.
%include "inc/macros.inc"
%include "inc/win.inc"
%define EXPORT_IMPL 1
%include "inc/export.inc"

extern malloc, free, wcslen, _snwprintf
extern sb_init, sb_append, sb_free, browse_save_as, set_status, msgbox
extern xlsx_save, docx_save

global ex_pick, ex_done, ex_esc, xw_colname, s_xw_cap
global xw_begin, xw_cell, xw_numcell, xw_endrow, xw_save
global dw_begin, dw_para, dw_tbl_begin, dw_row_begin, dw_cell, dw_row_end, dw_tbl_end, dw_save
global dwc_red, dwc_green, dwc_blue, dwc_orange, dwc_gray

section .rdata
WSTR s_ex_xlsx, "xlsx"
WSTR s_ex_docx, "docx"
flt_xlsx: dw __utf16__("Excel 통합 문서 (*.xlsx)"), 0, __utf16__("*.xlsx"), 0, 0
flt_docx: dw __utf16__("Word 문서 (*.docx)"), 0, __utf16__("*.docx"), 0, 0
WSTR s_ex_ok, "내보내기를 완료했습니다."
WSTR s_ex_fail, "파일을 저장하지 못했습니다. 경로와 쓰기 권한, 파일이 다른 프로그램에서 열려 있는지 확인하세요."
WSTR s_xw_cap, "행이 많아 처음 1,048,575행만 내보냈습니다 (Excel 시트의 최대 행 수)."
WSTR e_amp, "&amp;"
WSTR e_lt, "&lt;"
WSTR e_gt, "&gt;"
WSTR e_quot, "&quot;"
WSTR s_empty, ""
WSTR dwc_red, "FFD7D7"
WSTR dwc_green, "D7F0D7"
WSTR dwc_blue, "D7E1FF"
WSTR dwc_orange, "FFE6BE"
WSTR dwc_gray, "E6E6E6"

; ---- xlsx wide-char fragments
WSTR f_xrow, `<row r="%u">`
WSTR s_xrowend, `</row>`
WSTR f_xcell, `<c r="%s%u" s="%u" t="inlineStr"><is><t xml:space="preserve">`
WSTR s_xcellend, `</t></is></c>`
WSTR f_xnum, `<c r="%s%u" s="%u"><v>`
WSTR s_xnumend, `</v></c>`
WSTR s_xs_head, `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetViews><sheetView workbookViewId="0"><pane ySplit="1" topLeftCell="A2" activePane="bottomLeft" state="frozen"/></sheetView></sheetViews><sheetFormatPr defaultRowHeight="15"/><cols>`
WSTR f_xcol, `<col min="%u" max="%u" width="%u" customWidth="1"/>`
WSTR s_xs_data, `</cols><sheetData>`
WSTR s_xs_end1, `</sheetData>`
WSTR s_xs_af1, `<autoFilter ref="A1:`
WSTR s_xs_af2, `"/>`
WSTR s_xs_end2, `</worksheet>`
WSTR f_xref, `%s%u`
WSTR s_xw_head, `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><sheets><sheet name="`
WSTR s_xw_tail, `" sheetId="1" r:id="rId1"/></sheets></workbook>`

; ---- docx wide-char fragments
WSTR s_dx_docH, `<?xml version="1.0" encoding="UTF-8" standalone="yes"?><w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body>`
WSTR s_dx_docT, `<w:sectPr><w:pgSz w:w="16838" w:h="11906" w:orient="landscape"/><w:pgMar w:top="850" w:right="850" w:bottom="850" w:left="850" w:header="708" w:footer="708" w:gutter="0"/></w:sectPr></w:body></w:document>`
WSTR f_dpara, `<w:p><w:pPr><w:spacing w:before="80" w:after="80"/></w:pPr><w:r><w:rPr>%s<w:sz w:val="%u"/></w:rPr><w:t xml:space="preserve">`
WSTR s_dparaend, `</w:t></w:r></w:p>`
WSTR s_dbold, `<w:b/>`
WSTR s_dtblH, `<w:tbl><w:tblPr><w:tblW w:w="0" w:type="auto"/><w:tblBorders><w:top w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:left w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:bottom w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:right w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:insideH w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:insideV w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/></w:tblBorders><w:tblLayout w:type="fixed"/><w:tblCellMar><w:left w:w="60" w:type="dxa"/><w:right w:w="60" w:type="dxa"/></w:tblCellMar></w:tblPr><w:tblGrid>`
WSTR f_dgrid, `<w:gridCol w:w="%u"/>`
WSTR s_dgridend, `</w:tblGrid>`
WSTR s_dtr, `<w:tr>`
WSTR s_dtrh, `<w:tr><w:trPr><w:cantSplit/><w:tblHeader/></w:trPr>`
WSTR s_dtrend, `</w:tr>`
WSTR s_dtblend, `</w:tbl><w:p/>`
WSTR f_dtc, `<w:tc><w:tcPr><w:tcW w:w="%u" w:type="dxa"/>`
WSTR f_dshd, `<w:shd w:val="clear" w:color="auto" w:fill="%s"/>`
WSTR f_dtc2, `</w:tcPr><w:p><w:r><w:rPr>%s</w:rPr><w:t xml:space="preserve">`
WSTR s_dtcend, `</w:t></w:r></w:p></w:tc>`

section .text

; ================================================================ XML text
; ex_esc(rcx=sb, rdx=wide text): XML-escaped append; control characters and U+FFFE / U+FFFF dropped,
; 32000 chars max (Excel cell limit)
%macro FLUSHBUF 0
    mov word [rsp+96+rdi*2], 0
    invoke sb_append, rbx, &rsp+96
    xor edi, edi
%endmacro

ex_esc:
    PROLOG 608
    mov rbx, rcx
    mov rsi, rdx
    xor edi, edi
    xor r12d, r12d
.l:
    movzx eax, word [rsi]
    test eax, eax
    jz .end
    add rsi, 2
    inc r12d
    cmp r12d, 32000
    ja .end
    cmp eax, 32
    jae .ok
    cmp eax, 9
    je .ok
    cmp eax, 10
    je .ok
    cmp eax, 13
    je .ok
    jmp .l
.ok:
    cmp eax, 0xFFFE                  ; U+FFFE / U+FFFF are not XML characters: Excel and Word refuse the file
    jae .l
    cmp eax, '&'
    je .amp
    cmp eax, '<'
    je .lt
    cmp eax, '>'
    je .gt
    cmp eax, '"'
    je .q
    mov [rsp+96+rdi*2], ax
    inc edi
    cmp edi, 240
    jb .l
    FLUSHBUF
    jmp .l
.amp:
    FLUSHBUF
    invoke sb_append, rbx, &e_amp
    jmp .l
.lt:
    FLUSHBUF
    invoke sb_append, rbx, &e_lt
    jmp .l
.gt:
    FLUSHBUF
    invoke sb_append, rbx, &e_gt
    jmp .l
.q:
    FLUSHBUF
    invoke sb_append, rbx, &e_quot
    jmp .l
.end:
    FLUSHBUF
    EPILOG 608

; ================================================================ common dialog helpers
; ex_pick(hwnd, out wchar[520], kind) -> eax 1 when a path was chosen (kind 0 xlsx / 1 docx)
ex_pick:
    PROLOG 64
    mov rsi, rdx
    mov eax, r8d
    mov word [rsi], 0
    lea r8, [flt_xlsx]
    lea r9, [s_ex_xlsx]
    test eax, eax
    jz .go
    lea r8, [flt_docx]
    lea r9, [s_ex_docx]
.go:
    invoke browse_save_as, rcx, rsi, r8, r9
    EPILOG 64

; ex_done(hwnd, ok)
ex_done:
    PROLOG 64
    test edx, edx
    jz .fail
    invoke set_status, &s_ex_ok
    EPILOG 64
.fail:
    invoke msgbox, rcx, &s_ex_fail, MB_OK|MB_ICONERROR
    EPILOG 64

; ================================================================ XLSX
; XL ctx: { SB sheetData (0); dword row (24); dword col (28); dword maxcol (32); dword rowopen (36) }

; xw_begin(rcx=ctx)
xw_begin:
    xor eax, eax
    mov [rcx], rax
    mov [rcx+8], rax
    mov [rcx+16], rax
    mov dword [rcx+24], 1
    mov dword [rcx+28], 0
    mov dword [rcx+32], 0
    mov dword [rcx+36], 0
    ret

; xw_colname(rcx=wchar buf, edx=zero based column) -> eax length
xw_colname:
    mov eax, edx
    inc eax
    xor r8d, r8d
.l:
    dec eax
    xor edx, edx
    mov r9d, 26
    div r9d
    add edx, 'A'
    mov [rcx+r8*2], dx
    inc r8d
    test eax, eax
    jnz .l
    mov word [rcx+r8*2], 0
    xor r10d, r10d
    lea r11d, [r8-1]
.r:
    cmp r10d, r11d
    jae .d
    movzx eax, word [rcx+r10*2]
    movzx edx, word [rcx+r11*2]
    mov [rcx+r10*2], dx
    mov [rcx+r11*2], ax
    inc r10d
    dec r11d
    jmp .r
.d:
    mov eax, r8d
    ret

; xw_open_cell(rbx=ctx, rsi=format, buf=rsp+96 (wchar[64]), style in r12d): shared start of a cell
; (inlined by xw_cell / xw_numcell through this helper taking rcx=ctx, rdx=format, r8d=style, r9=scratch wchar[112]:
;  chars 0..95 hold the formatted cell start, chars 100..111 the column letters)
xw_start:
    PROLOG 96
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    mov r13, r9
    cmp dword [rbx+36], 0
    jne .open
    invoke _snwprintf, r13, 32, &f_xrow, [rbx+24]
    invoke sb_append, rbx, r13
    mov dword [rbx+36], 1
.open:
    lea r14, [r13+200]                ; column letters
    invoke xw_colname, r14, [rbx+28]
    ; the longest cell start is 55 chars + column letters + row digits + style digits: 64 chars cut it
    ; from row 100,000 on (no terminator, so the column letters leaked into the cell text)
    invoke _snwprintf, r13, 95, rsi, r14, [rbx+24], r12
    mov word [r13+190], 0
    invoke sb_append, rbx, r13
    inc dword [rbx+28]
    mov eax, [rbx+28]
    cmp eax, [rbx+32]
    jbe .d
    mov [rbx+32], eax
.d:
    EPILOG 96

; xw_cell(rcx=ctx, rdx=wide text, r8d=style): text cell (inline string)
xw_cell:
    PROLOG 400
    mov rbx, rcx
    mov rdi, rdx
    invoke xw_start, rbx, &f_xcell, r8, &rsp+96
    test rdi, rdi
    jz .nt
    invoke ex_esc, rbx, rdi
.nt:
    invoke sb_append, rbx, &s_xcellend
    EPILOG 400

; xw_numcell(rcx=ctx, rdx=digits text, r8d=style): numeric cell; an empty text gives an empty styled cell
xw_numcell:
    PROLOG 400
    mov rbx, rcx
    mov rdi, rdx
    mov r12d, r8d
    test rdi, rdi
    jz .txt
    cmp word [rdi], 0
    je .txt
    invoke xw_start, rbx, &f_xnum, r12, &rsp+96
    invoke ex_esc, rbx, rdi
    invoke sb_append, rbx, &s_xnumend
    EPILOG 400
.txt:
    invoke xw_cell, rbx, &s_empty, r12
    EPILOG 400

; xw_endrow(rcx=ctx)
xw_endrow:
    PROLOG 32
    mov rbx, rcx
    cmp dword [rbx+36], 0
    je .n
    invoke sb_append, rbx, &s_xrowend
    mov dword [rbx+36], 0
.n:
    inc dword [rbx+24]
    mov dword [rbx+28], 0
    EPILOG 32

; xw_save(rcx=ctx, rdx=path, r8=widths (dword[] in characters), r9d=ncols) -> eax 1 when written
%define XS_S1   96                    ; sheet xml builder (24)
%define XS_T1   128                   ; wchar[64]
%define XS_T2   256                   ; wchar[32]
xw_save:
    PROLOG 352
    mov rbx, rcx
    mov r12, rdx
    mov r14, r8
    mov r15d, r9d
    invoke sb_init, &rsp+XS_S1
    invoke sb_append, &rsp+XS_S1, &s_xs_head
    xor edi, edi
.cols:
    cmp edi, r15d
    jae .colsd
    mov ebp, [r14+rdi*4]
    lea esi, [rdi+1]
    invoke _snwprintf, &rsp+XS_T1, 64, &f_xcol, rsi, rsi, rbp
    invoke sb_append, &rsp+XS_S1, &rsp+XS_T1
    inc edi
    jmp .cols
.colsd:
    invoke sb_append, &rsp+XS_S1, &s_xs_data
    cmp qword [rbx+8], 0
    je .nodata
    invoke sb_append, &rsp+XS_S1, [rbx]
.nodata:
    invoke sb_append, &rsp+XS_S1, &s_xs_end1
    cmp dword [rbx+24], 2
    jb .noaf
    cmp dword [rbx+32], 0
    je .noaf
    mov edx, [rbx+32]
    dec edx
    invoke xw_colname, &rsp+XS_T1, rdx
    mov eax, [rbx+24]
    dec eax
    invoke _snwprintf, &rsp+XS_T2, 32, &f_xref, &rsp+XS_T1, rax
    invoke sb_append, &rsp+XS_S1, &s_xs_af1
    invoke sb_append, &rsp+XS_S1, &rsp+XS_T2
    invoke sb_append, &rsp+XS_S1, &s_xs_af2
.noaf:
    invoke sb_append, &rsp+XS_S1, &s_xs_end2
    invoke xlsx_save, r12, [rsp+XS_S1], [rsp+XS_S1+8]
    mov ebx, eax
    invoke sb_free, &rsp+XS_S1
    mov eax, ebx
    EPILOG 352

; ================================================================ DOCX
; DX ctx: { SB body (0); dword* widths (24); dword ncols (32); dword col (36) }

; dw_begin(rcx=ctx)
dw_begin:
    xor eax, eax
    mov [rcx], rax
    mov [rcx+8], rax
    mov [rcx+16], rax
    mov [rcx+24], rax
    mov [rcx+32], rax
    ret

; dw_para(rcx=ctx, rdx=text, r8d=size in half points, r9d=bold)
dw_para:
    PROLOG 560
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    lea rdi, [s_empty]
    test r9d, r9d
    jz .nb
    lea rdi, [s_dbold]
.nb:
    invoke _snwprintf, &rsp+96, 220, &f_dpara, rdi, r12
    invoke sb_append, rbx, &rsp+96
    test rsi, rsi
    jz .nt
    invoke ex_esc, rbx, rsi
.nt:
    invoke sb_append, rbx, &s_dparaend
    EPILOG 560

; dw_tbl_begin(rcx=ctx, rdx=widths (dword[] in twips), r8d=ncols)
dw_tbl_begin:
    PROLOG 160
    mov rbx, rcx
    mov [rbx+24], rdx
    mov [rbx+32], r8d
    mov r12, rdx
    mov r13d, r8d
    invoke sb_append, rbx, &s_dtblH
    xor edi, edi
.g:
    cmp edi, r13d
    jae .gd
    mov esi, [r12+rdi*4]
    invoke _snwprintf, &rsp+96, 32, &f_dgrid, rsi
    invoke sb_append, rbx, &rsp+96
    inc edi
    jmp .g
.gd:
    invoke sb_append, rbx, &s_dgridend
    EPILOG 160

; dw_row_begin(rcx=ctx, edx=header row)
dw_row_begin:
    PROLOG 32
    mov rbx, rcx
    lea rax, [s_dtr]
    test edx, edx
    jz .n
    lea rax, [s_dtrh]
.n:
    invoke sb_append, rbx, rax
    mov dword [rbx+36], 0
    EPILOG 32

; dw_cell(rcx=ctx, rdx=text, r8=fill hex (wide) or 0, r9d=bold)
dw_cell:
    PROLOG 400
    mov rbx, rcx
    mov rsi, rdx
    mov r12, r8
    mov r13d, r9d
    mov eax, 1500
    mov ecx, [rbx+36]
    cmp ecx, [rbx+32]
    jae .w
    mov rdx, [rbx+24]
    mov eax, [rdx+rcx*4]
.w:
    inc dword [rbx+36]
    invoke _snwprintf, &rsp+96, 100, &f_dtc, rax
    invoke sb_append, rbx, &rsp+96
    test r12, r12
    jz .nofill
    invoke _snwprintf, &rsp+96, 100, &f_dshd, r12
    invoke sb_append, rbx, &rsp+96
.nofill:
    lea rdi, [s_empty]
    test r13d, r13d
    jz .nb
    lea rdi, [s_dbold]
.nb:
    invoke _snwprintf, &rsp+96, 100, &f_dtc2, rdi
    invoke sb_append, rbx, &rsp+96
    test rsi, rsi
    jz .nt
    invoke ex_esc, rbx, rsi
.nt:
    invoke sb_append, rbx, &s_dtcend
    EPILOG 400

; dw_row_end(rcx=ctx)
dw_row_end:
    PROLOG 32
    invoke sb_append, rcx, &s_dtrend
    EPILOG 32

; dw_tbl_end(rcx=ctx)
dw_tbl_end:
    PROLOG 32
    invoke sb_append, rcx, &s_dtblend
    EPILOG 32

; dw_save(rcx=ctx, rdx=path) -> eax 1 when written
%define DS_S1  96
dw_save:
    PROLOG 160
    mov rbx, rcx
    mov r12, rdx
    invoke sb_init, &rsp+DS_S1
    invoke sb_append, &rsp+DS_S1, &s_dx_docH
    cmp qword [rbx+8], 0
    je .nb
    invoke sb_append, &rsp+DS_S1, [rbx]
.nb:
    invoke sb_append, &rsp+DS_S1, &s_dx_docT
    invoke docx_save, r12, [rsp+DS_S1], [rsp+DS_S1+8]
    mov ebx, eax
    invoke sb_free, &rsp+DS_S1
    mov eax, ebx
    EPILOG 160
