. $PSScriptRoot\ui.ps1
$t = "$env:TEMP\bctm06"
Start-App "merge $t\L.txt $t\R.txt" 3
Click 105 1500
"alive after remerge: " + (-not $script:app.HasExited)
Click 105 1500
"alive after remerge2: " + (-not $script:app.HasExited)
Stop-App
