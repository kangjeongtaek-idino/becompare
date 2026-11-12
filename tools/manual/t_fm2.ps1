. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W7 { [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y); [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e); }
"@
$sp = "$env:TEMP\bc-manual"
$r = "$env:TEMP\bcfm"
Start-App "foldermerge $r\L $r\R" 3
SetText 107 "$r\B"
SetText 109 "$r\O2"
Click 105 1500
[W7]::SetCursorPos(20 + 100, 20 + 271) | Out-Null
Start-Sleep -Milliseconds 300
[W7]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); [W7]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400
Click 715 2500
Shot "$sp\fm3.png"
Stop-App
