. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W3 {
  [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s, string k, string v, string f);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern void mouse_event(uint f, int dx, int dy, uint d, IntPtr e);
}
"@
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
$ini = "$env:LOCALAPPDATA\BeCompareAsm\settings.ini"
$out = "$t\ext_out.txt"
Remove-Item $out -ErrorAction SilentlyContinue
[W3]::WritePrivateProfileString("External", ".txt", "cmd.exe /c echo %1 %2 > `"$out`"", $ini) | Out-Null
$r = "$t\bcext"
Remove-Item -Recurse -Force $r -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$r\A", "$r\B" | Out-Null
[System.IO.File]::WriteAllText("$r\A\f.txt", "one"); [System.IO.File]::WriteAllText("$r\B\f.txt", "two")
Start-App "folder $r\A $r\B" 3
Shot "$sp\ext0.png"
[W3]::SetCursorPos(20 + 100, 20 + 203) | Out-Null
Start-Sleep -Milliseconds 300
[W3]::mouse_event(2, 0, 0, 0, [IntPtr]::Zero); [W3]::mouse_event(4, 0, 0, 0, [IntPtr]::Zero)
Start-Sleep -Milliseconds 300
$l = Ctrl 150
[U]::PostMessage($l, 0x100, [IntPtr]13, [IntPtr]0) | Out-Null
Start-Sleep 2
Shot "$sp\ext1.png"
Stop-App
[W3]::WritePrivateProfileString("External", $null, $null, $ini) | Out-Null
if (Test-Path $out) { "OUT: " + (Get-Content $out) } else { "no output" }
