; text_syn.inc - TC-04 syntax highlighting: extension based language pick, one-pass tokenizer, colours via RichEdit
extern strstr, rich_fg

section .rdata
WSTR s_c_syn, "구문 강조"
WSTR sx0, ".c"
WSTR sx1, ".h"
WSTR sx2, ".cpp"
WSTR sx3, ".hpp"
WSTR sx4, ".cc"
WSTR sx5, ".cxx"
WSTR sx6, ".hxx"
WSTR sx7, ".cs"
WSTR sx8, ".java"
WSTR sx9, ".js"
WSTR sx10, ".mjs"
WSTR sx11, ".ts"
WSTR sx12, ".jsx"
WSTR sx13, ".tsx"
WSTR sx14, ".go"
WSTR sx15, ".rs"
WSTR sx16, ".php"
WSTR sx17, ".swift"
WSTR sx18, ".kt"
WSTR sx19, ".kts"
WSTR sx20, ".scala"
WSTR sx21, ".dart"
WSTR sx22, ".m"
WSTR sx23, ".mm"
WSTR sx24, ".css"
WSTR sx25, ".scss"
WSTR sx26, ".less"
WSTR sx27, ".py"
WSTR sx28, ".pyw"
WSTR sx29, ".sh"
WSTR sx30, ".bash"
WSTR sx31, ".zsh"
WSTR sx32, ".yml"
WSTR sx33, ".yaml"
WSTR sx34, ".toml"
WSTR sx35, ".rb"
WSTR sx36, ".pl"
WSTR sx37, ".pm"
WSTR sx38, ".ps1"
WSTR sx39, ".psm1"
WSTR sx40, ".cmake"
WSTR sx41, ".r"
WSTR sx42, ".conf"
WSTR sx43, ".properties"
WSTR sx44, ".gitignore"
WSTR sx45, ".asm"
WSTR sx46, ".inc"
WSTR sx47, ".nasm"
WSTR sx48, ".s"
WSTR sx49, ".ini"
WSTR sx50, ".cfg"
WSTR sx51, ".reg"
WSTR sx52, ".desktop"
WSTR sx53, ".xml"
WSTR sx54, ".html"
WSTR sx55, ".htm"
WSTR sx56, ".xhtml"
WSTR sx57, ".xaml"
WSTR sx58, ".svg"
WSTR sx59, ".csproj"
WSTR sx60, ".vcxproj"
WSTR sx61, ".props"
WSTR sx62, ".targets"
WSTR sx63, ".config"
WSTR sx64, ".resx"
WSTR sx65, ".plist"
WSTR sx66, ".sql"
WSTR sx67, ".lua"
WSTR sx68, ".hs"
WSTR sx69, ".ada"
WSTR sx70, ".vhd"
WSTR sx71, ".vhdl"
WSTR sx72, ".json"
WSTR sx73, ".jsonc"
align 8
syn_exts:
    dq sx0, 1
    dq sx1, 1
    dq sx2, 1
    dq sx3, 1
    dq sx4, 1
    dq sx5, 1
    dq sx6, 1
    dq sx7, 1
    dq sx8, 1
    dq sx9, 1
    dq sx10, 1
    dq sx11, 1
    dq sx12, 1
    dq sx13, 1
    dq sx14, 1
    dq sx15, 1
    dq sx16, 1
    dq sx17, 1
    dq sx18, 1
    dq sx19, 1
    dq sx20, 1
    dq sx21, 1
    dq sx22, 1
    dq sx23, 1
    dq sx24, 1
    dq sx25, 1
    dq sx26, 1
    dq sx27, 2
    dq sx28, 2
    dq sx29, 2
    dq sx30, 2
    dq sx31, 2
    dq sx32, 2
    dq sx33, 2
    dq sx34, 2
    dq sx35, 2
    dq sx36, 2
    dq sx37, 2
    dq sx38, 2
    dq sx39, 2
    dq sx40, 2
    dq sx41, 2
    dq sx42, 2
    dq sx43, 2
    dq sx44, 2
    dq sx45, 3
    dq sx46, 3
    dq sx47, 3
    dq sx48, 3
    dq sx49, 3
    dq sx50, 3
    dq sx51, 3
    dq sx52, 3
    dq sx53, 4
    dq sx54, 4
    dq sx55, 4
    dq sx56, 4
    dq sx57, 4
    dq sx58, 4
    dq sx59, 4
    dq sx60, 4
    dq sx61, 4
    dq sx62, 4
    dq sx63, 4
    dq sx64, 4
    dq sx65, 4
    dq sx66, 5
    dq sx67, 5
    dq sx68, 5
    dq sx69, 5
    dq sx70, 5
    dq sx71, 5
    dq sx72, 6
    dq sx73, 6
    dq 0, 0
syn_kw: db "|if|else|elif|for|foreach|while|do|switch|case|break|continue|return|int|char|long|short|float|double|void|unsigned|signed|const|static|struct|class|enum|union|typedef|sizeof|new|delete|this|self|public|private|protected|namespace|using|include|define|ifdef|ifndef|endif|pragma|var|let|function|func|fn|def|true|false|null|none|nil|import|export|from|as|async|await|try|catch|except|finally|throw|raise|default|extends|implements|interface|package|abstract|virtual|override|final|bool|string|byte|object|in|is|not|and|or|with|yield|lambda|pass|global|extern|inline|volatile|goto|select|insert|update|create|table|where|join|inner|outer|left|right|on|order|by|group|having|values|set|into|alter|drop|index|primary|key|begin|end|then|unique|distinct|limit|use|mut|impl|pub|match|loop|mod|crate|trait|type|", 0

section .text

; syn_lang(path) -> eax language (0 none, 1 C-like, 2 hash comments, 3 asm/ini, 4 markup, 5 sql/lua, 6 json)
syn_lang:
    PROLOG 64
    mov rsi, rcx
    invoke wcsrchr, rsi, '.'
    test rax, rax
    jz .none
    mov rbx, rax
    lea rdi, [syn_exts]
.l:
    mov rax, [rdi]
    test rax, rax
    jz .none
    invoke _wcsicmp, rbx, rax
    test eax, eax
    jz .hit
    add rdi, 16
    jmp .l
.hit:
    mov eax, [rdi+8]
    EPILOG 64
.none:
    xor eax, eax
    EPILOG 64

%macro SYNP 3                        ; start reg, end reg, colour slot
    mov rcx, rbx
    mov rdx, %1
    mov r8, %2
    mov r9d, [rsp+%3]
    call rich_fg
    dec r15d
%endmacro

; syn_run(hwnd, text, len, lang)
syn_run:
    PROLOG 192
    mov rbx, rcx
    mov r12, rdx
    mov r13, r8
    mov r14d, r9d
    mov dword [rsp+96], 0x008000      ; comment
    mov dword [rsp+100], 0x1515A3     ; string
    mov dword [rsp+104], 0xFF0000     ; keyword
    mov dword [rsp+108], 0x588609     ; number
    cmp qword [g_dark], 0
    je .go
    mov dword [rsp+96], 0x55996A
    mov dword [rsp+100], 0x7891CE
    mov dword [rsp+104], 0xD69C56
    mov dword [rsp+108], 0xA8CEB5
.go:
    xor esi, esi
    mov r15d, 60000                   ; paint budget (RichEdit formatting is slow)
.top:
    cmp rsi, r13
    jae .done
    test r15d, r15d
    jz .done
    movzx eax, word [r12+rsi*2]
    cmp r14d, 4
    je .xml
    cmp r14d, 1
    jne .nc1
    cmp eax, '/'
    jne .nc1
    movzx ecx, word [r12+rsi*2+2]
    cmp ecx, '/'
    je .lcomment
    cmp ecx, '*'
    je .bcomment
.nc1:
    cmp r14d, 2
    jne .nc2
    cmp eax, '#'
    je .lcomment
.nc2:
    cmp r14d, 3
    jne .nc3
    cmp eax, ';'
    je .lcomment
.nc3:
    cmp r14d, 5
    jne .nc4
    cmp eax, '-'
    jne .nc4
    cmp word [r12+rsi*2+2], '-'
    je .lcomment
.nc4:
    cmp eax, '"'
    je .str
    cmp eax, 39
    je .str
    cmp eax, '0'
    jb .nonum
    cmp eax, '9'
    jbe .num
.nonum:
    cmp eax, '_'
    je .id
    mov ecx, eax
    or ecx, 0x20
    cmp ecx, 'a'
    jb .skip
    cmp ecx, 'z'
    jbe .id
.skip:
    inc rsi
    jmp .top
.lcomment:
    mov rdi, rsi
.lc:
    cmp rdi, r13
    jae .lcd
    cmp word [r12+rdi*2], 13
    je .lcd
    inc rdi
    jmp .lc
.lcd:
    SYNP rsi, rdi, 96
    mov rsi, rdi
    jmp .top
.bcomment:
    lea rdi, [rsi+2]
.bc:
    cmp rdi, r13
    jae .bcd
    cmp word [r12+rdi*2], '*'
    jne .bcn
    cmp word [r12+rdi*2+2], '/'
    jne .bcn
    add rdi, 2
    jmp .bcd
.bcn:
    inc rdi
    jmp .bc
.bcd:
    cmp rdi, r13
    jbe .bcp
    mov rdi, r13
.bcp:
    SYNP rsi, rdi, 96
    mov rsi, rdi
    jmp .top
.str:
    lea rdi, [rsi+1]
    mov ebp, eax
.sc:
    cmp rdi, r13
    jae .scd
    movzx eax, word [r12+rdi*2]
    cmp eax, 13
    je .scd
    cmp eax, ebp
    je .sclose
    cmp eax, 0x5C
    jne .scn
    cmp r14d, 3
    je .scn
    inc rdi
.scn:
    inc rdi
    jmp .sc
.sclose:
    inc rdi
.scd:
    cmp rdi, r13
    jbe .scp
    mov rdi, r13
.scp:
    SYNP rsi, rdi, 100
    mov rsi, rdi
    jmp .top
.num:
    mov rdi, rsi
.nl:
    cmp rdi, r13
    jae .nd
    movzx eax, word [r12+rdi*2]
    cmp eax, '.'
    je .nn
    cmp eax, '_'
    je .nn
    cmp eax, '0'
    jb .nd
    cmp eax, '9'
    jbe .nn
    or eax, 0x20
    cmp eax, 'a'
    jb .nd
    cmp eax, 'z'
    ja .nd
.nn:
    inc rdi
    jmp .nl
.nd:
    SYNP rsi, rdi, 108
    mov rsi, rdi
    jmp .top
.id:
    mov rdi, rsi
.il:
    cmp rdi, r13
    jae .idone
    movzx eax, word [r12+rdi*2]
    cmp eax, '_'
    je .in
    cmp eax, '0'
    jb .idone
    cmp eax, '9'
    jbe .in
    or eax, 0x20
    cmp eax, 'a'
    jb .idone
    cmp eax, 'z'
    ja .idone
.in:
    inc rdi
    jmp .il
.idone:
    mov rax, rdi
    sub rax, rsi
    cmp rax, 14
    ja .idnext
    cmp r14d, 3
    je .idnext
    mov byte [rsp+112], '|'
    xor ecx, ecx
.ic:
    cmp rcx, rax
    jae .icd
    lea rdx, [rsi+rcx]
    movzx edx, word [r12+rdx*2]
    or edx, 0x20
    lea r8, [rsp+113]
    mov [r8+rcx], dl
    inc rcx
    jmp .ic
.icd:
    lea r8, [rsp+113]
    mov byte [r8+rax], '|'
    mov byte [r8+rax+1], 0
    invoke strstr, &syn_kw, &rsp+112
    test rax, rax
    jz .idnext
    SYNP rsi, rdi, 104
.idnext:
    mov rsi, rdi
    jmp .top
.xml:
    cmp eax, '<'
    jne .skip
    cmp word [r12+rsi*2+2], '!'
    jne .tag
    cmp word [r12+rsi*2+4], '-'
    jne .tag
    cmp word [r12+rsi*2+6], '-'
    jne .tag
    lea rdi, [rsi+4]
.xc:
    cmp rdi, r13
    jae .xcd
    cmp word [r12+rdi*2], '-'
    jne .xcn
    cmp word [r12+rdi*2+2], '-'
    jne .xcn
    cmp word [r12+rdi*2+4], '>'
    jne .xcn
    add rdi, 3
    jmp .xcd
.xcn:
    inc rdi
    jmp .xc
.xcd:
    cmp rdi, r13
    jbe .xcp
    mov rdi, r13
.xcp:
    SYNP rsi, rdi, 96
    mov rsi, rdi
    jmp .top
.tag:
    lea rdi, [rsi+1]
    xor ebp, ebp
.tg:
    cmp rdi, r13
    jae .tgd
    movzx eax, word [r12+rdi*2]
    test ebp, ebp
    jz .tq
    cmp eax, ebp
    jne .tn
    xor ebp, ebp
    jmp .tn
.tq:
    cmp eax, '"'
    je .tsq
    cmp eax, 39
    je .tsq
    cmp eax, '>'
    je .tclose
    jmp .tn
.tsq:
    mov ebp, eax
.tn:
    inc rdi
    jmp .tg
.tclose:
    inc rdi
.tgd:
    SYNP rsi, rdi, 104
    lea r10, [rsi+1]
    mov [rsp+144], rdi
.tv:
    cmp r10, [rsp+144]
    jae .tvd
    movzx eax, word [r12+r10*2]
    cmp eax, '"'
    je .tvq
    cmp eax, 39
    je .tvq
    inc r10
    jmp .tv
.tvq:
    mov [rsp+168], eax
    lea r11, [r10+1]
.tvc:
    cmp r11, [rsp+144]
    jae .tvp
    movzx eax, word [r12+r11*2]
    cmp eax, [rsp+168]
    je .tvp1
    inc r11
    jmp .tvc
.tvp1:
    inc r11
.tvp:
    mov [rsp+152], r11
    mov [rsp+160], r10
    mov rdx, r10
    mov r8, r11
    mov rcx, rbx
    mov r9d, [rsp+100]
    call rich_fg
    dec r15d
    mov r10, [rsp+152]
    jmp .tv
.tvd:
    mov rsi, [rsp+144]
    jmp .top
.done:
    EPILOG 192

; tv_syn_apply(hwnd, tv): (re)colour both panes from their current text
tv_syn_apply:
    PROLOG 96
    mov rsi, rcx
    mov rbx, rdx
    ; colouring is not an edit: the modify flags are put back at the end
    ; (switching syntax colours on or off used to mark both files as changed)
    invoke SendMessageW, [rbx+TV_HA], EM_GETMODIFY, 0, 0
    mov [rsp+64], rax
    invoke SendMessageW, [rbx+TV_HB], EM_GETMODIFY, 0, 0
    mov [rsp+72], rax
    invoke SendMessageW, [rbx+TV_HA], WM_SETREDRAW, 0, 0
    invoke SendMessageW, [rbx+TV_HB], WM_SETREDRAW, 0, 0
    invoke rich_theme, [rbx+TV_HA]
    invoke rich_theme, [rbx+TV_HB]
    invoke get_chk, rsi, ID_CHK_SYN
    test eax, eax
    jz .off
    invoke syn_lang, &rbx+TV_PATHA
    mov r12d, eax
    test r12d, r12d
    jnz .have
    invoke syn_lang, &rbx+TV_PATHB
    mov r12d, eax
    test r12d, r12d
    jz .off
.have:
    xor r13d, r13d
.pane:
    mov rdi, [rbx+TV_HA]
    test r13d, r13d
    jz .p
    mov rdi, [rbx+TV_HB]
.p:
    invoke rich_get_text, rdi, &rsp+80
    mov r14, rax
    test rax, rax
    jz .nx
    invoke rich_batch, rdi, 1
    invoke syn_run, rdi, r14, [rsp+80], r12
    invoke rich_batch, rdi, 0
    invoke free, r14
    invoke rich_unsel, rdi
.nx:
    inc r13d
    cmp r13d, 2
    jb .pane
.off:
    invoke SendMessageW, [rbx+TV_HA], EM_SETMODIFY, [rsp+64], 0
    invoke SendMessageW, [rbx+TV_HB], EM_SETMODIFY, [rsp+72], 0
    invoke SendMessageW, [rbx+TV_HA], WM_SETREDRAW, 1, 0
    invoke SendMessageW, [rbx+TV_HB], WM_SETREDRAW, 1, 0
    invoke InvalidateRect, [rbx+TV_HA], 0, 1
    invoke InvalidateRect, [rbx+TV_HB], 0, 1
    EPILOG 96
