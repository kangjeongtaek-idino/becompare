. $PSScriptRoot\ui.ps1
$t = "$env:TEMP\bczip2"
Start-App "folder $t\a.zip $t\a.zip" 3
"before pane460: " + (Ctrl 460)
Click 170 300
Click 105 1500
"after pane460: " + (Ctrl 460)
Stop-App
