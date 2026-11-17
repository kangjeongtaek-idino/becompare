; undo.asm - snapshot stacks for undo / redo, shared by the text compare, text merge and folder merge views
;
;   A view keeps one pointer (the "slot", 0 until the first step). It points to a block with two stacks,
;   0 = undo and 1 = redo, of up to US_MAX steps each. A step is three qwords:
;       +0  tag    (meaning is up to the view: which side, how many entries, ...)
;       +8  ptr    malloc'd snapshot, owned by the stack while it is stored
;       +16 bytes  size of the snapshot (only used for the memory budget)
;   A full stack, or one that holds more than US_BUDGET bytes, drops its oldest steps.
%include "inc/macros.inc"

extern calloc, free, memmove, GetFocus, GetClassNameW, _wcsicmp

global us_push, us_pop, us_clear, us_free, us_count, us_focus_edit, us_same_text
global s_un_done, s_un_redone, s_un_none, s_un_noredo

section .rdata
WSTR s_un_done, "되돌렸습니다."
WSTR s_un_redone, "다시 실행했습니다."
WSTR s_un_none, "되돌릴 내용이 없습니다."
WSTR s_un_noredo, "다시 실행할 내용이 없습니다."
WSTR s_un_edit, "Edit"

%define US_MAX    64
%define US_ENT    24
%define US_STK    (US_MAX*US_ENT)
%define US_SIZE   (16 + 2*US_STK)        ; {undo count, redo count, undo steps, redo steps}
%define US_BUDGET 0x4000000              ; 64 MB of snapshots in one stack

section .text

; us_drop(r13 = address of the count, r14 = first step): forget the oldest step
us_drop:
    sub rsp, 40
    mov rcx, [r14+8]
    call free
    mov r8, [r13]
    dec r8
    mov [r13], r8
    imul r8, r8, US_ENT
    lea rdx, [r14+US_ENT]
    mov rcx, r14
    call memmove
    add rsp, 40
    ret

; us_wipe(r13 = address of the count, r14 = first step): forget every step
us_wipe:
    push rsi
    sub rsp, 32
    xor esi, esi
.l:
    cmp rsi, [r13]
    jae .e
    imul rax, rsi, US_ENT
    mov rcx, [r14+rax+8]
    call free
    inc rsi
    jmp .l
.e:
    mov qword [r13], 0
    add rsp, 32
    pop rsi
    ret

; us_focus_edit() -> eax 1 when the keyboard focus is in a plain edit box (a path, the find box, ...): it has
;   an undo of its own and Ctrl+Z belongs to it
us_focus_edit:
    PROLOG 128
    call GetFocus
    test rax, rax
    jz .no
    invoke GetClassNameW, rax, &rsp+64, 30
    test eax, eax
    jz .no
    invoke _wcsicmp, &rsp+64, &s_un_edit
    test eax, eax
    jnz .no
    mov eax, 1
    EPILOG 128
.no:
    xor eax, eax
    EPILOG 128

; us_same_text(a, a length, b, b length) -> eax 1 when the two texts have the same lines: CR, LF and CR LF
;   count as the same line break and a line break at the very end is ignored (a file as it was read and the
;   same text taken from an editor pane differ in exactly that) (leaf)
us_same_text:
    lea rdx, [rcx+rdx*2]             ; end of a
    lea r9, [r8+r9*2]                ; end of b
.l:
    cmp rcx, rdx
    jae .aend
    cmp r8, r9
    jae .bend
    movzx eax, word [rcx]
    movzx r10d, word [r8]
    cmp eax, 13
    je .abrk
    cmp eax, 10
    je .abrk
    cmp eax, r10d
    jne .no
    add rcx, 2
    add r8, 2
    jmp .l
.abrk:                               ; a line break in a: b must have one too
    cmp r10d, 13
    je .both
    cmp r10d, 10
    jne .no
.both:
    add rcx, 2
    cmp eax, 13
    jne .b1
    cmp rcx, rdx
    jae .b1
    cmp word [rcx], 10
    jne .b1
    add rcx, 2
.b1:
    add r8, 2
    cmp r10d, 13
    jne .l
    cmp r8, r9
    jae .l
    cmp word [r8], 10
    jne .l
    add r8, 2
    jmp .l
.aend:                               ; a is used up: b may hold one more line break
    mov rcx, r8
    mov rdx, r9
.bend:                               ; b is used up: the rest [rcx, rdx) may be one line break
    cmp rcx, rdx
    jae .yes
    movzx eax, word [rcx]
    add rcx, 2
    cmp eax, 10
    je .tail
    cmp eax, 13
    jne .no
    cmp rcx, rdx
    jae .yes
    cmp word [rcx], 10
    jne .no
    add rcx, 2
.tail:
    cmp rcx, rdx
    jb .no
.yes:
    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

; us_push(slot*, which 0 undo / 1 redo, step*) -> eax 1 stored (the stack owns the snapshot now), 0 out of
;   memory (the snapshot was freed)
us_push:
    PROLOG 64
    mov rbx, rcx
    mov r12d, edx
    mov rsi, r8
    mov rdi, [rbx]
    test rdi, rdi
    jnz .have
    invoke calloc, 1, US_SIZE
    test rax, rax
    jz .fail
    mov [rbx], rax
    mov rdi, rax
.have:
    lea r13, [rdi+r12*8]
    imul rax, r12, US_STK
    lea r14, [rdi+rax+16]
    cmp qword [r13], US_MAX
    jb .room
    call us_drop
.room:
    mov rax, [r13]
    imul rax, rax, US_ENT
    add rax, r14
    mov rcx, [rsi]
    mov [rax], rcx
    mov rcx, [rsi+8]
    mov [rax+8], rcx
    mov rcx, [rsi+16]
    mov [rax+16], rcx
    inc qword [r13]
.budget:
    cmp qword [r13], 1
    jbe .ok
    xor eax, eax
    xor ecx, ecx
.sum:
    cmp rcx, [r13]
    jae .summed
    imul rdx, rcx, US_ENT
    add rax, [r14+rdx+16]
    inc rcx
    jmp .sum
.summed:
    cmp rax, US_BUDGET
    jbe .ok
    call us_drop
    jmp .budget
.ok:
    mov eax, 1
    EPILOG 64
.fail:
    invoke free, [rsi+8]
    xor eax, eax
    EPILOG 64

; us_pop(slot*, which, step* out) -> eax 1 when a step was taken off (the caller owns the snapshot now) (leaf)
us_pop:
    mov rax, [rcx]
    test rax, rax
    jz .no
    mov edx, edx
    mov r9, [rax+rdx*8]
    test r9, r9
    jz .no
    dec r9
    mov [rax+rdx*8], r9
    imul r10, rdx, US_STK
    imul r9, r9, US_ENT
    lea rax, [rax+r10+16]
    add rax, r9
    mov rcx, [rax]
    mov [r8], rcx
    mov rcx, [rax+8]
    mov [r8+8], rcx
    mov rcx, [rax+16]
    mov [r8+16], rcx
    mov eax, 1
    ret
.no:
    xor eax, eax
    ret

; us_count(slot*, which) -> rax number of steps (leaf)
us_count:
    mov rax, [rcx]
    test rax, rax
    jz .e
    mov edx, edx
    mov rax, [rax+rdx*8]
.e:
    ret

; us_clear(slot*, which 0 undo / 1 redo / 2 both)
us_clear:
    PROLOG 64
    mov rbx, [rcx]
    mov r12d, edx
    test rbx, rbx
    jz .d
    cmp r12d, 1
    je .redo
    mov r13, rbx
    lea r14, [rbx+16]
    call us_wipe
    test r12d, r12d
    jz .d
.redo:
    lea r13, [rbx+8]
    lea r14, [rbx+16+US_STK]
    call us_wipe
.d:
    EPILOG 64

; us_free(slot*): forget everything and release the block
us_free:
    PROLOG 64
    mov rbx, rcx
    invoke us_clear, rbx, 2
    invoke free, [rbx]
    mov qword [rbx], 0
    EPILOG 64
