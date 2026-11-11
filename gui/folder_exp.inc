; folder_exp.inc - folder comparison export to Excel (.xlsx) and Word (.docx)   (included by folder.asm)
;   The exported rows are the ones the status filter currently shows, like the HTML report.

extern xlsx_save, docx_save, export_kind, browse_save_as, ShellExecuteW

section .rdata
flt_html: dw __utf16__("HTML 보고서 (*.html)"), 0, __utf16__("*.html"), 0, 0
flt_xlsx: dw __utf16__("Excel 통합 문서 (*.xlsx)"), 0, __utf16__("*.xlsx"), 0, 0
flt_docx: dw __utf16__("Word 문서 (*.docx)"), 0, __utf16__("*.docx"), 0, 0
WSTR s_de_html, "html"
WSTR s_de_xlsx, "xlsx"
WSTR s_de_docx, "docx"
WSTR s_xm_html, "HTML 보고서(.html)…"
WSTR s_xm_xlsx, "Excel 통합 문서(.xlsx)로 내보내기…"
WSTR s_xm_docx, "Word 문서(.docx)로 내보내기…"
WSTR s_x_defname, "폴더비교결과"
WSTR s_x_ok, "비교 결과를 내보냈습니다."
WSTR s_x_fail, "내보내지 못했습니다. 출력 경로와 쓰기 권한, 파일이 다른 프로그램에서 열려 있는지 확인하세요."
WSTR s_x_open, "저장했습니다. 지금 열어 볼까요?"
WSTR s_x_verb, "open"
WSTR s_x_title, "폴더 비교 보고서"
WSTR f_x_left, "왼쪽: %s"
WSTR f_x_right, "오른쪽: %s"
WSTR s_x_sum, "같음 %llu · 다름 %llu · 왼쪽만 %llu · 오른쪽만 %llu · 형식 다름 %llu (파일 기준)"
WSTR h_x_path, "경로"
WSTR h_x_kind, "종류"
WSTR s_k_dir, "폴더"
WSTR s_k_file, "파일"

; ---- sheet XML
WSTR x_head, `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetViews><sheetView workbookViewId="0"><pane ySplit="5" topLeftCell="A6" activePane="bottomLeft" state="frozen"/></sheetView></sheetViews><sheetFormatPr defaultRowHeight="15"/><cols><col min="1" max="1" width="64" customWidth="1"/><col min="2" max="2" width="8" customWidth="1"/><col min="3" max="3" width="12" customWidth="1"/><col min="4" max="4" width="15" customWidth="1"/><col min="5" max="5" width="21" customWidth="1"/><col min="6" max="6" width="15" customWidth="1"/><col min="7" max="7" width="21" customWidth="1"/></cols><sheetData>`
WSTR f_xrow, `<row r="%u">`
WSTR s_xrowend, `</row>\n`
WSTR f_xfoot, `</sheetData><autoFilter ref="A5:G%u"/></worksheet>`
WSTR f_xc_s, `<c r="%c%u" s="%u" t="inlineStr"><is><t xml:space="preserve">`
WSTR f_xc_e, `<c r="%c%u" s="%u"/>`
WSTR s_xc_end, `</t></is></c>`
WSTR f_xc_n, `<c r="%c%u" s="%u"><v>%llu</v></c>`

; ---- document XML
WSTR d_head, `<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"><w:body><w:p><w:pPr><w:spacing w:after="120"/></w:pPr><w:r><w:rPr><w:b/><w:sz w:val="32"/><w:szCs w:val="32"/></w:rPr><w:t>폴더 비교 보고서</w:t></w:r></w:p>`
WSTR d_para1, `<w:p><w:pPr><w:spacing w:after="60"/></w:pPr><w:r><w:rPr><w:sz w:val="18"/><w:szCs w:val="18"/></w:rPr><w:t xml:space="preserve">`
WSTR d_para2, `</w:t></w:r></w:p>`
WSTR d_tbl, `<w:tbl><w:tblPr><w:tblW w:w="15300" w:type="dxa"/><w:tblBorders><w:top w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:left w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:bottom w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:right w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:insideH w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/><w:insideV w:val="single" w:sz="4" w:space="0" w:color="BFBFBF"/></w:tblBorders><w:tblLayout w:type="fixed"/><w:tblCellMar><w:left w:w="60" w:type="dxa"/><w:right w:w="60" w:type="dxa"/></w:tblCellMar></w:tblPr><w:tblGrid><w:gridCol w:w="6200"/><w:gridCol w:w="800"/><w:gridCol w:w="1100"/><w:gridCol w:w="1300"/><w:gridCol w:w="2300"/><w:gridCol w:w="1300"/><w:gridCol w:w="2300"/></w:tblGrid>`
WSTR d_trh, `<w:tr><w:trPr><w:tblHeader/></w:trPr>`
WSTR d_tr, `<w:tr>`
WSTR d_trend, `</w:tr>\n`
WSTR d_foot, `</w:tbl><w:p/><w:sectPr><w:pgSz w:w="16838" w:h="11906" w:orient="landscape"/><w:pgMar w:top="720" w:right="720" w:bottom="720" w:left="720" w:header="360" w:footer="360" w:gutter="0"/></w:sectPr></w:body></w:document>`
WSTR f_dtc, `<w:tc><w:tcPr><w:tcW w:w="%u" w:type="dxa"/>`
WSTR f_dshd, `<w:shd w:val="clear" w:color="auto" w:fill="%s"/>`
WSTR s_dtc2, `</w:tcPr><w:p>`
WSTR s_djc, `<w:pPr><w:jc w:val="right"/></w:pPr>`
WSTR s_dr, `<w:r>`
WSTR s_db, `<w:rPr><w:b/></w:rPr>`
WSTR s_dt, `<w:t xml:space="preserve">`
WSTR s_dtc3, `</w:t></w:r></w:p></w:tc>`
WSTR s_fill_head, "D9D9D9"
WSTR s_fill_diff, "FFD7D7"
WSTR s_fill_lonly, "D7F0D7"
WSTR s_fill_ronly, "D7E1FF"
WSTR s_fill_type, "FFE6BE"

align 8
xh_text:  dq h_x_path, h_x_kind, h_status, h_sizea, h_mta, h_sizeb, h_mtb
dx_fill:  dq 0, s_fill_diff, s_fill_lonly, s_fill_ronly, s_fill_type
xm_flt:   dq flt_html, flt_xlsx, flt_docx
xm_ext:   dq s_de_html, s_de_xlsx, s_de_docx
dx_w:     dd 6200, 800, 1100, 1300, 2300, 1300, 2300

section .text

; xs_cell(sb, column letter, row, style, text): inline-string cell (an empty text still carries the style)
xs_cell:
    PROLOG 352
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov rsi, [rsp+352+112]
    lea r15, [f_xc_s]
    cmp word [rsi], 0
    jne .fmt
    lea r15, [f_xc_e]
.fmt:
    invoke _snwprintf, &rsp+96, 110, r15, r12, r13, r14
    mov word [rsp+96+220], 0
    invoke sb_append, rbx, &rsp+96
    cmp word [rsi], 0
    je .d
    invoke sb_append_esc, rbx, rsi
    invoke sb_append, rbx, &s_xc_end
.d:
    EPILOG 352

; xs_num(sb, column letter, row, style, value): numeric cell
xs_num:
    PROLOG 352
    mov rbx, rcx
    mov r12d, edx
    mov r13d, r8d
    mov r14d, r9d
    mov rsi, [rsp+352+112]
    invoke _snwprintf, &rsp+96, 110, &f_xc_n, r12, r13, r14, rsi
    mov word [rsp+96+220], 0
    invoke sb_append, rbx, &rsp+96
    EPILOG 352

; dx_cell(sb, column 0..6, fill colour text or 0, text, flags: 1 bold, 2 right aligned)
dx_cell:
    PROLOG 352
    mov rbx, rcx
    mov r12d, edx
    mov r13, r8
    mov r14, r9
    mov r15d, [rsp+352+112]
    lea rax, [dx_w]
    mov esi, [rax+r12*4]
    invoke _snwprintf, &rsp+96, 110, &f_dtc, rsi
    invoke sb_append, rbx, &rsp+96
    test r13, r13
    jz .noshd
    invoke _snwprintf, &rsp+96, 110, &f_dshd, r13
    invoke sb_append, rbx, &rsp+96
.noshd:
    invoke sb_append, rbx, &s_dtc2
    test r15d, 2
    jz .nojc
    invoke sb_append, rbx, &s_djc
.nojc:
    invoke sb_append, rbx, &s_dr
    test r15d, 1
    jz .nob
    invoke sb_append, rbx, &s_db
.nob:
    invoke sb_append, rbx, &s_dt
    invoke sb_append_esc, rbx, r14
    invoke sb_append, rbx, &s_dtc3
    EPILOG 352

; dx_para(sb, text)
dx_para:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    invoke sb_append, rbx, &d_para1
    invoke sb_append_esc, rbx, rsi
    invoke sb_append, rbx, &d_para2
    EPILOG 64

%define XP_SB    96                  ; string builder (24)
%define XP_KIND  120                 ; 1 xlsx, 2 docx
%define XP_BUF   128                 ; wchar[160]
%define XP_BIG   448                 ; wchar[1200]

; fold_export_path(fold, path, kind 1 xlsx / 2 docx) -> eax 1 when written
fold_export_path:
    PROLOG 2864
    mov rbx, rcx
    mov rdi, rdx
    mov [rsp+XP_KIND], r8d
    invoke sb_init, &rsp+XP_SB
    cmp dword [rsp+XP_KIND], 2
    je .dhead
    ; ---------------- xlsx: title, roots, summary, header (rows 1..5)
    invoke sb_append, &rsp+XP_SB, &x_head
    invoke _snwprintf, &rsp+XP_BUF, 40, &f_xrow, 1
    invoke sb_append, &rsp+XP_SB, &rsp+XP_BUF
    invoke xs_cell, &rsp+XP_SB, 'A', 1, 12, &s_x_title
    invoke sb_append, &rsp+XP_SB, &s_xrowend
    invoke _snwprintf, &rsp+XP_BUF, 40, &f_xrow, 2
    invoke sb_append, &rsp+XP_SB, &rsp+XP_BUF
    invoke _snwprintf, &rsp+XP_BIG, 1190, &f_x_left, &rbx+F_ROOTA
    mov word [rsp+XP_BIG+2380], 0
    invoke xs_cell, &rsp+XP_SB, 'A', 2, 0, &rsp+XP_BIG
    invoke sb_append, &rsp+XP_SB, &s_xrowend
    invoke _snwprintf, &rsp+XP_BUF, 40, &f_xrow, 3
    invoke sb_append, &rsp+XP_SB, &rsp+XP_BUF
    invoke _snwprintf, &rsp+XP_BIG, 1190, &f_x_right, &rbx+F_ROOTB
    mov word [rsp+XP_BIG+2380], 0
    invoke xs_cell, &rsp+XP_SB, 'A', 3, 0, &rsp+XP_BIG
    invoke sb_append, &rsp+XP_SB, &s_xrowend
    invoke _snwprintf, &rsp+XP_BUF, 40, &f_xrow, 4
    invoke sb_append, &rsp+XP_SB, &rsp+XP_BUF
    invoke _snwprintf, &rsp+XP_BIG, 1190, &s_x_sum, [rbx+F_CNT], [rbx+F_CNT+8], [rbx+F_CNT+16], [rbx+F_CNT+24], [rbx+F_CNT+32]
    invoke xs_cell, &rsp+XP_SB, 'A', 4, 0, &rsp+XP_BIG
    invoke sb_append, &rsp+XP_SB, &s_xrowend
    invoke _snwprintf, &rsp+XP_BUF, 40, &f_xrow, 5
    invoke sb_append, &rsp+XP_SB, &rsp+XP_BUF
    xor r12d, r12d
.xh:
    lea rax, [xh_text]
    mov rsi, [rax+r12*8]
    lea ebp, [r12+'A']
    invoke xs_cell, &rsp+XP_SB, rbp, 5, 1, rsi
    inc r12d
    cmp r12d, 7
    jb .xh
    invoke sb_append, &rsp+XP_SB, &s_xrowend
    jmp .rows
    ; ---------------- docx: title, roots, summary, table header
.dhead:
    invoke sb_append, &rsp+XP_SB, &d_head
    invoke _snwprintf, &rsp+XP_BIG, 1190, &f_x_left, &rbx+F_ROOTA
    mov word [rsp+XP_BIG+2380], 0
    invoke dx_para, &rsp+XP_SB, &rsp+XP_BIG
    invoke _snwprintf, &rsp+XP_BIG, 1190, &f_x_right, &rbx+F_ROOTB
    mov word [rsp+XP_BIG+2380], 0
    invoke dx_para, &rsp+XP_SB, &rsp+XP_BIG
    invoke _snwprintf, &rsp+XP_BIG, 1190, &s_x_sum, [rbx+F_CNT], [rbx+F_CNT+8], [rbx+F_CNT+16], [rbx+F_CNT+24], [rbx+F_CNT+32]
    invoke dx_para, &rsp+XP_SB, &rsp+XP_BIG
    invoke sb_append, &rsp+XP_SB, &d_tbl
    invoke sb_append, &rsp+XP_SB, &d_trh
    xor r12d, r12d
.dh:
    lea rax, [xh_text]
    mov rsi, [rax+r12*8]
    invoke dx_cell, &rsp+XP_SB, r12, &s_fill_head, rsi, 1
    inc r12d
    cmp r12d, 7
    jb .dh
    invoke sb_append, &rsp+XP_SB, &d_trend
    ; ---------------- one row per entry that passes the status filter
.rows:
    xor r12d, r12d                   ; entry index
    mov r14d, 5                      ; last sheet row written
.l:
    cmp r12, [rbx+F_NENT]
    jae .fin
    mov rax, r12
    shl rax, 6
    add rax, [rbx+F_ENTS]
    mov r13, rax
    mov ecx, [r13+E_STATUS]
    mov eax, 1
    shl eax, cl
    test eax, [rbx+F_FMASK]
    jz .n
    inc r14d
    lea rsi, [s_k_file]
    test dword [r13+E_FLAGS], EF_DIRA|EF_DIRB
    jz .kind
    lea rsi, [s_k_dir]
.kind:
    cmp dword [rsp+XP_KIND], 2
    je .drow
    ; ---- xlsx row
    invoke _snwprintf, &rsp+XP_BUF, 40, &f_xrow, r14
    invoke sb_append, &rsp+XP_SB, &rsp+XP_BUF
    mov r15d, [r13+E_STATUS]
    add r15d, 2                      ; text style of this status
    lea ebp, [r15+5]                 ; number style
    invoke xs_cell, &rsp+XP_SB, 'A', r14, r15, [r13+E_NAME]
    invoke xs_cell, &rsp+XP_SB, 'B', r14, r15, rsi
    mov eax, [r13+E_STATUS]
    lea rcx, [st_text]
    mov rsi, [rcx+rax*8]
    invoke xs_cell, &rsp+XP_SB, 'C', r14, r15, rsi
    mov eax, [r13+E_FLAGS]
    and eax, EF_EXA|EF_DIRA
    cmp eax, EF_EXA
    jne .xsa
    invoke xs_num, &rsp+XP_SB, 'D', r14, rbp, [r13+E_SIZEA]
    jmp .xta
.xsa:
    invoke xs_cell, &rsp+XP_SB, 'D', r14, r15, &s_empty
.xta:
    mov word [rsp+XP_BUF], 0
    test dword [r13+E_FLAGS], EF_EXA
    jz .xta2
    invoke fmt_time, &rsp+XP_BUF, [r13+E_MTA]
.xta2:
    invoke xs_cell, &rsp+XP_SB, 'E', r14, r15, &rsp+XP_BUF
    mov eax, [r13+E_FLAGS]
    and eax, EF_EXB|EF_DIRB
    cmp eax, EF_EXB
    jne .xsb
    invoke xs_num, &rsp+XP_SB, 'F', r14, rbp, [r13+E_SIZEB]
    jmp .xtb
.xsb:
    invoke xs_cell, &rsp+XP_SB, 'F', r14, r15, &s_empty
.xtb:
    mov word [rsp+XP_BUF], 0
    test dword [r13+E_FLAGS], EF_EXB
    jz .xtb2
    invoke fmt_time, &rsp+XP_BUF, [r13+E_MTB]
.xtb2:
    invoke xs_cell, &rsp+XP_SB, 'G', r14, r15, &rsp+XP_BUF
    invoke sb_append, &rsp+XP_SB, &s_xrowend
    jmp .n
    ; ---- docx row
.drow:
    mov eax, [r13+E_STATUS]
    lea rcx, [dx_fill]
    mov r15, [rcx+rax*8]             ; fill colour of this status (0 = none)
    invoke sb_append, &rsp+XP_SB, &d_tr
    invoke dx_cell, &rsp+XP_SB, 0, r15, [r13+E_NAME], 0
    invoke dx_cell, &rsp+XP_SB, 1, r15, rsi, 0
    mov eax, [r13+E_STATUS]
    lea rcx, [st_text]
    mov rsi, [rcx+rax*8]
    invoke dx_cell, &rsp+XP_SB, 2, r15, rsi, 0
    mov word [rsp+XP_BUF], 0
    mov eax, [r13+E_FLAGS]
    and eax, EF_EXA|EF_DIRA
    cmp eax, EF_EXA
    jne .dsa
    invoke fmt_size, &rsp+XP_BUF, [r13+E_SIZEA]
.dsa:
    invoke dx_cell, &rsp+XP_SB, 3, r15, &rsp+XP_BUF, 2
    mov word [rsp+XP_BUF], 0
    test dword [r13+E_FLAGS], EF_EXA
    jz .dta
    invoke fmt_time, &rsp+XP_BUF, [r13+E_MTA]
.dta:
    invoke dx_cell, &rsp+XP_SB, 4, r15, &rsp+XP_BUF, 0
    mov word [rsp+XP_BUF], 0
    mov eax, [r13+E_FLAGS]
    and eax, EF_EXB|EF_DIRB
    cmp eax, EF_EXB
    jne .dsb
    invoke fmt_size, &rsp+XP_BUF, [r13+E_SIZEB]
.dsb:
    invoke dx_cell, &rsp+XP_SB, 5, r15, &rsp+XP_BUF, 2
    mov word [rsp+XP_BUF], 0
    test dword [r13+E_FLAGS], EF_EXB
    jz .dtb
    invoke fmt_time, &rsp+XP_BUF, [r13+E_MTB]
.dtb:
    invoke dx_cell, &rsp+XP_SB, 6, r15, &rsp+XP_BUF, 0
    invoke sb_append, &rsp+XP_SB, &d_trend
.n:
    inc r12
    jmp .l
.fin:
    cmp dword [rsp+XP_KIND], 2
    je .dfin
    invoke _snwprintf, &rsp+XP_BUF, 100, &f_xfoot, r14
    invoke sb_append, &rsp+XP_SB, &rsp+XP_BUF
    invoke xlsx_save, rdi, [rsp+XP_SB], [rsp+XP_SB+8]
    jmp .res
.dfin:
    invoke sb_append, &rsp+XP_SB, &d_foot
    invoke docx_save, rdi, [rsp+XP_SB], [rsp+XP_SB+8]
.res:
    mov ebp, eax
    invoke sb_free, &rsp+XP_SB
    test ebp, ebp
    jz .fail
    invoke set_status, &s_x_ok
    mov eax, 1
    EPILOG 2864
.fail:
    invoke set_status, &s_x_fail
    xor eax, eax
    EPILOG 2864

; fold_report_any(fold, path): the extension picks the format (.xlsx, .docx, anything else = HTML)
fold_report_any:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    invoke export_kind, rsi
    test eax, eax
    jz .html
    mov edi, eax
    invoke fold_export_path, rbx, rsi, rdi
    EPILOG 64
.html:
    invoke fold_report_path, rbx, rsi
    EPILOG 64

; fold_report(hwnd, fold): format menu -> save dialog -> write -> offer to open the result
fold_report:
    PROLOG 1600
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+F_RUNNING], 0
    jne .busy
    cmp qword [rbx+F_NENT], 0
    je .norun
    call CreatePopupMenu
    mov rdi, rax
    invoke AppendMenuW, rdi, MF_STRING, 2, &s_xm_xlsx
    invoke AppendMenuW, rdi, MF_STRING, 3, &s_xm_docx
    invoke AppendMenuW, rdi, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, rdi, MF_STRING, 1, &s_xm_html
    invoke popup_at, rsi, ID_BTN_REPORT, rdi
    test eax, eax
    jz .d
    lea r12d, [rax-1]                ; 0 html, 1 xlsx, 2 docx
    lea rdi, [rsp+400]               ; save path buffer (1040 bytes)
    invoke wcscpy, rdi, &s_x_defname
    lea rax, [xm_flt]
    mov r13, [rax+r12*8]
    lea rax, [xm_ext]
    mov r14, [rax+r12*8]
    invoke browse_save_as, rsi, rdi, r13, r14
    test eax, eax
    jz .d
    test r12d, r12d
    jnz .ooxml
    invoke fold_report_path, rbx, rdi
    jmp .res
.ooxml:
    invoke fold_export_path, rbx, rdi, r12
.res:
    test eax, eax
    jz .failed
    invoke msgbox, rsi, &s_x_open, MB_YESNO|MB_ICONQUESTION
    cmp eax, IDYES
    jne .d
    invoke ShellExecuteW, rsi, &s_x_verb, rdi, 0, 0, SW_SHOWNORMAL
.d:
    EPILOG 1600
.failed:
    lea rdx, [s_report_fail]
    test r12d, r12d
    jz .fmsg
    lea rdx, [s_x_fail]
.fmsg:
    invoke msgbox, rsi, rdx, MB_OK|MB_ICONWARNING
    EPILOG 1600
.norun:
    invoke msgbox, rsi, &s_norun, MB_OK|MB_ICONINFO
    EPILOG 1600
.busy:
    invoke msgbox, rsi, &s_busy, MB_OK|MB_ICONWARNING
    EPILOG 1600
