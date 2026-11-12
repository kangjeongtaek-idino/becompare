. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "hex $t\ha.bin $t\hb.bin" 3
SetText 421 "41 42"
SetText 424 "58 59"
Click 418
Shot "$sp\hex_repl.png"
Click 415
Stop-App
[System.IO.File]::ReadAllText("$t\ha.bin")
