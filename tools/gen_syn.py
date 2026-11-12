import os
os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'gui'))
exts = {
 1: "c h cpp hpp cc cxx hxx cs java js mjs ts jsx tsx go rs php swift kt kts scala dart m mm css scss less",
 2: "py pyw sh bash zsh yml yaml toml rb pl pm ps1 psm1 cmake r conf properties gitignore",
 3: "asm inc nasm s ini cfg reg desktop",
 4: "xml html htm xhtml xaml svg csproj vcxproj props targets config resx plist",
 5: "sql lua hs ada vhd vhdl",
 6: "json jsonc",
}
lines = []
table = []
n = 0
for lang, s in exts.items():
    for e in s.split():
        lines.append('WSTR sx%d, ".%s"' % (n, e))
        table.append(('sx%d' % n, lang))
        n += 1
kw = "if else elif for foreach while do switch case break continue return int char long short float double void unsigned signed const static struct class enum union typedef sizeof new delete this self public private protected namespace using include define ifdef ifndef endif pragma var let function func fn def true false null none nil import export from as async await try catch except finally throw raise default extends implements interface package abstract virtual override final bool string byte object in is not and or with yield lambda pass global extern inline volatile goto select insert update create table where join inner outer left right on order by group having values set into alter drop index primary key begin end then unique distinct limit use mut impl pub match loop mod crate trait type"
kwtab = "|" + "|".join(dict.fromkeys(kw.split())) + "|"

body = open('../tools/syn_body.txt', encoding='utf-8').read()
head = "; text_syn.inc - TC-04 syntax highlighting: extension based language pick, one-pass tokenizer, colours via RichEdit\n"
head += "\nextern strstr, rich_fg\n\nsection .rdata\nWSTR s_c_syn, \"구문 강조\"\n"
head += "\n".join(lines) + "\nalign 8\nsyn_exts:\n"
head += "\n".join("    dq %s, %d" % (a, l) for a, l in table)
head += "\n    dq 0, 0\nsyn_kw: db \"" + kwtab + "\", 0\n\nsection .text\n"
open('text_syn.inc', 'w', encoding='utf-8', newline='\n').write(head + body)
print('ok', n)
