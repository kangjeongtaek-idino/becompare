; props.asm - property compare sessions sharing one list view:
;   T_VER   (VR-01) file version resources            props_ver.inc
;   T_REG   (RG-01) Windows registry keys / values    props_reg.inc
;   T_MEDIA (MD-01) audio tags (ID3v1/v2, FLAC)       props_media.inc
%include "inc/macros.inc"
%include "inc/win.inc"
%include "inc/app.inc"
%include "inc/export.inc"

extern CreateWindowExW, DefWindowProcW, SendMessageW, MoveWindow, GetClientRect, GetDlgItem, EnableWindow
extern SetWindowLongPtrW, GetWindowLongPtrW, FillRect, SetFocus, GetWindowTextW, SetWindowTextW, InvalidateRect, SetWindowTheme
extern GetFileAttributesExW, GetFileAttributesW, CreateFileW, ReadFile, CloseHandle, GetFileSizeEx, SetFilePointerEx
extern RegOpenKeyExW, RegCloseKey, RegEnumValueW, RegEnumKeyExW
extern malloc, calloc, realloc, free, memset, memcpy, wcslen, wcscpy, wcscat, wcsncpy, wcsrchr, wcschr, wcscmp, _wcsicmp, _wcsnicmp, _snwprintf, qsort
extern g_hinst, g_dark, th_bg, th_fg, th_panel, th_diff, th_lonly, th_ronly, th_same, th_panelbrush
extern mk, reg_class, scl, set_status, msgbox
extern mk_btn, mk_chk, mk_lbl, get_chk, ctl_color, theme_checks, lv_dark_header
extern pb_create, pb_layout, pb_load, pb_collect, pb_command
extern set_tab_title, str_dup, fmt_size, fmt_time, crc32_update
extern sb_free, PostMessageW, g_batch, g_dx_path, g_exitcode, g_hframe, batch_done
extern GetFileVersionInfoSizeW, GetFileVersionInfoW, VerQueryValueW

global props_register, props_create

; property object (one per session)
%define P_ROWS   0
%define P_N      8
%define P_CAP    16
%define P_VIS    24
%define P_NVIS   32
%define P_HLIST  40
%define P_MODE   48
%define P_SIZE   64

; row: name, a, b, status
%define R_NAME   0
%define R_A      8
%define R_B      16
%define R_ST     24
%define R_SZ     32

%define ID_LST   700
%define ID_CHKD  701
%define ID_LBLI  702
%define ID_BTN_DOC 703

section .rdata
WSTR cn_props, "BCProps"
WSTR cn_listv, "SysListView32"
WSTR s_empty, ""
WSTR s_light_theme, "Explorer"
WSTR s_dark_theme, "DarkMode_Explorer"
WSTR h_item, "항목"
WSTR h_a, "A (왼쪽)"
WSTR h_b, "B (오른쪽)"
WSTR h_st, "상태"
WSTR s_chk_diff, "다른 항목만 보기"
WSTR st_same, "같음"
WSTR st_diff, "다름"
WSTR st_onlya, "A만"
WSTR st_onlyb, "B만"
WSTR f_info, "다른 항목 %d · 한쪽에만 %d · 같음 %d (총 %d)"
WSTR f_title, "%s ↔ %s"
WSTR f_opts, "%d"
WSTR s_err_nopath, "A와 B 경로를 모두 입력하세요."
WSTR s_err_open, "대상을 읽을 수 없습니다."

align 8
col_text: dq h_item, h_a, h_b, h_st
col_w:    dd 260, 360, 360, 70
st_names: dq st_same, st_diff, st_onlya, st_onlyb

section .text

props_register:
    PROLOG 64
    invoke reg_class, &cn_props, &pp_proc, 0
    EPILOG 64

; props_create(parent, sess) -> hwnd
props_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_props, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- row store
; pr_dupn(rcx=wide or 0) -> copy or 0
pr_dupn:
    test rcx, rcx
    jz .z
    jmp str_dup
.z:
    xor eax, eax
    ret

; pr_free_rows(P)
pr_free_rows:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
.l:
    cmp rsi, [rbx+P_N]
    jae .done
    mov rdi, rsi
    shl rdi, 5
    add rdi, [rbx+P_ROWS]
    invoke free, [rdi+R_NAME]
    invoke free, [rdi+R_A]
    invoke free, [rdi+R_B]
    inc rsi
    jmp .l
.done:
    mov qword [rbx+P_N], 0
    invoke free, [rbx+P_VIS]
    mov qword [rbx+P_VIS], 0
    mov qword [rbx+P_NVIS], 0
    EPILOG 64

; pr_add(P, name, a or 0, b or 0): status derived from presence / equality
pr_add:
    PROLOG 64
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    mov r14, r9
    mov rax, [rbx+P_N]
    cmp rax, [rbx+P_CAP]
    jb .room
    lea r15, [rax*2+64]
    mov [rbx+P_CAP], r15
    shl r15, 5
    invoke realloc, [rbx+P_ROWS], r15
    mov [rbx+P_ROWS], rax
.room:
    mov r15, [rbx+P_N]
    shl r15, 5
    add r15, [rbx+P_ROWS]
    inc qword [rbx+P_N]
    invoke str_dup, r12
    mov [r15+R_NAME], rax
    invoke pr_dupn, r13
    mov [r15+R_A], rax
    invoke pr_dupn, r14
    mov [r15+R_B], rax
    mov dword [r15+R_ST], 2
    test r14, r14
    jz .done
    mov dword [r15+R_ST], 3
    test r13, r13
    jz .done
    invoke wcscmp, r13, r14
    test eax, eax
    setnz al
    movzx eax, al
    mov [r15+R_ST], eax
.done:
    EPILOG 64

; pr_vis(P, onlydiff): rebuild the visible index list
pr_vis:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    invoke free, [rbx+P_VIS]
    mov rcx, [rbx+P_N]
    lea rcx, [rcx*4+4]
    call malloc
    mov [rbx+P_VIS], rax
    mov rsi, rax
    xor edi, edi
    xor ecx, ecx
.l:
    cmp rcx, [rbx+P_N]
    jae .done
    mov rax, rcx
    shl rax, 5
    add rax, [rbx+P_ROWS]
    test r12d, r12d
    jz .take
    cmp dword [rax+R_ST], 0
    je .skip
.take:
    mov [rsi+rdi*4], ecx
    inc edi
.skip:
    inc rcx
    jmp .l
.done:
    mov [rbx+P_NVIS], rdi
    EPILOG 64

; ---------------------------------------------------------------- builders
%include "props_ver.inc"
%include "props_reg.inc"
%include "props_media.inc"
%include "props_doc.inc"

; ---------------------------------------------------------------- view
; pp_layout(hwnd)
pp_layout:
    PROLOG 192
    mov rsi, rcx
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, 4
    call scl
    mov r14d, eax
    mov ecx, 26
    call scl
    mov r15d, eax
    mov edx, [rsp+104]
    invoke pb_layout, rsi, rdx, r14
    mov r13d, eax
    mov r12d, r14d
    mov ecx, 96
    call scl
    mov ebx, eax
    MVID rsi, ID_COMPARE, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
    mov ecx, 170
    call scl
    mov ebx, eax
    MVID rsi, ID_CHKD, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
    mov ecx, 90
    call scl
    mov ebx, eax
    MVID rsi, ID_BTN_DOC, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
    mov ecx, 520
    call scl
    mov ebx, eax
    MVID rsi, ID_LBLI, r12, r13, rbx, r15
    lea r13d, [r13+r15]
    add r13d, r14d
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .h
    xor ebp, ebp
.h:
    mov ebx, [rsp+104]
    MVID rsi, ID_LST, 0, r13, rbx, rbp
    EPILOG 192

; pp_theme(P, hview)
pp_theme:
    PROLOG 64
    mov rbx, rcx
    mov rdi, rdx
    mov rsi, [rbx+P_HLIST]
    invoke SendMessageW, rsi, LVM_SETBKCOLOR, 0, #[th_bg]
    invoke SendMessageW, rsi, LVM_SETTEXTCOLOR, 0, #[th_fg]
    invoke SendMessageW, rsi, LVM_SETTEXTBKCOLOR, 0, #[th_bg]
    lea rdx, [s_light_theme]
    cmp qword [g_dark], 0
    je .t
    lea rdx, [s_dark_theme]
.t:
    invoke SetWindowTheme, rsi, rdx, 0
    invoke InvalidateRect, rsi, 0, 1
    invoke theme_checks, rdi
    EPILOG 64

; pp_info(hwnd, P)
pp_info:
    PROLOG 320
    mov rsi, rcx
    mov rbx, rdx
    xor r12d, r12d                   ; diff
    xor r13d, r13d                   ; one-sided
    xor r14d, r14d                   ; same
    xor ecx, ecx
.l:
    cmp rcx, [rbx+P_N]
    jae .done
    mov rax, rcx
    shl rax, 5
    add rax, [rbx+P_ROWS]
    mov eax, [rax+R_ST]
    test eax, eax
    jz .same
    cmp eax, 1
    je .diff
    inc r13d
    jmp .nx
.same:
    inc r14d
    jmp .nx
.diff:
    inc r12d
.nx:
    inc rcx
    jmp .l
.done:
    mov r15, [rbx+P_N]
    invoke _snwprintf, &rsp+96, 100, &f_info, r12, r13, r14, r15
    invoke GetDlgItem, rsi, ID_LBLI
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 320

; pp_refill(hwnd, P): apply the diff filter to the list
pp_refill:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke get_chk, rsi, ID_CHKD
    invoke pr_vis, rbx, rax
    invoke SendMessageW, [rbx+P_HLIST], LVM_SETITEMCOUNT, [rbx+P_NVIS], 0
    invoke InvalidateRect, [rbx+P_HLIST], 0, 1
    invoke pp_info, rsi, rbx
    EPILOG 64

; pp_tail(path) -> pointer to the last component
pp_tail:
    PROLOG 64
    mov rbx, rcx
    invoke wcsrchr, rbx, 0x5C
    test rax, rax
    jz .whole
    lea rax, [rax+2]
    EPILOG 64
.whole:
    mov rax, rbx
    EPILOG 64

; pp_compare(hwnd, P, sess)
pp_compare:
    PROLOG 2304
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    lea r13, [rsp+96]                ; path A (520 wchar)
    lea r14, [rsp+1144]              ; path B
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, r13, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, r14, 519
    cmp word [r13], 0
    je .nopath
    cmp word [r14], 0
    je .nopath
    invoke pr_free_rows, rbx
    mov rax, [r12+SS_TYPE]
    cmp rax, T_VER
    je .ver
    cmp rax, T_REG
    je .reg
    invoke pr_media, rbx, r13, r14
    jmp .built
.ver:
    invoke pr_ver, rbx, r13, r14
    jmp .built
.reg:
    invoke pr_reg, rbx, r13, r14
.built:
    test eax, eax
    jz .err
    invoke pp_refill, rsi, rbx
    invoke pp_tail, r13
    mov r15, rax
    invoke pp_tail, r14
    invoke _snwprintf, &rsp+2200, 50, &f_title, r15, rax
    invoke set_tab_title, r12, &rsp+2200
    test qword [g_batch], 4           ; unattended: the first compare ends the run
    jz .okb
    test qword [g_batch], 0x20        ; /docx file: export it first
    jz .okx
    and qword [g_batch], ~0x20
    invoke pp_docx_path, rsi, rbx, r12, &g_dx_path
    test eax, eax
    jnz .okx
    mov qword [g_exitcode], 2
.okx:
    xor eax, eax                      ; exit code like /report: 0 same, 1 different, 2 error
    mov rcx, [rbx+P_N]
    mov rdx, [rbx+P_ROWS]
.okc:
    test rcx, rcx
    jz .okd
    or eax, [rdx+R_ST]
    add rdx, 32
    dec rcx
    jmp .okc
.okd:
    invoke batch_done, rax
.okb:
    EPILOG 2304
.nopath:
    invoke msgbox, rsi, &s_err_nopath, MB_OK|0x40
    EPILOG 2304
.err:
    invoke pp_refill, rsi, rbx
    invoke msgbox, rsi, &s_err_open, MB_OK|MB_ICONWARNING
    EPILOG 2304

pp_proc:
    PROLOG 320
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_ERASEBKGND
    je .erase
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    test rax, rax
    jz .def
    mov r14, rax
    mov r15, [r14+SS_DATA]
    test r15, r15
    jz .def
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_COMMAND
    je .command
    cmp edi, WM_NOTIFY
    je .notify
    cmp edi, BC_RUN
    je .run
    cmp edi, BC_COLLECT
    je .collect
    cmp edi, BC_THEME
    je .theme
    cmp edi, WM_CTLCOLOREDIT
    je .cedit
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLORBTN
    je .cstatic
    cmp edi, WM_SETFOCUS
    je .focus
    cmp edi, WM_DESTROY
    je .destroy
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 320

.create:
    mov r14, [r13]
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, r14
    invoke calloc, 1, P_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    mov rax, [r14+SS_TYPE]
    mov [r15+P_MODE], rax
    invoke pb_create, rsi
    xor r9d, r9d
    cmp word [r14+SS_OPTS], '1'
    sete r9b
    invoke mk_chk, rsi, &s_chk_diff, ID_CHKD, r9
    invoke mk_btn, rsi, &s_b_doc, ID_BTN_DOC
    invoke mk_lbl, rsi, &s_empty, ID_LBLI
    invoke tips_apply, rsi, &tt_props
    invoke mk, rsi, &cn_listv, 0, LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER, ID_LST
    mov [r15+P_HLIST], rax
    mov rbx, rax
    invoke SendMessageW, rbx, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_GRIDLINES
    invoke lv_dark_header, rbx
    xor ebp, ebp
.col:
    cmp ebp, 4
    jae .coldone
    xor eax, eax
    mov [rsp+96], rax
    mov [rsp+104], rax
    mov [rsp+112], rax
    mov [rsp+120], rax
    mov [rsp+128], rax
    mov dword [rsp+96], LVCF_FMT|LVCF_WIDTH|LVCF_TEXT
    lea rax, [col_w]
    mov ecx, [rax+rbp*4]
    call scl
    mov [rsp+104], eax
    lea rax, [col_text]
    mov rax, [rax+rbp*8]
    mov [rsp+112], rax
    invoke SendMessageW, rbx, LVM_INSERTCOLUMNW, rbp, &rsp+96
    inc ebp
    jmp .col
.coldone:
    cmp qword [r15+P_MODE], T_REG
    jne .nobr
    invoke GetDlgItem, rsi, ID_BROWSE_A
    invoke EnableWindow, rax, 0
    invoke GetDlgItem, rsi, ID_BROWSE_B
    invoke EnableWindow, rax, 0
.nobr:
    invoke pb_load, rsi, r14
    invoke pp_theme, r15, rsi
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320

.size:
    invoke pp_layout, rsi
    jmp .zero
.run:
    invoke pp_compare, rsi, r15, r14
    jmp .zero
.collect:
    invoke pb_collect, rsi, r14
    invoke get_chk, rsi, ID_CHKD
    invoke _snwprintf, &r14+SS_OPTS, 1023, &f_opts, rax
    jmp .zero
.theme:
    invoke pp_theme, r15, rsi
    jmp .zero

.command:
    movzx ebx, r12w
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_CHKD
    je .filter
    cmp ebx, ID_BTN_DOC
    je .doc
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_BROWSE_B
    je .browse
    cmp ebx, ID_SWAP
    je .browse
    jmp .def
.browse:
    invoke pb_command, rsi, rbx, 0
    jmp .zero
.filter:
    invoke pp_refill, rsi, r15
    jmp .zero
.doc:
    invoke pp_export, rsi, r15, r14
    jmp .zero

.notify:
    mov rax, [r13+NMH_FROM]
    cmp rax, [r15+P_HLIST]
    jne .def
    mov eax, [r13+NMH_CODE]
    cmp eax, LVN_GETDISPINFOW
    je .disp
    cmp eax, NM_CUSTOMDRAW
    je .cdraw
    jmp .def
.disp:
    test dword [r13+DI_MASK], LVIF_TEXT
    jz .zero
    mov eax, [r13+DI_ITEM]
    cmp rax, [r15+P_NVIS]
    jae .zero
    mov rcx, [r15+P_VIS]
    mov eax, [rcx+rax*4]
    shl rax, 5
    add rax, [r15+P_ROWS]
    mov ecx, [r13+DI_SUB]
    lea rdx, [s_empty]
    cmp ecx, 1
    jb .c0
    je .c1
    cmp ecx, 2
    je .c2
    mov ecx, [rax+R_ST]
    lea rdx, [st_names]
    mov rdx, [rdx+rcx*8]
    jmp .cset
.c0:
    mov rcx, [rax+R_NAME]
    jmp .cptr
.c1:
    mov rcx, [rax+R_A]
    jmp .cptr
.c2:
    mov rcx, [rax+R_B]
.cptr:
    test rcx, rcx
    jz .cset
    mov rdx, rcx
.cset:
    mov [r13+DI_TEXT], rdx
    jmp .zero
.cdraw:
    mov eax, [r13+CD_STAGE]
    cmp eax, CDDS_PREPAINT
    jne .cd2
    mov eax, CDRF_NOTIFYITEMDRAW
    EPILOG 320
.cd2:
    cmp eax, CDDS_ITEMPREPAINT
    jne .zero
    mov rax, [r13+CD_ITEMSPEC]
    cmp rax, [r15+P_NVIS]
    jae .zero
    mov rcx, [r15+P_VIS]
    mov eax, [rcx+rax*4]
    shl rax, 5
    add rax, [r15+P_ROWS]
    mov ecx, [rax+R_ST]
    mov edx, [th_bg]
    mov r8d, [th_same]
    cmp ecx, 1
    jne .d2
    mov edx, [th_diff]
    mov r8d, [th_fg]
    jmp .dset
.d2:
    cmp ecx, 2
    jne .d3
    mov edx, [th_lonly]
    mov r8d, [th_fg]
    jmp .dset
.d3:
    cmp ecx, 3
    jne .dset
    mov edx, [th_ronly]
    mov r8d, [th_fg]
.dset:
    mov [r13+CD_CLRTEXTBK], edx
    mov [r13+CD_CLRTEXT], r8d
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+P_HLIST]
    jmp .zero
.destroy:
    invoke pr_free_rows, r15
    invoke free, [r15+P_ROWS]
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320

; ---------------------------------------------------------------- tooltips of this view
extern tips_apply
section .rdata
WSTR tt_props_0, "값이 다른 항목만 표시합니다"
WSTR tt_props_1, "비교 결과를 워드 파일(.docx)로 저장합니다"
align 8
tt_props:
    dq ID_CHKD, tt_props_0
    dq ID_BTN_DOC, tt_props_1
    dq 0, 0
section .text
