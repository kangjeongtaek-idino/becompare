; export.asm - OOXML export: stored (uncompressed) ZIP writer + XLSX / DOCX package skeletons
;   xlsx_save(path, sheet xml (wide), chars) / docx_save(path, document xml (wide), chars) -> eax 1 when written
%include "inc/macros.inc"
%include "inc/win.inc"

extern malloc, realloc, free, memcpy, memset, strlen, wcsrchr, _wcsicmp
extern WideCharToMultiByte
extern crc32_update, safe_writev, sb_free

global xlsx_save, docx_save, xlsx_save3, docx_save3, export_kind

; byte buffer BB: { ptr, len, cap }
; zip writer ZW: { BB file (0), BB central directory (24), entry count (48), error flag (56),
;                 data of the last entry kept outside the file buffer: pointer (64), length (72) }
%define ZW_CD    24
%define ZW_COUNT 48
%define ZW_ERR   56
%define ZW_EXTP  64
%define ZW_EXTN  72
%define ZW_SIZE  80

%macro PART 2+
%1: db %2
%1_len equ $-%1
%endmacro

section .rdata
WSTR s_ext_xlsx, ".xlsx"
WSTR s_ext_docx, ".docx"

; ---- part names
pn_types:   db "[Content_Types].xml", 0
pn_rels:    db "_rels/.rels", 0
pn_wb:      db "xl/workbook.xml", 0
pn_wbrels:  db "xl/_rels/workbook.xml.rels", 0
pn_xstyles: db "xl/styles.xml", 0
pn_sheet:   db "xl/worksheets/sheet1.xml", 0
pn_doc:     db "word/document.xml", 0
pn_docrels: db "word/_rels/document.xml.rels", 0
pn_dstyles: db "word/styles.xml", 0

; ---- XLSX fixed parts
PART xp_types, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/><Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/><Override PartName="/xl/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"/></Types>'
PART xp_rels, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>'
PART xp_wb, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"><bookViews><workbookView/></bookViews><sheets><sheet name="비교 결과" sheetId="1" r:id="rId1"/></sheets></workbook>'
PART xp_wbrels, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/><Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>'
; cell formats: 0 default, 1 header, 2..6 text by status (same/diff/left only/right only/type), 7..11 numbers by status, 12 title,
; 13..16 Consolas text (plain / red / green / orange fill) used by the text and hex exports
PART xp_styles, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">', \
  '<fonts count="5"><font><sz val="10"/><name val="맑은 고딕"/></font><font><b/><sz val="10"/><name val="맑은 고딕"/></font><font><sz val="10"/><color rgb="FF808080"/><name val="맑은 고딕"/></font><font><b/><sz val="14"/><name val="맑은 고딕"/></font><font><sz val="10"/><name val="Consolas"/></font></fonts>', \
  '<fills count="7"><fill><patternFill patternType="none"/></fill><fill><patternFill patternType="gray125"/></fill>', \
  '<fill><patternFill patternType="solid"><fgColor rgb="FFD9D9D9"/><bgColor indexed="64"/></patternFill></fill>', \
  '<fill><patternFill patternType="solid"><fgColor rgb="FFFFD7D7"/><bgColor indexed="64"/></patternFill></fill>', \
  '<fill><patternFill patternType="solid"><fgColor rgb="FFD7F0D7"/><bgColor indexed="64"/></patternFill></fill>', \
  '<fill><patternFill patternType="solid"><fgColor rgb="FFD7E1FF"/><bgColor indexed="64"/></patternFill></fill>', \
  '<fill><patternFill patternType="solid"><fgColor rgb="FFFFE6BE"/><bgColor indexed="64"/></patternFill></fill></fills>', \
  '<borders count="2"><border><left/><right/><top/><bottom/><diagonal/></border><border><left style="thin"><color rgb="FFBFBFBF"/></left><right style="thin"><color rgb="FFBFBFBF"/></right><top style="thin"><color rgb="FFBFBFBF"/></top><bottom style="thin"><color rgb="FFBFBFBF"/></bottom><diagonal/></border></borders>', \
  '<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>', \
  '<cellXfs count="17"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>', \
  '<xf numFmtId="0" fontId="1" fillId="2" borderId="1" xfId="0" applyFont="1" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="0" fontId="2" fillId="0" borderId="1" xfId="0" applyFont="1" applyBorder="1"/>', \
  '<xf numFmtId="0" fontId="0" fillId="3" borderId="1" xfId="0" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="0" fontId="0" fillId="4" borderId="1" xfId="0" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="0" fontId="0" fillId="5" borderId="1" xfId="0" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="0" fontId="0" fillId="6" borderId="1" xfId="0" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="3" fontId="2" fillId="0" borderId="1" xfId="0" applyNumberFormat="1" applyFont="1" applyBorder="1"/>', \
  '<xf numFmtId="3" fontId="0" fillId="3" borderId="1" xfId="0" applyNumberFormat="1" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="3" fontId="0" fillId="4" borderId="1" xfId="0" applyNumberFormat="1" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="3" fontId="0" fillId="5" borderId="1" xfId="0" applyNumberFormat="1" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="3" fontId="0" fillId="6" borderId="1" xfId="0" applyNumberFormat="1" applyFill="1" applyBorder="1"/>', \
  '<xf numFmtId="0" fontId="3" fillId="0" borderId="0" xfId="0" applyFont="1"/>',   '<xf numFmtId="0" fontId="4" fillId="0" borderId="1" xfId="0" applyFont="1" applyBorder="1"/>',   '<xf numFmtId="0" fontId="4" fillId="3" borderId="1" xfId="0" applyFont="1" applyFill="1" applyBorder="1"/>',   '<xf numFmtId="0" fontId="4" fillId="4" borderId="1" xfId="0" applyFont="1" applyFill="1" applyBorder="1"/>',   '<xf numFmtId="0" fontId="4" fillId="6" borderId="1" xfId="0" applyFont="1" applyFill="1" applyBorder="1"/></cellXfs>', \
  '<cellStyles count="1"><cellStyle name="Normal" xfId="0" builtinId="0"/></cellStyles></styleSheet>'

; ---- DOCX fixed parts
PART dp_types, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="xml" ContentType="application/xml"/><Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/><Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/></Types>'
PART dp_rels, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/></Relationships>'
PART dp_docrels, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/></Relationships>'
PART dp_styles, '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>', 10, '<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii="맑은 고딕" w:eastAsia="맑은 고딕" w:hAnsi="맑은 고딕" w:cs="맑은 고딕"/><w:sz w:val="16"/><w:szCs w:val="16"/></w:rPr></w:rPrDefault><w:pPrDefault><w:pPr><w:spacing w:after="0" w:line="240" w:lineRule="auto"/></w:pPr></w:pPrDefault></w:docDefaults></w:styles>'

; package description: { part count, main part name, then { name, data, length } per fixed part }
align 8
xl_desc: dq 5, pn_sheet
         dq pn_types,   xp_types,   xp_types_len
         dq pn_rels,    xp_rels,    xp_rels_len
         dq pn_wb,      xp_wb,      xp_wb_len
         dq pn_wbrels,  xp_wbrels,  xp_wbrels_len
         dq pn_xstyles, xp_styles,  xp_styles_len
dx_desc: dq 4, pn_doc
         dq pn_types,   dp_types,   dp_types_len
         dq pn_rels,    dp_rels,    dp_rels_len
         dq pn_docrels, dp_docrels, dp_docrels_len
         dq pn_dstyles, dp_styles,  dp_styles_len

section .text

; export_kind(path) -> eax: 0 other (HTML), 1 .xlsx, 2 .docx
export_kind:
    PROLOG 64
    invoke wcsrchr, rcx, '.'
    test rax, rax
    jz .other
    mov rbx, rax
    invoke _wcsicmp, rbx, &s_ext_xlsx
    test eax, eax
    jz .xlsx
    invoke _wcsicmp, rbx, &s_ext_docx
    test eax, eax
    jz .docx
.other:
    xor eax, eax
    EPILOG 64
.xlsx:
    mov eax, 1
    EPILOG 64
.docx:
    mov eax, 2
    EPILOG 64

; bb_put(bb, data, length) -> eax 1 ok / 0 out of memory
bb_put:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov rax, [rbx+8]
    add rax, rdi
    cmp rax, [rbx+16]
    jbe .copy
    mov rcx, [rbx+16]
    add rcx, rcx
    cmp rcx, rax
    jae .c1
    mov rcx, rax
.c1:
    cmp rcx, 65536
    jae .sz
    mov ecx, 65536
.sz:
    mov r12, rcx
    invoke realloc, [rbx], r12
    test rax, rax
    jz .fail
    mov [rbx], rax
    mov [rbx+16], r12
.copy:
    mov rcx, [rbx]
    add rcx, [rbx+8]
    invoke memcpy, rcx, rsi, rdi
    add [rbx+8], rdi
    mov eax, 1
    EPILOG 64
.fail:
    xor eax, eax
    EPILOG 64

; zw_head(zw, name (ASCII), crc, length): local header and central directory record of a stored entry;
; its data comes next (zw_add puts it in the file buffer, pkg_save3 keeps it outside)
zw_head:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    mov ebx, r8d                     ; CRC-32
    mov r15, r9
    invoke strlen, r13
    mov ebp, eax                     ; name length
    mov rdi, [r12+8]                 ; offset of the local header
    lea rsi, [rsp+96]
    ; local file header (30 bytes)
    mov dword [rsi], 0x04034B50
    mov word [rsi+4], 20
    mov word [rsi+6], 0
    mov word [rsi+8], 0              ; stored
    mov word [rsi+10], 0
    mov word [rsi+12], 0x0021        ; 1980-01-01
    mov [rsi+14], ebx
    mov [rsi+18], r15d
    mov [rsi+22], r15d
    mov [rsi+26], bp
    mov word [rsi+28], 0
    invoke bb_put, r12, rsi, 30
    test eax, eax
    jz .err
    invoke bb_put, r12, r13, rbp
    test eax, eax
    jz .err
    ; central directory record (46 bytes)
    mov dword [rsi], 0x02014B50
    mov word [rsi+4], 20
    mov word [rsi+6], 20
    mov word [rsi+8], 0
    mov word [rsi+10], 0
    mov word [rsi+12], 0
    mov word [rsi+14], 0x0021
    mov [rsi+16], ebx
    mov [rsi+20], r15d
    mov [rsi+24], r15d
    mov [rsi+28], bp
    mov word [rsi+30], 0
    mov word [rsi+32], 0
    mov word [rsi+34], 0
    mov word [rsi+36], 0
    mov dword [rsi+38], 0
    mov [rsi+42], edi
    invoke bb_put, &r12+ZW_CD, rsi, 46
    test eax, eax
    jz .err
    invoke bb_put, &r12+ZW_CD, r13, rbp
    test eax, eax
    jz .err
    inc qword [r12+ZW_COUNT]
    EPILOG 160
.err:
    mov qword [r12+ZW_ERR], 1
    EPILOG 160

; zw_add(zw, name (ASCII), data, length): one stored entry
zw_add:
    PROLOG 64
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    invoke crc32_update, 0, r14, r15
    mov ebx, eax
    invoke zw_head, r12, r13, rbx, r15
    invoke bb_put, r12, r14, r15
    test eax, eax
    jnz .d
    mov qword [r12+ZW_ERR], 1
.d:
    EPILOG 64

; zw_finish(zw, path) -> eax 1 when the archive was committed; always frees the buffers
; The file is the buffer, then the data kept outside it (ZW_EXTP), then the central directory.
zw_finish:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    xor r14d, r14d                   ; result
    mov rdi, [r12+8]
    add rdi, [r12+ZW_EXTN]           ; offset of the central directory
    mov rbx, [r12+ZW_CD+8]           ; its size
    lea rsi, [rsp+96]                ; end of central directory (22 bytes)
    mov dword [rsi], 0x06054B50
    mov dword [rsi+4], 0
    mov rax, [r12+ZW_COUNT]
    mov [rsi+8], ax
    mov [rsi+10], ax
    mov [rsi+12], ebx
    mov [rsi+16], edi
    mov word [rsi+20], 0
    invoke bb_put, &r12+ZW_CD, rsi, 22
    test eax, eax
    jz .free
    cmp qword [r12+ZW_ERR], 0
    jne .free
    mov rax, [r12]
    mov [rsp+128], rax
    mov rax, [r12+8]
    mov [rsp+136], rax
    mov rax, [r12+ZW_EXTP]
    mov [rsp+144], rax
    mov rax, [r12+ZW_EXTN]
    mov [rsp+152], rax
    mov rax, [r12+ZW_CD]
    mov [rsp+160], rax
    mov rax, [r12+ZW_CD+8]
    mov [rsp+168], rax
    invoke safe_writev, r13, &rsp+128, 3
    mov r14d, eax
.free:
    invoke free, [r12]
    invoke free, [r12+ZW_CD]
    mov eax, r14d
    EPILOG 192

; xlsx_save(path, wide xml, chars) / docx_save: the main part as one text
xlsx_save:
    lea r9, [xl_desc]
    jmp pkg_save
docx_save:
    lea r9, [dx_desc]
pkg_save:
    sub rsp, 88
    mov [rsp+32], rdx                ; three pieces, the last two empty
    mov [rsp+40], r8
    xor eax, eax
    mov [rsp+48], rax
    mov [rsp+56], rax
    mov [rsp+64], rax
    mov [rsp+72], rax
    lea rdx, [rsp+32]
    mov r8, r9
    xor r9d, r9d
    call pkg_save3
    add rsp, 88
    ret

; xlsx_save3(path, pieces, sb) / docx_save3: the main part as three pieces of wide text { pointer, chars },
; so that a large middle piece does not have to be copied between a head and a tail first.
; sb (or 0) is a string builder that is freed as soon as the text has been converted - normally the one
; that owns the middle piece: the UTF-16 text is then gone before the file is written.
xlsx_save3:
    mov r9, r8
    lea r8, [xl_desc]
    jmp pkg_save3
docx_save3:
    mov r9, r8
    lea r8, [dx_desc]

; pkg_save3(path, pieces, package description, sb) -> eax 1 when written
%define PK_ZW   96                   ; ZW (80) -> 176
%define PK_L    176                  ; UTF-8 length of each piece (3 qwords) -> 200
%define PK_BIG  200                  ; the main part as UTF-8
%define PK_SB   208
%define PK_T    216
pkg_save3:
    PROLOG 256
    mov r12, rcx
    mov r13, rdx
    mov r15, r8
    mov [rsp+PK_SB], r9
    mov qword [rsp+PK_BIG], 0
    invoke memset, &rsp+PK_ZW, 0, ZW_SIZE
    mov rbp, [r15]                   ; fixed part count
    lea rbx, [r15+16]
.l:
    invoke zw_add, &rsp+PK_ZW, [rbx], [rbx+8], [rbx+16]
    add rbx, 24
    dec rbp
    jnz .l
    xor r14d, r14d                   ; UTF-8 bytes in all
    xor ebx, ebx
.len:
    cmp ebx, 3
    jae .lend
    mov qword [rsp+PK_L+rbx*8], 0
    mov rax, rbx
    shl rax, 4
    mov rsi, [r13+rax]
    mov rdi, [r13+rax+8]
    test rdi, rdi
    jz .ln
    cmp rdi, 0x7FFFFFF0              ; the conversion takes a 32-bit count
    ja .err
    invoke WideCharToMultiByte, CP_UTF8, 0, rsi, rdi, 0, 0, 0, 0
    test eax, eax
    jle .err
    mov eax, eax
    mov [rsp+PK_L+rbx*8], rax
    add r14, rax
.ln:
    inc ebx
    jmp .len
.lend:
    mov eax, 0xFFFF0000              ; sizes and offsets in this ZIP format are 32 bits
    cmp r14, rax
    jae .err
    lea rcx, [r14+8]
    call malloc
    test rax, rax
    jz .err
    mov [rsp+PK_BIG], rax
    mov rbp, rax
    xor ebx, ebx
.cv:
    cmp ebx, 3
    jae .cvd
    mov rax, [rsp+PK_L+rbx*8]
    test rax, rax
    jz .cn
    mov [rsp+PK_T], rax
    mov rax, rbx
    shl rax, 4
    mov rsi, [r13+rax]
    mov rdi, [r13+rax+8]
    invoke WideCharToMultiByte, CP_UTF8, 0, rsi, rdi, rbp, [rsp+PK_T], 0, 0
    cmp eax, [rsp+PK_T]
    jne .err
    add rbp, [rsp+PK_T]
.cn:
    inc ebx
    jmp .cv
.cvd:
    mov rcx, [rsp+PK_SB]             ; the wide text is not needed any more
    test rcx, rcx
    jz .nosb
    call sb_free
.nosb:
    invoke crc32_update, 0, [rsp+PK_BIG], r14
    mov ebx, eax
    invoke zw_head, &rsp+PK_ZW, [r15+8], rbx, r14
    mov rax, [rsp+PK_BIG]
    mov [rsp+PK_ZW+ZW_EXTP], rax
    mov [rsp+PK_ZW+ZW_EXTN], r14
    invoke zw_finish, &rsp+PK_ZW, r12
    mov ebx, eax
    invoke free, [rsp+PK_BIG]
    mov eax, ebx
    EPILOG 256
.err:
    mov qword [rsp+PK_ZW+ZW_ERR], 1
    invoke zw_finish, &rsp+PK_ZW, r12        ; frees the buffers and writes nothing
    invoke free, [rsp+PK_BIG]
    xor eax, eax
    EPILOG 256
