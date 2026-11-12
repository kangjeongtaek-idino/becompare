. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "edit $t\e1.c" 3
Shot "$sp\ed1.png"
SetText 303 "int"
SetText 306 "long"
Click 308 800
Shot "$sp\ed2.png"
Click 301 800
Stop-App
[System.IO.File]::ReadAllBytes("$t\e1.c").Length
[System.IO.File]::ReadAllText("$t\e1.c")
