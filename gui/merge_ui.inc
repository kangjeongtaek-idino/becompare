; merge_ui.inc - Text Merge: texts, navigation, resolve, save, layout, window procedure

; mg_sel_lines(hrich, out lo, out hi): first and last line of the selection (a caret = one line)
mg_sel_lines:
    PROLOG 96
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    invoke SendMessageW, rbx, EM_EXGETSEL, 0, &rsp+80
    mov r12d, [rsp+80]                ; cpMin
    mov r13d, [rsp+84]                ; cpMax
    invoke SendMessageW, rbx, EM_LINEFROMCHAR, r12, 0
    mov [rsi], eax
    mov [rdi], eax
    cmp r13d, r12d
    jbe .d
    lea r14d, [r13-1]                 ; a selection ending at a line start does not include that line
    invoke SendMessageW, rbx, EM_LINEFROMCHAR, r14, 0
    mov [rdi], eax
.d:
    EPILOG 96

; mg_align_set(hwnd, mg, sess): TM-06 pin the selected left lines to the selected right lines, then merge again
mg_align_set:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    cmp qword [rbx+MG_THREE], 0
    jne .three
    cmp qword [rbx+MG_PRIS], 0
    je .hint
    ; hand-edited output would be lost by regenerating
    invoke rich_get_text, [rbx+MG_HM], &rsp+96
    test rax, rax
    jz .sel
    mov r13, rax
    invoke wcscmp, r13, [rbx+MG_PRIS]
    mov r14d, eax
    invoke free, r13
    test r14d, r14d
    jz .sel
    invoke msgbox, rsi, &s_mg_edited, MB_YESNO|MB_ICONQUESTION
    cmp eax, IDYES
    jne .d
.sel:
    invoke mg_sel_lines, [rbx+MG_HL], &rsp+104, &rsp+108   ; left lo/hi
    invoke mg_sel_lines, [rbx+MG_HR], &rsp+112, &rsp+116   ; right lo/hi
    mov r13d, [rsp+104]
    mov r14d, [rsp+112]
    invoke tv_anchor_add, [rbx+MG_ANCB], r13, r14
    test eax, eax
    jz .conf
    ; a multi-line selection on both sides: the last lines are pinned together as well
    mov r13d, [rsp+108]
    mov r14d, [rsp+116]
    cmp r13d, [rsp+104]
    jbe .go
    cmp r14d, [rsp+112]
    jbe .go
    invoke tv_anchor_add, [rbx+MG_ANCB], r13, r14
    test eax, eax
    jz .conf
.go:
    invoke mg_merge, rsi, rbx, r12
    invoke set_status, &s_mg_ok
    jmp .d
.three:
    invoke set_status, &s_mg_3way
    jmp .d
.hint:
    invoke set_status, &s_mg_hint
    jmp .d
.conf:
    invoke msgbox, rsi, &s_mg_conf, MB_OK|MB_ICONWARNING
.d:
    EPILOG 192

; mg_align_clear(hwnd, mg, sess)
mg_align_clear:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov rax, [rbx+MG_ANCB]
    cmp dword [rax+2880], 0           ; TV_NANC
    je .d
    mov dword [rax+2880], 0
    cmp qword [rbx+MG_PRIS], 0
    je .d
    invoke mg_merge, rsi, rbx, r12
.d:
    EPILOG 64

; mg_free_texts(mg)
mg_free_texts:
    PROLOG 64
    mov rbx, rcx
    invoke free, [rbx+MG_TXL]
    invoke free, [rbx+MG_TXR]
    invoke free, [rbx+MG_TXO]
    xor eax, eax
    mov [rbx+MG_TXL], rax
    mov [rbx+MG_TXR], rax
    mov [rbx+MG_TXO], rax
    EPILOG 64

; mg_caretline(mg) -> eax line of the caret in the output pane
mg_caretline:
    PROLOG 96
    mov rbx, rcx
    invoke SendMessageW, [rbx+MG_HM], EM_EXGETSEL, 0, &rsp+80
    mov eax, [rsp+80]
    invoke SendMessageW, [rbx+MG_HM], EM_LINEFROMCHAR, rax, 0
    EPILOG 96

; mg_goto(mg, line): caret to the start of a line and into view
mg_goto:
    PROLOG 64
    mov rbx, rcx
    mov rdi, rdx
    invoke SendMessageW, [rbx+MG_HM], EM_LINEINDEX, rdi, 0
    movsxd rsi, eax
    invoke SendMessageW, [rbx+MG_HM], EM_SETSEL, rsi, rsi
    invoke SendMessageW, [rbx+MG_HM], EM_SCROLLCARET, 0, 0
    invoke SetFocus, [rbx+MG_HM]
    EPILOG 64

; mg_nav(mg, dir 1 next / 0 prev)
mg_nav:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    invoke mg_scan, rbx
    cmp qword [rbx+MG_CONF+8], 0
    je .none
    invoke mg_caretline, rbx
    mov r13d, eax
    mov rcx, [rbx+MG_CONF]
    xor edx, edx
    mov r14, -1
    test r12d, r12d
    jz .prev
.nx:
    cmp rdx, [rbx+MG_CONF+8]
    jae .have
    lea rax, [rdx+rdx*2]
    mov eax, [rcx+rax*4]             ; start line
    cmp eax, r13d
    ja .nf
    inc rdx
    jmp .nx
.nf:
    mov r14, rdx
    jmp .have
.prev:
    cmp rdx, [rbx+MG_CONF+8]
    jae .have
    lea rax, [rdx+rdx*2]
    mov eax, [rcx+rax*4]
    cmp eax, r13d
    jae .have
    mov r14, rdx
    inc rdx
    jmp .prev
.have:
    cmp r14, -1
    je .none
    lea rax, [r14+r14*2]
    mov rcx, [rbx+MG_CONF]
    mov edx, [rcx+rax*4]
    invoke mg_goto, rbx, rdx
    EPILOG 64
.none:
    invoke set_status, &s_noconf
    EPILOG 64

; mg_resolve(hwnd, mg, choice 0 left / 1 right / 2 both)
mg_resolve:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    invoke mg_scan, rbx
    cmp qword [rbx+MG_CONF+8], 0
    je .none
    invoke mg_caretline, rbx
    mov r13d, eax                    ; caret line
    ; the conflict holding the caret, else the next one
    mov rcx, [rbx+MG_CONF]
    xor edx, edx
    mov r14, -1
.f:
    cmp rdx, [rbx+MG_CONF+8]
    jae .picked
    lea rax, [rdx+rdx*2]
    lea r8, [rcx+rax*4]
    cmp [r8], r13d                   ; start > caret -> the next conflict
    ja .after
    cmp r13d, [r8+8]
    jbe .inside
    inc rdx
    jmp .f
.after:
.inside:
    mov r14, rdx
.picked:
    cmp r14, -1
    je .none
    ; rebuild the text
    invoke sb_init, &rsp+96
    xor edi, edi                     ; line
    xor r15d, r15d                   ; next conflict index to meet
.l:
    cmp rdi, [rbx+MG_SCL+8]
    jae .built
    cmp r15, [rbx+MG_CONF+8]
    jae .copy
    lea rax, [r15+r15*2]
    mov rcx, [rbx+MG_CONF]
    lea rbp, [rcx+rax*4]
    cmp edi, [rbp]
    jne .copy
    cmp r15, r14
    jne .keep
    ; chosen conflict: left part start+1..mid-1, right part mid+1..end-1
    cmp r12d, 1
    je .right
    mov r13d, [rbp]
    inc r13d
    mov eax, [rbp+4]
    invoke emit_range, &rsp+96, &rbx+MG_SCL, r13, rax
.right:
    cmp r12d, 0
    je .skip
    mov r13d, [rbp+4]
    inc r13d
    mov eax, [rbp+8]
    invoke emit_range, &rsp+96, &rbx+MG_SCL, r13, rax
.skip:
    mov edi, [rbp+8]
    inc edi
    inc r15
    jmp .l
.keep:
    mov r13d, [rbp]
    mov eax, [rbp+8]
    inc eax
    invoke emit_range, &rsp+96, &rbx+MG_SCL, r13, rax
    mov edi, [rbp+8]
    inc edi
    inc r15
    jmp .l
.copy:
    lea rax, [rdi+1]
    invoke emit_range, &rsp+96, &rbx+MG_SCL, rdi, rax
    inc rdi
    jmp .l
.built:
    mov rax, [rsp+96+8]
    test rax, rax
    jz .set
    dec rax
    mov [rsp+96+8], rax
    mov rcx, [rsp+96]
    mov word [rcx+rax*2], 0
.set:
    mov rax, [rsp+96]
    test rax, rax
    jnz .have
    lea rax, [s_empty]
.have:
    invoke rich_set_text, [rbx+MG_HM], rax
    invoke SendMessageW, [rbx+MG_HM], EM_SETMODIFY, 1, 0
    invoke rich_theme, [rbx+MG_HM]
    invoke sb_free, &rsp+96
    invoke mg_colorout, rbx
    invoke mg_info, rsi, rbx
    EPILOG 192
.none:
    invoke set_status, &s_nocur
    EPILOG 192

; mg_resolve_all(hwnd, mg, choice): accept one side for every remaining conflict (TM-05)
mg_resolve_all:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    xor r13d, r13d
.l:
    invoke mg_scan, rbx
    cmp qword [rbx+MG_CONF+8], 0
    je .d
    invoke SendMessageW, [rbx+MG_HM], EM_SETSEL, 0, 0
    invoke mg_resolve, rsi, rbx, r12
    inc r13d
    cmp r13d, 100000
    jb .l
.d:
    EPILOG 64

; mg_save(hwnd, mg, sess) -> eax 1 saved
mg_save:
    PROLOG 256
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke GetWindowTextW, rax, &r12+SS_PATHO, 519
    cmp word [r12+SS_PATHO], 0
    je .noout
    invoke mg_scan, rbx
    mov rax, [rbx+MG_CONF+8]
    test rax, rax
    jz .write
    invoke _snwprintf, &rsp+96, 100, &f_leftconf, rax
    invoke msgbox, rsi, &rsp+96, MB_YESNO|MB_ICONQUESTION
    cmp eax, IDYES
    jne .no
.write:
    invoke rich_get_text, [rbx+MG_HM], &rsp+96
    test rax, rax
    jz .no
    mov r13, rax
    mov r14, [rsp+96]
    cmp dword [rbx+MG_EOL], 0
    je .nofinal
    mov word [r13+r14*2], 13
    inc r14
    mov word [r13+r14*2], 0
.nofinal:
    mov r9d, [rbx+MG_CRLF]
    shl r9d, 8
    or r9d, [rbx+MG_ENC]
    invoke text_save, &r12+SS_PATHO, r13, r14, r9
    mov r15d, eax
    invoke free, r13
    test r15d, r15d
    jnz .saved
    invoke save_error, rsi
    jmp .no
.saved:
    invoke SendMessageW, [rbx+MG_HM], EM_SETMODIFY, 0, 0
    invoke set_status, &s_saved
    mov eax, 1
    EPILOG 256
.noout:
    invoke msgbox, rsi, &s_noout, MB_OK|MB_ICONWARNING
.no:
    xor eax, eax
    EPILOG 256

; ---------------------------------------------------------------- layout
%macro PLACE 2
    mov ecx, %2
    call scl
    mov ebx, eax
    ; wrap to the next toolbar row when the control would leave the client area
    cmp r12d, r14d
    je %%fit
    lea eax, [r12+rbx]
    cmp eax, [rsp+104]
    jbe %%fit
    mov r12d, r14d
    lea r13d, [r13+r15]
    add r13d, r14d
%%fit:
    MVID rsi, %1, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
%endmacro

; mg_layout(hwnd, mg)
mg_layout:
    PROLOG 224
    mov rsi, rcx
    mov rdi, rdx
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, 4
    call scl
    mov r14d, eax                    ; gap
    mov ecx, 26
    call scl
    mov r15d, eax                    ; row height
    mov edx, [rsp+104]
    invoke pb_layout, rsi, rdx, r14
    mov r13d, eax                    ; y of row 2
    ; --- row 2: base and output paths
    mov eax, r14d
    lea eax, [rax*2+r14]
    mov edx, [rsp+104]
    sub edx, eax
    jns .h
    xor edx, edx
.h:
    shr edx, 1
    mov [rsp+112], edx               ; half
    mov ecx, 30
    call scl
    mov [rsp+116], eax               ; browse width
    mov ecx, 96
    call scl
    mov [rsp+120], eax               ; label width
    mov r12d, r14d
    mov ebx, [rsp+120]
    MVID rsi, ID_LBL_BASE, r12, r13, rbx, r15
    add r12d, [rsp+120]
    mov ebx, [rsp+112]
    sub ebx, [rsp+120]
    sub ebx, [rsp+116]
    MVID rsi, ID_EDIT_C, r12, r13, rbx, r15
    add r12d, ebx
    mov ebx, [rsp+116]
    MVID rsi, ID_BROWSE_C, r12, r13, rbx, r15
    mov r12d, r14d
    add r12d, [rsp+112]
    add r12d, r14d                   ; x of the right cell
    mov ebx, [rsp+120]
    MVID rsi, ID_LBL_OUT, r12, r13, rbx, r15
    add r12d, [rsp+120]
    mov ebx, [rsp+112]
    sub ebx, [rsp+120]
    sub ebx, [rsp+116]
    MVID rsi, ID_EDIT_O, r12, r13, rbx, r15
    add r12d, ebx
    mov ebx, [rsp+116]
    MVID rsi, ID_BROWSE_O, r12, r13, rbx, r15
    ; --- row 3: buttons
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_COMPARE, 96
    PLACE ID_BTN_PREV, 40
    PLACE ID_BTN_NEXT, 40
    PLACE ID_BTN_USEL, 56
    PLACE ID_BTN_USER, 56
    PLACE ID_BTN_BOTH, 40
    PLACE ID_BTN_ALN, 80
    PLACE ID_BTN_ALC, 80
    PLACE ID_BTN_ALLL, 110
    PLACE ID_BTN_ALLR, 120
    PLACE ID_BTN_SAVE, 40
    PLACE ID_BTN_DOC, 90
    PLACE ID_LBL_INFO, 300
    ; --- panes
    lea r13d, [r13+r15]
    add r13d, r14d                   ; y4
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .a
    xor ebp, ebp
.a:
    mov eax, ebp
    shl eax, 1
    xor edx, edx
    mov ecx, 5
    div ecx
    mov [rsp+124], eax               ; top panes height (40 %)
    mov ebx, [rsp+104]
    ; widths
    mov eax, ebx
    xor edx, edx
    mov ecx, 3
    div ecx
    sub eax, 2
    mov [rsp+128], eax               ; 1/3 width
    mov eax, ebx
    shr eax, 1
    sub eax, 2
    mov [rsp+132], eax               ; 1/2 width
    cmp qword [rdi+MG_THREE], 0
    je .two
    mov ebp, [rsp+124]
    mov ebx, [rsp+128]
    MVID rsi, ID_RL, 0, r13, rbx, rbp
    mov r12d, [rsp+128]
    add r12d, 2
    MVID rsi, ID_RO, r12, r13, rbx, rbp
    mov r12d, [rsp+128]
    lea r12d, [r12*2+4]
    MVID rsi, ID_RR, r12, r13, rbx, rbp
    jmp .out
.two:
    mov ebp, [rsp+124]
    mov ebx, [rsp+132]
    MVID rsi, ID_RL, 0, r13, rbx, rbp
    mov r12d, [rsp+104]
    shr r12d, 1
    MVID rsi, ID_RO, r12, r13, 0, rbp
    mov r12d, [rsp+104]
    shr r12d, 1
    add r12d, 2
    MVID rsi, ID_RR, r12, r13, rbx, rbp
.out:
    mov eax, [rsp+124]
    add eax, r13d
    add eax, r14d
    mov r13d, eax                    ; y of the output pane
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .o
    xor ebp, ebp
.o:
    mov ebx, [rsp+104]
    MVID rsi, ID_RM, 0, r13, rbx, rbp
    EPILOG 224

mg_theme:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke rich_theme, [rbx+MG_HL]
    invoke rich_theme, [rbx+MG_HO]
    invoke rich_theme, [rbx+MG_HR]
    invoke rich_theme, [rbx+MG_HM]
    invoke theme_checks, rsi
    EPILOG 64

; ---------------------------------------------------------------- window procedure
mg_proc:
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
    cmp edi, BC_RUN
    je .run
    cmp edi, BC_COLLECT
    je .collect
    cmp edi, BC_ISDIRTY
    je .isdirty
    cmp edi, BC_SAVE
    je .save
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
    invoke calloc, 1, MG_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    mov dword [r15+MG_ENC], 1
    mov dword [r15+MG_CRLF], 1
    mov dword [r15+MG_EOL], 1
    invoke calloc, 1, MG_ANCSZ
    mov [r15+MG_ANCB], rax
    invoke pb_create, rsi
    invoke GetDlgItem, rsi, ID_COMPARE
    invoke SetWindowTextW, rax, &s_merge
    invoke mk_lbl, rsi, &s_l_base, ID_LBL_BASE
    invoke mk_edit, rsi, ID_EDIT_C
    invoke mk_btn, rsi, &s_dots, ID_BROWSE_C
    invoke mk_lbl, rsi, &s_l_out, ID_LBL_OUT
    invoke mk_edit, rsi, ID_EDIT_O
    invoke mk_btn, rsi, &s_dots, ID_BROWSE_O
    invoke mk_btn, rsi, &s_b_prev, ID_BTN_PREV
    invoke mk_btn, rsi, &s_b_next, ID_BTN_NEXT
    invoke mk_btn, rsi, &s_b_usel, ID_BTN_USEL
    invoke mk_btn, rsi, &s_b_user, ID_BTN_USER
    invoke mk_btn, rsi, &s_b_both, ID_BTN_BOTH
    invoke mk_btn, rsi, &s_b_aln, ID_BTN_ALN
    invoke mk_btn, rsi, &s_b_alc, ID_BTN_ALC
    invoke mk_btn, rsi, &s_b_alll, ID_BTN_ALLL
    invoke mk_btn, rsi, &s_b_allr, ID_BTN_ALLR
    invoke mk_btn, rsi, &s_b_save, ID_BTN_SAVE
    invoke mk_btn, rsi, &s_b_doc, ID_BTN_DOC
    invoke icon_set, rsi, ID_BTN_PREV, &g_prev, &s_b_prev
    invoke icon_set, rsi, ID_BTN_NEXT, &g_next, &s_b_next
    invoke icon_set, rsi, ID_BTN_USEL, &g_usel, &s_b_usel
    invoke icon_set, rsi, ID_BTN_USER, &g_user, &s_b_user
    invoke icon_set, rsi, ID_BTN_BOTH, &g_both, &s_b_both
    invoke icon_set, rsi, ID_BTN_SAVE, &g_save, &s_b_save
    invoke mk_lbl, rsi, &s_empty, ID_LBL_INFO
    invoke rich_create, rsi, ID_RL
    mov [r15+MG_HL], rax
    invoke SendMessageW, rax, EM_SETREADONLY, 1, 0
    invoke rich_create, rsi, ID_RO
    mov [r15+MG_HO], rax
    invoke SendMessageW, rax, EM_SETREADONLY, 1, 0
    invoke rich_create, rsi, ID_RR
    mov [r15+MG_HR], rax
    invoke SendMessageW, rax, EM_SETREADONLY, 1, 0
    invoke rich_create, rsi, ID_RM
    mov [r15+MG_HM], rax
    invoke pb_load, rsi, r14
    invoke GetDlgItem, rsi, ID_EDIT_C
    invoke SetWindowTextW, rax, &r14+SS_PATHC
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke SetWindowTextW, rax, &r14+SS_PATHO
    invoke mg_theme, rsi, r15
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320

.size:
    invoke mg_layout, rsi, r15
    jmp .zero
.run:
    mov rax, [r15+MG_ANCB]
    mov dword [rax+2880], 0           ; TV_NANC
    invoke mg_merge, rsi, r15, r14
    invoke mg_layout, rsi, r15
    test qword [g_batch], 4           ; unattended: the first merge ends the run
    jz .zero
    cmp qword [g_exitcode], 2         ; an input could not be read: no report of an empty merge
    je .bdx_end
    test qword [g_batch], 0x20        ; /docx file: export it first
    jz .bdx_ok
    and qword [g_batch], ~0x20
    invoke mg_docx_path, rsi, r15, r14, &g_dx_path
    test eax, eax
    jnz .bdx_ok
    mov qword [g_exitcode], 2
    jmp .bdx_end
.bdx_ok:
    invoke SendMessageW, [r15+MG_HM], EM_SETMODIFY, 0, 0   ; do not ask to save the merge output
    invoke mg_scan, r15
    invoke batch_done, [r15+MG_CONF+8]                     ; exit code: 0 merged cleanly, 1 conflicts left, 2 error
    jmp .zero
.bdx_end:
    invoke SendMessageW, [r15+MG_HM], EM_SETMODIFY, 0, 0
    invoke batch_done, 0
    jmp .zero
.collect:
    invoke pb_collect, rsi, r14
    invoke GetDlgItem, rsi, ID_EDIT_C
    invoke GetWindowTextW, rax, &r14+SS_PATHC, 519
    invoke GetDlgItem, rsi, ID_EDIT_O
    invoke GetWindowTextW, rax, &r14+SS_PATHO, 519
    jmp .zero
.save:
    invoke mg_save, rsi, r15, r14
    jmp .zero
.doc:
    invoke mg_export, rsi, r15, r14
    jmp .zero
.theme:
    invoke mg_theme, rsi, r15
    cmp qword [r15+MG_SCTEXT], 0
    je .zero
    invoke mg_colorout, r15
    jmp .zero

.command:
    movzx ebx, r12w
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_BROWSE_B
    je .browse
    cmp ebx, ID_SWAP
    je .browse
    cmp ebx, ID_BROWSE_C
    je .bc
    cmp ebx, ID_BROWSE_O
    je .bo
    cmp ebx, ID_BTN_PREV
    je .b_prev
    cmp ebx, ID_BTN_NEXT
    je .b_next
    cmp ebx, ID_BTN_USEL
    je .b_l
    cmp ebx, ID_BTN_USER
    je .b_r
    cmp ebx, ID_BTN_BOTH
    je .b_b
    cmp ebx, ID_BTN_ALLL
    je .b_al
    cmp ebx, ID_BTN_ALLR
    je .b_ar
    cmp ebx, ID_BTN_SAVE
    je .save
    cmp ebx, ID_BTN_DOC
    je .doc
    cmp ebx, ID_BTN_ALN
    je .b_aln
    cmp ebx, ID_BTN_ALC
    je .b_alc
    jmp .def
.browse:
    invoke pb_command, rsi, rbx, 0
    jmp .zero
.bc:
    invoke GetDlgItem, rsi, ID_EDIT_C
    mov rbx, rax
    invoke GetWindowTextW, rbx, &rsp+96, 519
    invoke browse_open, rsi, &rsp+96
    test eax, eax
    jz .zero
    invoke SetWindowTextW, rbx, &rsp+96
    jmp .zero
.bo:
    invoke GetDlgItem, rsi, ID_EDIT_O
    mov rbx, rax
    invoke GetWindowTextW, rbx, &rsp+96, 519
    invoke browse_save, rsi, &rsp+96
    test eax, eax
    jz .zero
    invoke SetWindowTextW, rbx, &rsp+96
    jmp .zero
.b_prev:
    invoke mg_nav, r15, 0
    jmp .zero
.b_next:
    invoke mg_nav, r15, 1
    jmp .zero
.b_l:
    invoke mg_resolve, rsi, r15, 0
    jmp .zero
.b_r:
    invoke mg_resolve, rsi, r15, 1
    jmp .zero
.b_b:
    invoke mg_resolve, rsi, r15, 2
    jmp .zero
.b_aln:
    invoke mg_align_set, rsi, r15, r14
    jmp .zero
.b_alc:
    invoke mg_align_clear, rsi, r15, r14
    jmp .zero
.b_al:
    invoke mg_resolve_all, rsi, r15, 0
    jmp .zero
.b_ar:
    invoke mg_resolve_all, rsi, r15, 1
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+MG_HM]
    jmp .zero
.isdirty:
    invoke SendMessageW, [r15+MG_HM], EM_GETMODIFY, 0, 0
    EPILOG 320
.destroy:
    invoke mg_unscan, r15
    invoke mg_free_texts, r15
    invoke free, [r15+MG_ANCB]
    invoke free, [r15+MG_PRIS]
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320
