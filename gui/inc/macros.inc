; macros.inc - shared macros for the Win64 GUI build
;
; invoke fn, a0, a1, ...   call with Win64 ABI; operand prefixes:
;     &expr   -> lea reg,[expr]      (expr has no brackets, e.g. &s_title or &rsp+LV_RECT)
;     #expr   -> 32-bit load (mov r32,expr), zero extended
;     else    -> mov reg, operand (64-bit)
; args 0-3 go to rcx,rdx,r8,r9; the rest to [rsp+32+8*(i-4)] via rax.
; Callers must not use rcx/rdx/r8/r9/rax as *sources* of later arguments.

default rel
bits 64

%macro PROLOG 1
    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, %1+8
%endmacro

%macro EPILOG 1
    add rsp, %1+8
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
%endmacro

%macro ARGSET 2
  %defstr __as %2
  %substr __c1 __as 1,1
  %ifidn __c1, "&"
    %substr __rest __as 2,-1
    %deftok __tk __rest
    %assign __mode 1
  %elifidn __c1, "#"
    %substr __rest __as 2,-1
    %deftok __tk __rest
    %assign __mode 2
  %else
    %define __tk %2
    %assign __mode 0
  %endif
  %if %1 == 0
    %define __d64 rcx
    %define __d32 ecx
  %elif %1 == 1
    %define __d64 rdx
    %define __d32 edx
  %elif %1 == 2
    %define __d64 r8
    %define __d32 r8d
  %elif %1 == 3
    %define __d64 r9
    %define __d32 r9d
  %endif
  %if %1 < 4
    %if __mode == 1
      lea __d64, [__tk]
    %elif __mode == 2
      mov __d32, __tk
    %else
      mov __d64, __tk
    %endif
  %else
    %if __mode == 1
      lea rax, [__tk]
    %elif __mode == 2
      mov eax, __tk
    %else
      mov rax, __tk
    %endif
    mov [rsp+32+8*(%1-4)], rax
  %endif
%endmacro

%macro invoke 1-*
  %define __fn %1
  %assign __i 0
  %rep %0-1
    %rotate 1
    ARGSET __i, %1
    %assign __i __i+1
  %endrep
  call __fn
%endmacro

; UTF-16 string constant
%macro WSTR 2
%1: dw __utf16__(%2), 0
%endmacro

; The C runtime's _snwprintf leaves the buffer without a terminator when the text is as long as, or longer
; than, the count, and then returns a negative length. Every module formats through bc_snwprintf (engine.asm)
; instead, which always terminates within the count and returns the length actually written.
%ifndef SNW_IMPL
%define _snwprintf bc_snwprintf
%endif

; zero-extending constants helper
%define NULL 0
