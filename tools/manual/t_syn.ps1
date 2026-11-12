. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "text $t\s1.c $t\s2.c" 3
Shot "$sp\syn1.png"
Stop-App
