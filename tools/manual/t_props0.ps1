. $PSScriptRoot\ui.ps1
Start-App "version C:\Windows\System32\kernel32.dll C:\Windows\System32\ntdll.dll" 3
"exited=" + $script:app.HasExited + " hw=" + $script:hw
if ($script:app.HasExited) { "code " + $script:app.ExitCode }
Stop-App
