. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
Start-App "" 2
Shot "$sp\home1.png"
Stop-App
