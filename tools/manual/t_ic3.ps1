. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$r = "$env:TEMP\bcfm"
Start-App "foldermerge $r\L $r\R" 3
SetText 107 "$r\B"
Click 105 1500
Shot "$sp\ic_fm.png"; Stop-App
