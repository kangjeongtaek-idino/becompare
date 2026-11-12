. $PSScriptRoot\ui.ps1
$p = Start-Process -FilePath $PSScriptRoot\..\..\bcgui.exe -ArgumentList "version C:\Windows\System32\kernel32.dll C:\Windows\System32\ntdll.dll" -PassThru
Start-Sleep 3
$p.Refresh()
$g = Start-Process -FilePath gdb -ArgumentList "-p $($p.Id) -batch -ex `"handle SIGSEGV stop print nopass`" -ex continue -ex bt -ex `"x/8i `$rip`" -ex `"info registers`"" -PassThru -RedirectStandardOutput C:\Users\Public\gdb_att.txt -RedirectStandardError C:\Users\Public\gdb_att_err.txt -WindowStyle Hidden
Start-Sleep 4
[U]::SetWindowPos($p.MainWindowHandle, [IntPtr](-1), 20, 20, 1280, 800, 0x40) | Out-Null
Start-Sleep 5
"exited=" + $p.HasExited
if (-not $g.HasExited) { Stop-Process -Id $g.Id -Force }
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
