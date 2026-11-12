. $PSScriptRoot\ui.ps1
Start-App "patch $env:TEMP\t.patch" 3
"exited=" + $script:app.HasExited
"OLD: " + ((GetText 201) -replace "`r", " | ")
"NEW: " + ((GetText 202) -replace "`r", " | ")
Stop-App
