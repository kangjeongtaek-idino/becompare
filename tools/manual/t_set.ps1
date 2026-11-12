. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W2 { [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c, string t); [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint c); }
"@
$sp = "$env:TEMP\bc-manual"
Start-App "" 2
Cmd 1060
Start-Sleep 1
Shot "$sp\set1.png"
$d = [W2]::GetWindow($script:hw, 6)
"dialog=" + $d
function DSet([int]$id, [string]$t) { $h = [U]::FindVisible($d, $id); [U]::SendMessage($h, 0x000C, [IntPtr]::Zero, $t) | Out-Null }
DSet 504 "00FF00"
DSet 511 "F6"
DSet 540 ".txt=notepad.exe %1 %2"
$h = [U]::FindVisible($d, 1)
[U]::SendMessage($h, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
Start-Sleep 1
Shot "$sp\set2.png"
Stop-App
Get-Content "$env:LOCALAPPDATA\BeCompareAsm\settings.ini"
