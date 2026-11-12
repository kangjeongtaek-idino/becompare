. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "text $t\m1.txt $t\m2.txt" 3
Shot "$sp\aln0.png"
Msg 201 0xB1 15 15 | Out-Null
Msg 202 0xB1 0 0 | Out-Null
Click 247 1500
Shot "$sp\aln1.png"
Click 248 1500
Shot "$sp\aln2.png"
Stop-App
