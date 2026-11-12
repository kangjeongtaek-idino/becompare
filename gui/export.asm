; export.asm - OOXML export: stored (uncompressed) ZIP writer + XLSX / DOCX package skeletons
;   xlsx_save(path, sheet xml (wide), chars) / docx_save(path, document xml (wide), chars) -> eax 1 when written
%include "inc/macros.inc"
%include "inc/win.inc"

extern malloc, realloc, free, memcpy, memset, strlen, wcsrchr, _wcsicmp
extern WideCharToMultiByte
extern crc32_update, safe_write2

global xlsx_save, docx_save, export_kind

; byte buffer BB: { ptr, len, cap }
; zip writer ZW: { BB file (0), BB central directory (24), entry count (48), error flag (56) }
%define ZW_CD    24
%define ZW_COUNT 48
%define ZW_ERR   56
%define ZW_SIZE  64

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

; zw_add(zw, name (ASCII), data, length): one stored entry
zw_add:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    invoke strlen, r13
    mov ebp, eax                     ; name length
    invoke crc32_update, 0, r14, r15
    mov ebx, eax                     ; CRC-32
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
    invoke bb_put, r12, r14, r15
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

; zw_add_w(zw, name, wide text, chars): entry holding the text as UTF-8
zw_add_w:
    PROLOG 96
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    xor esi, esi
    xor ebx, ebx
    test r15, r15
    jz .add
    invoke WideCharToMultiByte, CP_UTF8, 0, r14, r15, 0, 0, 0, 0
    test eax, eax
    jle .err
    mov ebx, eax
    invoke malloc, rbx
    test rax, rax
    jz .err
    mov rsi, rax
    invoke WideCharToMultiByte, CP_UTF8, 0, r14, r15, rsi, rbx, 0, 0
    test eax, eax
    jle .freeerr
    mov ebx, eax
.add:
    invoke zw_add, r12, r13, rsi, rbx
    invoke free, rsi
    EPILOG 96
.freeerr:
    invoke free, rsi
.err:
    mov qword [r12+ZW_ERR], 1
    EPILOG 96

; zw_finish(zw, path) -> eax 1 when the archive was committed; always frees the buffers
zw_finish:
    PROLOG 160
    mov r12, rcx
    mov r13, rdx
    xor r14d, r14d                   ; result
    mov rdi, [r12+8]                 ; offset of the central directory
    mov rbx, [r12+ZW_CD+8]           ; its size
    invoke bb_put, r12, [r12+ZW_CD], rbx
    test eax, eax
    jz .free
    lea rsi, [rsp+96]                ; end of central directory (22 bytes)
    mov dword [rsi], 0x06054B50
    mov dword [rsi+4], 0
    mov rax, [r12+ZW_COUNT]
    mov [rsi+8], ax
    mov [rsi+10], ax
    mov [rsi+12], ebx
    mov [rsi+16], edi
    mov word [rsi+20], 0
    invoke bb_put, r12, rsi, 22
    test eax, eax
    jz .free
    cmp qword [r12+ZW_ERR], 0
    jne .free
    invoke safe_write2, r13, [r12], [r12+8], 0, 0
    mov r14d, eax
.free:
    invoke free, [r12]
    invoke free, [r12+ZW_CD]
    mov eax, r14d
    EPILOG 160

; pkg_save(path, wide xml, chars, package description) -> eax
xlsx_save:
    lea r9, [xl_desc]
    jmp pkg_save
docx_save:
    lea r9, [dx_desc]
pkg_save:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    invoke memset, &rsp+96, 0, ZW_SIZE
    mov rbp, [r15]                   ; fixed part count
    lea rbx, [r15+16]
.l:
    invoke zw_add, &rsp+96, [rbx], [rbx+8], [rbx+16]
    add rbx, 24
    dec rbp
    jnz .l
    invoke zw_add_w, &rsp+96, [r15+8], r13, r14
    invoke zw_finish, &rsp+96, r12
    EPILOG 192
