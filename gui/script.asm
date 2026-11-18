; script.asm - AU-04 batch script language:  bcgui /script file.bcscript
;   ; comment  /  # comment
;   set NAME=value            %NAME% is expanded in later lines (also %ERRORLEVEL%, %SCRIPTDIR%)
;   echo text                 write a line to <script>.log
;   run <bcgui arguments>     run one comparison job headless (a "/exit" is added when missing), waits, sets ERRORLEVEL
;   if errorlevel N <command> run the command when the last job returned N or more
;   goto label  /  :label
;   exit [code]
%include "inc/macros.inc"
%include "inc/win.inc"

extern GetCommandLineW, CommandLineToArgvW, LocalFree, GetModuleFileNameW, CreateProcessW, WaitForSingleObject
extern GetExitCodeProcess, CloseHandle, CreateFileW, WriteFile, SetFilePointer, MultiByteToWideChar, WideCharToMultiByte
extern malloc, calloc, free, memset, wcslen, wcscpy, wcscat, wcsncpy, wcsrchr, _wcsicmp, _wcsnicmp, _snwprintf, wcstol
extern read_file_bytes, g_exitcode

global script_check

%define SC_LINES  0
%define SC_N      8
%define SC_PC     16
%define SC_ERR    24
%define SC_EXIT   28
%define SC_LOG    32
%define SC_STOP   40
%define SC_VARS   48
%define SC_NVARS  56
%define SC_DIR    64
%define SC_EXE    72
%define SC_SIZE   96
%define OPEN_ALWAYS 4

%define VAR_NAME  64                 ; bytes (32 wchar)
%define VAR_VAL   1040               ; bytes (520 wchar)
%define VAR_SZ    1104
%define MAX_VARS  32

section .rdata
WSTR sw_script, "/script"
WSTR sw_script2, "script"
WSTR w_set, "set"
WSTR w_echo, "echo"
WSTR w_run, "run"
WSTR w_if, "if"
WSTR w_goto, "goto"
WSTR w_exit, "exit"
WSTR w_errorlevel, "errorlevel"
WSTR v_errorlevel, "ERRORLEVEL"
WSTR v_scriptdir, "SCRIPTDIR"
WSTR s_exitsw, "/exit"
WSTR f_dec, "%d"
WSTR f_quote, '"%s" %s'
WSTR f_runlog, "run: %s -> %d"
WSTR f_unknown, "unknown command: %s"
WSTR f_nolabel, "label not found: %s"
WSTR s_crlf, ""
WSTR s_empty, ""

section .text

; sc_log(SC, wide text)
sc_log:
    PROLOG 4224
    mov rbx, rcx
    mov rsi, rdx
    cmp qword [rbx+SC_LOG], 0
    je .d
    invoke WideCharToMultiByte, 65001, 0, rsi, -1, &rsp+96, 4000, 0, 0
    test eax, eax
    jle .d
    dec eax
    mov word [rsp+96+rax], 0x0A0D
    lea r12d, [rax+2]
    invoke WriteFile, [rbx+SC_LOG], &rsp+96, r12, &rsp+4150, 0
.d:
    EPILOG 4224

; sc_word(src, out) -> rax = src after the word and following blanks ; out holds up to 255 wchar (NUL terminated)
sc_word:
    xor r8d, r8d
.skip:
    cmp word [rcx], ' '
    je .adv
    cmp word [rcx], 9
    jne .l
.adv:
    add rcx, 2
    jmp .skip
.l:
    movzx eax, word [rcx]
    test eax, eax
    jz .e
    cmp eax, ' '
    je .e
    cmp eax, 9
    je .e
    cmp r8d, 254
    jae .n
    mov [rdx+r8*2], ax
    inc r8d
.n:
    add rcx, 2
    jmp .l
.e:
    mov word [rdx+r8*2], 0
.s2:
    cmp word [rcx], ' '
    je .a2
    cmp word [rcx], 9
    jne .done
.a2:
    add rcx, 2
    jmp .s2
.done:
    mov rax, rcx
    ret

; sc_getvar(SC, name) -> wchar* value or 0
sc_getvar:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    invoke _wcsicmp, rsi, &v_errorlevel
    test eax, eax
    jnz .u
    lea rdi, [rbx+SC_EXE+8]            ; scratch for the decimal text lives after the exe pointer
    mov rdi, [rbx+SC_EXE]
    lea rdi, [rdi+1040]
    mov r13d, [rbx+SC_ERR]
    invoke _snwprintf, rdi, 20, &f_dec, r13
    mov rax, rdi
    EPILOG 64
.u:
    invoke _wcsicmp, rsi, &v_scriptdir
    test eax, eax
    jnz .v
    mov rax, [rbx+SC_DIR]
    EPILOG 64
.v:
    xor edi, edi
.l:
    cmp rdi, [rbx+SC_NVARS]
    jae .no
    mov rax, rdi
    imul rax, VAR_SZ
    add rax, [rbx+SC_VARS]
    mov r12, rax
    invoke _wcsicmp, r12, rsi
    test eax, eax
    jz .hit
    inc rdi
    jmp .l
.hit:
    lea rax, [r12+VAR_NAME]
    EPILOG 64
.no:
    xor eax, eax
    EPILOG 64

; sc_setvar(SC, name, value)
sc_setvar:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r13, r8
    xor edi, edi
.l:
    cmp rdi, [rbx+SC_NVARS]
    jae .new
    mov rax, rdi
    imul rax, VAR_SZ
    add rax, [rbx+SC_VARS]
    mov r12, rax
    invoke _wcsicmp, r12, rsi
    test eax, eax
    jz .set
    inc rdi
    jmp .l
.new:
    cmp rdi, MAX_VARS
    jae .d
    inc qword [rbx+SC_NVARS]
    mov rax, rdi
    imul rax, VAR_SZ
    add rax, [rbx+SC_VARS]
    mov r12, rax
    invoke wcsncpy, r12, rsi, 31
    mov word [r12+62], 0
.set:
    lea rcx, [r12+VAR_NAME]
    invoke wcsncpy, rcx, r13, 519
    mov word [r12+VAR_NAME+1038], 0
.d:
    EPILOG 64

; sc_expand(SC, src, dst): %NAME% substitution (dst: 1100 wchar)
sc_expand:
    PROLOG 192
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, r8
    xor r12d, r12d                     ; out index
.l:
    movzx eax, word [rsi]
    test eax, eax
    jz .e
    cmp r12d, 1090
    jae .e
    cmp eax, '%'
    jne .copy
    ; find the closing %
    lea r13, [rsi+2]
    xor ecx, ecx
.f:
    movzx eax, word [r13+rcx*2]
    test eax, eax
    jz .copy1
    cmp eax, '%'
    je .got
    cmp ecx, 30
    jae .copy1
    inc ecx
    jmp .f
.got:
    test ecx, ecx
    jz .copy1
    mov r14d, ecx
    lea r15, [rsp+96]
    xor edx, edx
.cn:
    cmp edx, r14d
    jae .cd
    movzx eax, word [r13+rdx*2]
    mov [r15+rdx*2], ax
    inc edx
    jmp .cn
.cd:
    mov word [r15+rdx*2], 0
    invoke sc_getvar, rbx, r15
    test rax, rax
    jz .copy1
    mov r15, rax
.v:
    movzx eax, word [r15]
    test eax, eax
    jz .vd
    cmp r12d, 1090
    jae .vd
    mov [rdi+r12*2], ax
    inc r12d
    add r15, 2
    jmp .v
.vd:
    lea rsi, [r13+r14*2+2]
    jmp .l
.copy1:
    mov eax, '%'
.copy:
    mov [rdi+r12*2], ax
    inc r12d
    add rsi, 2
    jmp .l
.e:
    mov word [rdi+r12*2], 0
    EPILOG 192

; sc_run(SC, args)
sc_run:
    PROLOG 2800
    mov rbx, rcx
    mov rsi, rdx
    ; command line: "exe" args [/exit]
    invoke _snwprintf, &rsp+96, 1090, &f_quote, [rbx+SC_EXE], rsi
    mov word [rsp+96+2180], 0
    ; append /exit when missing
    xor edi, edi
    lea r12, [rsp+96]
.ex:
    movzx eax, word [r12+rdi*2]
    test eax, eax
    jz .noexit
    invoke _wcsnicmp, &r12+rdi*2, &s_exitsw, 5
    test eax, eax
    jz .have
    inc edi
    jmp .ex
.noexit:
    invoke wcscat, &rsp+96, &s_exitsw_sp
.have:
    invoke memset, &rsp+2400, 0, 104
    mov dword [rsp+2400], 104
    invoke CreateProcessW, 0, &rsp+96, 0, 0, 0, 0, 0, 0, &rsp+2400, &rsp+2520
    test eax, eax
    jz .fail
    invoke WaitForSingleObject, [rsp+2520], -1
    mov dword [rsp+2560], 0
    invoke GetExitCodeProcess, [rsp+2520], &rsp+2560
    invoke CloseHandle, [rsp+2520]
    invoke CloseHandle, [rsp+2528]
    mov eax, [rsp+2560]
    mov [rbx+SC_ERR], eax
    jmp .log
.fail:
    mov dword [rbx+SC_ERR], 255
.log:
    mov r13d, [rbx+SC_ERR]
    invoke _snwprintf, &rsp+1300, 500, &f_runlog, rsi, r13
    invoke sc_log, rbx, &rsp+1300
    EPILOG 2800

section .rdata
WSTR s_exitsw_sp, " /exit"
section .text

; sc_findlabel(SC, name) -> eax line index or -1
sc_findlabel:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    xor edi, edi
.l:
    cmp rdi, [rbx+SC_N]
    jae .no
    mov rax, [rbx+SC_LINES]
    mov r12, [rax+rdi*8]
.sk:
    cmp word [r12], ' '
    jne .t
    add r12, 2
    jmp .sk
.t:
    cmp word [r12], ':'
    jne .n
    lea rcx, [r12+2]
    invoke _wcsicmp, rcx, rsi
    test eax, eax
    jz .hit
.n:
    inc rdi
    jmp .l
.hit:
    mov eax, edi
    EPILOG 64
.no:
    mov eax, -1
    EPILOG 64

; sc_exec(SC, expanded line)
sc_exec:
    PROLOG 1200
    mov rbx, rcx
    mov rsi, rdx
    invoke sc_word, rsi, &rsp+96
    mov rdi, rax                       ; arguments
    cmp word [rsp+96], 0
    je .d
    cmp word [rsp+96], ';'
    je .d
    cmp word [rsp+96], '#'
    je .d
    cmp word [rsp+96], ':'
    je .d
    invoke _wcsicmp, &rsp+96, &w_set
    test eax, eax
    jz .set
    invoke _wcsicmp, &rsp+96, &w_echo
    test eax, eax
    jz .echo
    invoke _wcsicmp, &rsp+96, &w_run
    test eax, eax
    jz .run
    invoke _wcsicmp, &rsp+96, &w_if
    test eax, eax
    jz .if
    invoke _wcsicmp, &rsp+96, &w_goto
    test eax, eax
    jz .goto
    invoke _wcsicmp, &rsp+96, &w_exit
    test eax, eax
    jz .exit
    invoke _snwprintf, &rsp+600, 250, &f_unknown, &rsp+96
    invoke sc_log, rbx, &rsp+600
    jmp .d
.set:
    ; NAME=value
    invoke wcschr_, rdi
    jmp .d
.echo:
    invoke sc_log, rbx, rdi
    jmp .d
.run:
    invoke sc_run, rbx, rdi
    jmp .d
.if:
    invoke sc_word, rdi, &rsp+96
    mov rdi, rax
    invoke _wcsicmp, &rsp+96, &w_errorlevel
    test eax, eax
    jnz .d
    invoke sc_word, rdi, &rsp+96
    mov rdi, rax
    invoke wcstol, &rsp+96, 0, 10
    cmp [rbx+SC_ERR], eax
    jl .d
    invoke sc_exec, rbx, rdi
    jmp .d
.goto:
    invoke sc_word, rdi, &rsp+96
    invoke sc_findlabel, rbx, &rsp+96
    cmp eax, -1
    jne .gset
    invoke _snwprintf, &rsp+600, 250, &f_nolabel, &rsp+96
    invoke sc_log, rbx, &rsp+600
    mov qword [rbx+SC_STOP], 1
    jmp .d
.gset:
    mov [rbx+SC_PC], rax
    jmp .d
.exit:
    mov qword [rbx+SC_STOP], 1
    cmp word [rdi], 0
    je .d
    invoke wcstol, rdi, 0, 10
    mov [rbx+SC_ERR], eax
.d:
    EPILOG 1200

; wcschr_(SC in rbx via caller) - handles "set NAME=value"
wcschr_:
    PROLOG 1200
    mov rsi, rcx
    xor edi, edi
.f:
    movzx eax, word [rsi+rdi*2]
    test eax, eax
    jz .d
    cmp eax, '='
    je .got
    inc edi
    jmp .f
.got:
    cmp edi, 31
    ja .d
    xor ecx, ecx
.c:
    cmp ecx, edi
    jae .cd
    movzx eax, word [rsi+rcx*2]
    mov [rsp+96+rcx*2], ax
    inc ecx
    jmp .c
.cd:
    mov word [rsp+96+rdi*2], 0
    ; trailing blanks of the name
.tr:
    test edi, edi
    jz .go
    cmp word [rsp+96+rdi*2-2], ' '
    jne .go
    dec edi
    mov word [rsp+96+rdi*2], 0
    jmp .tr
.go:
    mov rcx, [g_sc_cur]
    lea rdx, [rsp+96]
    lea r8, [rsi]
.v:
    cmp word [r8], '='
    je .vv
    add r8, 2
    jmp .v
.vv:
    add r8, 2
    invoke sc_setvar, rcx, rdx, r8
.d:
    EPILOG 1200

section .bss
alignb 8
g_sc_cur: resq 1
section .text

; script_main(path) -> exit code
script_main:
    PROLOG 2400
    mov r12, rcx
    invoke calloc, 1, SC_SIZE
    mov rbx, rax
    mov [g_sc_cur], rbx
    invoke calloc, 1, VAR_SZ*MAX_VARS
    mov [rbx+SC_VARS], rax
    invoke calloc, 1, 4400
    mov [rbx+SC_EXE], rax
    invoke GetModuleFileNameW, 0, rax, 520
    ; script directory
    invoke wcslen, r12
    lea rcx, [rax*2+16]
    call malloc
    mov [rbx+SC_DIR], rax
    invoke wcscpy, rax, r12
    invoke wcsrchr, [rbx+SC_DIR], 0x5C
    test rax, rax
    jz .nodir
    mov word [rax], 0
    jmp .dirok
.nodir:
    mov rax, [rbx+SC_DIR]
    mov word [rax], '.'
    mov word [rax+2], 0
.dirok:
    ; log file next to the script
    invoke _snwprintf, &rsp+96, 1000, &f_logname, r12
    invoke CreateFileW, &rsp+96, GENERIC_WRITE, 1, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0
    cmp rax, -1
    je .nolog
    mov [rbx+SC_LOG], rax
    invoke SetFilePointer, rax, 0, 0, 2
.nolog:
    ; read the script (UTF-8)
    invoke read_file_bytes, r12, &rsp+1200
    test rax, rax
    jz .fail
    mov r13, rax
    mov r14, [rsp+1200]
    xor esi, esi
    cmp r14, 3
    jb .bom
    cmp byte [r13], 0xEF
    jne .bom
    cmp byte [r13+1], 0xBB
    jne .bom
    cmp byte [r13+2], 0xBF
    jne .bom
    mov esi, 3
.bom:
    lea rcx, [r14*2+8]
    call malloc
    mov r15, rax
    mov rax, r14
    sub rax, rsi
    lea rdi, [r13+rsi]
    lea rbp, [r14+1]
    invoke MultiByteToWideChar, 65001, 0, rdi, rax, r15, rbp
    test eax, eax
    jns .term
    xor eax, eax
.term:
    mov word [r15+rax*2], 0
    invoke free, r13
    ; split lines
    invoke calloc, 8192, 8
    mov [rbx+SC_LINES], rax
    mov rsi, r15
    xor edi, edi
.sp:
    cmp word [rsi], 0
    je .spd
    cmp rdi, 8000
    jae .spd
    mov rax, [rbx+SC_LINES]
    mov [rax+rdi*8], rsi
    inc rdi
.sl:
    movzx eax, word [rsi]
    test eax, eax
    jz .spd
    cmp eax, 10
    je .eol
    cmp eax, 13
    je .eol
    add rsi, 2
    jmp .sl
.eol:
    mov word [rsi], 0
    add rsi, 2
    cmp eax, 13
    jne .sp
    cmp word [rsi], 10
    jne .sp
    add rsi, 2
    jmp .sp
.spd:
    mov [rbx+SC_N], rdi
    ; run
    mov qword [rbx+SC_PC], 0
.loop:
    cmp qword [rbx+SC_STOP], 0
    jne .done
    mov rax, [rbx+SC_PC]
    cmp rax, [rbx+SC_N]
    jae .done
    inc qword [rbx+SC_PC]
    mov rcx, [rbx+SC_LINES]
    mov rdx, [rcx+rax*8]
    invoke sc_expand, rbx, rdx, &rsp+96
    invoke sc_exec, rbx, &rsp+96
    jmp .loop
.done:
    mov eax, [rbx+SC_ERR]
    mov r12d, eax
    cmp qword [rbx+SC_LOG], 0
    je .ret
    invoke CloseHandle, [rbx+SC_LOG]
.ret:
    mov eax, r12d
    EPILOG 2400
.fail:
    mov dword [rbx+SC_ERR], 2        ; .done takes the exit code from here
    invoke sc_log, rbx, &s_unreadable
    jmp .done

section .rdata
WSTR f_logname, "%s.log"
WSTR s_unreadable, "script could not be read"
section .text

; script_check() -> eax 1 when "/script file" was given (the script was run, g_exitcode set)
script_check:
    PROLOG 160
    invoke GetCommandLineW
    invoke CommandLineToArgvW, rax, &rsp+96
    test rax, rax
    jz .no
    mov rbx, rax
    cmp dword [rsp+96], 3
    jl .free
    invoke _wcsicmp, [rbx+8], &sw_script
    test eax, eax
    jz .go
    invoke _wcsicmp, [rbx+8], &sw_script2
    test eax, eax
    jnz .free
.go:
    invoke script_main, [rbx+16]
    movsxd rax, eax
    mov [g_exitcode], rax
    invoke LocalFree, rbx
    mov eax, 1
    EPILOG 160
.free:
    invoke LocalFree, rbx
.no:
    xor eax, eax
    EPILOG 160
