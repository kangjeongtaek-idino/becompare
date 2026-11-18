; folder_pv.inc - the preview under the list of the folder compare (included by folder.asm)
;   A text compare view (the one of the Text Compare session, without its own toolbar) shows the selected pair
;   of files below the list. It edits, merges blocks and saves like the Text Compare session does; the toolbar
;   above it only sends it the same commands.

%define TVID_CASE  211
%define TVID_SPC   212
%define TVID_TRIM  213
%define TVID_SYN   215
%define TVID_FOLD  217
%define TVID_UNI   218
%define TVID_WORD  219
%define TVID_SCRL  220
%define TVID_WS    221
%define TVID_CLR   243
%define TVID_CRL   244
%define PV_NCTL    13

section .rdata
WSTR s_pv_msg, "미리 볼 수 있는 항목이 없습니다. 양쪽에 있는 텍스트 파일을 고르면 여기에 비교가 나옵니다."
WSTR s_pv_side, "나란히"
WSTR s_pv_uni, "합쳐서"
WSTR f_pv_count, "차이점 %u개"
WSTR s_pv_unsaved, "미리보기에서 바꾼 내용이 저장되지 않았습니다. 저장할까요?"
WSTR s_pv_saved, "저장하고 폴더 비교 목록에 반영했습니다."
WSTR s_pv_nofile, "더 비교할 다른 파일이 없습니다."
g_pvup:   dw 0xE74A, 0
g_pvdn:   dw 0xE74B, 0
g_pvsrc:  dw 0xE70F, 0
g_pvback: dw 0xE72B, 0
g_pvfwd:  dw 0xE72A, 0
g_pvfold: dw 0xE73F, 0
g_pvsave: dw 0xE74E, 0
g_pvcpr:  dw 0xE8C8, 0xE72A, 0
g_pvcpl:  dw 0xE72B, 0xE8C8, 0
g_pvtog:  dw 0xE8A1, 0
WSTR tt_pv_0, "이전 차이로 이동 (Shift+F7)"
WSTR tt_pv_1, "다음 차이로 이동 (F7)"
WSTR tt_pv_2, "이 파일을 텍스트 편집기로 엽니다 (캐럿이 있던 쪽 파일)"
WSTR tt_pv_3, "이전 파일 비교: 위쪽의 다른 파일로 이동합니다"
WSTR tt_pv_4, "다음 파일 비교: 아래쪽의 다른 파일로 이동합니다"
WSTR tt_pv_5, "현재 차이를 왼쪽에서 오른쪽으로 가져옵니다"
WSTR tt_pv_6, "현재 차이를 오른쪽에서 왼쪽으로 가져옵니다"
WSTR tt_pv_7, "바꾼 파일을 저장합니다"
WSTR tt_pv_8, "변경되지 않은 조각을 접어 차이만 모아 봅니다"
WSTR tt_pv_9, "나란히 보기: 왼쪽과 오른쪽을 두 창에 보여 줍니다"
WSTR tt_pv_10, "합쳐서 보기: 삭제·추가 줄을 한 창에 이어서 보여 줍니다"
WSTR tt_pv_11, "미리보기 설정"
WSTR tt_pv_12, "아래쪽 비교 미리보기를 보이거나 숨깁니다"
align 8
tt_pv:
    dq ID_PV_PREV, tt_pv_0
    dq ID_PV_NEXT, tt_pv_1
    dq ID_PV_SRC, tt_pv_2
    dq ID_PV_PF, tt_pv_3
    dq ID_PV_NF, tt_pv_4
    dq ID_PV_CLR, tt_pv_5
    dq ID_PV_CRL, tt_pv_6
    dq ID_PV_SAVE, tt_pv_7
    dq ID_PV_FOLD, tt_pv_8
    dq ID_PV_SIDE, tt_pv_9
    dq ID_PV_UNI, tt_pv_10
    dq ID_PV_SET, tt_pv_11
    dq ID_BTN_PVTOG, tt_pv_12
    dq 0, 0
pv_ids: dd ID_PV_PREV, ID_PV_NEXT, ID_PV_SRC, ID_PV_PF, ID_PV_NF, ID_PV_CLR, ID_PV_CRL, ID_PV_SAVE, ID_PV_FOLD, ID_PV_LBL, ID_PV_SIDE, ID_PV_UNI, ID_PV_SET
section .text

; fold_pv_create(hwnd, fold): the controls of the preview and the view itself (hidden until a pair is chosen)
fold_pv_create:
    PROLOG 96
    mov rsi, rcx
    mov rbx, rdx
%macro PVBTN 2
    invoke mk_btn, rsi, &s_empty, %1
    invoke glyph_only, rsi, %1, &%2
%endmacro
%macro PVCHK 3                       ; id, checked, text or glyph: a button that stays pressed
    invoke mk_chk, rsi, &s_empty, %1, %2
    invoke GetDlgItem, rsi, %1
    mov r13, rax
    invoke GetWindowLongPtrW, r13, -16
    or eax, 0x1000                   ; BS_PUSHLIKE
    invoke SetWindowLongPtrW, r13, -16, rax
    invoke glyph_only, rsi, %1, &%3
%endmacro
    PVBTN ID_PV_PREV, g_pvup
    PVBTN ID_PV_NEXT, g_pvdn
    PVBTN ID_PV_SRC, g_pvsrc
    PVBTN ID_PV_PF, g_pvback
    PVBTN ID_PV_NF, g_pvfwd
    PVBTN ID_PV_CLR, g_pvcpr
    PVBTN ID_PV_CRL, g_pvcpl
    PVBTN ID_PV_SAVE, g_pvsave
    PVCHK ID_PV_FOLD, 0, g_pvfold
    PVBTN ID_PV_SET, g_opts
    PVCHK ID_BTN_PVTOG, 1, g_pvtog
    invoke mk_chk, rsi, &s_pv_side, ID_PV_SIDE, 1       ; side by side / unified: two buttons, one of them pressed
    invoke GetDlgItem, rsi, ID_PV_SIDE
    mov r13, rax
    invoke GetWindowLongPtrW, r13, -16
    or eax, 0x1000
    invoke SetWindowLongPtrW, r13, -16, rax
    invoke mk_chk, rsi, &s_pv_uni, ID_PV_UNI, 0
    invoke GetDlgItem, rsi, ID_PV_UNI
    mov r13, rax
    invoke GetWindowLongPtrW, r13, -16
    or eax, 0x1000
    invoke SetWindowLongPtrW, r13, -16, rax
    invoke mk_lbl, rsi, &s_empty, ID_PV_LBL
    invoke mk_lbl, rsi, &s_pv_msg, ID_LBL_PVMSG
    invoke tips_apply, rsi, &tt_pv
    ; the view: a Text Compare view of its own, not one of the tabs
    invoke calloc, 1, SS_SIZE
    test rax, rax
    jz .d
    mov r12, rax
    mov qword [r12+SS_TYPE], T_TEXT
    mov qword [r12+SS_EMBED], 1
    invoke text_create, rsi, r12
    mov [r12+SS_HVIEW], rax
    mov [rbx+F_PVVIEW], rax
    mov [rbx+F_PVSESS], r12
.d:
    mov qword [rbx+F_PVENT], -1
    mov qword [rbx+F_PVROW], -1
    mov qword [rbx+F_SPLIT], 55
    EPILOG 96

; fold_pv_vis(hwnd, fold): what of the preview is on screen - the toolbar and the view while a pair is shown,
;   a message while there is none, nothing when the preview is switched off
fold_pv_vis:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke get_chk, rsi, ID_BTN_PVTOG
    mov r12d, eax
    xor edi, edi
.l:
    cmp edi, PV_NCTL
    jae .ld
    lea rax, [pv_ids]
    mov edx, [rax+rdi*4]
    invoke GetDlgItem, rsi, rdx
    xor r10d, r10d
    test r12d, r12d
    jz .sh
    mov r10d, 5
.sh:
    invoke ShowWindow, rax, r10
    inc edi
    jmp .l
.ld:
    xor r13d, r13d                   ; the view
    xor r14d, r14d                   ; the message
    test r12d, r12d
    jz .apply
    cmp qword [rbx+F_PVENT], 0
    jl .msg
    mov r13d, 5
    jmp .apply
.msg:
    mov r14d, 5
.apply:
    mov rcx, [rbx+F_PVVIEW]
    test rcx, rcx
    jz .m
    invoke ShowWindow, rcx, r13
.m:
    invoke GetDlgItem, rsi, ID_LBL_PVMSG
    invoke ShowWindow, rax, r14
    EPILOG 64

; fold_pv_label(hwnd, fold): "differences N" over the view
fold_pv_label:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    mov word [rsp+96], 0
    cmp qword [rbx+F_PVENT], 0
    jl .set
    invoke SendMessageW, [rbx+F_PVVIEW], BC_TVQ, 0, 0
    mov r10d, eax
    invoke _snwprintf, &rsp+96, 32, &f_pv_count, r10
.set:
    invoke GetDlgItem, rsi, ID_PV_LBL
    invoke SetWindowTextW, rax, &rsp+96
    EPILOG 160

; fold_pv_row(fold) -> rax: the row of the list that shows the entry of the preview, -1 when it is not listed
fold_pv_row:
    mov rax, [rcx+F_PVENT]
    test rax, rax
    js .no
    xor edx, edx
.l:
    cmp rdx, [rcx+F_NVIS]
    jae .no
    mov r8, [rcx+F_VIS]
    mov r8d, [r8+rdx*4]
    cmp r8, rax
    je .hit
    inc rdx
    jmp .l
.hit:
    mov rax, rdx
    ret
.no:
    mov rax, -1
    ret

; fold_pv_dirty(fold) -> eax: the view holds changes that are not saved
fold_pv_dirty:
    PROLOG 64
    mov rbx, rcx
    xor eax, eax
    cmp qword [rbx+F_PVVIEW], 0
    je .d
    invoke SendMessageW, [rbx+F_PVVIEW], BC_ISDIRTY, 0, 0
.d:
    EPILOG 64

; fold_pv_confirm(hwnd, fold) -> eax 1 when it is fine to replace what the view shows, 0 to stay
;   with unsaved changes: save, discard or cancel
fold_pv_confirm:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke fold_pv_dirty, rbx
    test eax, eax
    jz .yes
    invoke msgbox, rsi, &s_pv_unsaved, 3|MB_ICONQUESTION       ; yes / no / cancel
    cmp eax, 7                       ; IDNO: discard
    je .yes
    cmp eax, IDYES
    jne .no
    invoke SendMessageW, [rbx+F_PVVIEW], BC_SAVE, 0, 0
    invoke fold_pv_dirty, rbx
    test eax, eax
    jnz .no
    invoke fold_pv_sync_ent, rsi, rbx, 0
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; fold_pv_sync_ent(hwnd, fold, keep_sel): after saving in the preview, refresh the entry, parent folder statuses and counts
fold_pv_sync_ent:
    PROLOG 704
    mov rsi, rcx
    mov rbx, rdx
    mov r14d, r8d
    invoke fold_pv_dirty, rbx
    test eax, eax
    jnz .d
    mov r12, [rbx+F_PVENT]
    test r12, r12
    js .d
    cmp r12, [rbx+F_NENT]
    jae .d
    cmp qword [rbx+F_RUNNING], 0
    jne .d
    cmp qword [rbx+F_SYNCRUN], 0
    jne .d
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .d
    mov rdi, r12
    shl rdi, 6
    add rdi, [rbx+F_ENTS]
    test dword [rdi+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .d
    mov eax, [rdi+E_FLAGS]
    and eax, EF_EXA|EF_EXB
    cmp eax, EF_EXA|EF_EXB
    jne .d
    invoke GetFileAttributesExW, &rbx+F_PVPA, 0, &rsp+96
    test eax, eax
    jz .d
    mov eax, [rsp+96+20]
    mov edx, [rsp+96+24]
    shl rdx, 32
    or rax, rdx
    mov [rdi+E_MTA], rax
    mov eax, [rsp+96+32]
    mov edx, [rsp+96+28]
    shl rdx, 32
    or rax, rdx
    mov [rdi+E_SIZEA], rax
    invoke GetFileAttributesExW, &rbx+F_PVPB, 0, &rsp+96
    test eax, eax
    jz .d
    mov eax, [rsp+96+20]
    mov edx, [rsp+96+24]
    shl rdx, 32
    or rax, rdx
    mov [rdi+E_MTB], rax
    mov eax, [rsp+96+32]
    mov edx, [rsp+96+28]
    shl rdx, 32
    or rax, rdx
    mov [rdi+E_SIZEB], rax
    mov r13d, ST_DIFF
    mov rax, [rdi+E_SIZEA]
    cmp rax, [rdi+E_SIZEB]
    jne .setst
    invoke files_equal, rbx, &rbx+F_PVPA, &rbx+F_PVPB
    test eax, eax
    jz .setst
    mov r13d, ST_SAME
    and dword [rdi+E_FLAGS], ~(7 << EF_ACTSH)
.setst:
    mov [rdi+E_STATUS], r13d
    ; recompute file status counts and folder masks from bottom to top
    xor eax, eax
    mov [rbx+F_CNT], rax
    mov [rbx+F_CNT+8], rax
    mov [rbx+F_CNT+16], rax
    mov [rbx+F_CNT+24], rax
    mov [rbx+F_CNT+32], rax
    mov rcx, [rbx+F_NENT]
.recalc:
    test rcx, rcx
    jz .recalcdone
    dec rcx
    mov rax, rcx
    shl rax, 6
    add rax, [rbx+F_ENTS]
    mov r15, rax
    test dword [r15+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .isdir
    mov eax, [r15+E_STATUS]
    inc qword [rbx+F_CNT+rax*8]
    mov edx, 1
    mov r8, rcx
    mov ecx, eax
    shl edx, cl
    mov rcx, r8
    mov [r15+E_MASK], edx
    jmp .recalc
.isdir:
    lea r8, [rcx+1]
    mov r9d, [r15+E_SUBEND]
    xor r10d, r10d
    mov r11, [rbx+F_ENTS]
.subm:
    cmp r8, r9
    jae .submd
    mov rax, r8
    shl rax, 6
    or r10d, [r11+rax+E_MASK]
    inc r8
    jmp .subm
.submd:
    mov eax, [r15+E_FLAGS]
    and eax, EF_EXA|EF_EXB
    cmp eax, EF_EXA|EF_EXB
    jne .dirown
    cmp dword [r15+E_STATUS], ST_TYPE
    je .dirown
    mov dword [r15+E_STATUS], ST_SAME
    test r10d, 0x1E
    jz .dirown
    mov dword [r15+E_STATUS], ST_DIFF
.dirown:
    mov eax, [r15+E_STATUS]
    mov edx, 1
    mov r8, rcx
    mov ecx, eax
    shl edx, cl
    mov rcx, r8
    or edx, r10d
    mov [r15+E_MASK], edx
    jmp .recalc
.recalcdone:
    invoke fold_refresh, rbx
    test r14d, r14d
    jz .stat
    mov rcx, rbx
    call fold_pv_row
    test rax, rax
    js .stat
    invoke fold_pv_select, rbx, rax
.stat:
    invoke _snwprintf, &rsp+160, 250, &s_scan_done, [rbx+F_CNT], [rbx+F_CNT+8], [rbx+F_CNT+16], [rbx+F_CNT+24], [rbx+F_CNT+32]
    invoke set_status_part, 1, &rsp+160
.d:
    EPILOG 704

; fold_pv_load(hwnd, fold, row): show the pair of the row (a file on both sides that is text), or nothing
%define PL_A 96                      ; wchar[520]
%define PL_B 1136
fold_pv_load:
    PROLOG 2240
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, r8
    mov qword [rbx+F_PVENT], -1
    test rdi, rdi
    js .done
    cmp rdi, [rbx+F_NVIS]
    jae .done
    invoke fold_ent, rbx, rdi
    mov r12, rax
    test dword [r12+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .done
    mov eax, [r12+E_FLAGS]
    and eax, EF_EXA|EF_EXB
    cmp eax, EF_EXA|EF_EXB
    jne .done
    mov rax, [r12+E_SIZEA]
    cmp rax, 0x1000000               ; 16MB: a text view is no place for more
    ja .done
    mov rax, [r12+E_SIZEB]
    cmp rax, 0x1000000
    ja .done
    invoke fold_side_path, rbx, 0, [r12+E_NAME], &rsp+PL_A
    test eax, eax
    jz .done
    invoke fold_side_path, rbx, 1, [r12+E_NAME], &rsp+PL_B
    test eax, eax
    jz .done
    invoke type_for_path, &rsp+PL_A
    cmp eax, T_TEXT
    jne .done
    invoke type_for_path, &rsp+PL_B
    cmp eax, T_TEXT
    jne .done
    invoke wcscpy, &rbx+F_PVPA, &rsp+PL_A
    invoke wcscpy, &rbx+F_PVPB, &rsp+PL_B
    invoke GetDlgItem, [rbx+F_PVVIEW], ID_EDIT_A
    invoke SetWindowTextW, rax, &rsp+PL_A
    invoke GetDlgItem, [rbx+F_PVVIEW], ID_EDIT_B
    invoke SetWindowTextW, rax, &rsp+PL_B
    mov rax, [rbx+F_VIS]
    mov eax, [rax+rdi*4]
    mov [rbx+F_PVENT], rax
    invoke SendMessageW, [rbx+F_PVVIEW], BC_RUN, 0, 0
.done:
    invoke fold_pv_vis, rsi, rbx
    invoke fold_pv_label, rsi, rbx
    EPILOG 2240

; fold_pv_select(fold, row): that row, and only that one, is selected (without the preview following by itself)
%define PS_IT 96
fold_pv_select:
    PROLOG 160
    mov rbx, rcx
    mov rsi, rdx
    mov qword [rbx+F_PVGUARD], 1
    xor eax, eax
    mov [rsp+PS_IT], rax
    mov [rsp+PS_IT+8], rax
    mov [rsp+PS_IT+16], rax
    mov [rsp+PS_IT+24], rax
    mov dword [rsp+PS_IT+16], 2      ; stateMask: selected
    invoke SendMessageW, [rbx+F_HLIST], LVM_SETITEMSTATE, -1, &rsp+PS_IT
    mov dword [rsp+PS_IT+12], 3      ; state: selected and focused
    mov dword [rsp+PS_IT+16], 3
    invoke SendMessageW, [rbx+F_HLIST], LVM_SETITEMSTATE, rsi, &rsp+PS_IT
    invoke SendMessageW, [rbx+F_HLIST], LVM_ENSUREVISIBLE, rsi, 0
    mov qword [rbx+F_PVGUARD], 0
    EPILOG 160

; fold_pv_switch(hwnd, fold, row): the row was selected: show its pair, after asking about unsaved changes
fold_pv_switch:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov rdi, r8
    invoke get_chk, rsi, ID_BTN_PVTOG
    test eax, eax
    jz .d
    test rdi, rdi
    js .d
    cmp rdi, [rbx+F_NVIS]
    jae .d
    mov rax, [rbx+F_VIS]
    mov eax, [rax+rdi*4]
    cmp rax, [rbx+F_PVENT]
    je .d                            ; already shown
    invoke fold_pv_confirm, rsi, rbx
    test eax, eax
    jz .revert
    invoke fold_pv_load, rsi, rbx, rdi
.d:
    EPILOG 64
.revert:                             ; cancelled: the selection goes back to what the view still shows
    mov rcx, rbx
    call fold_pv_row
    test rax, rax
    js .d
    invoke fold_pv_select, rbx, rax
    jmp .d

; fold_pv_step(hwnd, fold, dir): the next (dir 1) or previous (dir -1) file that exists on both sides and
;   differs; nothing happens when there is none
fold_pv_step:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    movsxd r12, r8d
    invoke SendMessageW, [rbx+F_HLIST], LVM_GETNEXTITEM, -1, LVNI_FOCUSED
    movsxd r13, eax
    test r13, r13
    jns .have
    mov r13, -1                      ; nothing focused: from the top, or from the bottom
    cmp r12, 0
    jg .have
    mov r13, [rbx+F_NVIS]
.have:
.l:
    add r13, r12
    test r13, r13
    js .none
    cmp r13, [rbx+F_NVIS]
    jae .none
    invoke fold_ent, rbx, r13
    mov rdi, rax
    test dword [rdi+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .l
    mov eax, [rdi+E_FLAGS]
    and eax, EF_EXA|EF_EXB
    cmp eax, EF_EXA|EF_EXB
    jne .l
    cmp dword [rdi+E_STATUS], ST_SAME
    je .l
    invoke fold_pv_select, rbx, r13
    invoke fold_pv_switch, rsi, rbx, r13
    EPILOG 64
.none:
    invoke set_status, &s_pv_nofile
    EPILOG 64

; fold_pv_source(hwnd, fold): the file of the pane that had the caret opens in the text editor
fold_pv_source:
    PROLOG 64
    mov rbx, rdx
    cmp qword [rbx+F_PVENT], 0
    jl .d
    invoke SendMessageW, [rbx+F_PVVIEW], BC_TVQ, 1, 0
    lea rdx, [rbx+F_PVPA]
    test eax, eax
    jz .go
    lea rdx, [rbx+F_PVPB]
.go:
    invoke add_session, T_EDIT, rdx, 0, 0
.d:
    EPILOG 64

; fold_pv_cmd(hwnd, fold, id): a button of the preview
fold_pv_cmd:
    PROLOG 96
    mov rsi, rcx
    mov rbx, rdx
    mov edi, r8d
    mov r12, [rbx+F_PVVIEW]
    cmp edi, ID_PV_PREV
    je .prev
    cmp edi, ID_PV_NEXT
    je .next
    cmp edi, ID_PV_SRC
    je .src
    cmp edi, ID_PV_PF
    je .pf
    cmp edi, ID_PV_NF
    je .nf
    cmp edi, ID_PV_CLR
    je .clr
    cmp edi, ID_PV_CRL
    je .crl
    cmp edi, ID_PV_SAVE
    je .save
    cmp edi, ID_PV_FOLD
    je .fold
    cmp edi, ID_PV_SIDE
    je .side
    cmp edi, ID_PV_UNI
    je .uni
    cmp edi, ID_PV_SET
    je .set
    cmp edi, ID_BTN_PVTOG
    je .tog
    jmp .d
.prev:
    invoke SendMessageW, r12, BC_PREVDIFF, 0, 0
    jmp .lbl
.next:
    invoke SendMessageW, r12, BC_NEXTDIFF, 0, 0
    jmp .lbl
.src:
    invoke fold_pv_source, rsi, rbx
    jmp .d
.pf:
    invoke fold_pv_step, rsi, rbx, -1
    jmp .d
.nf:
    invoke fold_pv_step, rsi, rbx, 1
    jmp .d
.clr:
    invoke SendMessageW, r12, WM_COMMAND, TVID_CLR, 0
    jmp .lbl
.crl:
    invoke SendMessageW, r12, WM_COMMAND, TVID_CRL, 0
    jmp .lbl
.save:
    invoke SendMessageW, r12, BC_SAVE, 0, 0
    invoke SendMessageW, r12, BC_RUN, 0, 0
    invoke fold_pv_sync_ent, rsi, rbx, 1
    invoke set_status, &s_pv_saved
    jmp .lbl
.fold:
    invoke get_chk, rsi, ID_PV_FOLD
    mov ebp, eax
    invoke set_chk_id, r12, TVID_FOLD, rbp
    invoke SendMessageW, r12, WM_COMMAND, TVID_FOLD, 0
    jmp .lbl
.side:
    xor ebp, ebp
    jmp .mode
.uni:
    mov ebp, 1
.mode:
    mov r8d, 1
    sub r8d, ebp
    invoke set_chk_id, rsi, ID_PV_SIDE, r8       ; the two buttons: exactly one is pressed
    invoke set_chk_id, rsi, ID_PV_UNI, rbp
    invoke set_chk_id, r12, TVID_UNI, rbp
    invoke SendMessageW, r12, WM_COMMAND, TVID_UNI, 0
    jmp .lbl
.set:
    invoke fold_pv_menu, rsi, rbx
    jmp .d
.tog:
    invoke fold_layout, rsi
    invoke get_chk, rsi, ID_BTN_PVTOG
    test eax, eax
    jz .off
    invoke SendMessageW, [rbx+F_HLIST], LVM_GETNEXTITEM, -1, LVNI_SELECTED
    movsxd r8, eax
    test r8, r8
    js .off
    invoke fold_pv_switch, rsi, rbx, r8
.off:
    invoke fold_pv_vis, rsi, rbx
.lbl:
    invoke fold_pv_label, rsi, rbx
.d:
    EPILOG 96

; fold_pv_menu(hwnd, fold): the settings of the preview, as a popup under the gear button
;   ids: 1 scroll together, 2 own window, 10-12 ignore whitespace (none / trim / all), 13 ignore case,
;   20-21 highlight (words / lines), 30 syntax colours, 31 show blanks
%macro PVMI 3                        ; menu item with a check mark: id, text, hidden option of the view (0: none)
%if %3 != 0
    invoke get_chk, r12, %3
    shl eax, 3                       ; MF_CHECKED
%else
    xor eax, eax
%endif
    mov r10d, eax
    invoke AppendMenuW, rdi, r10, %1, &%2
%endmacro
%macro PVMR 4                        ; radio-like item: id, text, value of the current mode, mode register r13d
    xor r10d, r10d
    cmp r13d, %3
    jne %%n
    mov r10d, MF_CHECKED
%%n:
    invoke AppendMenuW, rdi, r10, %1, &%2
%endmacro
fold_pv_menu:
    PROLOG 96
    mov rsi, rcx
    mov rbx, rdx
    mov r12, [rbx+F_PVVIEW]
    call CreatePopupMenu
    mov rdi, rax
    PVMI 1, s_pm_scroll, TVID_SCRL
    PVMI 2, s_pm_window, 0
    invoke AppendMenuW, rdi, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, rdi, MF_GRAYED, 0, &s_pm_ign
    invoke get_chk, r12, TVID_SPC
    mov r13d, 2                      ; mode: 2 all whitespace, 1 trim, 0 none
    test eax, eax
    jnz .gm
    invoke get_chk, r12, TVID_TRIM
    mov r13d, eax
.gm:
    PVMR 10, s_pm_ign0, 0, r13
    PVMR 11, s_pm_ign1, 1, r13
    PVMR 12, s_pm_ign2, 2, r13
    PVMI 13, s_pm_case, TVID_CASE
    invoke AppendMenuW, rdi, MF_SEPARATOR, 0, 0
    invoke AppendMenuW, rdi, MF_GRAYED, 0, &s_pm_hl
    invoke get_chk, r12, TVID_WORD
    mov r13d, eax
    PVMR 20, s_pm_hl1, 1, r13
    PVMR 21, s_pm_hl0, 0, r13
    invoke AppendMenuW, rdi, MF_SEPARATOR, 0, 0
    call CreatePopupMenu
    mov r14, rax
    xchg rdi, r14                    ; rdi: the submenu while it is filled
    PVMI 30, s_pm_syn, TVID_SYN
    PVMI 31, s_pm_ws, TVID_WS
    xchg rdi, r14
    invoke AppendMenuW, rdi, MF_POPUP, r14, &s_pm_look
    invoke popup_at, rsi, ID_PV_SET, rdi
    test eax, eax
    jz .d
    mov r14d, eax
    cmp eax, 2
    je .win
    cmp eax, 10
    jb .flip                         ; 1: scroll together
    cmp eax, 12
    ja .more
    xor r15d, r15d                   ; 10..12: the two whitespace options
    xor r13d, r13d
    cmp eax, 11
    jne .w2
    mov r13d, 1                      ; trim
.w2:
    cmp eax, 12
    jne .w3
    mov r15d, 1                      ; all
.w3:
    invoke set_chk_id, r12, TVID_SPC, r15
    invoke set_chk_id, r12, TVID_TRIM, r13
    mov r14d, TVID_SPC
    jmp .send
.more:
    cmp eax, 13
    je .flipcase
    cmp eax, 20
    je .words
    cmp eax, 21
    je .lines
    cmp eax, 30
    je .flipsyn
    mov r14d, TVID_WS                ; 31
    jmp .flip2
.flipcase:
    mov r14d, TVID_CASE
    jmp .flip2
.flipsyn:
    mov r14d, TVID_SYN
    jmp .flip2
.words:
    mov r15d, 1
    jmp .setw
.lines:
    xor r15d, r15d
.setw:
    invoke set_chk_id, r12, TVID_WORD, r15
    mov r14d, TVID_WORD
    jmp .send
.flip:
    mov r14d, TVID_SCRL
.flip2:
    invoke get_chk, r12, r14
    xor eax, 1
    invoke set_chk_id, r12, r14, rax
.send:
    invoke SendMessageW, r12, WM_COMMAND, r14, 0
    jmp .d
.win:
    mov rcx, rbx
    call fold_pv_row
    test rax, rax
    js .d
    invoke fold_open_pair, rbx, rax
.d:
    EPILOG 96

section .rdata
WSTR s_pm_scroll, "스크롤 동기화"
WSTR s_pm_window, "별도 창에 Diff 표시"
WSTR s_pm_ign, "차이점 무시"
WSTR s_pm_ign0, "  사용 안 함"
WSTR s_pm_ign1, "  공백 자르기"
WSTR s_pm_ign2, "  공백 무시"
WSTR s_pm_case, "  대소문자 무시"
WSTR s_pm_hl, "차이점을 강조 표시하는 중"
WSTR s_pm_hl1, "  단어"
WSTR s_pm_hl0, "  행"
WSTR s_pm_look, "모양"
WSTR s_pm_syn, "구문 강조"
WSTR s_pm_ws, "공백 표시"
section .text
