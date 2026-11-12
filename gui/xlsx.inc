; xlsx.inc - TB-01/TB-03 Excel workbooks: unzip (vtab/inflate), read sharedStrings + first worksheet into a SIDE grid

extern wcschr, wcscmp, wcscpy, _wcsicmp, GetCurrentProcessId
extern zip_open, zip_extract, vt_free, GetTempPathW, DeleteFileW, MultiByteToWideChar, read_file_bytes, wcsstr, wcscat, _wcsnicmp, free

; VT / VE layout of vtab.asm
%define XV_VE     0
%define XV_N      8
%define XV_STRUCT 48
%define XE_NAME   0
%define XE_IMPL   52
%define XE_BYTES  56

; XC: parse context
%define XC_BASE   0
%define XC_PTR    8
%define XC_STRS   16
%define XC_NSTR   24
%define XC_EMPTY  32
%define XC_MAXCOL 40
%define XC_ROWNO  44
%define XC_NEXT   48
%define XC_SIDE   56
%define XC_LASTCOL 64
%define XC_ROW    96               ; qword[16384] cell pointers of the current row
%define XC_SIZE   96+16384*8

section .rdata
WSTR xl_sst, "xl\sharedStrings.xml"
WSTR xl_sheet1, "xl\worksheets\sheet1.xml"
WSTR xl_pre, "xl\worksheets\sheet"
WSTR xl_tmp1, "bcxl_sst.xml"
WSTR xl_tmp2, "bcxl_sheet.xml"
WSTR xl_tmp3, "bcxl_book.xml"
WSTR xl_wb, "xl\workbook.xml"
WSTR xl_wbrels, "xl\_rels\workbook.xml.rels"
WSTR xl_dir, "xl\"
WSTR xk_sheet, "<sheet "
WSTR xk_rel, "<Relationship "
WSTR xk_name, ` name="`
WSTR xk_rid, ` r:id="`
WSTR xk_id, ` Id="`
WSTR xk_target, ` Target="`
WSTR f_xl_tmp, "%X_%s"                ; temporary names carry the process id: two instances do not collide
WSTR xn_si, "si"
WSTR xn_t, "t"
WSTR xn_rph, "rPh"
WSTR xn_row, "row"
WSTR xn_c, "c"
WSTR xn_v, "v"
WSTR xn_csi, "/si"
WSTR xn_cc, "/c"
WSTR xe_rph, "</rPh>"
WSTR xa_r, "r"
WSTR xa_t, "t"
WSTR xl_ext, ".xlsx"
WSTR xl_extm, ".xlsm"

section .text

; x_name_eq(q = text after '<', name) -> eax 1 when the tag name equals `name`
x_name_eq:
    xor eax, eax
.l:
    movzx r8d, word [rdx+rax*2]
    test r8d, r8d
    jz .end
    cmp r8w, [rcx+rax*2]
    jne .no
    inc rax
    jmp .l
.end:
    movzx r8d, word [rcx+rax*2]
    cmp r8d, '>'
    je .yes
    cmp r8d, ' '
    je .yes
    cmp r8d, '/'
    je .yes
    cmp r8d, 9
    je .yes
    cmp r8d, 13
    je .yes
    cmp r8d, 10
    je .yes
    test r8d, r8d
    jz .yes
.no:
    xor eax, eax
    ret
.yes:
    mov eax, 1
    ret

; x_tag_end(p) -> rax pointer to the closing '>' (or the terminating NUL)
x_tag_end:
    xor r8d, r8d
.l:
    movzx eax, word [rcx]
    test eax, eax
    jz .d
    test r8d, r8d
    jz .nq
    cmp eax, r8d
    jne .n
    xor r8d, r8d
    jmp .n
.nq:
    cmp eax, '"'
    je .q
    cmp eax, 39
    je .q
    cmp eax, '>'
    je .d
    jmp .n
.q:
    mov r8d, eax
.n:
    add rcx, 2
    jmp .l
.d:
    mov rax, rcx
    ret

; x_find_lt(p) -> rax pointer to the next '<' (or the NUL)
x_find_lt:
    movzx eax, word [rcx]
    test eax, eax
    jz .d
    cmp eax, '<'
    je .d
    add rcx, 2
    jmp x_find_lt
.d:
    mov rax, rcx
    ret

; x_attr(tag start, tag end, name, out (32 wchar)) -> eax 1 found
x_attr:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12, r8
    mov r13, r9
    mov word [r13], 0
.s:
    cmp rsi, rdi
    jae .no
    movzx eax, word [rsi]
    cmp eax, ' '
    je .try
    cmp eax, 9
    je .try
    cmp eax, 10
    je .try
    cmp eax, 13
    je .try
.adv:
    add rsi, 2
    jmp .s
.try:
    lea rbx, [rsi+2]
    xor ecx, ecx
.c:
    movzx eax, word [r12+rcx*2]
    test eax, eax
    jz .nm
    cmp ax, [rbx+rcx*2]
    jne .adv
    inc ecx
    jmp .c
.nm:
    cmp word [rbx+rcx*2], '='
    jne .adv
    movzx eax, word [rbx+rcx*2+2]
    cmp eax, '"'
    je .q
    cmp eax, 39
    jne .adv
.q:
    lea rdx, [rbx+rcx*2+4]
    xor ecx, ecx
.cp:
    movzx r8d, word [rdx+rcx*2]
    test r8d, r8d
    jz .cd
    cmp r8d, eax
    je .cd
    cmp ecx, 30
    jae .cd
    mov [r13+rcx*2], r8w
    inc ecx
    jmp .cp
.cd:
    mov word [r13+rcx*2], 0
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; x_text(src, dst) -> rax = src at the '<' that ends the text, rdx = dst end (entities decoded, line breaks -> space)
x_text:
.l:
    movzx eax, word [rcx]
    test eax, eax
    jz .d
    cmp eax, '<'
    je .d
    cmp eax, '&'
    jne .copy
    lea r8, [rcx+2]
    mov r9, [r8]
    mov r10, 'a'|('m'<<16)|('p'<<32)|(';'<<48)
    cmp r9, r10
    jne .e2
    mov eax, '&'
    lea rcx, [rcx+10]
    jmp .copy2
.e2:
    mov r10, 'l'|('t'<<16)|(';'<<32)
    mov r11, r9
    shl r11, 16
    shr r11, 16                       ; low 48 bits
    cmp r11, r10
    jne .e3
    mov eax, '<'
    lea rcx, [rcx+8]
    jmp .copy2
.e3:
    mov r10, 'g'|('t'<<16)|(';'<<32)
    cmp r11, r10
    jne .e4
    mov eax, '>'
    lea rcx, [rcx+8]
    jmp .copy2
.e4:
    mov r10, 'q'|('u'<<16)|('o'<<32)|('t'<<48)
    cmp r9, r10
    jne .e5
    cmp word [r8+8], ';'
    jne .e5
    mov eax, '"'
    lea rcx, [rcx+12]
    jmp .copy2
.e5:
    mov r10, 'a'|('p'<<16)|('o'<<32)|('s'<<48)
    cmp r9, r10
    jne .e6
    cmp word [r8+8], ';'
    jne .e6
    mov eax, 39
    lea rcx, [rcx+12]
    jmp .copy2
.e6:
    cmp word [r8], '#'
    jne .copy
    ; numeric reference
    lea r8, [rcx+4]
    xor r9d, r9d
    mov r10d, 10
    cmp word [r8], 'x'
    jne .nd
    mov r10d, 16
    add r8, 2
.nd:
    movzx eax, word [r8]
    cmp eax, ';'
    je .ndone
    test eax, eax
    jz .bad
    cmp eax, '0'
    jb .bad
    cmp eax, '9'
    jbe .dg
    or eax, 0x20
    cmp eax, 'a'
    jb .bad
    cmp eax, 'f'
    ja .bad
    sub eax, 'a'-10
    jmp .acc
.dg:
    sub eax, '0'
.acc:
    imul r9d, r10d
    add r9d, eax
    add r8, 2
    jmp .nd
.bad:
    mov eax, '&'
    jmp .copy
.ndone:
    lea rcx, [r8+2]
    mov eax, r9d
    cmp eax, 0xFFFF
    jbe .copy2
    mov eax, '?'
    jmp .copy2
.copy:
    add rcx, 2
.copy2:
    cmp eax, 10
    je .sp
    cmp eax, 13
    jne .st
.sp:
    mov eax, ' '
.st:
    mov [rdx], ax
    add rdx, 2
    jmp .l
.d:
    mov rax, rcx
    ret

; xl_col(name wchar*) -> eax zero based column index from "AB12"
xl_col:
    xor eax, eax
.l:
    movzx edx, word [rcx]
    or edx, 0x20
    cmp edx, 'a'
    jb .d
    cmp edx, 'z'
    ja .d
    sub edx, 'a'-1
    imul eax, 26
    add eax, edx
    add rcx, 2
    jmp .l
.d:
    dec eax
    ret

; xl_flush(XC*): emit the current row into the SIDE
xl_flush:
    PROLOG 64
    mov rbx, rcx
    cmp dword [rbx+XC_ROWNO], 0
    je .d
    mov r12, [rbx+XC_SIDE]
    invoke vec_push, &r12+SD_ROWOFF, 4
    mov rcx, [r12+SD_CELLS+8]
    mov [rax], ecx
    inc qword [r12+SD_NROWS]
    mov r13d, [rbx+XC_MAXCOL]
    inc r13d                          ; number of columns
    cmp r13d, [r12+SD_MAXC]
    jbe .nm
    mov [r12+SD_MAXC], r13d
.nm:
    xor esi, esi
.c:
    cmp esi, r13d
    jae .z
    mov rax, [rbx+XC_ROW+rsi*8]
    test rax, rax
    jnz .have
    mov rax, [rbx+XC_EMPTY]
.have:
    mov rdi, rax
    invoke vec_push, &r12+SD_CELLS, 8
    mov [rax], rdi
    inc esi
    jmp .c
.z:
    xor esi, esi
.zl:
    cmp esi, r13d
    jae .zd
    mov qword [rbx+XC_ROW+rsi*8], 0
    inc esi
    jmp .zl
.zd:
    mov dword [rbx+XC_ROWNO], 0
    mov dword [rbx+XC_MAXCOL], -1
.d:
    EPILOG 64

; xl_sst_parse(XC*, text)
xl_sst_parse:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
.top:
    mov rcx, rsi
    call x_find_lt
    mov rsi, rax
    cmp word [rsi], 0
    je .done
    lea rcx, [rsi+2]
    lea rdx, [xn_si]
    call x_name_eq
    test eax, eax
    jnz .si
    add rsi, 2
    jmp .top
.si:
    mov rdi, [rbx+XC_PTR]             ; entry start
    mov rcx, rsi
    call x_tag_end
    lea rsi, [rax+2]
    cmp word [rax-2], '/'
    je .entryend
.inner:
    mov rcx, rsi
    call x_find_lt
    mov rsi, rax
    cmp word [rsi], 0
    je .entryend
    lea rcx, [rsi+2]
    lea rdx, [xn_csi]
    call x_name_eq
    test eax, eax
    jnz .siend
    lea rcx, [rsi+2]
    lea rdx, [xn_rph]
    call x_name_eq
    test eax, eax
    jz .nrph
    invoke wcsstr, rsi, &xe_rph
    test rax, rax
    jz .entryend
    lea rsi, [rax+12]
    jmp .inner
.nrph:
    lea rcx, [rsi+2]
    lea rdx, [xn_t]
    call x_name_eq
    test eax, eax
    jz .skip
    mov rcx, rsi
    call x_tag_end
    cmp word [rax-2], '/'
    je .skipend
    lea rcx, [rax+2]
    mov rdx, [rbx+XC_PTR]
    call x_text
    mov [rbx+XC_PTR], rdx
    mov rsi, rax
    jmp .inner
.skipend:
    lea rsi, [rax+2]
    jmp .inner
.skip:
    add rsi, 2
    jmp .inner
.siend:
    mov rcx, rsi
    call x_tag_end
    lea rsi, [rax+2]
.entryend:
    mov rax, [rbx+XC_PTR]
    mov word [rax], 0
    add rax, 2
    mov [rbx+XC_PTR], rax
    ; record the entry
    mov rax, [rbx+XC_NSTR]
    lea r13, [rax*8+16]
    mov r12, rax
    invoke realloc, [rbx+XC_STRS], r13
    mov [rbx+XC_STRS], rax
    mov [rax+r12*8], rdi
    inc qword [rbx+XC_NSTR]
    jmp .top
.done:
    EPILOG 64

; xl_sheet_parse(XC*, text)
xl_sheet_parse:
    PROLOG 256
    mov rbx, rcx
    mov rsi, rdx
.top:
    mov rcx, rsi
    call x_find_lt
    mov rsi, rax
    cmp word [rsi], 0
    je .done
    lea rcx, [rsi+2]
    lea rdx, [xn_row]
    call x_name_eq
    test eax, eax
    jnz .row
    lea rcx, [rsi+2]
    lea rdx, [xn_c]
    call x_name_eq
    test eax, eax
    jnz .cell
    add rsi, 2
    jmp .top
.row:
    invoke xl_flush, rbx
    mov rcx, rsi
    call x_tag_end
    mov r12, rax
    invoke x_attr, rsi, r12, &xa_r, &rsp+96
    mov r13d, [rbx+XC_NEXT]
    cmp word [rsp+96], 0
    je .norow
    lea rcx, [rsp+96]
    call xl_num
    mov r13d, eax
.norow:
    ; blank rows for gaps
    mov r14d, [rbx+XC_NEXT]
    mov r15d, 0
.gap:
    cmp r14d, r13d
    jae .gapd
    cmp r15d, 20000
    jae .gapd
    mov rdi, [rbx+XC_SIDE]
    invoke vec_push, &rdi+SD_ROWOFF, 4
    mov rcx, [rdi+SD_CELLS+8]
    mov [rax], ecx
    inc qword [rdi+SD_NROWS]
    inc r14d
    inc r15d
    jmp .gap
.gapd:
    mov [rbx+XC_ROWNO], r13d
    lea eax, [r13+1]
    mov [rbx+XC_NEXT], eax
    mov dword [rbx+XC_MAXCOL], -1
    mov dword [rbx+XC_LASTCOL], -1
    lea rsi, [r12+2]
    jmp .top
.cell:
    cmp dword [rbx+XC_ROWNO], 0
    jne .hasrow
    ; a cell without a row element: open row 1
    mov dword [rbx+XC_ROWNO], 1
    mov dword [rbx+XC_NEXT], 2
    mov dword [rbx+XC_MAXCOL], -1
    mov dword [rbx+XC_LASTCOL], -1
.hasrow:
    mov rcx, rsi
    call x_tag_end
    mov r12, rax                      ; end of the <c ...> tag
    mov r14d, [rbx+XC_LASTCOL]
    inc r14d                          ; default column
    invoke x_attr, rsi, r12, &xa_r, &rsp+96
    cmp word [rsp+96], 0
    je .nocol
    lea rcx, [rsp+96]
    call xl_col
    mov r14d, eax
.nocol:
    cmp r14d, 16383
    ja .skipcell
    mov [rbx+XC_LASTCOL], r14d
    invoke x_attr, rsi, r12, &xa_t, &rsp+160
    movzx eax, word [rsp+160]         ; cell type: 's' = shared string (t="str" etc. are plain)
    cmp word [rsp+162], 0
    je .t1
    mov eax, 'x'
.t1:
    mov r15d, eax
    xor edi, edi                      ; value pointer
    lea rsi, [r12+2]
    cmp word [r12-2], '/'
    je .store
.cl:
    mov rcx, rsi
    call x_find_lt
    mov rsi, rax
    cmp word [rsi], 0
    je .store
    lea rcx, [rsi+2]
    lea rdx, [xn_cc]
    call x_name_eq
    test eax, eax
    jnz .cend
    lea rcx, [rsi+2]
    lea rdx, [xn_v]
    call x_name_eq
    test eax, eax
    jnz .val
    lea rcx, [rsi+2]
    lea rdx, [xn_t]
    call x_name_eq
    test eax, eax
    jnz .val
    add rsi, 2
    jmp .cl
.val:
    mov rcx, rsi
    call x_tag_end
    cmp word [rax-2], '/'
    je .vskip
    mov rdi, [rbx+XC_PTR]
    lea rcx, [rax+2]
    mov rdx, rdi
    call x_text
    mov rsi, rax
    mov word [rdx], 0
    add rdx, 2
    mov [rbx+XC_PTR], rdx
    jmp .cl
.vskip:
    lea rsi, [rax+2]
    jmp .cl
.cend:
    mov rcx, rsi
    call x_tag_end
    lea rsi, [rax+2]
.store:
    cmp r15d, 's'
    jne .plain
    test rdi, rdi
    jz .plain
    mov rcx, rdi
    call xl_num
    xor edi, edi
    cmp rax, [rbx+XC_NSTR]
    jae .plain
    mov rcx, [rbx+XC_STRS]
    mov rdi, [rcx+rax*8]
.plain:
    mov [rbx+XC_ROW+r14*8], rdi
    cmp r14d, [rbx+XC_MAXCOL]
    jle .top
    mov [rbx+XC_MAXCOL], r14d
    jmp .top
.skipcell:
    lea rsi, [r12+2]
    jmp .top
.done:
    invoke xl_flush, rbx
    EPILOG 256

; xl_num(wchar*) -> eax number (digits only)
xl_num:
    xor eax, eax
.l:
    movzx edx, word [rcx]
    cmp edx, '0'
    jb .d
    cmp edx, '9'
    ja .d
    imul eax, 10
    lea eax, [rax+rdx-'0']
    add rcx, 2
    jmp .l
.d:
    ret

; xl_find(VT*, name) -> VE* or 0
xl_find:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    xor edi, edi
.l:
    cmp rdi, [rbx+XV_N]
    jae .no
    mov rax, rdi
    imul rax, XE_BYTES
    add rax, [rbx+XV_VE]
    mov r12, rax
    cmp dword [r12+XE_IMPL], 0
    jne .n
    invoke _wcsicmp, [r12+XE_NAME], rsi
    test eax, eax
    jz .hit
.n:
    inc rdi
    jmp .l
.hit:
    mov rax, r12
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; xl_sheet(VT*) -> VE* of the first worksheet
xl_sheet:
    PROLOG 64
    mov rbx, rcx
    invoke xl_find, rbx, &xl_sheet1
    test rax, rax
    jnz .d
    xor edi, edi
.l:
    cmp rdi, [rbx+XV_N]
    jae .no
    mov rax, rdi
    imul rax, XE_BYTES
    add rax, [rbx+XV_VE]
    mov r12, rax
    invoke _wcsnicmp, [r12+XE_NAME], &xl_pre, 18
    test eax, eax
    jnz .n
    mov rax, r12
    jmp .d
.n:
    inc rdi
    jmp .l
.no:
    xor eax, eax
.d:
    EPILOG 64

; xl_attr(element text (ends at its '>'), key such as ` name="`, out, capacity in chars) -> eax 1 when found
xl_attr:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, r8
    mov r12, r9
    invoke wcsstr, rsi, rbx
    test rax, rax
    jz .no
    mov rsi, rax
    invoke wcslen, rbx
    lea rsi, [rsi+rax*2]
    xor ecx, ecx
.c:
    movzx eax, word [rsi+rcx*2]
    test eax, eax
    jz .no
    cmp eax, '"'
    je .end
    cmp rcx, r12
    jae .no
    mov [rdi+rcx*2], ax
    inc rcx
    jmp .c
.end:
    mov word [rdi+rcx*2], 0
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; xl_pick(VT*, selector) -> VE* of the worksheet the selector names, or 0
;   selector: digits = position among the sheet tabs (1 = first), anything else = sheet name (case ignored).
;   The workbook lists the sheets in tab order with a relationship id; the relationships give the part.
%define XP_RID  96                   ; wchar[64] -> 224
%define XP_NAME 224                  ; wchar[260] -> 744
%define XP_PART 744                  ; wchar[220] -> 1184
xl_pick:
    PROLOG 1216
    mov rbx, rcx
    mov r12, rdx
    xor r13d, r13d                   ; wanted position, 0 = by name
    xor ecx, ecx
.num:
    movzx eax, word [r12+rcx*2]
    test eax, eax
    jz .numd
    sub eax, '0'
    cmp eax, 9
    ja .byname0
    imul r13d, r13d, 10
    add r13d, eax
    inc ecx
    cmp ecx, 6
    jb .num
.byname0:
    xor r13d, r13d
.numd:
    xor r14d, r14d
    invoke xl_find, rbx, &xl_wb
    test rax, rax
    jz .no
    invoke xl_read, rbx, rax, &xl_tmp3
    test rax, rax
    jz .no
    mov r14, rax                     ; workbook text
    mov rsi, r14
    xor r15d, r15d                   ; sheets seen
.s:
    invoke wcsstr, rsi, &xk_sheet
    test rax, rax
    jz .nf
    mov rsi, rax
    invoke wcschr, rsi, '>'
    test rax, rax
    jz .nf
    mov rdi, rax
    mov word [rdi], 0
    inc r15d
    test r13d, r13d
    jz .byname
    cmp r15d, r13d
    jne .snext
    jmp .smatch
.byname:
    invoke xl_attr, rsi, &xk_name, &rsp+XP_NAME, 258
    test eax, eax
    jz .snext
    invoke _wcsicmp, &rsp+XP_NAME, r12
    test eax, eax
    jnz .snext
.smatch:
    invoke xl_attr, rsi, &xk_rid, &rsp+XP_RID, 62
    mov word [rdi], '>'
    test eax, eax
    jz .nf
    jmp .rels
.snext:
    mov word [rdi], '>'
    lea rsi, [rdi+2]
    jmp .s
.rels:
    invoke free, r14
    xor r14d, r14d
    invoke xl_find, rbx, &xl_wbrels
    test rax, rax
    jz .no
    invoke xl_read, rbx, rax, &xl_tmp3
    test rax, rax
    jz .no
    mov r14, rax
    mov rsi, r14
.r:
    invoke wcsstr, rsi, &xk_rel
    test rax, rax
    jz .nf
    mov rsi, rax
    invoke wcschr, rsi, '>'
    test rax, rax
    jz .nf
    mov rdi, rax
    mov word [rdi], 0
    invoke xl_attr, rsi, &xk_id, &rsp+XP_NAME, 62
    test eax, eax
    jz .rnext
    invoke wcscmp, &rsp+XP_NAME, &rsp+XP_RID
    test eax, eax
    jnz .rnext
    invoke xl_attr, rsi, &xk_target, &rsp+XP_NAME, 200
    mov word [rdi], '>'
    test eax, eax
    jz .nf
    ; part name: "/xl/worksheets/x.xml" is absolute, "worksheets/x.xml" is relative to the xl folder
    lea rsi, [rsp+XP_NAME]
    mov word [rsp+XP_PART], 0
    cmp word [rsi], '/'
    jne .relp
    add rsi, 2
    jmp .cat
.relp:
    invoke wcscpy, &rsp+XP_PART, &xl_dir
.cat:
    invoke wcscat, &rsp+XP_PART, rsi
    lea rax, [rsp+XP_PART]
.sl:
    cmp word [rax], 0
    je .sld
    cmp word [rax], '/'
    jne .sln
    mov word [rax], 0x5C
.sln:
    add rax, 2
    jmp .sl
.sld:
    invoke free, r14
    invoke xl_find, rbx, &rsp+XP_PART
    EPILOG 1216
.rnext:
    mov word [rdi], '>'
    lea rsi, [rdi+2]
    jmp .r
.nf:
    invoke free, r14
.no:
    xor eax, eax
    EPILOG 1216

; xl_read(VT*, VE*, tmp name) -> malloc'd UTF-16 text or 0
xl_read:
    PROLOG 1216
    mov rbx, rcx
    mov rsi, rdx
    mov r12, r8
    invoke GetTempPathW, 520, &rsp+96
    invoke wcslen, &rsp+96
    lea r13, [rsp+96+rax*2]
    call GetCurrentProcessId
    mov r14d, eax
    invoke _snwprintf, r13, 60, &f_xl_tmp, r14, r12
    invoke zip_extract, rbx, rsi, &rsp+96
    test eax, eax
    jz .no
    invoke read_file_bytes, &rsp+96, &rsp+1150
    mov r13, rax
    invoke DeleteFileW, &rsp+96
    test r13, r13
    jz .no
    mov r14, [rsp+1150]
    xor esi, esi
    cmp r14, 3
    jb .bom
    cmp byte [r13], 0xEF
    jne .bom
    cmp byte [r13+1], 0xBB
    jne .bom
    cmp byte [r13+2], 0xBF
    jne .bom
    mov esi, 3
.bom:
    lea rcx, [r14*2+8]
    call malloc
    mov r15, rax
    test rax, rax
    jz .free
    lea rdi, [r13+rsi]
    mov rbp, r14
    sub rbp, rsi
    lea rbx, [r14+1]
    invoke MultiByteToWideChar, 65001, 0, rdi, rbp, r15, rbx
    test eax, eax
    jns .term
    xor eax, eax
.term:
    mov word [r15+rax*2], 0
.free:
    invoke free, r13
    mov rax, r15
    EPILOG 1216
.no:
    xor eax, eax
    EPILOG 1216

; xlsx_load(path, SIDE*, sheet selector or 0) -> eax 1 ok
;   without a selector the first worksheet part is read (sheet1.xml), as before
xlsx_load:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    mov [rsp+144], r8
    invoke memset, &rsp+96, 0, XV_STRUCT
    invoke zip_open, r12, &rsp+96
    test eax, eax
    jz .fail
    mov rax, [rsp+144]
    test rax, rax
    jz .first
    cmp word [rax], 0
    je .first
    invoke xl_pick, &rsp+96, rax
    jmp .picked
.first:
    invoke xl_sheet, &rsp+96
.picked:
    test rax, rax
    jz .closefail
    mov rsi, rax
    invoke xl_read, &rsp+96, rsi, &xl_tmp2
    mov r14, rax                      ; sheet text
    test rax, rax
    jz .closefail
    xor r15d, r15d                    ; shared strings text
    invoke xl_find, &rsp+96, &xl_sst
    test rax, rax
    jz .nosst
    invoke xl_read, &rsp+96, rax, &xl_tmp1
    mov r15, rax
.nosst:
    invoke wcslen, r14
    mov rbx, rax
    test r15, r15
    jz .cap
    invoke wcslen, r15
    add rbx, rax
.cap:
    add rbx, 64
    invoke calloc, 1, XC_SIZE
    mov rsi, rax                      ; XC
    test rax, rax
    jz .cleanup
    lea rcx, [rbx*2]
    call malloc
    mov [rsi+XC_BASE], rax
    mov [rsi+XC_PTR], rax
    mov word [rax], 0
    mov [rsi+XC_EMPTY], rax
    add qword [rsi+XC_PTR], 2
    mov [rsi+XC_SIDE], r13
    mov dword [rsi+XC_MAXCOL], -1
    mov dword [rsi+XC_NEXT], 1
    test r15, r15
    jz .sheet
    invoke xl_sst_parse, rsi, r15
.sheet:
    invoke xl_sheet_parse, rsi, r14
    mov rax, [rsi+XC_BASE]
    mov [r13+SD_TEXT], rax
    ; sentinel row offset
    invoke vec_push, &r13+SD_ROWOFF, 4
    mov rcx, [r13+SD_CELLS+8]
    mov [rax], ecx
    invoke free, [rsi+XC_STRS]
    invoke free, rsi
    invoke free, r14
    invoke free, r15
    invoke vt_free, &rsp+96
    mov eax, 1
    EPILOG 192
.cleanup:
    invoke free, r14
    invoke free, r15
.closefail:
    invoke vt_free, &rsp+96
.fail:
    xor eax, eax
    EPILOG 192
