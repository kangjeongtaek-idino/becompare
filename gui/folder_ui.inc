; folder_ui.inc - Folder Compare view, actions, sync, report (included by folder.asm)

folder_register:
    PROLOG 64
    invoke reg_class, &cn_folder, &fold_proc, 0
    EPILOG 64

; folder_create(parent, sess) -> hwnd
folder_create:
    PROLOG 128
    mov rbx, rcx
    mov rsi, rdx
    invoke CreateWindowExW, 0, &cn_folder, 0, WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS, 0, 0, 100, 100, rbx, 0, [g_hinst], rsi
    EPILOG 128

; ---------------------------------------------------------------- small helpers

; set_chk_id(parent, id, value)
set_chk_id:
    PROLOG 64
    mov rbx, r8
    invoke GetDlgItem, rcx, rdx
    invoke SendMessageW, rax, BM_SETCHECK, rbx, 0
    EPILOG 64

; trim_slash(rcx=path): drop trailing backslashes (keeps "C:\")
trim_slash:
    PROLOG 64
    mov rsi, rcx
    invoke wcslen, rsi
.l:
    cmp rax, 3
    jbe .d
    cmp word [rsi+rax*2-2], 0x5C
    jne .d
    dec rax
    mov word [rsi+rax*2], 0
    jmp .l
.d:
    EPILOG 64

; glyph_only(parent, id, glyph): the button shows an icon of the Segoe MDL2 font (its tooltip is set elsewhere)
glyph_only:
    PROLOG 64
    mov rbx, r8
    invoke GetDlgItem, rcx, rdx
    mov rsi, rax
    test rax, rax
    jz .d
    invoke SetWindowTextW, rsi, rbx
    invoke SendMessageW, rsi, WM_SETFONT, [g_iconfont], 1
.d:
    EPILOG 64

; fold_crit_sync(hwnd): the compare-by box shows the preset that the check boxes add up to, or "custom"
fold_crit_sync:
    PROLOG 64
    mov rsi, rcx
    xor edi, edi
    invoke get_chk, rsi, ID_CHK_SIZE
    or edi, eax
    invoke get_chk, rsi, ID_CHK_MTIME
    shl eax, 1
    or edi, eax
    invoke get_chk, rsi, ID_CHK_CONT
    shl eax, 2
    or edi, eax
    invoke get_chk, rsi, ID_CHK_CRC
    shl eax, 3
    or edi, eax
    invoke get_chk, rsi, ID_CHK_ATTR
    shl eax, 4
    or edi, eax
    xor ebx, ebx
.l:
    cmp ebx, CR_N
    jae .set
    lea rax, [cr_masks]
    cmp [rax+rbx*4], edi
    je .set
    inc ebx
    jmp .l
.set:
    invoke GetDlgItem, rsi, ID_CRIT
    invoke SendMessageW, rax, CB_SETCURSEL, rbx, 0
    EPILOG 64

; fold_crit_apply(hwnd, preset): the check boxes of a preset of the compare-by box
fold_crit_apply:
    PROLOG 64
    mov rsi, rcx
    cmp edx, CR_N
    jae .d
    lea rax, [cr_masks]
    mov edi, [rax+rdx*4]
    mov r8d, edi
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_SIZE, r8
    mov r8d, edi
    shr r8d, 1
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_MTIME, r8
    mov r8d, edi
    shr r8d, 2
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_CONT, r8
    mov r8d, edi
    shr r8d, 3
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_CRC, r8
    mov r8d, edi
    shr r8d, 4
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_ATTR, r8
.d:
    EPILOG 64

; fold_glyph(entry) -> eax what the middle column shows: 0 nothing, 1 -> (copy to the right), 2 <- (copy to the
;   left), 3 not equal and no action, 4 equal, 5 skip. An action chosen by the user (bits 26..28 of the flags:
;   1 right, 2 left, 3 skip) wins; otherwise a new item goes over to the other side and of two different files
;   the newer one is copied.
fold_glyph:
    mov eax, [rcx+E_STATUS]
    test eax, eax
    jz .same
    cmp eax, ST_TYPE
    je .ne
    mov edx, [rcx+E_FLAGS]
    test edx, EF_ALT
    jnz .ne
    shr edx, EF_ACTSH
    and edx, 7
    cmp edx, 3
    je .skip
    cmp eax, ST_LONLY
    je .r
    cmp eax, ST_RONLY
    je .l
    ; two different items: a folder has nothing to copy
    test dword [rcx+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .ne
    cmp edx, 1
    je .r
    cmp edx, 2
    je .l
    mov rax, [rcx+E_MTA]
    cmp rax, [rcx+E_MTB]
    je .ne
    ja .r
.l:
    mov eax, 2
    ret
.r:
    mov eax, 1
    ret
.ne:
    mov eax, 3
    ret
.same:
    mov eax, 4
    ret
.skip:
    mov eax, 5
    ret

; fold_cycle(fold, row): the next action of the item in that row: new and removed items alternate between the
;   copy and "skip", two different files go through right, left and skip
fold_cycle:
    PROLOG 64
    mov rbx, rcx
    mov r12, rdx
    cmp rdx, [rbx+F_NVIS]
    jae .d
    invoke fold_ent, rbx, rdx
    mov rsi, rax
    mov eax, [rsi+E_STATUS]
    cmp eax, ST_DIFF
    je .diff
    cmp eax, ST_LONLY
    je .two
    cmp eax, ST_RONLY
    jne .d
.two:
    mov eax, [rsi+E_FLAGS]
    shr eax, EF_ACTSH
    and eax, 7
    xor edi, edi                     ; copy again
    cmp eax, 3
    je .set
    mov edi, 3                       ; skip
    jmp .set
.diff:
    test dword [rsi+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .d
    test dword [rsi+E_FLAGS], EF_ALT
    jnz .d
    invoke fold_glyph, rsi
    mov edi, 1                       ; what is shown now -> the next one
    cmp eax, 1
    jne .d2
    mov edi, 2
    jmp .set
.d2:
    cmp eax, 2
    jne .set
    mov edi, 3
.set:
    mov eax, [rsi+E_FLAGS]
    and eax, ~(7 << EF_ACTSH)
    mov ecx, edi
    shl ecx, EF_ACTSH
    or eax, ecx
    mov [rsi+E_FLAGS], eax
    invoke SendMessageW, [rbx+F_HLIST], LVM_REDRAWITEMS, r12, r12
.d:
    EPILOG 64

; fold_icon(entry) -> eax index in the system icon list of the type of the item (a folder, or by extension);
;   the answers are kept in a small table: asking the shell for every cell of every repaint would be slow
%define IC_KEY 96                    ; wchar[16]
%define IC_PATH 128                  ; wchar[24]
%define IC_SFI 176                   ; SHFILEINFOW (696) -> 872
fold_icon:
    PROLOG 880
    mov rbx, rcx
    lea rdi, [rsp+IC_KEY]
    mov word [rdi], 1                ; folders: the key is the character 1
    mov word [rdi+2], 0
    mov r12d, 0x10                   ; FILE_ATTRIBUTE_DIRECTORY
    test dword [rbx+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .look
    mov r12d, 0x80                   ; FILE_ATTRIBUTE_NORMAL
    mov rax, [rbx+E_NAME]
    mov ecx, [rbx+E_NAMEOFF]
    lea rsi, [rax+rcx*2]
    xor r13d, r13d                   ; the last dot
.dot:
    movzx eax, word [rsi]
    test eax, eax
    jz .dotd
    cmp eax, '.'
    jne .dn
    mov r13, rsi
.dn:
    add rsi, 2
    jmp .dot
.dotd:
    mov word [rdi], 0
    test r13, r13
    jz .look
    lea rsi, [r13+2]
    xor ecx, ecx
.ex:
    movzx eax, word [rsi+rcx*2]
    test eax, eax
    jz .exd
    cmp ecx, 14
    jae .exd
    cmp eax, 'A'
    jb .exs
    cmp eax, 'Z'
    ja .exs
    add eax, 32
.exs:
    mov [rdi+rcx*2], ax
    inc ecx
    jmp .ex
.exd:
    mov word [rdi+rcx*2], 0
.look:
    xor r14d, r14d
.find:
    cmp r14, [ico_n]
    jae .miss
    mov rax, r14
    shl rax, 5
    lea rcx, [ico_keys]
    add rcx, rax
    xor edx, edx
.cmp:
    movzx eax, word [rcx+rdx*2]
    cmp ax, [rdi+rdx*2]
    jne .next
    test eax, eax
    jz .hit
    inc edx
    cmp edx, 16
    jb .cmp
.hit:
    lea rcx, [ico_vals]
    mov eax, [rcx+r14*4]
    EPILOG 880
.next:
    inc r14
    jmp .find
.miss:
    ; ask the shell about "x.<ext>" (attributes only: the file does not exist)
    lea rsi, [rsp+IC_PATH]
    mov word [rsi], 'x'
    mov word [rsi+2], 0
    cmp word [rdi], 1
    je .ask
    cmp word [rdi], 0
    je .ask
    mov word [rsi+2], '.'
    xor ecx, ecx
.cp:
    movzx eax, word [rdi+rcx*2]
    mov [rsi+rcx*2+4], ax
    test eax, eax
    jz .ask
    inc ecx
    cmp ecx, 15
    jb .cp
    mov word [rsi+rcx*2+4], 0
.ask:
    invoke SHGetFileInfoW, rsi, r12, &rsp+IC_SFI, 696, 0x4011      ; USEFILEATTRIBUTES | SYSICONINDEX | SMALLICON
    mov eax, [rsp+IC_SFI+8]
    mov r15d, eax
    mov r14, [ico_n]
    cmp r14, 256
    jae .done
    mov rax, r14
    shl rax, 5
    lea rcx, [ico_keys]
    add rcx, rax
    xor edx, edx
.sv:
    movzx eax, word [rdi+rdx*2]
    mov [rcx+rdx*2], ax
    test eax, eax
    jz .svd
    inc edx
    cmp edx, 15
    jb .sv
    mov word [rcx+rdx*2], 0
.svd:
    lea rcx, [ico_vals]
    mov [rcx+r14*4], r15d
    inc qword [ico_n]
.done:
    mov eax, r15d
    EPILOG 880

; fold_free_ents(fold)
fold_free_ents:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
.l:
    cmp rsi, [rbx+F_NENT]
    jae .d
    mov rax, rsi
    shl rax, 6
    add rax, [rbx+F_ENTS]
    invoke free, [rax+E_NAME]
    inc rsi
    jmp .l
.d:
    mov qword [rbx+F_NENT], 0
    mov qword [rbx+F_NVIS], 0
    EPILOG 64

; fold_namehas(entry, text (lower case), length) -> eax 1 when the name of the entry contains the text
fold_namehas:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov r12d, r8d
    mov rax, [rsi+E_NAME]
    mov ecx, [rsi+E_NAMEOFF]
    lea r13, [rax+rcx*2]
    xor r14d, r14d
.nl:
    cmp word [r13+r14*2], 0
    je .nld
    inc r14d
    jmp .nl
.nld:
    sub r14d, r12d                   ; last position the text can start at
    js .no
    xor r15d, r15d
.st:
    cmp r15d, r14d
    ja .no
    xor ebx, ebx
.cm:
    cmp ebx, r12d
    jae .yes
    lea rax, [r15+rbx]
    movzx ecx, word [r13+rax*2]
    cmp ecx, 'A'
    jb .cmp
    cmp ecx, 'Z'
    ja .hi
    add ecx, 32
    jmp .cmp
.hi:
    cmp ecx, 0x80
    jb .cmp
    invoke CharLowerW, rcx           ; a character: the high word of the argument is zero
    mov ecx, eax
.cmp:
    cmp cx, [rdi+rbx*2]
    jne .sn
    inc ebx
    jmp .cm
.sn:
    inc r15d
    jmp .st
.yes:
    mov eax, 1
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; fold_rebuild(fold): recompute the visible row list (status filter, name filter, collapsed folders)
;   A folder stays while its own name or a file below it matches the name filter; while a name filter is
;   set, collapsed folders are listed open.
%define RB_FLT 96                    ; wchar[100]: the name filter, lower case
%define RB_FLN 296                   ; its length
fold_rebuild:
    PROLOG 320
    mov rbx, rcx
    mov word [rsp+RB_FLT], 0
    invoke GetDlgItem, [rbx+F_HVIEW], ID_FILTER
    test rax, rax
    jz .nof
    invoke GetWindowTextW, rax, &rsp+RB_FLT, 99
.nof:
    invoke wcslen, &rsp+RB_FLT
    mov [rsp+RB_FLN], rax
    test rax, rax
    jz .nolow
    invoke CharLowerBuffW, &rsp+RB_FLT, rax
.nolow:
    mov rax, [rbx+F_NENT]
    lea rdx, [rax*4+8]
    invoke realloc, [rbx+F_VIS], rdx
    mov [rbx+F_VIS], rax
    mov rsi, rax
    xor edi, edi                     ; n
    xor r12d, r12d                   ; i
    mov r13d, [rbx+F_FMASK]
    mov r14, [rbx+F_ENTS]
.l:
    cmp r12, [rbx+F_NENT]
    jae .done
    mov rax, r12
    shl rax, 6
    add rax, r14
    mov r15, rax                     ; entry
    mov edx, [r15+E_MASK]
    test edx, r13d
    jnz .vis
    mov r12d, [r15+E_SUBEND]
    jmp .l
.vis:
    cmp qword [rsp+RB_FLN], 0
    je .add
    test dword [r15+E_FLAGS], EF_DIRA|EF_DIRB
    jz .file
    invoke fold_namehas, r15, &rsp+RB_FLT, [rsp+RB_FLN]
    test eax, eax
    jnz .add
    lea rbp, [r12+1]
.sub:
    mov eax, [r15+E_SUBEND]
    cmp rbp, rax
    jae .nosub
    mov rcx, rbp
    shl rcx, 6
    add rcx, r14
    mov edx, [rcx+E_MASK]
    test edx, r13d
    jz .subn
    test dword [rcx+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .subn
    invoke fold_namehas, rcx, &rsp+RB_FLT, [rsp+RB_FLN]
    test eax, eax
    jnz .add
.subn:
    inc rbp
    jmp .sub
.nosub:
    mov r12d, [r15+E_SUBEND]         ; nothing below matches: the whole folder goes
    jmp .l
.file:
    invoke fold_namehas, r15, &rsp+RB_FLT, [rsp+RB_FLN]
    test eax, eax
    jnz .add
    inc r12
    jmp .l
.add:
    mov [rsi+rdi*4], r12d
    inc rdi
    cmp qword [rsp+RB_FLN], 0
    jne .next
    test dword [r15+E_FLAGS], EF_COLL
    jz .next
    test dword [r15+E_FLAGS], EF_DIRA|EF_DIRB
    jz .next
    mov r12d, [r15+E_SUBEND]
    jmp .l
.next:
    inc r12
    jmp .l
.done:
    mov [rbx+F_NVIS], rdi
    EPILOG 320

; fold_refresh(fold): rebuild + push the new count into the list view
fold_refresh:
    PROLOG 64
    mov rbx, rcx
    invoke fold_rebuild, rbx
    invoke SendMessageW, [rbx+F_HLIST], LVM_SETITEMCOUNT, [rbx+F_NVIS], 0
    invoke InvalidateRect, [rbx+F_HLIST], 0, 1
    EPILOG 64

; fold_read_ui(hwnd, fold): options, filter mask and exclude text from the controls
fold_read_ui:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    xor edi, edi
    invoke get_chk, rsi, ID_CHK_SIZE
    or edi, eax
    invoke get_chk, rsi, ID_CHK_MTIME
    shl eax, 1
    or edi, eax
    invoke get_chk, rsi, ID_CHK_CONT
    shl eax, 2
    or edi, eax
    invoke get_chk, rsi, ID_CHK_LINKS
    shl eax, 3
    or edi, eax
    invoke get_chk, rsi, ID_CHK_CRC
    shl eax, 5
    or edi, eax
    invoke get_chk, rsi, ID_CHK_ATTR
    shl eax, 6
    or edi, eax
    invoke get_chk, rsi, ID_CHK_ACL
    shl eax, 8
    or edi, eax
    invoke get_chk, rsi, ID_CHK_LOOSE
    shl eax, 7
    or edi, eax
    mov eax, [rbx+F_OPT]
    and eax, 0x10
    or edi, eax
    mov [rbx+F_OPT], edi
    xor edi, edi
    invoke get_chk, rsi, ID_CHK_SAME
    or edi, eax
    invoke get_chk, rsi, ID_CHK_DIFF
    test eax, eax
    jz .a
    or edi, 0x12
.a:
    invoke get_chk, rsi, ID_CHK_LONLY
    test eax, eax
    jz .b
    or edi, 4
.b:
    invoke get_chk, rsi, ID_CHK_RONLY
    test eax, eax
    jz .c
    or edi, 8
.c:
    mov [rbx+F_FMASK], edi
    invoke GetDlgItem, rsi, ID_EDIT_EXCL
    invoke GetWindowTextW, rax, &rbx+F_EXCL, 255
    ; FC-03 filters
    invoke GetDlgItem, rsi, ID_EDIT_FMIN
    invoke GetWindowTextW, rax, &rsp+96, 18
    invoke wcstoul, &rsp+96, 0, 10
    mov eax, eax
    shl rax, 10
    mov [rbx+F_FMIN], rax
    invoke GetDlgItem, rsi, ID_EDIT_FMAX
    invoke GetWindowTextW, rax, &rsp+96, 18
    invoke wcstoul, &rsp+96, 0, 10
    mov eax, eax
    shl rax, 10
    mov [rbx+F_FMAX], rax
    invoke GetDlgItem, rsi, ID_EDIT_FDAYS
    invoke GetWindowTextW, rax, &rsp+96, 18
    invoke wcstoul, &rsp+96, 0, 10
    mov [rbx+F_FDAYS], eax
    mov qword [rbx+F_FCUT], 0
    mov r13d, eax
    invoke GetDlgItem, rsi, ID_EDIT_MASK
    invoke GetWindowTextW, rax, &rbx+F_MASKS, 120
    ; FC-04: letters -> excluded attribute mask
    invoke GetDlgItem, rsi, ID_EDIT_ATTRX
    invoke GetWindowTextW, rax, &rsp+96, 18
    xor r14d, r14d
    xor ecx, ecx
.al:
    movzx eax, word [rsp+96+rcx*2]
    test eax, eax
    jz .ald
    or eax, 0x20
    xor edx, edx
.af:
    cmp edx, 8
    jae .an
    lea r8, [s_attr_letters]
    movzx r9d, word [r8+rdx*2]
    or r9d, 0x20
    cmp r9d, eax
    jne .ax
    mov r10d, 1
    push rcx
    mov ecx, edx
    shl r10d, cl
    pop rcx
    or r14d, r10d
    jmp .an
.ax:
    inc edx
    jmp .af
.an:
    inc ecx
    jmp .al
.ald:
    mov [rbx+F_ATTRX], r14d
    mov eax, r13d
    test eax, eax
    jz .nofcut
    mov r12d, eax
    invoke GetSystemTimeAsFileTime, &rsp+136
    mov rax, 864000000000
    imul rax, r12
    mov rcx, [rsp+136]
    sub rcx, rax
    jbe .nofcut
    mov [rbx+F_FCUT], rcx
.nofcut:
    EPILOG 160

; fold_collect(hwnd, sess, fold): paths + options string -> session  "SMCLBF RA|exclude"
fold_collect:
    PROLOG 160
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    invoke pb_collect, rsi, rdi
    invoke fold_read_ui, rsi, rbx
    xor r12d, r12d                   ; filter as a 4-bit value
    mov eax, [rbx+F_FMASK]
    test eax, 1
    jz .n1
    or r12d, 1
.n1:
    test eax, 2
    jz .n2
    or r12d, 2
.n2:
    test eax, 4
    jz .n3
    or r12d, 4
.n3:
    test eax, 8
    jz .n4
    or r12d, 8
.n4:
    mov eax, [rbx+F_OPT]
    mov ecx, eax
    shr ecx, 1
    and ecx, 1
    mov [rsp+32], rcx                ; M
    mov ecx, eax
    shr ecx, 2
    and ecx, 1
    mov [rsp+40], rcx                ; C
    mov ecx, eax
    shr ecx, 3
    and ecx, 1
    mov [rsp+48], rcx                ; L
    mov ecx, eax
    shr ecx, 4
    and ecx, 1
    mov [rsp+56], rcx                ; B
    mov [rsp+64], r12                ; F
    mov ecx, eax
    shr ecx, 5
    and ecx, 1
    mov [rsp+72], rcx                ; R (CRC)
    mov ecx, eax
    shr ecx, 6
    and ecx, 1
    mov [rsp+80], rcx                ; A (attributes)
    lea rcx, [rbx+F_EXCL]
    mov [rsp+88], rcx
    and eax, 1
    mov r9d, eax                     ; S
    lea rcx, [rdi+SS_OPTS]
    mov edx, 1023
    lea r8, [f_opts]
    call _snwprintf
    invoke wcslen, &rdi+SS_OPTS
    lea r15, [rdi+SS_OPTS]
    lea r15, [r15+rax*2]
    mov r12, [rbx+F_FMIN]
    shr r12, 10
    mov r13, [rbx+F_FMAX]
    shr r13, 10
    mov r14d, [rbx+F_FDAYS]
    invoke _snwprintf, r15, 40, &f_filt, r12, r13, r14
    EPILOG 160

; fold_opts_apply(hwnd, sess, fold): restore the controls from the saved options string
fold_opts_apply:
    PROLOG 160
    mov rsi, rcx
    lea rbx, [rdx+SS_OPTS]
    mov rdi, r8
    cmp word [rbx], 0
    je .d
    movzx r8d, word [rbx]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_SIZE, r8
    movzx r8d, word [rbx+2]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_MTIME, r8
    movzx r8d, word [rbx+4]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_CONT, r8
    movzx r8d, word [rbx+6]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_LINKS, r8
    movzx eax, word [rbx+8]
    sub eax, '0'
    shl eax, 4
    mov [rdi+F_OPT], eax
    movzx eax, word [rbx+10]
    cmp eax, '9'
    jbe .dig
    sub eax, 7
.dig:
    sub eax, '0'
    mov r12d, eax
    mov r8d, r12d
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_SAME, r8
    mov r8d, r12d
    shr r8d, 1
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_DIFF, r8
    mov r8d, r12d
    shr r8d, 2
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_LONLY, r8
    mov r8d, r12d
    shr r8d, 3
    and r8d, 1
    invoke set_chk_id, rsi, ID_CHK_RONLY, r8
    movzx r8d, word [rbx+12]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_CRC, r8
    movzx r8d, word [rbx+14]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_ATTR, r8
    lea r13, [rbx+18]
.f1f:
    movzx eax, word [r13]
    test eax, eax
    jz .nof
    cmp eax, 0x1F
    je .gotf
    add r13, 2
    jmp .f1f
.gotf:
    mov word [r13], 0
    invoke GetDlgItem, rsi, ID_EDIT_EXCL
    invoke SetWindowTextW, rax, &rbx+18
    mov word [r13], 0x1F
    lea r14, [r13+2]
    xor r15d, r15d
.pf:
    cmp r15d, 3
    jae .d
    invoke wcstoul, r14, &rsp+96, 10
    mov r12d, eax
    mov eax, ID_EDIT_FMIN
    cmp r15d, 1
    jb .have
    mov eax, ID_EDIT_FMAX
    cmp r15d, 2
    jb .have
    mov eax, ID_EDIT_FDAYS
.have:
    mov [rsp+104], eax
    mov word [rsp+112], 0
    test r12d, r12d
    jz .set
    invoke _snwprintf, &rsp+112, 16, &f_uint, r12
.set:
    mov eax, [rsp+104]
    invoke GetDlgItem, rsi, rax
    invoke SetWindowTextW, rax, &rsp+112
    mov r14, [rsp+96]
    cmp word [r14], ','
    jne .d
    add r14, 2
    inc r15d
    jmp .pf
.nof:
    invoke GetDlgItem, rsi, ID_EDIT_EXCL
    invoke SetWindowTextW, rax, &rbx+18
.d:
    EPILOG 160

; fold_theme(fold)
fold_theme:
    PROLOG 64
    mov rbx, rcx
    mov rsi, [rbx+F_HLIST]
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
    invoke theme_checks, [rbx+F_HVIEW]
    EPILOG 64

; ---------------------------------------------------------------- start a comparison
; fold_watch_close(fold): close directory change notification handles
fold_watch_close:
    PROLOG 64
    mov rbx, rcx
    mov rcx, [rbx+F_CHGA]
    test rcx, rcx
    jz .b
    cmp rcx, -1
    je .za
    call FindCloseChangeNotification
.za:
    mov qword [rbx+F_CHGA], 0
.b:
    mov rcx, [rbx+F_CHGB]
    test rcx, rcx
    jz .d
    cmp rcx, -1
    je .zb
    call FindCloseChangeNotification
.zb:
    mov qword [rbx+F_CHGB], 0
.d:
    EPILOG 64

; fold_watch_arm(hwnd, fold): arm directory change notifications when auto-refresh is enabled
fold_watch_arm:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke fold_watch_close, rbx
    invoke get_chk, rsi, ID_CHK_AUTOREF
    test eax, eax
    jz .d
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .d
    cmp word [rbx+F_ROOTA], 0
    je .d
    cmp word [rbx+F_ROOTB], 0
    je .d
    invoke FindFirstChangeNotificationW, &rbx+F_ROOTA, 1, 0x1F
    mov [rbx+F_CHGA], rax
    invoke FindFirstChangeNotificationW, &rbx+F_ROOTB, 1, 0x1F
    mov [rbx+F_CHGB], rax
.d:
    EPILOG 64

; fold_start(hwnd, fold)
fold_start:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+F_SYNCRUN], 0
    jne .busy
    invoke fold_pv_confirm, rsi, rbx
    test eax, eax
    jz .stay                         ; cancelled: the unsaved changes stay where they are
    invoke fold_watch_close, rbx
    invoke fold_pv_load, rsi, rbx, -1   ; the entries are replaced: the preview waits for a new choice
    cmp qword [rbx+F_RUNNING], 0
    je .idle
    mov byte [rbx+F_CANCEL], 1
.idle:
    mov rcx, [rbx+F_THREAD]          ; also a scan that has just ended and whose "done" message is still on its
    test rcx, rcx                    ; way: its handle used to be overwritten by the new thread's (a leak)
    jz .reaped
    invoke WaitForSingleObject, rcx, 0xFFFFFFFF
    invoke CloseHandle, [rbx+F_THREAD]
    mov qword [rbx+F_THREAD], 0
    mov qword [rbx+F_RUNNING], 0
.reaped:
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rbx+F_ROOTA, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, &rbx+F_ROOTB, 519
    invoke trim_slash, &rbx+F_ROOTA
    invoke trim_slash, &rbx+F_ROOTB
    ; AR-02: archives compared as single files
    invoke get_chk, rsi, ID_CHK_ASFILE
    test eax, eax
    jz .noasf
    invoke GetFileAttributesW, &rbx+F_ROOTA
    cmp eax, -1
    je .noasf
    test eax, 0x10
    jnz .noasf
    invoke GetFileAttributesW, &rbx+F_ROOTB
    cmp eax, -1
    je .noasf
    test eax, 0x10
    jnz .noasf
    invoke add_session, T_HEX, &rbx+F_ROOTA, &rbx+F_ROOTB, 0
    EPILOG 64
.noasf:
    invoke fold_free_vt, rbx
    invoke fold_open_root, rbx, 0, &rbx+F_ROOTA
    test eax, eax
    jz .bad
    invoke fold_open_root, rbx, 1, &rbx+F_ROOTB
    test eax, eax
    jz .bad
    invoke fold_read_ui, rsi, rbx
    invoke fold_free_ents, rbx
    invoke fold_refresh, rbx
    xor eax, eax
    mov [rbx+F_CNT], rax
    mov [rbx+F_CNT+8], rax
    mov [rbx+F_CNT+16], rax
    mov [rbx+F_CNT+24], rax
    mov [rbx+F_CNT+32], rax
    mov dword [rbx+F_SCANERR], 0
    invoke alt_clear, rbx
    mov byte [rbx+F_CANCEL], 0
    mov qword [rbx+F_RUNNING], 1
    invoke CreateThread, 0, 0x1000000, &scan_thread, rbx, 0, 0
    mov [rbx+F_THREAD], rax
    invoke set_status, &s_scanning_0
    EPILOG 64
.bad:
    test qword [g_batch], 4
    jz .badui
    mov qword [g_exitcode], 2
    invoke set_status, &s_badpath
    invoke PostMessageW, [g_hframe], WM_CLOSE, 0, 0
    EPILOG 64
.badui:
    invoke msgbox, rsi, &s_badpath, MB_OK|MB_ICONWARNING
    EPILOG 64
.busy:
    invoke msgbox, rsi, &s_busy, MB_OK|MB_ICONWARNING
    EPILOG 64
.stay:
    EPILOG 64

; fold_done(hwnd, sess, fold, cancelled)
fold_done:
    PROLOG 512
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    mov r12, r9
    mov rcx, [rbx+F_THREAD]
    test rcx, rcx
    jz .noh
    call CloseHandle
    mov qword [rbx+F_THREAD], 0
.noh:
    invoke fold_refresh, rbx
    test r12, r12
    jnz .cancel
    invoke _snwprintf, &rsp+96, 200, &s_scan_done, [rbx+F_CNT], [rbx+F_CNT+8], [rbx+F_CNT+16], [rbx+F_CNT+24], [rbx+F_CNT+32]
    invoke set_status_part, 1, &rsp+96
    invoke set_status, &s_done_short
    cmp dword [rbx+F_SCANERR], 0
    je .scanok
    invoke set_status, &s_scanerr_short
.scanok:
    invoke fold_watch_arm, rsi, rbx
    jmp .title
.cancel:
    invoke set_status, &s_scan_cancel
.title:
    invoke wcsrchr, &rbx+F_ROOTA, 0x5C
    lea r12, [rbx+F_ROOTA]
    test rax, rax
    jz .a
    lea r12, [rax+2]
.a:
    invoke wcsrchr, &rbx+F_ROOTB, 0x5C
    lea r13, [rbx+F_ROOTB]
    test rax, rax
    jz .b
    lea r13, [rax+2]
.b:
    invoke _snwprintf, &rdi+SS_TITLE, 60, &s_title_f, r12, r13
    invoke set_tab_title, rdi, &rdi+SS_TITLE
    EPILOG 512

; ---------------------------------------------------------------- cell text
; fold_cell(fold, row, col, buf)
fold_cell:
    PROLOG 192
    mov rbx, rcx
    mov rsi, r9
    mov rax, [rbx+F_VIS]
    mov eax, [rax+rdx*4]
    shl rax, 6
    add rax, [rbx+F_ENTS]
    mov rdi, rax
    mov word [rsi], 0
    cmp r8d, 8                       ; a column of the list: its kind of cell
    jae .kind
    lea rax, [col_kind]
    mov r8d, [rax+r8*4]
.kind:
    cmp r8d, 0
    je .name
    cmp r8d, 9
    je .name
    cmp r8d, 10
    je .out
    cmp r8d, 1
    je .status
    cmp r8d, 2
    je .dir
    cmp r8d, 3
    je .sza
    cmp r8d, 4
    je .mta
    cmp r8d, 5
    je .szb
    cmp r8d, 7
    je .attrc
    jmp .mtb
.attrc:
    ; "<A letters> | <B letters>"
    lea r10, [rsp+96]
    mov eax, [rdi+E_FLAGS]
    shr eax, 8
    movzx r11d, al
    xor ecx, ecx
    xor edx, edx
.l1:
    cmp ecx, 8
    jae .l1d
    bt r11d, ecx
    jnc .l1n
    lea r8, [s_attr_letters]
    movzx r9d, word [r8+rcx*2]
    mov [r10+rdx*2], r9w
    inc edx
.l1n:
    inc ecx
    jmp .l1
.l1d:
    mov word [r10+rdx*2], 0
    lea r10, [rsp+130]
    mov eax, [rdi+E_FLAGS]
    shr eax, 16
    movzx r11d, al
    xor ecx, ecx
    xor edx, edx
.l2:
    cmp ecx, 8
    jae .l2d
    bt r11d, ecx
    jnc .l2n
    lea r8, [s_attr_letters]
    movzx r9d, word [r8+rcx*2]
    mov [r10+rdx*2], r9w
    inc edx
.l2n:
    inc ecx
    jmp .l2
.l2d:
    mov word [r10+rdx*2], 0
    invoke _snwprintf, rsi, 40, &f_attrcell, &rsp+96, &rsp+130
    jmp .out
.name:
    ; the left name is empty when the item is not on the left, the right name when it is not on the right
    mov eax, [rdi+E_FLAGS]
    cmp r8d, 9
    je .rname
    test eax, EF_EXA
    jz .out
    jmp .nameok
.rname:
    test eax, EF_EXB
    jz .out
.nameok:
    mov ecx, [rdi+E_DEPTH]
    add ecx, ecx
    cmp ecx, 60
    jbe .ok
    mov ecx, 60
.ok:
    xor edx, edx
.sp:
    cmp edx, ecx
    jae .spd
    mov word [rsi+rdx*2], ' '
    inc edx
    jmp .sp
.spd:
    lea r10, [rsi+rdx*2]
    lea rax, [s_sp2]
    test dword [rdi+E_FLAGS], EF_DIRA|EF_DIRB
    jz .mk
    cmp r8d, 9
    jne .mkl
    test dword [rdi+E_FLAGS], EF_EXA
    jnz .mk                          ; on both sides: the marker is on the left
.mkl:
    lea rax, [s_marker_open]
    test dword [rdi+E_FLAGS], EF_COLL
    jz .mk
    lea rax, [s_marker_closed]
.mk:
    movzx ecx, word [rax]
    mov [r10], cx
    movzx ecx, word [rax+2]
    mov [r10+2], cx
    add r10, 4
    mov rax, [rdi+E_NAME]
    mov ecx, [rdi+E_NAMEOFF]
    lea rax, [rax+rcx*2]
    lea r11, [rsi+500]
.cp:
    cmp r10, r11
    jae .cpd
    movzx ecx, word [rax]
    test ecx, ecx
    jz .cpd
    mov [r10], cx
    add r10, 2
    add rax, 2
    jmp .cp
.cpd:
    mov word [r10], 0
    jmp .out
.dir:
    invoke fold_glyph, rdi
    lea r10, [s_empty]
    cmp eax, 1
    jne .g2
    lea r10, [s_ar_r]
.g2:
    cmp eax, 2
    jne .g3
    lea r10, [s_ar_l]
.g3:
    cmp eax, 3
    jne .g4
    lea r10, [s_ar_ne]
.g4:
    cmp eax, 4
    jne .g5
    lea r10, [s_ar_eq]
.g5:
    cmp eax, 5
    jne .dset
    lea r10, [s_ar_skip]
.dset:
    invoke wcscpy, rsi, r10
    jmp .out
.status:
    mov eax, [rdi+E_STATUS]
    lea rcx, [st_text]
    mov r10, [rcx+rax*8]
    invoke wcscpy, rsi, r10
    jmp .out
.sza:
    test dword [rdi+E_FLAGS], EF_EXA
    jz .out
    test dword [rdi+E_FLAGS], EF_DIRA
    jnz .out
    invoke fmt_size, rsi, [rdi+E_SIZEA]
    jmp .out
.mta:
    test dword [rdi+E_FLAGS], EF_EXA
    jz .out
    invoke fmt_time, rsi, [rdi+E_MTA]
    jmp .out
.szb:
    test dword [rdi+E_FLAGS], EF_EXB
    jz .out
    test dword [rdi+E_FLAGS], EF_DIRB
    jnz .out
    invoke fmt_size, rsi, [rdi+E_SIZEB]
    jmp .out
.mtb:
    test dword [rdi+E_FLAGS], EF_EXB
    jz .out
    invoke fmt_time, rsi, [rdi+E_MTB]
.out:
    EPILOG 192

; ---------------------------------------------------------------- list interaction

; fold_ent(fold, row) -> rax = entry pointer
fold_ent:
    mov rax, [rcx+F_VIS]
    mov eax, [rax+rdx*4]
    shl rax, 6
    add rax, [rcx+F_ENTS]
    ret

; fold_setcoll(fold, row, mode 0=expand 1=collapse 2=toggle)
fold_setcoll:
    PROLOG 192
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    cmp rsi, [rbx+F_NVIS]
    jae .d
    invoke fold_ent, rbx, rsi
    test dword [rax+E_FLAGS], EF_DIRA|EF_DIRB
    jz .d
    cmp rdi, 2
    je .tog
    test rdi, rdi
    jz .exp
    or dword [rax+E_FLAGS], EF_COLL
    jmp .ref
.exp:
    and dword [rax+E_FLAGS], ~EF_COLL
    jmp .ref
.tog:
    xor dword [rax+E_FLAGS], EF_COLL
.ref:
    invoke fold_refresh, rbx
    xor eax, eax
    mov [rsp+96], rax
    mov [rsp+104], rax
    mov [rsp+112], rax
    mov [rsp+120], rax
    mov [rsp+128], rax
    mov dword [rsp+108], 3
    mov dword [rsp+112], 3
    invoke SendMessageW, [rbx+F_HLIST], LVM_SETITEMSTATE, rsi, &rsp+96
.d:
    EPILOG 192

; fold_all(fold, collapse 0/1)
fold_all:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    xor ecx, ecx
    mov r8, [rbx+F_ENTS]
.l:
    cmp rcx, [rbx+F_NENT]
    jae .d
    mov rax, rcx
    shl rax, 6
    add rax, r8
    test dword [rax+E_FLAGS], EF_DIRA|EF_DIRB
    jz .n
    and dword [rax+E_FLAGS], ~EF_COLL
    test rsi, rsi
    jz .n
    or dword [rax+E_FLAGS], EF_COLL
.n:
    inc rcx
    jmp .l
.d:
    invoke fold_refresh, rbx
    EPILOG 64

; fold_open_pair(fold, row): folders toggle, files open a compare tab
fold_open_pair:
    PROLOG 4512
    mov rbx, rcx
    mov rsi, rdx
    cmp rsi, [rbx+F_NVIS]
    jae .d
    invoke fold_ent, rbx, rsi
    mov rdi, rax
    test dword [rdi+E_FLAGS], EF_DIRA|EF_DIRB
    jz .file
    invoke fold_setcoll, rbx, rsi, 2
    jmp .d
.file:
    mov eax, [rdi+E_FLAGS]
    and eax, EF_EXA|EF_EXB
    cmp eax, EF_EXA|EF_EXB
    jne .d
    invoke fold_side_path, rbx, 0, [rdi+E_NAME], &rsp+96
    test eax, eax
    jz .d
    invoke fold_side_path, rbx, 1, [rdi+E_NAME], &rsp+2296
    test eax, eax
    jz .d
    invoke ext_launch, &rsp+96, &rsp+2296
    test eax, eax
    jnz .d
    invoke type_for_path, &rsp+96
    invoke add_session, rax, &rsp+96, &rsp+2296, 0
.d:
    EPILOG 4512

; ---------------------------------------------------------------- selection -> path lists
%macro ADDPATH 2                      ; cursor register, root offset
    invoke _snwprintf, %1, 1099, &f_path, &r12+%2, [r14+E_NAME]
    invoke wcslen, %1
    lea %1, [%1+rax*2+2]
%endmacro

; build_lists(fold, mode, &src, &dst) -> eax = number of source paths
;   mode 1: A->B copy, 2: B->A copy, 3: delete A, 4: delete B, 5: delete both
build_lists:
    PROLOG 128
    mov r12, rcx
    mov r13d, edx
    mov [rsp+96], r8
    mov [rsp+104], r9
    invoke SendMessageW, [r12+F_HLIST], LVM_GETSELECTEDCOUNT, 0, 0
    test eax, eax
    jz .zero
    imul rax, rax, 4400
    add rax, 32
    mov r14, rax
    invoke calloc, 1, r14
    mov [rsp+112], rax
    invoke calloc, 1, r14
    mov [rsp+120], rax
    test rax, rax
    jz .zero
    mov rsi, [rsp+112]
    mov rdi, [rsp+120]
    xor r15d, r15d
    mov rbx, -1
.next:
    invoke SendMessageW, [r12+F_HLIST], LVM_GETNEXTITEM, rbx, LVNI_SELECTED
    movsxd rbx, eax
    test rbx, rbx
    js .done
    invoke fold_ent, r12, rbx
    mov r14, rax
    test dword [r14+E_FLAGS], EF_ALT
    jnz .next
    cmp r13d, 1
    je .lr
    cmp r13d, 2
    je .rl
    cmp r13d, 3
    je .dl
    cmp r13d, 4
    je .dr
    test dword [r14+E_FLAGS], EF_EXA
    jz .both_b
    ADDPATH rsi, F_ROOTA
    inc r15d
.both_b:
    test dword [r14+E_FLAGS], EF_EXB
    jz .next
    ADDPATH rsi, F_ROOTB
    inc r15d
    jmp .next
.lr:
    test dword [r14+E_FLAGS], EF_EXA
    jz .next
    cmp dword [r14+E_STATUS], ST_SAME        ; identical items need no copy
    je .next
    ADDPATH rsi, F_ROOTA
    ADDPATH rdi, F_ROOTB
    inc r15d
    jmp .next
.rl:
    test dword [r14+E_FLAGS], EF_EXB
    jz .next
    cmp dword [r14+E_STATUS], ST_SAME
    je .next
    ADDPATH rsi, F_ROOTB
    ADDPATH rdi, F_ROOTA
    inc r15d
    jmp .next
.dl:
    test dword [r14+E_FLAGS], EF_EXA
    jz .next
    ADDPATH rsi, F_ROOTA
    inc r15d
    jmp .next
.dr:
    test dword [r14+E_FLAGS], EF_EXB
    jz .next
    ADDPATH rsi, F_ROOTB
    inc r15d
    jmp .next
.done:
    mov rax, [rsp+96]
    mov rcx, [rsp+112]
    mov [rax], rcx
    mov rax, [rsp+104]
    mov rcx, [rsp+120]
    mov [rax], rcx
    mov eax, r15d
    EPILOG 128
.zero:
    mov rax, [rsp+96]
    mov qword [rax], 0
    mov rax, [rsp+104]
    mov qword [rax], 0
    xor eax, eax
    EPILOG 128

; ensure_parents(rcx=double-NUL list of paths): create every parent folder
ensure_parents:
    PROLOG 64
    mov rsi, rcx
.l:
    cmp word [rsi], 0
    je .d
    invoke wcsrchr, rsi, 0x5C
    test rax, rax
    jz .adv
    mov rdi, rax
    mov word [rdi], 0
    invoke SHCreateDirectoryExW, 0, rsi, 0
    mov word [rdi], 0x5C
.adv:
    invoke wcslen, rsi
    lea rsi, [rsi+rax*2+2]
    jmp .l
.d:
    EPILOG 64

; shfop(hwnd, op, from, to) -> eax : shell copy (multi destination) or recycle-bin delete
shfop:
    PROLOG 160
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    mov r12, r9
    invoke memset, &rsp+96, 0, SHF_SIZE
    mov [rsp+96], rbx
    mov [rsp+104], esi
    mov [rsp+112], rdi
    mov [rsp+120], r12
    mov word [rsp+128], 0x0201
    cmp esi, FO_DELETE
    jne .go
    mov word [rsp+128], FOF_ALLOWUNDO
    test rbx, rbx
    jnz .go
    or word [rsp+128], 0x414          ; no window: silent (no confirmation / no error UI)
.go:
    invoke SHFileOperationW, &rsp+96
    EPILOG 160

; fold_undo(hwnd, fold, redo 0 / 1): take back / do again the newest file operation of this view (copy, move,
;   delete, rename, sync - see fjournal.asm), then compare again
fold_undo:
    PROLOG 128
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    cmp qword [rbx+F_RUNNING], 0
    jne .d
    cmp qword [rbx+F_SYNCRUN], 0
    jne .d
    invoke us_pop, &rbx+F_UST, r12, &rsp+96
    test eax, eax
    jz .none
    invoke fj_step, rsi, &rbx+F_UST, &rsp+96, r12
    test eax, eax
    jz .d
    lea rcx, [s_un_done]
    test r12d, r12d
    jz .say
    lea rcx, [s_un_redone]
.say:
    call set_status
    invoke fold_start, rsi, rbx
.d:
    EPILOG 128
.none:
    lea rcx, [s_un_none]
    test r12d, r12d
    jz .say2
    lea rcx, [s_un_noredo]
.say2:
    call set_status
    EPILOG 128

; fold_copy(hwnd, fold, dir 0=A->B 1=B->A)
fold_copy:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .ro
    test qword [g_policy], 1
    jnz .pol
    lea edi, [r8+1]
    invoke build_lists, rbx, rdi, &rsp+96, &rsp+104
    test eax, eax
    jz .none
    call fj_begin                    ; Ctrl+Z takes it back
    mov [rsp+112], rax
    invoke fj_lists, [rsp+112], [rsp+96], [rsp+104], 1
    invoke ensure_parents, [rsp+104]
    invoke shfop, rsi, FO_COPY, [rsp+96], [rsp+104]
    invoke fj_commit, &rbx+F_UST, [rsp+112]
    invoke free, [rsp+96]
    invoke free, [rsp+104]
    invoke fold_start, rsi, rbx
    EPILOG 160
.none:
    invoke msgbox, rsi, &s_nosel, MB_OK|MB_ICONINFO
    EPILOG 160
.pol:
    invoke msgbox, rsi, &s_pol_blocked, MB_OK|MB_ICONWARNING
    EPILOG 160
.ro:
    invoke msgbox, rsi, &s_ro, MB_OK|MB_ICONINFO
    EPILOG 160

fold_move:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .ro
    test qword [g_policy], 1
    jnz .pol
    lea edi, [r8+1]
    invoke build_lists, rbx, rdi, &rsp+96, &rsp+104
    test eax, eax
    jz .none
    call fj_begin
    mov [rsp+112], rax
    invoke fj_lists, [rsp+112], [rsp+96], [rsp+104], 2
    invoke ensure_parents, [rsp+104]
    invoke shfop, rsi, FO_MOVE, [rsp+96], [rsp+104]
    invoke fj_commit, &rbx+F_UST, [rsp+112]
    invoke free, [rsp+96]
    invoke free, [rsp+104]
    invoke fold_start, rsi, rbx
    EPILOG 160
.none:
    invoke msgbox, rsi, &s_nosel, MB_OK|MB_ICONINFO
    EPILOG 160
.pol:
    invoke msgbox, rsi, &s_pol_blocked, MB_OK|MB_ICONWARNING
    EPILOG 160
.ro:
    invoke msgbox, rsi, &s_ro, MB_OK|MB_ICONINFO
    EPILOG 160


; fold_rename(hwnd, fold, visible index): rename the entry (both sides when it exists on both)
fold_rename:
    PROLOG 6992
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .ro
    test qword [g_policy], 1
    jnz .pol
    cmp r12, [rbx+F_NVIS]
    jae .d
    invoke fold_ent, rbx, r12
    mov r13, rax
    test dword [r13+E_FLAGS], EF_ALT
    jnz .d
    invoke wcsncpy, &rsp+96, [r13+E_NAME], 519
    mov word [rsp+96+1038], 0
    lea rdi, [rsp+96]
    invoke wcsrchr, rdi, 0x5C
    test rax, rax
    jz .lf
    lea rdi, [rax+2]
.lf:
    invoke wcsncpy, &rsp+1200, rdi, 259
    mov word [rsp+1200+518], 0
    invoke input_box, rsi, &s_ren_title, &rsp+1200
    test eax, eax
    jz .d
    cmp word [rsp+1200], 0
    je .bad
    xor ecx, ecx
.v:
    movzx eax, word [rsp+1200+rcx*2]
    test eax, eax
    jz .vd
    cmp eax, 0x5C
    je .bad
    cmp eax, '/'
    je .bad
    cmp eax, ':'
    je .bad
    cmp eax, '*'
    je .bad
    cmp eax, '?'
    je .bad
    cmp eax, '"'
    je .bad
    cmp eax, '<'
    je .bad
    cmp eax, '>'
    je .bad
    cmp eax, '|'
    je .bad
    inc ecx
    jmp .v
.vd:
    invoke wcsncpy, &rsp+1760, &rsp+96, 519
    mov word [rsp+1760+1038], 0
    invoke wcsrchr, &rsp+1760, 0x5C
    test rax, rax
    jz .nodir
    mov word [rax+2], 0
    jmp .cat
.nodir:
    mov word [rsp+1760], 0
.cat:
    invoke wcscat, &rsp+1760, &rsp+1200
    call fj_begin
    mov r14, rax
    xor edi, edi
.side:
    cmp edi, 2
    jae .done
    mov eax, EF_EXA
    mov ecx, edi
    shl eax, cl
    test [r13+E_FLAGS], eax
    jz .nx
    lea rbp, [rbx+F_ROOTA]
    test edi, edi
    jz .rt
    lea rbp, [rbx+F_ROOTB]
.rt:
    invoke _snwprintf, &rsp+2840, 1030, &f_path, rbp, &rsp+96
    invoke _snwprintf, &rsp+4912, 1030, &f_path, rbp, &rsp+1760
    invoke MoveFileW, &rsp+2840, &rsp+4912
    test eax, eax
    jz .nx
    invoke fj_moved, r14, &rsp+2840, &rsp+4912
.nx:
    inc edi
    jmp .side
.done:
    invoke fj_commit, &rbx+F_UST, r14
    invoke fold_start, rsi, rbx
.d:
    EPILOG 6992
.bad:
    invoke msgbox, rsi, &s_ren_bad, MB_OK|MB_ICONWARNING
    EPILOG 6992
.ro:
    invoke msgbox, rsi, &s_ro, MB_OK|MB_ICONINFO
    EPILOG 6992
.pol:
    invoke msgbox, rsi, &s_pol_blocked, MB_OK|MB_ICONWARNING
    EPILOG 6992

; popup_at(hwnd, anchor id, menu) -> chosen command
popup_at:
    PROLOG 160
    mov rsi, rcx
    mov rbx, r8
    invoke GetDlgItem, rsi, rdx
    invoke GetWindowRect, rax, &rsp+96
    mov r12d, [rsp+96]
    mov r13d, [rsp+108]
    invoke TrackPopupMenu, rbx, TPM_RETURNCMD|TPM_RIGHTBUTTON, r12, r13, 0, rsi, 0
    mov esi, eax
    invoke DestroyMenu, rbx
    mov eax, esi
    EPILOG 160

; fold_delete(hwnd, fold)
fold_delete:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .ro
    test qword [g_policy], 1
    jnz .pol
    call CreatePopupMenu
    mov rdi, rax
    invoke AppendMenuW, rdi, MF_STRING, 3, &s_mk_l
    invoke AppendMenuW, rdi, MF_STRING, 4, &s_mk_r
    invoke AppendMenuW, rdi, MF_STRING, 5, &s_mk_b
    invoke popup_at, rsi, ID_BTN_DEL, rdi
    test eax, eax
    jz .d
    mov edi, eax
    invoke build_lists, rbx, rdi, &rsp+96, &rsp+104
    test eax, eax
    jz .none
    call fj_begin
    mov [rsp+112], rax
    invoke fj_lists, [rsp+112], [rsp+96], 0, 3
    invoke shfop, rsi, FO_DELETE, [rsp+96], 0
    invoke fj_commit, &rbx+F_UST, [rsp+112]
    invoke free, [rsp+96]
    invoke free, [rsp+104]
    invoke fold_start, rsi, rbx
.d:
    EPILOG 160
.none:
    invoke msgbox, rsi, &s_nosel, MB_OK|MB_ICONINFO
    EPILOG 160
.pol:
    invoke msgbox, rsi, &s_pol_blocked, MB_OK|MB_ICONWARNING
    EPILOG 160
.ro:
    invoke msgbox, rsi, &s_ro, MB_OK|MB_ICONINFO
    EPILOG 160

; ---------------------------------------------------------------- sync
; plan_push(fold, type, idx)
plan_push:
    PROLOG 64
    mov rbx, rcx
    mov esi, edx
    mov edi, r8d
    lea rcx, [rbx+F_PLAN]
    mov edx, 8
    call vec_push
    mov [rax], esi
    mov [rax+4], edi
    EPILOG 64

; plan_build(fold, mode 1 update, 2 mirror, 3 two-way, 4 the actions shown in the middle column)
;   Mode 4 is the two-way plan, except for items whose action was chosen by hand.
plan_build:
    PROLOG 64
    mov r12, rcx
    mov r13d, edx
    xor r14d, r14d
    cmp r13d, 4
    jne .m
    mov r13d, 3
    mov r14d, 1
.m:
    mov qword [r12+F_NPLAN], 0
    xor ebx, ebx
.l:
    cmp rbx, [r12+F_NENT]
    jae .del
    mov rax, rbx
    shl rax, 6
    add rax, [r12+F_ENTS]
    mov rdi, rax
    mov esi, [rdi+E_FLAGS]
    test esi, EF_ALT
    jnz .n
    and esi, EF_DIRA|EF_DIRB         ; nonzero = directory
    test r14d, r14d
    jz .nov
    mov ecx, [rdi+E_FLAGS]
    shr ecx, EF_ACTSH
    and ecx, 7
    jz .nov                          ; nothing chosen: as in the two-way plan
    cmp ecx, 3
    je .n                            ; skip
    mov eax, [rdi+E_STATUS]
    cmp ecx, 1
    je .toright
    cmp eax, ST_RONLY                ; chosen: to the left
    je .ronly
    cmp eax, ST_DIFF
    jne .n
    test esi, esi
    jnz .n
    mov edx, OP_COPYRL
    jmp .push
.toright:
    cmp eax, ST_LONLY
    je .lonly
    cmp eax, ST_DIFF
    jne .n
    test esi, esi
    jnz .n
    mov edx, OP_COPYLR
    jmp .push
.nov:
    mov eax, [rdi+E_STATUS]
    cmp eax, ST_LONLY
    je .lonly
    cmp eax, ST_RONLY
    je .ronly
    cmp eax, ST_DIFF
    je .diff
    jmp .n
.lonly:
    mov edx, OP_COPYLR
    test esi, esi
    jz .push
    mov edx, OP_MKDIRR
    jmp .push
.ronly:
    cmp r13d, 3
    jne .n
    mov edx, OP_COPYRL
    test esi, esi
    jz .push
    mov edx, OP_MKDIRL
    jmp .push
.diff:
    test esi, esi
    jnz .n
    mov edx, OP_COPYLR
    cmp r13d, 3
    jne .push
    mov rax, [rdi+E_MTA]
    cmp rax, [rdi+E_MTB]
    ja .push
    je .n
    mov edx, OP_COPYRL
.push:
    invoke plan_push, r12, rdx, rbx
.n:
    inc rbx
    jmp .l
.del:
    cmp r13d, 2
    jne .d
    mov rbx, [r12+F_NENT]
.dl:
    test rbx, rbx
    jz .d
    dec rbx
    mov rax, rbx
    shl rax, 6
    add rax, [r12+F_ENTS]
    mov rdi, rax
    cmp dword [rdi+E_STATUS], ST_RONLY
    jne .dl
    mov edx, OP_DELR
    test dword [rdi+E_FLAGS], EF_DIRA|EF_DIRB
    jz .dp
    mov edx, OP_RMDIRR
.dp:
    invoke plan_push, r12, rdx, rbx
    jmp .dl
.d:
    EPILOG 64

; ensure_parent(rcx=path): create the folder that will hold this file
ensure_parent:
    PROLOG 64
    mov rsi, rcx
    invoke wcsrchr, rsi, 0x5C
    test rax, rax
    jz .d
    mov rdi, rax
    mov word [rdi], 0
    invoke SHCreateDirectoryExW, 0, rsi, 0
    mov word [rdi], 0x5C
.d:
    EPILOG 64

; copy_file(fold, src, dst) -> eax ok
copy_file:
    PROLOG 2400
    mov r12, rcx
    mov rsi, rdx
    mov rdi, r8
    invoke fj_dirs, [r12+F_JRN], rdi, 1
    invoke ensure_parent, rdi
    test dword [r12+F_OPT], 0x10
    jz .copy
    invoke GetFileAttributesW, rdi
    cmp eax, -1
    je .copy
    invoke _snwprintf, &rsp+96, 1100, &f_bak, rdi
    invoke fj_file, [r12+F_JRN], &rsp+96
    invoke CopyFileW, rdi, &rsp+96, 0
.copy:
    invoke fj_file, [r12+F_JRN], rdi
    invoke CopyFileW, rsi, rdi, 0
    mov ebx, eax
    test eax, eax
    jz .cfd
    test dword [r12+F_OPT], 0x100
    jz .cfd
    invoke copy_acl, rsi, rdi
.cfd:
    mov eax, ebx
    EPILOG 2400

; op_exec(fold, type, entidx) -> eax 1 ok
op_exec:
    PROLOG 6704
    mov r12, rcx
    mov r13d, edx
    mov rax, r8
    shl rax, 6
    add rax, [r12+F_ENTS]
    mov r14, [rax+E_NAME]
    invoke _snwprintf, &rsp+96, 1100, &f_path, &r12+F_ROOTA, r14
    invoke _snwprintf, &rsp+2296, 1100, &f_path, &r12+F_ROOTB, r14
    cmp r13d, OP_COPYLR
    je .clr
    cmp r13d, OP_COPYRL
    je .crl
    cmp r13d, OP_MKDIRR
    je .mkr
    cmp r13d, OP_MKDIRL
    je .mkl
    cmp r13d, OP_DELR
    je .delr
    invoke fj_rmdir, [r12+F_JRN], &rsp+2296
    invoke RemoveDirectoryW, &rsp+2296
    jmp .ret
.clr:
    invoke copy_file, r12, &rsp+96, &rsp+2296
    jmp .ret
.crl:
    invoke copy_file, r12, &rsp+2296, &rsp+96
    jmp .ret
.mkr:
    invoke fj_dirs, [r12+F_JRN], &rsp+2296, 0
    invoke SHCreateDirectoryExW, 0, &rsp+2296, 0
    jmp .dirres
.mkl:
    invoke fj_dirs, [r12+F_JRN], &rsp+96, 0
    invoke SHCreateDirectoryExW, 0, &rsp+96, 0
.dirres:
    mov ecx, 1
    test eax, eax
    jz .dok
    cmp eax, 183
    je .dok
    xor ecx, ecx
.dok:
    mov eax, ecx
    jmp .ret
.delr:
    invoke fj_file, [r12+F_JRN], &rsp+2296
    invoke wcscpy, &rsp+4496, &rsp+2296
    invoke wcslen, &rsp+4496
    mov word [rsp+4496+rax*2+2], 0
    invoke shfop, 0, FO_DELETE, &rsp+4496, 0
    test eax, eax
    setz al
    movzx eax, al
.ret:
    EPILOG 6704

; sync_thread(fold)
sync_thread:
    PROLOG 64
    mov rbx, rcx
    xor esi, esi
    xor edi, edi                     ; errors
.l:
    cmp rsi, [rbx+F_NPLAN]
    jae .fin
    cmp byte [rbx+F_CANCEL], 0
    jne .fin
    cmp qword [rbx+F_PAUSED], 0
    je .active
    invoke Sleep, 50
    jmp .l
.active:
    mov rax, rsi
    shl rax, 3
    add rax, [rbx+F_PLAN]
    mov edx, [rax]
    mov r8d, [rax+4]
    invoke op_exec, rbx, rdx, r8
    test eax, eax
    jnz .ok
    inc edi
.ok:
    inc rsi
    mov rax, rsi
    test al, 3
    jnz .l
    invoke PostMessageW, [rbx+F_HVIEW], BC_SYNCPROG, rsi, [rbx+F_NPLAN]
    jmp .l
.fin:
    mov qword [rbx+F_SYNCRUN], 0
    invoke PostMessageW, [rbx+F_HVIEW], BC_SYNCDONE, rdi, 0
    xor eax, eax
    EPILOG 64

; fold_sync(hwnd, fold)
fold_sync:
    PROLOG 704
    mov rsi, rcx
    mov rbx, rdx
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .ro
    test qword [g_policy], 1
    jnz .pol
    cmp dword [rbx+F_SCANERR], 0
    jne .scanerr
    cmp qword [rbx+F_NENT], 0
    je .norun
    cmp qword [rbx+F_RUNNING], 0
    jne .busy
    cmp qword [rbx+F_SYNCRUN], 0
    jne .busy
    call CreatePopupMenu
    mov rdi, rax
    invoke AppendMenuW, rdi, MF_STRING, 1, &s_sy1
    invoke AppendMenuW, rdi, MF_STRING, 2, &s_sy2
    invoke AppendMenuW, rdi, MF_STRING, 3, &s_sy3
    invoke AppendMenuW, rdi, MF_STRING, 5, &s_sy5
    invoke AppendMenuW, rdi, MF_SEPARATOR, 0, 0
    mov r8d, MF_STRING
    test dword [rbx+F_OPT], 0x10
    jz .nochk
    or r8d, MF_CHECKED
.nochk:
    invoke AppendMenuW, rdi, r8, 4, &s_sy4
    invoke popup_at, rsi, ID_BTN_SYNC, rdi
    test eax, eax
    jz .d
    cmp eax, 5
    jne .not5
    mov eax, 4                       ; the actions of the middle column
    jmp .plan
.not5:
    cmp eax, 4
    jne .plan
    xor dword [rbx+F_OPT], 0x10
    jmp .d
.plan:
    mov edi, eax
    invoke plan_build, rbx, rdi
    ; count: copies, mkdirs, deletes
    xor r12d, r12d
    xor r13d, r13d
    xor r14d, r14d
    xor ecx, ecx
.cnt:
    cmp rcx, [rbx+F_NPLAN]
    jae .cdone
    mov rax, rcx
    shl rax, 3
    add rax, [rbx+F_PLAN]
    mov eax, [rax]
    cmp eax, OP_COPYLR
    je .c1
    cmp eax, OP_COPYRL
    je .c1
    cmp eax, OP_MKDIRR
    je .c2
    cmp eax, OP_MKDIRL
    je .c2
    inc r14d
    jmp .cn
.c1:
    inc r12d
    jmp .cn
.c2:
    inc r13d
.cn:
    inc rcx
    jmp .cnt
.cdone:
    cmp qword [rbx+F_NPLAN], 0
    je .nothing
    invoke sync_preview, rsi, rbx
    test eax, eax
    jz .d
    call fj_begin                    ; what the sync changes can be taken back (Ctrl+Z)
    mov [rbx+F_JRN], rax
    mov byte [rbx+F_CANCEL], 0
    mov qword [rbx+F_SYNCRUN], 1
    mov qword [rbx+F_PAUSED], 0
    invoke CreateThread, 0, 0, &sync_thread, rbx, 0, 0
    mov [rbx+F_SYNCTHR], rax
.d:
    EPILOG 704
.nothing:
    invoke set_status, &s_sync_none
    EPILOG 704
.pol:
    invoke msgbox, rsi, &s_pol_blocked, MB_OK|MB_ICONWARNING
    EPILOG 704
.scanerr:
    invoke msgbox, rsi, &s_scanerr, MB_OK|MB_ICONWARNING
    EPILOG 704
.ro:
    invoke msgbox, rsi, &s_ro, MB_OK|MB_ICONINFO
    EPILOG 704
.norun:
    invoke msgbox, rsi, &s_norun, MB_OK|MB_ICONINFO
    EPILOG 704
.busy:
    invoke msgbox, rsi, &s_busy, MB_OK|MB_ICONWARNING
    EPILOG 704

; fold_pause(hwnd, fold)
fold_pause:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+F_SYNCRUN], 0
    je .d
    xor qword [rbx+F_PAUSED], 1
    cmp qword [rbx+F_PAUSED], 0
    je .res
    invoke GetDlgItem, rsi, ID_BTN_PAUSE
    invoke SetWindowTextW, rax, &s_b_resume
    jmp .d
.res:
    invoke GetDlgItem, rsi, ID_BTN_PAUSE
    invoke SetWindowTextW, rax, &s_b_pause
.d:
    EPILOG 64


; fold_batch(hwnd, fold): command-line automation after a scan (/report, /sync, /exit)
fold_batch:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    test qword [g_batch], 1
    jz .nr
    and qword [g_batch], ~1
    invoke fold_report_any, rbx, &g_rep_path
    test eax, eax
    jnz .nr
    mov qword [g_exitcode], 2
.nr:
    test qword [g_batch], 8
    jz .nsn
    and qword [g_batch], ~8
    invoke fold_snapshot_path, rbx, &g_snap_path
    test eax, eax
    jnz .nsn
    mov qword [g_exitcode], 2
.nsn:
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .ns
    test qword [g_batch], 2
    jz .ns
    cmp dword [rbx+F_SCANERR], 0
    jne .scanfail
    mov rax, [g_batch]
    shr rax, 8
    and eax, 0xFF
    mov edi, eax
    and qword [g_batch], ~2
    invoke plan_build, rbx, rdi
    cmp qword [rbx+F_NPLAN], 0
    je .ns
    mov byte [rbx+F_CANCEL], 0
    mov qword [rbx+F_SYNCRUN], 1
    mov qword [rbx+F_PAUSED], 0
    invoke CreateThread, 0, 0, &sync_thread, rbx, 0, 0
    mov [rbx+F_SYNCTHR], rax
    EPILOG 64
.scanfail:
    mov qword [g_exitcode], 2
    and qword [g_batch], ~2
    invoke set_status, &s_scanerr_short
.ns:
    test qword [g_batch], 4
    jz .d
    xor ecx, ecx
    mov rdx, [rbx+F_ENTS]
.checkdiff:
    cmp rcx, [rbx+F_NENT]
    jae .equal
    cmp dword [rdx+E_STATUS], ST_SAME
    jne .diff
    add rdx, E_SIZE
    inc rcx
    jmp .checkdiff
.equal:
    xor eax, eax
    jmp .set
.diff:
    mov eax, 1
.set:
    cmp qword [g_exitcode], 2
    je .post
    mov [g_exitcode], rax
.post:
    invoke PostMessageW, [g_hframe], WM_CLOSE, 0, 0
.d:
    EPILOG 64

; ---------------------------------------------------------------- HTML report
; fold_report(hwnd, fold) lives in folder_exp.inc (format menu: HTML / Excel / Word)

; fold_report_path(fold, path): build the HTML report and write it (UTF-8 with BOM)
fold_report_path:
    PROLOG 7088
    mov rbx, rcx
    mov rdi, rdx
    ; folder for the per-file reports: <report without extension>_files
    invoke wcscpy, &rsp+3820, rdi
    invoke wcsrchr, &rsp+3820, 0x5C
    mov r14, rax
    invoke wcsrchr, &rsp+3820, '.'
    test rax, rax
    jz .noext
    cmp r14, rax
    jae .noext
    mov word [rax], 0
.noext:
    invoke wcscat, &rsp+3820, &s_files
    ; link prefix "<name>_files/"
    lea rax, [rsp+3820]
    invoke wcsrchr, rax, 0x5C
    lea rdx, [rsp+3820]
    test rax, rax
    jz .pfx
    lea rdx, [rax+2]
.pfx:
    invoke wcscpy, &rsp+400, rdx
    invoke wcscat, &rsp+400, &s_slash2
    xor r15d, r15d                   ; folder created
    invoke sb_init, &rsp+96
    invoke sb_append, &rsp+96, &s_rep_head
    invoke sb_append_esc, &rsp+96, &rbx+F_ROOTA
    invoke sb_append, &rsp+96, &s_rep_mid
    invoke sb_append_esc, &rsp+96, &rbx+F_ROOTB
    invoke sb_append, &rsp+96, &s_rep_tab
    xor r12d, r12d
.l:
    cmp r12, [rbx+F_NENT]
    jae .fin
    mov rax, r12
    shl rax, 6
    add rax, [rbx+F_ENTS]
    mov r13, rax
    mov eax, [r13+E_MASK]
    test eax, [rbx+F_FMASK]
    jz .n
    ; only entries whose own status passes the filter
    mov ecx, [r13+E_STATUS]
    mov eax, 1
    shl eax, cl
    test eax, [rbx+F_FMASK]
    jz .n
    invoke _snwprintf, &rsp+136, 8, &f_one, [r13+E_STATUS]
    invoke sb_append, &rsp+96, &s_tr1
    invoke sb_append, &rsp+96, &rsp+136
    invoke sb_append, &rsp+96, &s_tr2
    ; FC-05: link to a per-file detail report for changed text files
    xor r14d, r14d
    cmp dword [r13+E_STATUS], ST_DIFF
    jne .plainname
    test dword [r13+E_FLAGS], EF_DIRA|EF_DIRB
    jnz .plainname
    mov eax, [r13+E_FLAGS]
    and eax, EF_EXA|EF_EXB
    cmp eax, EF_EXA|EF_EXB
    jne .plainname
    cmp qword [rbx+F_KIND], 0
    jne .plainname
    cmp qword [rbx+F_KIND+8], 0
    jne .plainname
    invoke _snwprintf, &rsp+1700, 529, &f_path, &rbx+F_ROOTA, [r13+E_NAME]
    invoke alt_bpath, rbx, [r13+E_NAME]
    invoke _snwprintf, &rsp+2760, 529, &f_path, &rbx+F_ROOTB, rax
    test r15d, r15d
    jnz .dirok
    invoke CreateDirectoryW, &rsp+3820, 0
    mov r15d, 1
.dirok:
    invoke _snwprintf, &rsp+4880, 1099, &f_detail, &rsp+3820, r12
    invoke fold_filereport, &rsp+1700, &rsp+2760, &rsp+4880
    mov r14d, eax
    cmp eax, -1
    jne .plainname
    or r15d, 2
    xor r14d, r14d
.plainname:
    test r14d, r14d
    jz .plain2
    invoke sb_append, &rsp+96, &s_a1
    invoke sb_append, &rsp+96, &rsp+400
    invoke _snwprintf, &rsp+136, 12, &f_one, r12
    invoke sb_append, &rsp+96, &rsp+136
    invoke sb_append, &rsp+96, &s_a2
    invoke sb_append_esc, &rsp+96, [r13+E_NAME]
    invoke sb_append, &rsp+96, &s_a3
    jmp .afterlink
.plain2:
    invoke sb_append_esc, &rsp+96, [r13+E_NAME]
.afterlink:
    invoke sb_append, &rsp+96, &s_td
    mov eax, [r13+E_STATUS]
    lea rcx, [st_text]
    mov r10, [rcx+rax*8]
    invoke sb_append, &rsp+96, r10
    invoke sb_append, &rsp+96, &s_tdn
    mov word [rsp+136], 0
    test dword [r13+E_FLAGS], EF_EXA
    jz .sa
    test dword [r13+E_FLAGS], EF_DIRA
    jnz .sa
    invoke fmt_size, &rsp+136, [r13+E_SIZEA]
.sa:
    invoke sb_append, &rsp+96, &rsp+136
    invoke sb_append, &rsp+96, &s_td
    mov word [rsp+136], 0
    test dword [r13+E_FLAGS], EF_EXA
    jz .ta
    invoke fmt_time, &rsp+136, [r13+E_MTA]
.ta:
    invoke sb_append, &rsp+96, &rsp+136
    invoke sb_append, &rsp+96, &s_tdn
    mov word [rsp+136], 0
    test dword [r13+E_FLAGS], EF_EXB
    jz .sb
    test dword [r13+E_FLAGS], EF_DIRB
    jnz .sb
    invoke fmt_size, &rsp+136, [r13+E_SIZEB]
.sb:
    invoke sb_append, &rsp+96, &rsp+136
    invoke sb_append, &rsp+96, &s_td
    mov word [rsp+136], 0
    test dword [r13+E_FLAGS], EF_EXB
    jz .tb
    invoke fmt_time, &rsp+136, [r13+E_MTB]
.tb:
    invoke sb_append, &rsp+96, &rsp+136
    invoke sb_append, &rsp+96, &s_trend
.n:
    inc r12
    jmp .l
.fin:
    invoke sb_append, &rsp+96, &s_rep_foot
    invoke write_utf8, rdi, [rsp+96], [rsp+104], 1
    mov ebp, eax
    test r15d, 2
    jz .reportresult
    xor ebp, ebp
.reportresult:
    invoke sb_free, &rsp+96
    test ebp, ebp
    jz .reportfail
    invoke set_status, &s_report_ok
    mov eax, 1
    EPILOG 7088
.reportfail:
    invoke set_status, &s_report_fail
    xor eax, eax
    EPILOG 7088

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

%macro PLACEH 3                      ; like PLACE; the height of a combo box includes its drop-down list
    mov ecx, %2
    call scl
    mov ebx, eax
    cmp r12d, r14d
    je %%fit
    lea eax, [r12+rbx]
    cmp eax, [rsp+104]
    jbe %%fit
    mov r12d, r14d
    lea r13d, [r13+r15]
    add r13d, r14d
%%fit:
    mov ecx, %3
    call scl
    mov [rsp+136], rax
    MVID rsi, %1, r12, r13, rbx, [rsp+136]
    lea r12d, [r12+rbx]
    add r12d, r14d
%endmacro

%macro PVP 2                         ; a control of the preview toolbar, from the left
    mov ecx, %2
    call scl
    mov ebx, eax
    MVID rsi, %1, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
%endmacro

%macro PVR 2                         ; the same, from the right edge (the cursor is in [rsp+160])
    mov ecx, %2
    call scl
    mov ebx, eax
    mov rax, [rsp+160]
    sub eax, ebx
    mov [rsp+160], rax
    MVID rsi, %1, [rsp+160], r13, rbx, r15
    mov rax, [rsp+160]
    sub eax, r14d
    mov [rsp+160], rax
%endmacro

%macro SEP 0                         ; a little more room between two groups of buttons
    add r12d, r14d
    add r12d, r14d
%endmacro

%macro PANEL 2                       ; a control of the options row: placed while the row is open, hidden otherwise
    cmp qword [rsp+128], 0
    je %%hide
    PLACE %1, %2
    invoke GetDlgItem, rsi, %1
    invoke ShowWindow, rax, 5
    jmp %%done
%%hide:
    invoke GetDlgItem, rsi, %1
    invoke ShowWindow, rax, 0
%%done:
%endmacro

; fold_layout(hwnd): path row, then a toolbar of two rows - what to compare and what to show, what to do
; with the selection - and, when the gear button is pressed, the two rows of less used options
fold_layout:
    PROLOG 192
    mov rsi, rcx
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, 4
    call scl
    mov r14d, eax                    ; gap
    mov ecx, 26
    call scl
    mov r15d, eax                    ; row height
    invoke get_chk, rsi, ID_BTN_OPTS
    mov [rsp+128], rax               ; the options rows are open
    mov edx, [rsp+104]               ; client width
    invoke pb_layout, rsi, rdx, r14
    mov r13d, eax                    ; y of the toolbar
    mov r12d, r14d
    PLACE ID_COMPARE, 96
    PLACE ID_STOP, 34
    SEP
    PLACE ID_CHK_LONLY, 36
    PLACE ID_CHK_DIFF, 36
    PLACE ID_CHK_SAME, 36
    PLACE ID_CHK_RONLY, 36
    SEP
    PLACE ID_LBL_CRIT, 76
    PLACEH ID_CRIT, 190, 240
    PLACE ID_BTN_OPTS, 40
    PLACE ID_BTN_PVTOG, 40
    ; the name filter at the right end of this row; on a row of its own when the window is too narrow
    mov ecx, 52
    call scl
    mov [rsp+144], rax               ; label width
    mov ecx, 200
    call scl
    mov [rsp+152], rax               ; field width
    mov eax, [rsp+104]
    sub eax, r14d
    sub eax, [rsp+152]
    mov [rsp+160], rax               ; x of the field
    sub eax, r14d
    sub eax, [rsp+144]               ; x of the label
    cmp eax, r12d
    jge .fgo
    mov r12d, r14d                   ; no room: a new row, from the left
    lea r13d, [r13+r15]
    add r13d, r14d
    mov eax, r12d
    add eax, [rsp+144]
    add eax, r14d
    mov [rsp+160], rax
    mov eax, r12d
.fgo:
    mov [rsp+168], rax               ; x of the label (rax does not survive the call that finds the control)
    MVID rsi, ID_LBL_FILTER, [rsp+168], r13, [rsp+144], r15
    MVID rsi, ID_FILTER, [rsp+160], r13, [rsp+152], r15
    ; what to do with the selection
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_BTN_COPYLR, 40
    PLACE ID_BTN_COPYRL, 40
    PLACE ID_BTN_MOVELR, 40
    PLACE ID_BTN_MOVERL, 40
    SEP
    PLACE ID_BTN_STAMPLR, 44
    PLACE ID_BTN_STAMPRL, 44
    SEP
    PLACE ID_BTN_DEL, 40
    PLACE ID_BTN_SYNC, 40
    PLACE ID_BTN_PAUSE, 40
    SEP
    PLACE ID_BTN_UNDO, 40
    PLACE ID_BTN_REDO, 40
    SEP
    PLACE ID_BTN_REPORT, 96
    PLACE ID_BTN_SNAP, 40
    SEP
    PLACE ID_BTN_EXPAND, 40
    PLACE ID_BTN_COLLAP, 40
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    ; the options rows
    PANEL ID_CHK_SIZE, 60
    PANEL ID_CHK_MTIME, 90
    PANEL ID_CHK_CONT, 110
    PANEL ID_CHK_LINKS, 110
    PANEL ID_CHK_CRC, 70
    PANEL ID_CHK_ATTR, 60
    PANEL ID_LBL_EXCL, 40
    PANEL ID_EDIT_EXCL, 240
    PANEL ID_BTN_EXCL, 84
    cmp qword [rsp+128], 0
    je .norow2
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
.norow2:
    PANEL ID_LBL_FSZ, 170
    PANEL ID_EDIT_FMIN, 70
    PANEL ID_LBL_FTO, 16
    PANEL ID_EDIT_FMAX, 70
    PANEL ID_LBL_FDAY, 130
    PANEL ID_EDIT_FDAYS, 50
    PANEL ID_LBL_ATTRX, 190
    PANEL ID_EDIT_ATTRX, 70
    PANEL ID_CHK_ASFILE, 250
    PANEL ID_CHK_ACL, 220
    PANEL ID_CHK_LOOSE, 270
    PANEL ID_CHK_AUTOREF, 130
    PANEL ID_LBL_MASK, 170
    PANEL ID_EDIT_MASK, 200
    cmp qword [rsp+128], 0
    je .norow3
    lea r13d, [r13+r15]
    add r13d, r14d                   ; y of the list
.norow3:
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    xor ecx, ecx
    test rax, rax
    jz .nofold
    mov rcx, [rax+SS_DATA]
.nofold:
    mov [rsp+184], rcx               ; the state of the folder view (0 while it is being created)
    mov eax, [rsp+108]
    sub eax, r13d
    jns .av
    xor eax, eax
.av:
    mov [rsp+176], rax               ; the height under the toolbar
    xor edi, edi                     ; the height of the preview
    invoke get_chk, rsi, ID_BTN_PVTOG
    test eax, eax
    jz .nopv
    mov rcx, [rsp+184]
    test rcx, rcx
    jz .nopv
    mov edx, 100
    sub edx, [rcx+F_SPLIT]
    mov eax, [rsp+176]
    imul eax, edx
    xor edx, edx
    mov ecx, 100
    div ecx
    mov edi, eax
.nopv:
    mov ebp, [rsp+176]
    sub ebp, edi
    sub ebp, r15d                    ; the hint line takes the last row of the upper part
    jns .h
    xor ebp, ebp
.h:
    mov ebx, [rsp+104]
    MVID rsi, ID_LIST, 0, r13, rbx, rbp
    lea eax, [r13+rbp]
    mov [rsp+136], rax
    sub ebx, r14d
    sub ebx, r14d
    MVID rsi, ID_LBL_HINT, r14, [rsp+136], rbx, r15
    mov rcx, [rsp+184]
    test rcx, rcx
    jz .out
    mov eax, r13d
    mov [rcx+F_LTOP], rax
    mov eax, [rsp+176]
    mov [rcx+F_AVAIL], rax
    mov qword [rcx+F_SPH], 0
    test edi, edi
    jz .vis
    ; the preview: a strip to drag, a toolbar row, the view
    mov eax, [rsp+176]
    sub eax, edi
    add eax, r13d
    mov r12d, eax                    ; y of the strip
    mov [rcx+F_SPY], rax
    mov ecx, 6
    call scl
    mov ebx, eax                     ; its height
    mov rcx, [rsp+184]
    mov [rcx+F_SPH], rbx
    add r12d, ebx
    mov r13d, r12d                   ; y of the toolbar row
    mov r12d, r14d
    PVP ID_PV_PREV, 34
    PVP ID_PV_NEXT, 34
    SEP
    PVP ID_PV_SRC, 34
    SEP
    PVP ID_PV_PF, 34
    PVP ID_PV_NF, 34
    SEP
    PVP ID_PV_CLR, 44
    PVP ID_PV_CRL, 44
    PVP ID_PV_SAVE, 34
    SEP
    PVP ID_PV_FOLD, 34
    SEP
    PVP ID_PV_LBL, 130
    mov eax, [rsp+104]
    sub eax, r14d
    mov [rsp+160], rax
    PVR ID_PV_SET, 34
    PVR ID_PV_UNI, 64
    PVR ID_PV_SIDE, 64
    ; the view and the message take the rest
    mov eax, r13d
    add eax, r15d
    add eax, r14d
    mov [rsp+136], rax               ; y of the view
    mov eax, [rsp+108]
    sub eax, [rsp+136]
    jns .vh
    xor eax, eax
.vh:
    mov [rsp+144], rax
    mov ebx, [rsp+104]
    MVID rsi, ID_LBL_PVMSG, 0, [rsp+136], rbx, [rsp+144]
    mov rcx, [rsp+184]
    mov rcx, [rcx+F_PVVIEW]
    test rcx, rcx
    jz .vis
    invoke MoveWindow, rcx, 0, [rsp+136], rbx, [rsp+144], 1
.vis:
    mov rdx, [rsp+184]
    invoke fold_pv_vis, rsi, rdx
.out:
    EPILOG 192

; ---------------------------------------------------------------- window procedure
fold_proc:
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
    cmp edi, WM_TIMER
    je .timer
    cmp edi, 0x201                   ; WM_LBUTTONDOWN
    je .lbd
    cmp edi, 0x200                   ; WM_MOUSEMOVE
    je .mm
    cmp edi, 0x202                   ; WM_LBUTTONUP
    je .lbu
    cmp edi, 0x20                    ; WM_SETCURSOR
    je .setcur
    cmp edi, BC_ISDIRTY
    je .isdirty
    cmp edi, BC_SAVE
    je .savepv
    cmp edi, BC_NEXTDIFF
    je .nextdiff
    cmp edi, BC_PREVDIFF
    je .prevdiff
    cmp edi, BC_RUN
    je .run
    cmp edi, BC_STOPPING
    je .stopping
    cmp edi, BC_CANCEL
    je .cancel
    cmp edi, BC_COLLECT
    je .collect
    cmp edi, BC_PROGRESS
    je .progress
    cmp edi, BC_DONE
    je .done
    cmp edi, BC_SYNCPROG
    je .syncprog
    cmp edi, BC_SYNCDONE
    je .syncdone
    cmp edi, BC_UNDO
    je .undo
    cmp edi, BC_REDO
    je .undo
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
    invoke calloc, 1, F_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    mov [r15+F_HVIEW], rsi
    mov [r15+F_SESS], r14
    mov dword [r15+F_FMASK], 0x1F
    mov dword [r15+F_OPT], 3
    invoke malloc, 65536
    mov [r15+F_BUFA], rax
    invoke malloc, 65536
    mov [r15+F_BUFB], rax
    invoke pb_create, rsi
    invoke glyph_only, rsi, ID_BROWSE_A, &g_foldopen        ; a folder icon instead of the dots
    invoke glyph_only, rsi, ID_BROWSE_B, &g_foldopen
    invoke mk_btn, rsi, &s_stop, ID_STOP
    invoke mk_chk, rsi, &s_c_size, ID_CHK_SIZE, 1
    invoke mk_chk, rsi, &s_c_mtime, ID_CHK_MTIME, 1
    invoke mk_chk, rsi, &s_c_cont, ID_CHK_CONT, 0
    invoke mk_chk, rsi, &s_c_links, ID_CHK_LINKS, 0
    invoke mk_chk, rsi, &s_c_crc, ID_CHK_CRC, 0
    invoke mk_chk, rsi, &s_c_attr, ID_CHK_ATTR, 0
    invoke mk_lbl, rsi, &s_excl, ID_LBL_EXCL
    invoke mk_edit, rsi, ID_EDIT_EXCL
    invoke mk_btn, rsi, &s_b_excl, ID_BTN_EXCL
    invoke mk_lbl, rsi, &s_fsz, ID_LBL_FSZ
    invoke mk_edit, rsi, ID_EDIT_FMIN
    invoke mk_lbl, rsi, &s_fto, ID_LBL_FTO
    invoke mk_edit, rsi, ID_EDIT_FMAX
    invoke mk_lbl, rsi, &s_fday, ID_LBL_FDAY
    invoke mk_edit, rsi, ID_EDIT_FDAYS
    invoke mk_lbl, rsi, &s_lbl_attrx, ID_LBL_ATTRX
    invoke mk_edit, rsi, ID_EDIT_ATTRX
    invoke mk_chk, rsi, &s_c_asfile, ID_CHK_ASFILE, 0
    invoke mk_chk, rsi, &s_c_acl, ID_CHK_ACL, 0
    invoke mk_chk, rsi, &s_c_loose, ID_CHK_LOOSE, 0
    invoke mk_chk, rsi, &s_c_autoref, ID_CHK_AUTOREF, 0
    invoke mk_lbl, rsi, &s_l_mask, ID_LBL_MASK
    invoke mk_edit, rsi, ID_EDIT_MASK
    invoke mk_chk, rsi, &s_f_same, ID_CHK_SAME, 1
    invoke mk_chk, rsi, &s_f_diff, ID_CHK_DIFF, 1
    invoke mk_chk, rsi, &s_f_lonly, ID_CHK_LONLY, 1
    invoke mk_chk, rsi, &s_f_ronly, ID_CHK_RONLY, 1
    mov ebp, ID_CHK_SAME
.segs:
    invoke GetDlgItem, rsi, rbp
    mov r13, rax
    invoke GetWindowLongPtrW, r13, -16
    or eax, 0x1000
    invoke SetWindowLongPtrW, r13, -16, rax
    invoke InvalidateRect, r13, 0, 1
    inc ebp
    cmp ebp, ID_CHK_RONLY+1
    jb .segs
    invoke mk_btn, rsi, &s_b_copylr, ID_BTN_COPYLR
    invoke mk_btn, rsi, &s_b_copyrl, ID_BTN_COPYRL
    invoke mk_btn, rsi, &s_b_movelr, ID_BTN_MOVELR
    invoke mk_btn, rsi, &s_b_moverl, ID_BTN_MOVERL
    invoke mk_btn, rsi, &s_b_stamplr, ID_BTN_STAMPLR
    invoke mk_btn, rsi, &s_b_stamprl, ID_BTN_STAMPRL
    invoke mk_btn, rsi, &s_b_del, ID_BTN_DEL
    invoke mk_btn, rsi, &s_b_sync, ID_BTN_SYNC
    invoke mk_btn, rsi, &s_b_pause, ID_BTN_PAUSE
    invoke mk_btn, rsi, &s_b_report, ID_BTN_REPORT     ; a labelled button: an icon alone was not found as "export"
    invoke tip_add, rax, &s_tip_report
    invoke mk_btn, rsi, &s_b_snap, ID_BTN_SNAP
    invoke mk_btn, rsi, &s_b_expand, ID_BTN_EXPAND
    invoke mk_btn, rsi, &s_b_collap, ID_BTN_COLLAP
    invoke icon_set, rsi, ID_BTN_COPYLR, &g_copyr, &s_b_copylr
    invoke icon_set, rsi, ID_BTN_COPYRL, &g_copyl, &s_b_copyrl
    invoke icon_set, rsi, ID_BTN_MOVELR, &g_mover, &s_b_movelr
    invoke icon_set, rsi, ID_BTN_MOVERL, &g_movel, &s_b_moverl
    invoke icon_set, rsi, ID_BTN_DEL, &g_del, &s_b_del
    invoke icon_set, rsi, ID_BTN_SYNC, &g_sync, &s_b_sync
    invoke icon_set, rsi, ID_BTN_PAUSE, &g_pause, &s_b_pause
    invoke mk_btn, rsi, &s_b_undo, ID_BTN_UNDO
    invoke mk_btn, rsi, &s_b_redo, ID_BTN_REDO
    invoke icon_set, rsi, ID_BTN_UNDO, &g_undo, &s_b_undo
    invoke icon_set, rsi, ID_BTN_REDO, &g_redo, &s_b_redo
    invoke icon_set, rsi, ID_BTN_SNAP, &g_snap, &s_b_snap
    invoke icon_set, rsi, ID_BTN_EXPAND, &g_expand, &s_b_expand
    invoke icon_set, rsi, ID_BTN_COLLAP, &g_collapse, &s_b_collap
    invoke icon_set, rsi, ID_STOP, &g_stop, &tt_folder_0
    invoke icon_set, rsi, ID_BTN_STAMPLR, &g_stamplr, &tt_folder_21
    invoke icon_set, rsi, ID_BTN_STAMPRL, &g_stamprl, &tt_folder_22
    ; compare-by box, the gear that opens the options rows, the name filter
    invoke mk_lbl, rsi, &s_l_crit, ID_LBL_CRIT
    invoke mk, rsi, &cn_combo, &s_empty, CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP, ID_CRIT
    mov rbx, rax
    xor ebp, ebp
.cr:
    lea rax, [cr_names]
    mov r13, [rax+rbp*8]
    invoke SendMessageW, rbx, CB_ADDSTRING, 0, r13
    inc ebp
    cmp ebp, CR_N+1
    jb .cr
    invoke mk_chk, rsi, &s_empty, ID_BTN_OPTS, 0
    mov r13, rax
    invoke GetWindowLongPtrW, r13, -16
    or eax, 0x1000                   ; BS_PUSHLIKE: a button that stays pressed while the rows are open
    invoke SetWindowLongPtrW, r13, -16, rax
    invoke glyph_only, rsi, ID_BTN_OPTS, &g_opts
    invoke mk_lbl, rsi, &s_l_filter, ID_LBL_FILTER
    invoke mk_edit, rsi, ID_FILTER
    invoke SendMessageW, rax, 0x1501, 1, &s_cue_filter      ; EM_SETCUEBANNER
    invoke tips_apply, rsi, &tt_folder
    invoke mk, rsi, &cn_list, 0, LVS_REPORT|LVS_OWNERDATA|LVS_SHOWSELALWAYS|WS_TABSTOP|WS_BORDER, ID_LIST
    mov [r15+F_HLIST], rax
    mov rbx, rax
    invoke SendMessageW, rbx, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER
    invoke lv_dark_header, rbx
    invoke SHGetFileInfoW, &s_x, 0x80, &ico_sfi, 696, 0x4011      ; the shell's small icons: the list of the system
    mov [g_sysil], rax
    invoke SendMessageW, rbx, LVM_SETIMAGELIST, 1, rax          ; LVSIL_SMALL
    invoke mk_lbl, rsi, &s_hint, ID_LBL_HINT
    invoke fold_pv_create, rsi, r15
    xor ebp, ebp
.col:
    cmp ebp, 8
    jae .coldone
    xor eax, eax
    mov [rsp+96], rax
    mov [rsp+104], rax
    mov [rsp+112], rax
    mov [rsp+120], rax
    mov [rsp+128], rax
    mov dword [rsp+96], LVCF_FMT|LVCF_WIDTH|LVCF_TEXT
    lea rax, [col_fmt]
    mov eax, [rax+rbp*4]
    mov [rsp+100], eax
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
    invoke pb_load, rsi, r14
    invoke fold_opts_apply, rsi, r14, r15
    invoke fold_crit_sync, rsi
    invoke SetTimer, rsi, 52, 500, 0        ; keeps "differences N" of the preview up to date
    invoke fold_theme, r15
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    test rax, rax
    jz .erdone
    mov rax, [rax+SS_DATA]
    test rax, rax
    jz .erdone
    cmp qword [rax+F_SPH], 0
    je .erdone
    mov ecx, [rax+F_SPY]
    add ecx, 2
    mov [rsp+100], ecx               ; a thin bar in the middle of the strip
    add ecx, 2
    mov [rsp+108], ecx
    invoke FillRect, r12, &rsp+96, [th_bgbrush]
.erdone:
    mov eax, 1
    EPILOG 320

.size:
    invoke fold_layout, rsi
    xor eax, eax
    EPILOG 320

.command:
    movzx ebx, r12w                  ; id
    mov eax, r12d
    shr eax, 16
    mov ebp, eax                     ; notification code
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_STOP
    je .cancel
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_BROWSE_B
    je .browse
    cmp ebx, ID_SWAP
    je .browse
    cmp ebx, ID_CHK_SIZE
    jb .btns
    cmp ebx, ID_CHK_ATTR
    ja .nocrit
    invoke fold_crit_sync, rsi
    cmp qword [r15+F_RUNNING], 0             ; a changed criterion re-runs the comparison - also one that has
    jne .run                                 ; just started and has no entries yet: it still uses the old one
    cmp qword [r15+F_NENT], 0
    je .zero
    jmp .run
.nocrit:
    cmp ebx, ID_CHK_SAME
    jb .btns
    cmp ebx, ID_CHK_RONLY
    ja .btns
    invoke fold_read_ui, rsi, r15
    invoke fold_refresh, r15
    jmp .zero
.browse:
    invoke pb_command, rsi, rbx, 1
    jmp .zero
.btns:
    cmp ebx, ID_BTN_COPYLR
    je .b_lr
    cmp ebx, ID_BTN_COPYRL
    je .b_rl
    cmp ebx, ID_BTN_MOVELR
    je .b_mlr
    cmp ebx, ID_BTN_MOVERL
    je .b_mrl
    cmp ebx, ID_BTN_STAMPLR
    je .b_slr
    cmp ebx, ID_BTN_STAMPRL
    je .b_srl
    cmp ebx, ID_BTN_DEL
    je .b_del
    cmp ebx, ID_BTN_SYNC
    je .b_sync
    cmp ebx, ID_BTN_UNDO
    je .btn_undo
    cmp ebx, ID_BTN_REDO
    je .btn_redo
    cmp ebx, ID_BTN_REPORT
    je .b_rep
    cmp ebx, ID_BTN_SNAP
    je .b_snap
    cmp ebx, ID_BTN_EXPAND
    je .b_exp
    cmp ebx, ID_BTN_COLLAP
    je .b_col
    cmp ebx, ID_BTN_PAUSE
    je .b_pause
    cmp ebx, ID_BTN_EXCL
    je .b_excl
    cmp ebx, ID_CRIT
    je .b_crit
    cmp ebx, ID_BTN_OPTS
    je .b_opts
    cmp ebx, ID_FILTER
    je .b_filter
    cmp ebx, ID_CHK_AUTOREF
    je .b_autoref
    cmp ebx, ID_PV_PREV
    jb .def
    cmp ebx, ID_BTN_PVTOG
    ja .def
    invoke fold_pv_cmd, rsi, r15, rbx
    jmp .zero
.b_lr:
    invoke fold_copy, rsi, r15, 0
    jmp .zero
.b_rl:
    invoke fold_copy, rsi, r15, 1
    jmp .zero
.b_mlr:
    invoke fold_move, rsi, r15, 0
    jmp .zero
.b_mrl:
    invoke fold_move, rsi, r15, 1
    jmp .zero
.b_slr:
    invoke fold_stamp, rsi, r15, 0
    jmp .zero
.b_srl:
    invoke fold_stamp, rsi, r15, 1
    jmp .zero
.b_del:
    invoke fold_delete, rsi, r15
    jmp .zero
.b_sync:
    invoke fold_sync, rsi, r15
    jmp .zero
.b_rep:
    invoke fold_report, rsi, r15
    jmp .zero
.b_snap:
    invoke fold_snapshot, rsi, r15
    jmp .zero
.b_exp:
    invoke fold_all, r15, 0
    jmp .zero
.b_col:
    invoke fold_all, r15, 1
    jmp .zero
.b_pause:
    invoke fold_pause, rsi, r15
    jmp .zero
.b_excl:
    invoke fold_excl_dialog, rsi, r15
    jmp .zero
.b_crit:
    cmp ebp, CBN_SELCHANGE
    jne .zero
    invoke GetDlgItem, rsi, ID_CRIT
    invoke SendMessageW, rax, CB_GETCURSEL, 0, 0
    cmp eax, CR_N
    jae .zero                                ; "custom" is only ever shown, never chosen
    mov edx, eax
    invoke fold_crit_apply, rsi, rdx
    cmp qword [r15+F_RUNNING], 0
    jne .run
    cmp qword [r15+F_NENT], 0
    je .zero
    jmp .run
.b_opts:
    invoke fold_layout, rsi
    jmp .zero
.b_filter:
    cmp ebp, 0x0300                          ; EN_CHANGE: the list follows what is typed
    jne .zero
    invoke fold_refresh, r15
    jmp .zero
.b_autoref:
    invoke fold_watch_arm, rsi, r15
    jmp .zero

.run:
    invoke fold_start, rsi, r15
    jmp .zero
.cancel:
    mov qword [r15+F_PAUSED], 0
    mov byte [r15+F_CANCEL], 1
    jmp .zero
.collect:
    invoke fold_collect, rsi, r14, r15
    jmp .zero
.progress:
    invoke _snwprintf, &rsp+96, 100, &s_scanning, r12
    invoke set_status, &rsp+96
    jmp .zero
.done:
    cmp qword [r15+F_CLOSING], 0
    jne .zero
    cmp qword [r15+F_RUNNING], 0
    jne .zero
    invoke fold_done, rsi, r14, r15, r12
    test r12, r12
    jnz .zero
    cmp qword [g_batch], 0
    je .zero
    invoke fold_batch, rsi, r15
    jmp .zero
.syncprog:
    invoke _snwprintf, &rsp+96, 100, &s_sync_prog, r12, r13
    invoke set_status, &rsp+96
    jmp .zero
.syncdone:
    cmp qword [r15+F_CLOSING], 0
    jne .zero
    invoke CloseHandle, [r15+F_SYNCTHR]
    mov qword [r15+F_SYNCTHR], 0
    invoke fj_commit, &r15+F_UST, [r15+F_JRN]
    mov qword [r15+F_JRN], 0
    invoke _snwprintf, &rsp+96, 100, &s_sync_done, r12
    invoke set_status, &rsp+96
    test r12, r12
    jz .sdok
    mov qword [g_exitcode], 2
.sdok:
    invoke fold_start, rsi, r15
    jmp .zero
.theme:
    invoke fold_theme, r15
    invoke SendMessageW, [r15+F_PVVIEW], BC_THEME, 0, 0
    invoke InvalidateRect, rsi, 0, 1
    jmp .zero

.timer:
    cmp r12, 51                      ; the selection settled: show its pair
    jne .tm2
    invoke KillTimer, rsi, 51
    mov r8, [r15+F_PVROW]
    invoke fold_pv_switch, rsi, r15, r8
    jmp .zero
.tm2:
    cmp r12, 52
    jne .zero
    invoke fold_pv_label, rsi, r15
    invoke get_chk, rsi, ID_CHK_AUTOREF
    test eax, eax
    jz .zero
    cmp qword [r15+F_RUNNING], 0
    jne .zero
    cmp qword [r15+F_SYNCRUN], 0
    jne .zero
    invoke fold_pv_dirty, r15
    test eax, eax
    jnz .zero
    mov rcx, [r15+F_CHGA]
    test rcx, rcx
    jz .chkb
    cmp rcx, -1
    je .chkb
    invoke WaitForSingleObject, rcx, 0
    test eax, eax
    jz .autorun
.chkb:
    mov rcx, [r15+F_CHGB]
    test rcx, rcx
    jz .zero
    cmp rcx, -1
    je .zero
    invoke WaitForSingleObject, rcx, 0
    test eax, eax
    jnz .zero
.autorun:
    invoke fold_start, rsi, r15
    jmp .zero
.isdirty:
    invoke fold_pv_dirty, r15
    EPILOG 320
.btn_undo:                           ; the toolbar buttons do what Ctrl+Z / Ctrl+Y do
    invoke SendMessageW, rsi, BC_UNDO, 0, 0
    jmp .zero
.btn_redo:
    invoke SendMessageW, rsi, BC_REDO, 0, 0
    jmp .zero
.undo:                               ; not in a path or filter box: those edit boxes undo by themselves
    call us_focus_edit
    test eax, eax
    jnz .zero
    cmp qword [r15+F_PVVIEW], 0      ; the caret is in the preview: its text is meant
    je .undo2
    call GetFocus
    invoke IsChild, [r15+F_PVVIEW], rax
    test eax, eax
    jz .undo2
    invoke SendMessageW, [r15+F_PVVIEW], rdi, 0, 0
    EPILOG 320
.undo2:
    xor r8d, r8d
    cmp edi, BC_REDO
    sete r8b
    invoke fold_undo, rsi, r15, r8
    mov eax, 1
    EPILOG 320
.savepv:
    invoke SendMessageW, [r15+F_PVVIEW], BC_SAVE, 0, 0
    invoke SendMessageW, [r15+F_PVVIEW], BC_RUN, 0, 0
    invoke fold_pv_sync_ent, rsi, r15, 1
    invoke set_status, &s_pv_saved
    invoke fold_pv_label, rsi, r15
    jmp .zero
.nextdiff:
    invoke SendMessageW, [r15+F_PVVIEW], BC_NEXTDIFF, 0, 0
    jmp .zero
.prevdiff:
    invoke SendMessageW, [r15+F_PVVIEW], BC_PREVDIFF, 0, 0
    jmp .zero
; the strip between the list and the preview can be dragged
.lbd:
    cmp qword [r15+F_SPH], 0
    je .def
    mov rax, r13
    shr rax, 16
    movsx eax, ax
    cmp eax, [r15+F_SPY]
    jl .def
    mov edx, [r15+F_SPY]
    add edx, [r15+F_SPH]
    cmp eax, edx
    jge .def
    invoke SetCapture, rsi
    mov qword [r15+F_DRAG], 1
    jmp .zero
.mm:
    cmp qword [r15+F_DRAG], 0
    je .def
    mov rax, r13
    shr rax, 16
    movsx eax, ax
    sub eax, [r15+F_LTOP]
    jns .mp
    xor eax, eax
.mp:
    imul eax, 100
    xor edx, edx
    mov ecx, [r15+F_AVAIL]
    test ecx, ecx
    jz .zero
    div ecx
    cmp eax, 15
    jae .m1
    mov eax, 15
.m1:
    cmp eax, 85
    jbe .m2
    mov eax, 85
.m2:
    mov [r15+F_SPLIT], rax
    invoke fold_layout, rsi
    jmp .zero
.lbu:
    cmp qword [r15+F_DRAG], 0
    je .def
    call ReleaseCapture
    mov qword [r15+F_DRAG], 0
    jmp .zero
.setcur:
    cmp qword [r15+F_SPH], 0
    je .def
    invoke GetCursorPos, &rsp+96
    invoke ScreenToClient, rsi, &rsp+96
    mov eax, [rsp+100]
    cmp eax, [r15+F_SPY]
    jl .def
    mov edx, [r15+F_SPY]
    add edx, [r15+F_SPH]
    cmp eax, edx
    jge .def
    invoke LoadCursorW, 0, 32645     ; IDC_SIZENS
    invoke SetCursor, rax
    mov eax, 1
    EPILOG 320

.notify:
    mov rax, [r13+NMH_FROM]
    cmp rax, [r15+F_HLIST]
    jne .def
    mov eax, [r13+NMH_CODE]
    cmp eax, -101                    ; LVN_ITEMCHANGED: a row was selected
    je .itemchg
    cmp eax, LVN_GETDISPINFOW
    je .disp
    cmp eax, NM_CUSTOMDRAW
    je .cdraw
    cmp eax, NM_DBLCLK
    je .dbl
    cmp eax, NM_CLICK
    je .click
    cmp eax, LVN_KEYDOWN
    je .keydown
    cmp eax, NM_RCLICK
    je .rclick
    jmp .def
.disp:
    test dword [r13+DI_MASK], LVIF_IMAGE
    jz .dtext
    cmp dword [r13+DI_SUB], 0
    jne .dtext
    mov eax, [r13+DI_ITEM]
    cmp rax, [r15+F_NVIS]
    jae .dtext
    invoke fold_ent, r15, rax
    mov rdi, rax
    mov dword [r13+DI_IMAGE], -2     ; I_IMAGENONE: nothing on this side
    test dword [rdi+E_FLAGS], EF_EXA
    jz .dtext
    invoke fold_icon, rdi
    mov [r13+DI_IMAGE], eax
.dtext:
    test dword [r13+DI_MASK], LVIF_TEXT
    jz .zero
    mov eax, [r13+DI_ITEM]
    cmp rax, [r15+F_NVIS]
    jae .zero
    mov r8d, [r13+DI_SUB]
    mov r9, [r13+DI_TEXT]
    invoke fold_cell, r15, rax, r8, r9
    jmp .zero
.cdraw:
    mov ebx, [r13+CD_STAGE]
    cmp ebx, CDDS_PREPAINT
    jne .cd2
    mov eax, CDRF_NOTIFYITEMDRAW
    EPILOG 320
.cd2:
    cmp ebx, CDDS_ITEMPREPAINT
    je .cdrow
    cmp ebx, CD_SUBSTATE
    je .cdrow
    cmp ebx, CD_SUBPOST
    je .cdpost
    jmp .zero
.cdrow:
    mov rax, [r13+CD_ITEMSPEC]
    cmp rax, [r15+F_NVIS]
    jae .zero
    mov rcx, [r15+F_VIS]
    mov eax, [rcx+rax*4]
    shl rax, 6
    add rax, [r15+F_ENTS]
    mov rdi, rax
    mov ecx, [rax+E_STATUS]
    mov edx, [th_bg]
    mov r8d, [th_same]
    cmp ecx, ST_DIFF
    jne .s2
    mov edx, [th_diff]
    mov r8d, [th_fg]
    jmp .cset
.s2:
    cmp ecx, ST_LONLY
    jne .s3
    mov edx, [th_lonly]
    mov r8d, [th_fg]
    jmp .cset
.s3:
    cmp ecx, ST_RONLY
    jne .s4
    mov edx, [th_ronly]
    mov r8d, [th_fg]
    jmp .cset
.s4:
    cmp ecx, ST_TYPE
    jne .cset
    mov edx, [th_type]
    mov r8d, [th_fg]
.cset:
    mov [r13+CD_CLRTEXTBK], edx
    mov [r13+CD_CLRTEXT], r8d
    cmp ebx, CDDS_ITEMPREPAINT
    jne .cdsub
    mov eax, CDRF_NOTIFYSUBITEMDRAW  ; the cells of the row are drawn one by one
    EPILOG 320
.cdsub:
    mov eax, [r13+CD_SUBITEM]
    cmp eax, 3
    jne .cdsub6
    invoke fold_glyph, rdi           ; an arrow in the accent colour
    cmp eax, 1
    je .acc
    cmp eax, 2
    jne .zero
.acc:
    mov eax, [th_accent]
    mov [r13+CD_CLRTEXT], eax
    jmp .zero
.cdsub6:
    cmp eax, 6
    jne .zero
    mov eax, 0x10                    ; CDRF_NOTIFYPOSTPAINT: the icon of the right side goes over this cell
    EPILOG 320
.cdpost:
    mov rax, [r13+CD_ITEMSPEC]
    cmp rax, [r15+F_NVIS]
    jae .zero
    mov rcx, [r15+F_VIS]
    mov eax, [rcx+rax*4]
    shl rax, 6
    add rax, [r15+F_ENTS]
    mov rdi, rax
    test dword [rdi+E_FLAGS], EF_EXB
    jz .zero
    invoke fold_icon, rdi
    mov ebx, eax
    mov dword [rsp+96], 0            ; LVIR_BOUNDS of the cell: the list tells where it is
    mov dword [rsp+100], 6
    mov dword [rsp+104], 0
    mov dword [rsp+108], 0
    invoke SendMessageW, [r15+F_HLIST], LVM_GETSUBITEMRECT, [r13+CD_ITEMSPEC], &rsp+96
    mov eax, [rsp+108]
    sub eax, [rsp+100]
    sub eax, 16
    shr eax, 1
    add eax, [rsp+100]
    mov r11d, eax                    ; y: the icon is centred in the row
    mov r10d, [rsp+96]
    add r10d, 3
    invoke ImageList_Draw, [g_sysil], rbx, [r13+CD_HDC], r10, r11, 1       ; ILD_TRANSPARENT
    jmp .zero
.rclick:
    invoke fold_ctx, rsi, r15
    jmp .zero
.itemchg:
    test dword [r13+40], 8           ; uChanged: the state
    jz .zero
    test dword [r13+32], 2           ; uNewState: selected
    jz .zero
    cmp qword [r15+F_PVGUARD], 0
    jne .zero
    movsxd rax, dword [r13+24]
    mov [r15+F_PVROW], rax
    invoke SetTimer, rsi, 51, 120, 0
    jmp .zero
.click:
    mov rax, [r13+44]                ; where the mouse was: NMITEMACTIVATE.ptAction
    mov [rsp+96], rax
    mov dword [rsp+104], 0
    mov dword [rsp+108], -1
    mov dword [rsp+112], 0
    mov dword [rsp+116], 0
    invoke SendMessageW, [r15+F_HLIST], LVM_SUBITEMHITTEST, 0, &rsp+96
    cmp dword [rsp+108], 0
    jl .zero
    cmp dword [rsp+112], 3           ; the middle column
    jne .zero
    movsxd rdx, dword [rsp+108]
    invoke fold_cycle, r15, rdx
    jmp .zero
.dbl:
    mov eax, [r13+IA_ITEM]
    cmp eax, -1
    je .zero
    movsxd rax, eax
    invoke fold_open_pair, r15, rax
    jmp .zero
.keydown:
    movzx ebx, word [r13+24]
    invoke SendMessageW, [r15+F_HLIST], LVM_GETNEXTITEM, -1, LVNI_FOCUSED
    movsxd rbp, eax
    test rbp, rbp
    js .zero
    cmp ebx, 0x20                    ; Space: the next action of every selected item
    je .kspace
    cmp ebx, VK_RETURN
    je .kopen
    cmp ebx, 0x71
    je .krename
    cmp ebx, 0x25
    je .kleft
    cmp ebx, 0x27
    je .kright
    jmp .zero
.kspace:
    mov rbp, -1
.ks:
    invoke SendMessageW, [r15+F_HLIST], LVM_GETNEXTITEM, rbp, LVNI_SELECTED
    movsxd rbp, eax
    test rbp, rbp
    js .zero
    invoke fold_cycle, r15, rbp
    jmp .ks
.kopen:
    invoke fold_open_pair, r15, rbp
    jmp .zero
.krename:
    invoke fold_rename, rsi, r15, rbp
    jmp .zero
.kleft:
    invoke fold_setcoll, r15, rbp, 1
    jmp .zero
.kright:
    invoke fold_setcoll, r15, rbp, 0
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+F_HLIST]
    jmp .zero
.stopping:
    mov qword [r15+F_CLOSING], 1
    mov byte [r15+F_CANCEL], 1
    mov qword [r15+F_PAUSED], 0
    mov rcx, [r15+F_THREAD]
    test rcx, rcx
    jz .checksync
    invoke WaitForSingleObject, rcx, 0
    test eax, eax
    jnz .stillstopping
.checksync:
    mov rcx, [r15+F_SYNCTHR]
    test rcx, rcx
    jz .zero
    invoke WaitForSingleObject, rcx, 0
    test eax, eax
    jz .zero
.stillstopping:
    mov eax, 1
    EPILOG 320
.destroy:
    mov byte [r15+F_CANCEL], 1
    mov qword [r15+F_PAUSED], 0
    ; Normal closing polls asynchronously before destroying the view. This
    ; fallback also protects direct parent destruction from freeing live state.
    mov rcx, [r15+F_THREAD]
    test rcx, rcx
    jz .noscan
    invoke WaitForSingleObject, rcx, 0xFFFFFFFF
    invoke CloseHandle, [r15+F_THREAD]
.noscan:
    mov rcx, [r15+F_SYNCTHR]
    test rcx, rcx
    jz .nothr
    invoke WaitForSingleObject, rcx, 0xFFFFFFFF
    invoke CloseHandle, [r15+F_SYNCTHR]
.nothr:
    invoke KillTimer, rsi, 51
    invoke KillTimer, rsi, 52
    invoke fold_watch_close, r15
    mov rcx, [r15+F_PVVIEW]
    test rcx, rcx
    jz .nopv
    invoke DestroyWindow, rcx        ; before the record it reads is freed
.nopv:
    invoke free, [r15+F_PVSESS]
    invoke fold_free_vt, r15
    invoke fold_free_ents, r15
    invoke free, [r15+F_ENTS]
    invoke free, [r15+F_VIS]
    invoke free, [r15+F_PLAN]
    invoke free, [r15+F_BUFA]
    invoke free, [r15+F_BUFB]
    invoke fj_discard, [r15+F_JRN]
    invoke us_free, &r15+F_UST
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320

; copy_acl(src, dst): SY-06 copy the NTFS DACL
copy_acl:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    mov qword [rsp+80], 0
    mov qword [rsp+88], 0
    invoke GetNamedSecurityInfoW, rsi, 1, 4, 0, 0, &rsp+80, 0, &rsp+88
    test eax, eax
    jnz .d
    invoke SetNamedSecurityInfoW, rdi, 1, 4, 0, 0, [rsp+80], 0
    invoke LocalFree, [rsp+88]
.d:
    EPILOG 64

; fold_stamp(hwnd, fold, dir 0 = A->B / 1 = B->A): SY-04 copy modification time and attributes onto the other side
fold_stamp:
    PROLOG 4400
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov rax, [rbx+F_KIND]
    or rax, [rbx+F_KIND+8]
    jnz .ro
    test qword [g_policy], 1
    jnz .pol
    mov rdi, -1
.n:
    invoke SendMessageW, [rbx+F_HLIST], LVM_GETNEXTITEM, rdi, LVNI_SELECTED
    movsxd rdi, eax
    test rdi, rdi
    js .done
    invoke fold_ent, rbx, rdi
    mov r13, rax
    test dword [r13+E_FLAGS], EF_ALT
    jnz .n
    mov eax, [r13+E_FLAGS]
    and eax, EF_EXA|EF_EXB|EF_DIRA|EF_DIRB
    cmp eax, EF_EXA|EF_EXB
    jne .n
    invoke _snwprintf, &rsp+96, 1030, &f_path, &rbx+F_ROOTA, [r13+E_NAME]   ; 2060 bytes each: below rsp+4380
    invoke _snwprintf, &rsp+2300, 1030, &f_path, &rbx+F_ROOTB, [r13+E_NAME]
    lea r14, [rsp+96]                 ; source
    lea r15, [rsp+2300]               ; target
    mov rbp, [r13+E_MTA]
    test r12d, r12d
    jz .go
    xchg r14, r15
    mov rbp, [r13+E_MTB]
.go:
    mov [rsp+4380], rbp
    invoke GetFileAttributesW, r14
    mov ebp, eax
    invoke SetFileAttributesW, r15, 0x80
    invoke CreateFileW, r15, 0x100, 7, 0, OPEN_EXISTING, 0x02000000, 0
    cmp rax, -1
    je .attr
    mov r13, rax
    invoke SetFileTime, r13, 0, 0, &rsp+4380
    invoke CloseHandle, r13
.attr:
    cmp ebp, -1
    je .n
    and ebp, 0x27                    ; read-only, hidden, system, archive
    invoke SetFileAttributesW, r15, rbp
    jmp .n
.done:
    invoke fold_start, rsi, rbx
    EPILOG 4400
.ro:
    invoke msgbox, rsi, &s_ro, MB_OK|MB_ICONINFO
    EPILOG 4400
.pol:
    invoke msgbox, rsi, &s_pol_blocked, MB_OK|MB_ICONWARNING
    EPILOG 4400

; ---------------------------------------------------------------- tooltips of this view
extern tips_apply
section .rdata
WSTR tt_folder_0, "진행 중인 스캔을 중지합니다"
WSTR tt_folder_1, "크기가 다르면 다른 파일로 판정합니다"
WSTR tt_folder_2, "수정 시각이 다르면 다른 파일로 판정합니다"
WSTR tt_folder_3, "파일 내용을 바이트 단위로 비교합니다 (오래 걸릴 수 있음)"
WSTR tt_folder_4, "심볼릭 링크와 정션이 가리키는 폴더 안까지 비교합니다"
WSTR tt_folder_5, "CRC32 값으로 내용을 비교합니다"
WSTR tt_folder_6, "파일 속성(읽기 전용·숨김 등) 차이도 비교합니다"
WSTR tt_folder_7, "비교에서 제외할 이름 패턴 (예: *.obj)"
WSTR tt_folder_8, "제외할 패턴을 목록에서 고릅니다"
WSTR tt_folder_9, "이 크기(KB)보다 작은 파일은 제외합니다"
WSTR tt_folder_10, "이 크기(KB)보다 큰 파일은 제외합니다"
WSTR tt_folder_11, "최근 이 일수 안에 수정된 파일만 비교합니다"
WSTR tt_folder_12, "이 속성을 가진 항목은 제외합니다 (R H S A L C O E)"
WSTR tt_folder_13, "압축 파일 안을 열지 않고 파일 하나로 비교합니다"
WSTR tt_folder_14, "복사할 때 NTFS 권한(ACL)도 함께 복사합니다"
WSTR tt_folder_15, "유니코드 표기나 확장자가 조금 달라도 같은 이름으로 짝짓습니다"
WSTR tt_folder_16, "이름이 다른 파일을 짝짓는 규칙 (예: *.txt=*.bak)"
WSTR tt_folder_17, "= 같은 항목을 목록에 표시합니다"
WSTR tt_folder_18, "≠ 다른 항목을 목록에 표시합니다"
WSTR tt_folder_19, "→ 왼쪽에만 있는 항목을 목록에 표시합니다"
WSTR tt_folder_20, "← 오른쪽에만 있는 항목을 목록에 표시합니다"
WSTR tt_folder_24, "무엇이 같은지를 정하는 기준. 아래 옵션 줄의 체크 상자와 같이 움직입니다"
WSTR tt_folder_25, "덜 쓰는 옵션 줄(제외·크기·기간·속성·마스크 등)을 열고 닫습니다"
WSTR tt_folder_26, "목록에서 이 글자가 이름에 든 항목만 보여 줍니다 (폴더는 안에 든 파일이 맞을 때만 남습니다)"
WSTR tt_folder_autoref, "양쪽 폴더 안의 파일이 외부에서 바뀌면 자동으로 다시 비교합니다"
WSTR tt_folder_21, "선택한 항목의 수정 시각과 속성을 왼쪽 값으로 오른쪽에 맞춥니다"
WSTR tt_folder_22, "선택한 항목의 수정 시각과 속성을 오른쪽 값으로 왼쪽에 맞춥니다"
WSTR tt_folder_23, "비교 결과를 HTML·엑셀·워드 파일로 내보냅니다"
align 8
tt_folder:
    dq ID_CHK_SIZE, tt_folder_1
    dq ID_CHK_MTIME, tt_folder_2
    dq ID_CHK_CONT, tt_folder_3
    dq ID_CHK_LINKS, tt_folder_4
    dq ID_CHK_CRC, tt_folder_5
    dq ID_CHK_ATTR, tt_folder_6
    dq ID_EDIT_EXCL, tt_folder_7
    dq ID_BTN_EXCL, tt_folder_8
    dq ID_EDIT_FMIN, tt_folder_9
    dq ID_EDIT_FMAX, tt_folder_10
    dq ID_EDIT_FDAYS, tt_folder_11
    dq ID_EDIT_ATTRX, tt_folder_12
    dq ID_CHK_ASFILE, tt_folder_13
    dq ID_CHK_ACL, tt_folder_14
    dq ID_CHK_LOOSE, tt_folder_15
    dq ID_EDIT_MASK, tt_folder_16
    dq ID_CHK_SAME, tt_folder_17
    dq ID_CHK_DIFF, tt_folder_18
    dq ID_CHK_LONLY, tt_folder_19
    dq ID_CHK_RONLY, tt_folder_20
    dq ID_BTN_REPORT, tt_folder_23
    dq ID_CRIT, tt_folder_24
    dq ID_BTN_OPTS, tt_folder_25
    dq ID_FILTER, tt_folder_26
    dq ID_CHK_AUTOREF, tt_folder_autoref
    dq 0, 0
section .text
