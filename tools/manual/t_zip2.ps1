. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W8 { [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e); }
"@
$t = "$env:TEMP\bczip2"
Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$t\B\sub1", "$t\B\sub2" | Out-Null
[System.IO.File]::WriteAllText("$t\B\sub1\x.txt", "bee one")
[System.IO.File]::WriteAllText("$t\B\sub2\x.txt", "bee two")
New-Item -ItemType Directory -Force "$t\src\sub1", "$t\src\sub2" | Out-Null
[System.IO.File]::WriteAllText("$t\src\sub1\x.txt", "zip one")
[System.IO.File]::WriteAllText("$t\src\sub2\x.txt", "zip two")
Compress-Archive -Path "$t\src\*" -DestinationPath "$t\a.zip"
Start-App "folder $t\a.zip $t\B" 3
[W8]::SetCursorPos(20 + 100, 20 + 232) | Out-Null
Start-Sleep -Milliseconds 300
[W8]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); [W8]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400
$l = Ctrl 150
[U]::PostMessage($l, 0x100, [IntPtr]0x27, [IntPtr]0) | Out-Null
Start-Sleep -Milliseconds 500
[U]::PostMessage($l, 0x100, [IntPtr]0x28, [IntPtr]0) | Out-Null
Start-Sleep -Milliseconds 500
[U]::PostMessage($l, 0x100, [IntPtr]13, [IntPtr]0) | Out-Null
Start-Sleep 2
Stop-App
Get-ChildItem "$env:TEMP\BeCompareAsm" -Recurse -File | Where-Object { $_.LastWriteTime -gt (Get-Date).AddMinutes(-2) } | ForEach-Object { $_.FullName.Substring($env:TEMP.Length) }
