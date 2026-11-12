. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
Start-App 'registry HKCU\Software\BCTest\A HKCU\Software\BCTest\B' 3
Shot "$sp\reg2.png"
Stop-App
