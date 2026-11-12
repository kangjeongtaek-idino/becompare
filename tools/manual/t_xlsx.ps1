. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "table $t\x1.xlsx $t\x2.xlsx" 3
Shot "$sp\xl1.png"
Stop-App
