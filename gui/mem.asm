; mem.asm - the allocator every module uses (macros.inc maps malloc / calloc / realloc to these)
;
;   Most of the code stores the result of an allocation and uses it without a test. When the memory really is
;   gone that used to be a write through address 0 somewhere far from the cause. Now a request below
;   MEM_BIG that fails ends the program with a message. A request of MEM_BIG bytes and more (a whole file,
;   a table of a large comparison) returns 0 as before: those callers test it and report the file.
%define BC_MEM_IMPL
%include "inc/macros.inc"
%include "inc/win.inc"

extern malloc, calloc, realloc, MessageBoxW, ExitProcess

global bc_malloc, bc_calloc, bc_realloc, g_mem_batch

%define MEM_BIG 0x2000000            ; 32 MB

section .rdata
WSTR s_mem_title, "BeCompare"
WSTR s_mem_out, "메모리가 부족해 계속할 수 없습니다. 프로그램을 끝냅니다."

section .bss
g_mem_batch: resq 1                  ; address of the batch flags (set by WinMain): bit 2 = unattended

section .text

; mem_out(): no return
mem_out:
    sub rsp, 40
    mov rax, [g_mem_batch]           ; unattended: no box that nobody answers
    test rax, rax
    jz .box
    test qword [rax], 4
    jnz .exit
.box:
    xor ecx, ecx
    lea rdx, [s_mem_out]
    lea r8, [s_mem_title]
    mov r9d, 0x10                    ; MB_ICONERROR
    call MessageBoxW
.exit:
    mov ecx, 3
    call ExitProcess

bc_malloc:
    PROLOG 64
    mov rbx, rcx
    call malloc
    test rax, rax
    jnz .d
    test rbx, rbx                    ; malloc(0) may return 0
    jz .d
    cmp rbx, MEM_BIG
    jae .d
    call mem_out
.d:
    EPILOG 64

bc_calloc:
    PROLOG 64
    mov rbx, rcx
    imul rbx, rdx
    call calloc
    test rax, rax
    jnz .d
    test rbx, rbx
    jz .d
    cmp rbx, MEM_BIG
    jae .d
    call mem_out
.d:
    EPILOG 64

bc_realloc:
    PROLOG 64
    mov rbx, rdx
    call realloc
    test rax, rax
    jnz .d
    test rbx, rbx                    ; realloc(p, 0) frees
    jz .d
    cmp rbx, MEM_BIG
    jae .d
    call mem_out
.d:
    EPILOG 64
