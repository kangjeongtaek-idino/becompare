. $PSScriptRoot\ui.ps1
$t = "$env:TEMP\bcfc04"
Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
foreach ($d in 'A','B') { New-Item -ItemType Directory -Force "$t\$d" | Out-Null
  foreach ($n in 'a.txt','h.txt','r.txt') { [System.IO.File]::WriteAllText("$t\$d\$n", "x") }
  (Get-Item "$t\$d\h.txt").Attributes = 'Hidden'
  (Get-Item "$t\$d\r.txt").Attributes = 'ReadOnly' }
function Part1() { "rows=" + [U]::SendMessage((Ctrl 150), 0x1004, [IntPtr]::Zero, [IntPtr]::Zero) }
Start-App "folder $t\A $t\B" 3
"none : " + (Part1)
SetText 169 "H"; Click 105 1500
"H     : " + (Part1)
SetText 169 "HR"; Click 105 1500
"HR    : " + (Part1)
Stop-App
foreach ($d in 'A','B') { foreach ($n in 'h.txt','r.txt') { (Get-Item "$t\$d\$n" -Force).Attributes = 'Normal' } }
