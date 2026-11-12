. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W6 {
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
  [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint c);
}
"@
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
$r = "$t\bcmv"
Remove-Item -Recurse -Force $r -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$r\A", "$r\B" | Out-Null
[System.IO.File]::WriteAllText("$r\A\mv.txt", "move me")
[System.IO.File]::WriteAllText("$r\A\ren.txt", "rename me"); [System.IO.File]::WriteAllText("$r\B\ren.txt", "rename me too")
Start-App "folder $r\A $r\B" 3
# list rows: mv.txt (row0), ren.txt (row1); row height ~19px starting y=203 (window offset 20,20)
[W6]::SetCursorPos(20 + 100, 20 + 232) | Out-Null
Start-Sleep -Milliseconds 300
[W6]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); [W6]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400
Shot "$sp\mv0.png"
$l = Ctrl 150
[U]::PostMessage($l, 0x100, [IntPtr]0x71, [IntPtr]0) | Out-Null
Start-Sleep 1
$d = [W6]::GetWindow($script:hw, 6)
"dialog=" + $d
$e = [U]::FindVisible($d, 801)
[U]::SendMessage($e, 0x000C, [IntPtr]::Zero, "renamed.txt") | Out-Null
$ok = [U]::FindVisible($d, 1)
[U]::SendMessage($ok, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep 2
Shot "$sp\mv1.png"
[W6]::SetCursorPos(20 + 100, 20 + 252) | Out-Null
Start-Sleep -Milliseconds 300
[W6]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); [W6]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 400
Click 166 3000
Start-Sleep 2
Shot "$sp\mv2.png"
Stop-App
Get-ChildItem $r -Recurse -File | ForEach-Object { $_.FullName.Substring($r.Length) }
