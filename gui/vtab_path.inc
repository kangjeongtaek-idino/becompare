; A virtual entry must map to one unambiguous relative Windows path.
; vt_safe_name(wchar*, length) -> eax 1. The caller normalizes '/' to '\'.
; No rooted paths, streams, device names, dot components or shell-normalized aliases.
vt_safe_name:
    PROLOG 64
    mov rsi, rcx
    mov rdi, rdx
    test rdi, rdi
    jz .bad
    cmp rdi, 511
    ja .bad
    xor ebx, ebx                    ; component start
    xor r12d, r12d                  ; current character
.char:
    cmp r12, rdi
    jae .component
    movzx eax, word [rsi+r12*2]
    cmp eax, 0x5C
    je .component
    cmp eax, 32
    jb .bad
    cmp eax, ':'
    je .bad
    cmp eax, '/'
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
    inc r12
    jmp .char
.component:
    mov r13, r12
    sub r13, rbx
    jz .bad
    cmp r13, 255
    ja .bad
    movzx eax, word [rsi+r12*2-2]
    cmp eax, '.'                    ; includes '.' and '..'
    je .bad
    cmp eax, ' '
    je .bad
    lea r14, [rsi+rbx*2]
    xor r15d, r15d
.base:
    cmp r15, r13
    jae .device
    cmp word [r14+r15*2], '.'
    je .device
    inc r15
    jmp .base
.device:
    cmp r15, 3
    jb .next
    cmp r15, 4
    ja .next
    movzx eax, word [r14]
    movzx ecx, word [r14+2]
    movzx edx, word [r14+4]
    or eax, 32
    or ecx, 32
    or edx, 32
    shl ecx, 8
    shl edx, 16
    or eax, ecx
    or eax, edx
    cmp r15, 4
    je .port
    cmp eax, 'con'
    je .bad
    cmp eax, 'prn'
    je .bad
    cmp eax, 'aux'
    je .bad
    cmp eax, 'nul'
    je .bad
    jmp .next
.port:
    cmp eax, 'com'
    je .digit
    cmp eax, 'lpt'
    jne .next
.digit:
    movzx eax, word [r14+6]
    cmp eax, 0xB9                   ; superscript 1/2/3 are also device digits
    je .bad
    cmp eax, 0xB2
    je .bad
    cmp eax, 0xB3
    je .bad
    sub eax, '1'
    cmp eax, 8
    jbe .bad
.next:
    cmp r12, rdi
    jae .ok
    inc r12
    mov rbx, r12
    jmp .char
.ok:
    mov eax, 1
    EPILOG 64
.bad:
    xor eax, eax
    EPILOG 64
