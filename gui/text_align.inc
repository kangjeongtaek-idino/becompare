; text_align.inc - TC-03 manual alignment: anchor pairs force two lines onto the same row


section .rdata
WSTR s_b_aln, "정렬 지정"
WSTR s_b_alc, "정렬 해제"
WSTR s_aln_fill, "정렬 지정: 양쪽 창에서 실제 줄(빈 채움 줄이 아님)에 커서를 놓아야 합니다."
WSTR s_aln_conf, "정렬 지정: 기존 정렬 지점과 순서가 맞지 않아 추가할 수 없습니다."
WSTR s_aln_hint, "정렬 지정: 왼쪽·오른쪽 창에서 맞추려는 줄을 각각 클릭한 뒤 누르세요."
WSTR s_aln_ok, "정렬 지점을 추가했습니다."

section .text

; tv_anchor_add(tv, a, b) -> eax 1 ok / 0 order conflict or full
tv_anchor_add:
    PROLOG 64
    mov rbx, rcx
    mov esi, edx
    mov edi, r8d
    lea r12, [rbx+TV_ANC]
    mov ecx, [rbx+TV_NANC]
    xor eax, eax                      ; insert position
.f:
    cmp eax, ecx
    jae .have
    cmp [r12+rax*8], esi
    jae .have
    inc eax
    jmp .f
.have:
    mov r13d, eax
    ; same line already anchored: drop the old pair first
    cmp r13d, ecx
    jae .check
    cmp [r12+r13*8], esi
    jne .check
    mov eax, r13d
.rm:
    lea edx, [rax+1]
    cmp edx, ecx
    jae .rmd
    mov r8, [r12+rdx*8]
    mov [r12+rax*8], r8
    mov eax, edx
    jmp .rm
.rmd:
    dec ecx
    mov [rbx+TV_NANC], ecx
.check:
    test r13d, r13d
    jz .nprev
    mov eax, [r12+r13*8-4]            ; previous pair's b
    cmp eax, edi
    jae .fail
.nprev:
    cmp r13d, ecx
    jae .room
    cmp [r12+r13*8+4], edi            ; next pair's b
    jbe .fail
.room:
    cmp ecx, MAX_ANC
    jae .fail
    mov eax, ecx                      ; shift up
.sh:
    cmp eax, r13d
    jbe .store
    mov r8, [r12+rax*8-8]
    mov [r12+rax*8], r8
    dec eax
    jmp .sh
.store:
    mov [r12+r13*8], esi
    mov [r12+r13*8+4], edi
    inc ecx
    mov [rbx+TV_NANC], ecx
    mov eax, 1
    EPILOG 64
.fail:
    xor eax, eax
    EPILOG 64

; tv_align_set(hwnd, tv, sess)
tv_align_set:
    PROLOG 160
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke tv_unfold, rsi, rbx
    cmp dword [rbx+TV_LDA], 0
    je .hint
    invoke SendMessageW, [rbx+TV_HA], EM_EXGETSEL, 0, &rsp+96
    mov eax, [rsp+96]
    invoke SendMessageW, [rbx+TV_HA], EM_LINEFROMCHAR, rax, 0
    mov r13d, eax
    invoke SendMessageW, [rbx+TV_HB], EM_EXGETSEL, 0, &rsp+96
    mov eax, [rsp+96]
    invoke SendMessageW, [rbx+TV_HB], EM_LINEFROMCHAR, rax, 0
    mov r14d, eax
    cmp r13, [rbx+TV_NROWS]
    jae .fill
    cmp r14, [rbx+TV_NROWS]
    jae .fill
    mov rax, r13
    shl rax, 4
    add rax, [rbx+TV_ROWS]
    mov r13d, [rax+4]                 ; line index in A
    mov rax, r14
    shl rax, 4
    add rax, [rbx+TV_ROWS]
    mov r14d, [rax+8]                 ; line index in B
    cmp r13d, -1
    je .fill
    cmp r14d, -1
    je .fill
    invoke tv_anchor_add, rbx, r13, r14
    test eax, eax
    jz .conf
    invoke text_compare, rsi, rbx, r12
    invoke set_status, &s_aln_ok
    jmp .d
.hint:
    invoke set_status, &s_aln_hint
    jmp .d
.fill:
    invoke msgbox, rsi, &s_aln_fill, MB_OK|MB_ICONINFORMATION
    jmp .d
.conf:
    invoke msgbox, rsi, &s_aln_conf, MB_OK|MB_ICONWARNING
.d:
    EPILOG 160

; tv_align_clear(hwnd, tv, sess)
tv_align_clear:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke tv_unfold, rsi, rbx
    cmp dword [rbx+TV_NANC], 0
    je .d
    mov dword [rbx+TV_NANC], 0
    cmp dword [rbx+TV_LDA], 0
    je .d
    invoke text_compare, rsi, rbx, r12
.d:
    EPILOG 64

; tv_seg(ctx*, pa, ea, pb, eb): diff the A range [pa,ea) against the B range [pb,eb) and append the hunks
;   ctx: +0 LA*, +8 LB*, +16 flags, +24 cp*, +32 hunk vec*
tv_seg:
    PROLOG 288
    mov r12, rcx
    mov r13d, edx                     ; pa
    mov r14d, r8d                     ; ea
    mov r15d, r9d                     ; pb
    mov ebp, [rsp+288+112]            ; eb
    mov eax, r14d
    sub eax, r13d                     ; cntA
    mov ecx, ebp
    sub ecx, r15d                     ; cntB
    mov esi, eax
    mov edi, ecx
    or eax, ecx
    jz .d
    test esi, esi
    jz .one
    test edi, edi
    jz .one
    ; sub lists
    mov rax, [r12]
    mov rdx, [rax]
    lea rdx, [rdx+r13*8]
    mov [rsp+96], rdx
    mov [rsp+104], rsi
    mov rax, [r12+8]
    mov rdx, [rax]
    lea rdx, [rdx+r15*8]
    mov [rsp+112], rdx
    mov [rsp+120], rdi
    xor eax, eax
    mov [rsp+128], rax
    mov [rsp+136], rax
    mov [rsp+144], rax
    mov r8d, [r12+16]
    mov r9, [r12+24]
    lea rax, [rsp+128]
    mov [rsp+32], rax
    lea rcx, [rsp+96]
    lea rdx, [rsp+112]
    call compare_lines
    xor ebx, ebx
.h:
    cmp rbx, [rsp+136]
    jae .free
    lea rax, [rbx+rbx*2]
    shl rax, 3
    add rax, [rsp+128]
    mov rsi, rax
    mov rax, [r12+32]
    invoke vec_push, rax, 24
    mov ecx, [rsi]
    mov [rax], ecx
    mov ecx, [rsi+4]
    add ecx, r13d
    mov [rax+4], ecx
    mov ecx, [rsi+8]
    mov [rax+8], ecx
    mov ecx, [rsi+12]
    add ecx, r15d
    mov [rax+12], ecx
    mov ecx, [rsi+16]
    mov [rax+16], ecx
    mov dword [rax+20], 0
    inc ebx
    jmp .h
.free:
    invoke hunk_free, &rsp+128
    jmp .d
.one:
    mov rax, [r12+32]
    invoke vec_push, rax, 24
    mov dword [rax], 1
    mov [rax+4], r13d
    mov [rax+8], esi
    mov [rax+12], r15d
    mov [rax+16], edi
    mov dword [rax+20], 0
.d:
    EPILOG 288

; tv_cmp_anch(tv, LA, LB, flags, cp, hunk vec): compare with the manual anchors applied
tv_cmp_anch:
    PROLOG 256
    mov rbx, rcx
    mov [rsp+96], rdx                 ; ctx
    mov [rsp+104], r8
    mov [rsp+112], r9
    mov rax, [rsp+256+112]
    mov [rsp+120], rax
    mov rax, [rsp+256+120]
    mov [rsp+128], rax
    mov r13, rdx
    mov r14, r8
    cmp dword [rbx+TV_NANC], 0
    jne .anch
    invoke compare_lines, r13, r14, r9, [rsp+120], [rsp+128]
    jmp .d
.anch:
    xor esi, esi                      ; pa
    xor edi, edi                      ; pb
    xor ebp, ebp                      ; anchor index
.l:
    cmp ebp, [rbx+TV_NANC]
    jae .tail
    lea rax, [rbx+TV_ANC]
    mov r12d, [rax+rbp*8]             ; a
    mov r15d, [rax+rbp*8+4]           ; b
    inc ebp
    cmp r12d, esi
    jb .l
    cmp r15d, edi
    jb .l
    mov rax, [r13+8]
    cmp r12, rax
    jae .l
    mov rax, [r14+8]
    cmp r15, rax
    jae .l
    invoke tv_seg, &rsp+96, rsi, r12, rdi, r15
    ; the anchored pair itself
    mov rax, [rsp+128]
    invoke vec_push, rax, 24
    mov r8, rax
    mov rax, [r13]
    mov rcx, [rax+r12*8]
    mov rax, [r14]
    mov rdx, [rax+r15*8]
    mov [rsp+144], r8
    invoke wcscmp, rcx, rdx
    mov r8, [rsp+144]
    test eax, eax
    setnz al
    movzx eax, al
    mov [r8], eax
    mov [r8+4], r12d
    mov dword [r8+8], 1
    mov [r8+12], r15d
    mov dword [r8+16], 1
    mov dword [r8+20], 0
    lea esi, [r12+1]
    lea edi, [r15+1]
    jmp .l
.tail:
    mov rax, [r13+8]
    mov r10, [r14+8]
    invoke tv_seg, &rsp+96, rsi, rax, rdi, r10
.d:
    EPILOG 256
