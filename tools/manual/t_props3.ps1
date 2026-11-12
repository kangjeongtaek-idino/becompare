. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$d = "$env:TEMP\bcmedia"
Start-App "media $d\a.mp3 $d\b.mp3" 3
Shot "$sp\md1.png"
Stop-App
Start-App "media $d\a.flac $d\b.flac" 3
Shot "$sp\md2.png"
Stop-App
