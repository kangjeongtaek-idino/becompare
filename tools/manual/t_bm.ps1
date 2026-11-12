. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class WB { [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h); }
"@
$t = "$env:TEMP"
Start-App "text $t\s1.c $t\s2.c" 3
$ra = Ctrl 201
$view = [WB]::GetParent($ra)
function Sel() { $r = [U]::SendMessage($ra, 0x00B0, [IntPtr]::Zero, [IntPtr]::Zero); [int64]$r.ToInt64() -band 0xFFFF }
# caret on line 4 (offset after 3 lines: "#include <stdio.h>\r/* block\r   comment */\r")
$off = 0; $txt = GetText 201; $lines = $txt -split "`r"; for ($i = 0; $i -lt 3; $i++) { $off += $lines[$i].Length + 1 }
Msg 201 0xB1 $off $off | Out-Null
"toggle -> " + [U]::SendMessage($view, 0x8020, [IntPtr]::Zero, [IntPtr]::Zero)
Msg 201 0xB1 0 0 | Out-Null
"caret before next: " + (Sel)
"next -> " + [U]::SendMessage($view, 0x8021, [IntPtr]::Zero, [IntPtr]::Zero)
"caret after next: " + (Sel) + " (expected $off)"
Msg 201 0xB1 0 0 | Out-Null
"prev -> " + [U]::SendMessage($view, 0x8022, [IntPtr]::Zero, [IntPtr]::Zero)
"caret after prev: " + (Sel)
# find / replace
SetText 303 "main"
Msg 201 0xB1 0 0 | Out-Null
[U]::SendMessage($view, 0x0111, [IntPtr]304, [IntPtr]::Zero) | Out-Null
"find 'main' caret: " + (Sel) + " (expected " + $txt.IndexOf("main") + "+4)"
SetText 306 "entry"
[U]::SendMessage($view, 0x0111, [IntPtr]307, [IntPtr]::Zero) | Out-Null
"after replace: " + (((GetText 201) -split "`r")[4])
Stop-App
