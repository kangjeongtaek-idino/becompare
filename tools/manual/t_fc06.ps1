. $PSScriptRoot\ui.ps1
$t = "$env:TEMP\bcfc06"
Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
foreach ($d in 'A','B') { New-Item -ItemType Directory -Force "$t\$d" | Out-Null }
function W($p, $c) { [System.IO.File]::WriteAllText($p, $c) }
W "$t\A\x.txt" "1"; W "$t\A\caf$([char]0xE9).txt" "2"; W "$t\A\doc.md" "3"
W "$t\B\x.bak" "1"; W "$t\B\cafe$([char]0x301).txt" "2"; W "$t\B\doc.mdx" "3"
function Rows() { "rows=" + [U]::SendMessage((Ctrl 150), 0x1004, [IntPtr]::Zero, [IntPtr]::Zero) }
Start-App "folder $t\A $t\B" 3
"plain : " + (Rows)
Click 174 300; Click 105 1500
"loose : " + (Rows)
SetText 176 "*.txt=*.bak"; Click 105 1500
"masks : " + (Rows)
Click 174 300; Click 105 1500
"masks only: " + (Rows)
Stop-App
