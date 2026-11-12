. $PSScriptRoot\ui.ps1
$p = Get-Process bcgui | Select-Object -First 1
$p.Refresh()
[U]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
Start-Sleep 1
[U]::SetWindowPos($p.MainWindowHandle, [IntPtr](-1), 20, 20, 1280, 800, 0x40) | Out-Null
Start-Sleep 2
"exited=" + $p.HasExited
