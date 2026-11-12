. $PSScriptRoot\ui.ps1
$t = "$env:TEMP\bctm06"
Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $t | Out-Null
[System.IO.File]::WriteAllText("$t\L.txt", "A1`r`nA2`r`nA3`r`nA4`r`nA5")
[System.IO.File]::WriteAllText("$t\R.txt", "B1`r`nB2`r`nB3`r`nB4`r`nB5")
Start-App "merge $t\L.txt $t\R.txt" 3
"info0: " + (GetText 650)
# select line 2 on both sides (offset 3 = start of line 2 with CR separators), align
Msg 601 0xB1 3 3 | Out-Null
Msg 603 0xB1 3 3 | Out-Null
Click 653 1500
"info1 (line 2 pinned): " + (GetText 650)
# multi-line selection: lines 4..5 on both sides (offset 9..14)
Msg 601 0xB1 9 14 | Out-Null
Msg 603 0xB1 9 14 | Out-Null
Click 653 1500
"info2 (lines 4-5 pinned): " + (GetText 650)
"output: " + ((GetText 610) -replace "`r", "|")
Click 654 1500
"info3 (cleared): " + (GetText 650)
Stop-App
