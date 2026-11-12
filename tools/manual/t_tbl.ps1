. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "table $t\c1.csv $t\c2.csv" 3
SetText 331 "1,2"
SetText 332 "60"
Click 105 1200
Shot "$sp\tbl1.png"
Stop-App
