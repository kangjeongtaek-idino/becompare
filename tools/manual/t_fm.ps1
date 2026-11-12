. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$r = "$env:TEMP\bcfm"
Remove-Item -Recurse -Force $r -ErrorAction SilentlyContinue
foreach ($d in 'L','R','B','O') { New-Item -ItemType Directory -Force "$r\$d" | Out-Null }
New-Item -ItemType Directory -Force "$r\L\sub", "$r\R\sub", "$r\B\sub" | Out-Null
function W($p, $t) { [System.IO.File]::WriteAllText($p, $t) }
# same in all
W "$r\B\same.txt" "same"; W "$r\L\same.txt" "same"; W "$r\R\same.txt" "same"
# left changed only
W "$r\B\lchg.txt" "base"; W "$r\L\lchg.txt" "left edit"; W "$r\R\lchg.txt" "base"
# right changed only
W "$r\B\rchg.txt" "base"; W "$r\L\rchg.txt" "base"; W "$r\R\rchg.txt" "right edit"
# both changed differently -> conflict
W "$r\B\conf.txt" "base"; W "$r\L\conf.txt" "L"; W "$r\R\conf.txt" "R"
# added left / right
W "$r\L\addl.txt" "new left"; W "$r\R\addr.txt" "new right"
# deleted on right, unchanged on left -> delete
W "$r\B\del.txt" "gone"; W "$r\L\del.txt" "gone"
# nested
W "$r\B\sub\n.txt" "n"; W "$r\L\sub\n.txt" "n2"; W "$r\R\sub\n.txt" "n"
Start-App "foldermerge $r\L $r\R" 3
SetText 107 "$r\B"
SetText 109 "$r\O"
Click 105 1500
Shot "$sp\fm1.png"
Click 710 1500
Shot "$sp\fm2.png"
Stop-App
Get-ChildItem $r\O -Recurse -File | ForEach-Object { $_.FullName.Substring($r.Length) + " = " + [System.IO.File]::ReadAllText($_.FullName) }
