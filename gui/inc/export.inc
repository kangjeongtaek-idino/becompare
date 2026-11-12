; export.inc - shared declarations for the XLSX / DOCX exporters (export.asm)
;
; XLSX: xw_begin(ctx) ; per row: xw_cell / xw_numcell ... xw_endrow ; xw_save(ctx, path, widths, ncols)
;   ctx = 40 bytes (XL_SIZE).  Row 1 is the header row (frozen + filtered); xw_endrow after each row.
; DOCX: dw_begin(ctx) ; dw_para / dw_tbl_begin / dw_row_begin / dw_cell / dw_row_end / dw_tbl_end ; dw_save(ctx, path)
;   ctx = 40 bytes (DX_SIZE).
; ex_pick(hwnd, out wchar[520], kind) -> eax 1 when a path was chosen (kind 0 = xlsx, 1 = docx)
; ex_done(hwnd, ok): status bar message + error box

%define XL_SIZE 40
%define DX_SIZE 40

; xlsx cell styles
%define XS_NORMAL     0
%define XS_HEAD       1
%define XS_RED        3
%define XS_GREEN      4
%define XS_BLUE       5
%define XS_ORANGE     6
%define XS_MONO       13
%define XS_MONORED    14
%define XS_MONOGREEN  15
%define XS_MONOORANGE 16

%ifndef EXPORT_IMPL
extern ex_pick, ex_done
extern xw_begin, xw_cell, xw_numcell, xw_endrow, xw_save
extern dw_begin, dw_para, dw_tbl_begin, dw_row_begin, dw_cell, dw_row_end, dw_tbl_end, dw_save
extern dwc_red, dwc_green, dwc_blue, dwc_orange, dwc_gray
%endif
