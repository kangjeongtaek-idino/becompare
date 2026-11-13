; edit_ui.inc - Text Edit session: single file editor (RichEdit) with find / replace, syntax colours, encoding aware save

%define E_TXT     0                  ; TXT { text, len, enc, crlf, final-eol } (32 bytes)
%define E_LOADED  32
%define E_HR      40
%define E_SIZE    64

%define ID_ED_SAVE   301
%define ID_ED_SAVEAS 302
%define ID_ED_FIND   303
%define ID_ED_NEXT   304
%define ID_ED_PREV   305
%define ID_ED_REPL   306
%define ID_ED_RONE   307
%define ID_ED_RALL   308
%define ID_ED_CASE   309
%define ID_ED_SYN    310
%define ID_ED_INFO   311
%define ID_ED_LFIND  312
%define ID_ED_LREPL  313
%define EM_FINDTEXTEXW 0x047C
%define EM_GETSELTEXT  0x043E
%define EN_SELCHANGE   0x0702

extern browse_save, GetFileAttributesW, CreateWindowExW

section .rdata
WSTR cn_edit, "BCEdit"
WSTR s_ed_dots, "..."
WSTR s_ed_open, "열기"
WSTR s_ed_save, "저장"
WSTR s_ed_saveas, "다른 이름으로 저장..."
WSTR s_ed_lfind, "찾기:"
WSTR s_ed_next, "다음 ▼"
WSTR s_ed_prev, "이전 ▲"
WSTR s_ed_lrepl, "바꾸기:"
WSTR s_ed_rone, "바꾸기"
WSTR s_ed_rall, "모두 바꾸기"
WSTR s_ed_case, "대소문자 구분"
WSTR f_ed_info, "줄 %d, 열 %d · 총 %d줄 · %s · %s"
WSTR f_ed_rep, "%d곳을 바꿨습니다."
WSTR s_ed_nf, "찾을 수 없습니다."
WSTR f_ed_opts, "%d%d"
WSTR s_ed_nopath, "저장할 파일 경로를 입력하세요."
WSTR s_ed_loadfail, "파일을 읽을 수 없습니다."

section .text

edit_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_edit, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

%macro EPLACE 2
    mov ecx, %2
    call scl
    mov ebx, eax
    MVID rsi, %1, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
%endmacro

; ed_layout(hwnd)
ed_layout:
    PROLOG 192
    mov rsi, rcx
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, 4
    call scl
    mov r14d, eax
    mov ecx, 26
    call scl
    mov r15d, eax
    mov r12d, r14d
    mov r13d, r14d
    ; row 1: path edit takes the remaining width
    mov ecx, 30+70+70+160
    call scl
    mov ebx, eax
    lea eax, [r14*4+r14]
    add ebx, eax
    mov ebp, [rsp+104]
    sub ebp, ebx
    jns .pw
    mov ebp, 100
.pw:
    MVID rsi, ID_EDIT_A, r12, r13, rbp, r15
    lea r12d, [r12+rbp]
    add r12d, r14d
    EPLACE ID_BROWSE_A, 30
    EPLACE ID_COMPARE, 70
    EPLACE ID_ED_SAVE, 70
    EPLACE ID_ED_SAVEAS, 160
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    EPLACE ID_ED_LFIND, 40
    EPLACE ID_ED_FIND, 180
    EPLACE ID_ED_NEXT, 76
    EPLACE ID_ED_PREV, 76
    EPLACE ID_ED_LREPL, 56
    EPLACE ID_ED_REPL, 180
    EPLACE ID_ED_RONE, 76
    EPLACE ID_ED_RALL, 100
    EPLACE ID_ED_CASE, 120
    EPLACE ID_ED_SYN, 90
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    EPLACE ID_ED_INFO, 700
    lea r13d, [r13+r15]
    add r13d, r14d
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .h
    xor ebp, ebp
.h:
    mov ebx, [rsp+104]
    MVID rsi, ID_RA, 0, r13, rbx, rbp
    EPILOG 192

; ed_info(hwnd, E)
ed_info:
    PROLOG 320
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, [rbx+E_HR]
    invoke SendMessageW, rdi, EM_EXGETSEL, 0, &rsp+96
    mov r12d, [rsp+96]
    invoke SendMessageW, rdi, EM_LINEFROMCHAR, r12, 0
    mov r13d, eax
    invoke SendMessageW, rdi, EM_LINEINDEX, r13, 0
    mov r14d, r12d
    sub r14d, eax
    inc r14d                          ; column
    inc r13d                          ; line
    invoke SendMessageW, rdi, EM_GETLINECOUNT, 0, 0
    mov r15d, eax
    mov eax, [rbx+E_TXT+16]
    and eax, 3
    lea rcx, [enc_names]
    mov rbp, [rcx+rax*8]
    mov eax, [rbx+E_TXT+20]
    and eax, 1
    lea rcx, [eol_names]
    mov rdi, [rcx+rax*8]
    invoke _snwprintf, &rsp+112, 60, &f_ed_info, r13, r14, r15, rbp, rdi
    invoke GetDlgItem, rsi, ID_ED_INFO
    invoke SetWindowTextW, rax, &rsp+112
    EPILOG 320

; ed_syn(hwnd, E): recolour the text for the current file type
ed_syn:
    PROLOG 1216
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, [rbx+E_HR]
    invoke SendMessageW, rdi, WM_SETREDRAW, 0, 0
    invoke rich_theme, rdi
    invoke get_chk, rsi, ID_ED_SYN
    test eax, eax
    jz .off
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rsp+96, 519
    invoke syn_lang, &rsp+96
    mov r12d, eax
    test r12d, r12d
    jz .off
    invoke rich_get_text, rdi, &rsp+1144
    mov r13, rax
    test rax, rax
    jz .off
    invoke syn_run, rdi, r13, [rsp+1144], r12
    invoke free, r13
    invoke rich_unsel, rdi
.off:
    invoke SendMessageW, rdi, WM_SETREDRAW, 1, 0
    invoke InvalidateRect, rdi, 0, 1
    EPILOG 1216

; ed_load(hwnd, E, sess)
ed_load:
    PROLOG 1216
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rsp+96, 519
    cmp word [rsp+96], 0
    je .none
    invoke GetFileAttributesW, &rsp+96
    cmp eax, -1
    je .newfile
    invoke free, [rbx+E_TXT]
    mov qword [rbx+E_TXT], 0
    invoke text_load, &rsp+96, &rbx+E_TXT
    test eax, eax
    jz .fail
    jmp .show
.newfile:
    invoke free, [rbx+E_TXT]
    mov ecx, 16
    call malloc
    mov [rbx+E_TXT], rax
    mov word [rax], 0
    mov qword [rbx+E_TXT+8], 0
    mov dword [rbx+E_TXT+16], 1
    mov dword [rbx+E_TXT+20], 1
    mov dword [rbx+E_TXT+24], 0
.show:
    invoke rich_set_text, [rbx+E_HR], [rbx+E_TXT]
    mov qword [rbx+E_LOADED], 1
    invoke ed_syn, rsi, rbx
    invoke ed_info, rsi, rbx
    invoke wcsrchr, &rsp+96, 0x5C
    lea rdx, [rsp+96]
    test rax, rax
    jz .t
    lea rdx, [rax+2]
.t:
    invoke set_tab_title, r12, rdx
    invoke SetFocus, [rbx+E_HR]
.none:
    EPILOG 1216
.fail:
    invoke msgbox, rsi, &s_ed_loadfail, MB_OK|MB_ICONWARNING
    EPILOG 1216

; ed_save(hwnd, E, saveas)
ed_save:
    PROLOG 1216
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    invoke GetDlgItem, rsi, ID_EDIT_A
    mov r13, rax
    invoke GetWindowTextW, r13, &rsp+96, 519
    test r12d, r12d
    jnz .ask
    cmp word [rsp+96], 0
    jne .have
.ask:
    invoke browse_save, rsi, &rsp+96
    test eax, eax
    jz .d
    invoke SetWindowTextW, r13, &rsp+96
.have:
    cmp qword [rbx+E_LOADED], 0
    jne .go
    mov qword [rbx+E_LOADED], 1
    invoke free, [rbx+E_TXT]
    mov ecx, 16
    call malloc
    mov [rbx+E_TXT], rax
    mov dword [rbx+E_TXT+16], 1
    mov dword [rbx+E_TXT+20], 1
    mov dword [rbx+E_TXT+24], 0
.go:
    invoke rich_get_text, [rbx+E_HR], &rsp+1144
    test rax, rax
    jz .d
    mov rbp, rax
    mov r13, [rsp+1144]
    mov r9d, [rbx+E_TXT+20]
    shl r9d, 8
    or r9d, [rbx+E_TXT+16]
    invoke text_save, &rsp+96, rbp, r13, r9
    mov r14d, eax
    invoke free, rbp
    test r14d, r14d
    jnz .saved
    invoke save_error, rsi
    jmp .d
.saved:
    invoke SendMessageW, [rbx+E_HR], EM_SETMODIFY, 0, 0
    invoke set_status, &s_saved
    invoke ed_syn, rsi, rbx
.d:
    EPILOG 1216

; ed_find(hwnd, E, down) -> eax 1 found
ed_find:
    PROLOG 640
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov rdi, [rbx+E_HR]
    invoke GetDlgItem, rsi, ID_ED_FIND
    invoke GetWindowTextW, rax, &rsp+256, 150
    cmp word [rsp+256], 0
    je .nf
    mov r13d, 0
    invoke get_chk, rsi, ID_ED_CASE
    test eax, eax
    jz .nocase
    mov r13d, 4                       ; FR_MATCHCASE
.nocase:
    test r12d, r12d
    jz .up
    or r13d, 1                        ; FR_DOWN
.up:
    invoke SendMessageW, rdi, EM_EXGETSEL, 0, &rsp+96
    mov eax, [rsp+96]                 ; selection start
    mov ecx, [rsp+100]                ; selection end
    xor r14d, r14d                    ; wrapped
.try:
    test r12d, r12d
    jz .fup
    mov [rsp+112], ecx                ; cpMin = selection end
    mov dword [rsp+116], -1
    jmp .fcall
.fup:
    mov [rsp+112], eax                ; search upwards from the selection start
    mov dword [rsp+116], 0
.fcall:
    lea rdx, [rsp+256]
    mov [rsp+120], rdx
    mov qword [rsp+128], 0
    invoke SendMessageW, rdi, EM_FINDTEXTEXW, r13, &rsp+112
    cmp eax, -1
    jne .found
    test r14d, r14d
    jnz .nf
    mov r14d, 1
    invoke SendMessageW, rdi, WM_GETTEXTLENGTH, 0, 0
    mov ecx, 0                        ; wrap: restart from the beginning (down) or the end (up)
    test r12d, r12d
    jnz .wr
    mov ecx, eax
    mov eax, eax
    jmp .try
.wr:
    xor eax, eax
    xor ecx, ecx
    jmp .try
.found:
    invoke SendMessageW, rdi, EM_EXSETSEL, 0, &rsp+128
    invoke SendMessageW, rdi, EM_SCROLLCARET, 0, 0
    mov eax, 1
    EPILOG 640
.nf:
    invoke set_status, &s_ed_nf
    xor eax, eax
    EPILOG 640

; ed_replace(hwnd, E, all)
ed_replace:
    PROLOG 1216
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov rdi, [rbx+E_HR]
    invoke GetDlgItem, rsi, ID_ED_FIND
    invoke GetWindowTextW, rax, &rsp+96, 200
    cmp word [rsp+96], 0
    je .d
    invoke GetDlgItem, rsi, ID_ED_REPL
    invoke GetWindowTextW, rax, &rsp+600, 200
    test r12d, r12d
    jnz .all
    ; single: replace the selection when it matches, then move on
    invoke SendMessageW, rdi, EM_EXGETSEL, 0, &rsp+1208
    mov eax, [rsp+1212]
    sub eax, [rsp+1208]
    jle .next
    cmp eax, 100
    ja .next
    invoke SendMessageW, rdi, EM_GETSELTEXT, 0, &rsp+1000
    invoke get_chk, rsi, ID_ED_CASE
    test eax, eax
    jz .ci
    invoke wcscmp, &rsp+1000, &rsp+96
    jmp .cmpd
.ci:
    invoke _wcsicmp, &rsp+1000, &rsp+96
.cmpd:
    test eax, eax
    jnz .next
    invoke SendMessageW, rdi, EM_REPLACESEL, 1, &rsp+600
.next:
    invoke ed_find, rsi, rbx, 1
    jmp .d
.all:
    invoke SendMessageW, rdi, EM_SETSEL, 0, 0
    xor r13d, r13d
.loop:
    cmp r13d, 100000
    jae .done
    invoke ed_findfwd, rsi, rbx
    test eax, eax
    jz .done
    invoke SendMessageW, rdi, EM_REPLACESEL, 1, &rsp+600
    inc r13d
    jmp .loop
.done:
    invoke _snwprintf, &rsp+1000, 60, &f_ed_rep, r13
    invoke set_status, &rsp+1000
.d:
    EPILOG 1216

; ed_findfwd(hwnd, E): forward find without wrapping -> eax 1 found
ed_findfwd:
    PROLOG 640
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, [rbx+E_HR]
    invoke GetDlgItem, rsi, ID_ED_FIND
    invoke GetWindowTextW, rax, &rsp+256, 150
    mov r13d, 1
    invoke get_chk, rsi, ID_ED_CASE
    test eax, eax
    jz .nc
    or r13d, 4
.nc:
    invoke SendMessageW, rdi, EM_EXGETSEL, 0, &rsp+96
    mov ecx, [rsp+100]
    mov [rsp+112], ecx
    mov dword [rsp+116], -1
    lea rdx, [rsp+256]
    mov [rsp+120], rdx
    mov qword [rsp+128], 0
    invoke SendMessageW, rdi, EM_FINDTEXTEXW, r13, &rsp+112
    cmp eax, -1
    je .no
    invoke SendMessageW, rdi, EM_EXSETSEL, 0, &rsp+128
    mov eax, 1
    EPILOG 640
.no:
    xor eax, eax
    EPILOG 640

ed_proc:
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
    cmp edi, BC_ISDIRTY
    je .isdirty
    cmp edi, BC_SAVE
    je .saveall
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
    invoke calloc, 1, E_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    invoke mk_edit, rsi, ID_EDIT_A
    invoke mk_btn, rsi, &s_ed_dots, ID_BROWSE_A
    invoke mk_btn, rsi, &s_ed_open, ID_COMPARE
    invoke mk_btn, rsi, &s_ed_save, ID_ED_SAVE
    invoke mk_btn, rsi, &s_ed_saveas, ID_ED_SAVEAS
    invoke mk_lbl, rsi, &s_ed_lfind, ID_ED_LFIND
    invoke mk_edit, rsi, ID_ED_FIND
    invoke mk_btn, rsi, &s_ed_next, ID_ED_NEXT
    invoke mk_btn, rsi, &s_ed_prev, ID_ED_PREV
    invoke mk_lbl, rsi, &s_ed_lrepl, ID_ED_LREPL
    invoke mk_edit, rsi, ID_ED_REPL
    invoke mk_btn, rsi, &s_ed_rone, ID_ED_RONE
    invoke mk_btn, rsi, &s_ed_rall, ID_ED_RALL
    invoke mk_chk, rsi, &s_ed_case, ID_ED_CASE, 0
    invoke mk_chk, rsi, &s_c_syn, ID_ED_SYN, 1
    invoke mk_lbl, rsi, &s_empty, ID_ED_INFO
    invoke tips_apply, rsi, &tt_edit
    invoke rich_create, rsi, ID_RA
    mov [r15+E_HR], rax
    ; options "<syntax><case>"
    cmp word [r14+SS_OPTS], 0
    je .noopt
    movzx r8d, word [r14+SS_OPTS]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_ED_SYN, r8
    movzx r8d, word [r14+SS_OPTS+2]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_ED_CASE, r8
.noopt:
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke SetWindowTextW, rax, &r14+SS_PATHA
    invoke rich_theme, [r15+E_HR]
    invoke theme_checks, rsi
    cmp word [r14+SS_PATHA], 0
    je .cdone
    invoke PostMessageW, rsi, BC_RUN, 0, 0
.cdone:
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320

.size:
    invoke ed_layout, rsi
    jmp .zero
.run:
    invoke ed_load, rsi, r15, r14
    jmp .zero
.collect:
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &r14+SS_PATHA, 519
    invoke get_chk, rsi, ID_ED_SYN
    mov ebx, eax
    invoke get_chk, rsi, ID_ED_CASE
    invoke _snwprintf, &r14+SS_OPTS, 1023, &f_ed_opts, rbx, rax
    jmp .zero
.saveall:
    invoke SendMessageW, [r15+E_HR], EM_GETMODIFY, 0, 0
    test eax, eax
    jz .zero
    invoke ed_save, rsi, r15, 0
    jmp .zero
.theme:
    invoke rich_theme, [r15+E_HR]
    invoke ed_syn, rsi, r15
    invoke theme_checks, rsi
    jmp .zero

.command:
    movzx ebx, r12w
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_ED_SAVE
    je .b_save
    cmp ebx, ID_ED_SAVEAS
    je .b_saveas
    cmp ebx, ID_ED_NEXT
    je .b_next
    cmp ebx, ID_ED_PREV
    je .b_prev
    cmp ebx, ID_ED_RONE
    je .b_rone
    cmp ebx, ID_ED_RALL
    je .b_rall
    cmp ebx, ID_ED_SYN
    je .b_syn
    jmp .def
.browse:
    invoke pb_command, rsi, rbx, 0
    cmp qword [r15+E_LOADED], 0
    jne .zero
    jmp .run
.b_save:
    invoke ed_save, rsi, r15, 0
    jmp .zero
.b_saveas:
    invoke ed_save, rsi, r15, 1
    jmp .zero
.b_next:
    invoke ed_find, rsi, r15, 1
    jmp .zero
.b_prev:
    invoke ed_find, rsi, r15, 0
    jmp .zero
.b_rone:
    invoke ed_replace, rsi, r15, 0
    jmp .zero
.b_rall:
    invoke ed_replace, rsi, r15, 1
    jmp .zero
.b_syn:
    invoke ed_syn, rsi, r15
    jmp .zero

.notify:
    mov eax, [r13+NMH_CODE]
    cmp eax, EN_SELCHANGE
    jne .def
    mov rax, [r13+NMH_FROM]
    cmp rax, [r15+E_HR]
    jne .def
    invoke ed_info, rsi, r15
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+E_HR]
    jmp .zero
.isdirty:
    invoke SendMessageW, [r15+E_HR], EM_GETMODIFY, 0, 0
    EPILOG 320
.destroy:
    invoke free, [r15+E_TXT]
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320

; ---------------------------------------------------------------- tooltips of this view
extern tips_apply
section .rdata
WSTR tt_edit_0, "편집할 파일 경로"
WSTR tt_edit_1, "파일 찾아보기"
WSTR tt_edit_2, "경로의 파일을 엽니다"
WSTR tt_edit_3, "파일을 저장합니다 (Ctrl+S)"
WSTR tt_edit_4, "다른 이름으로 저장합니다"
WSTR tt_edit_5, "파일 형식에 맞춰 구문에 색을 입힙니다"
WSTR tt_edit_6, "찾을 문자열"
WSTR tt_edit_7, "다음 일치 항목으로 이동합니다"
WSTR tt_edit_8, "이전 일치 항목으로 이동합니다"
WSTR tt_edit_9, "바꿀 문자열"
WSTR tt_edit_10, "현재 일치 항목을 바꿉니다"
WSTR tt_edit_11, "일치하는 항목을 모두 바꿉니다"
WSTR tt_edit_12, "찾을 때 대소문자를 구분합니다"
align 8
tt_edit:
    dq ID_EDIT_A, tt_edit_0
    dq ID_BROWSE_A, tt_edit_1
    dq ID_COMPARE, tt_edit_2
    dq ID_ED_SAVE, tt_edit_3
    dq ID_ED_SAVEAS, tt_edit_4
    dq ID_ED_SYN, tt_edit_5
    dq ID_ED_FIND, tt_edit_6
    dq ID_ED_NEXT, tt_edit_7
    dq ID_ED_PREV, tt_edit_8
    dq ID_ED_REPL, tt_edit_9
    dq ID_ED_RONE, tt_edit_10
    dq ID_ED_RALL, tt_edit_11
    dq ID_ED_CASE, tt_edit_12
    dq 0, 0
section .text
