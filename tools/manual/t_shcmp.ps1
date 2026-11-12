. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
Start-App "/shellcompare $env:TEMP\s2.c" 3
Shot "$sp\shcmp.png"
Stop-App
