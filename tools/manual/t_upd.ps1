. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices;
public class WA { [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s, string k, string v, string f);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s, IntPtr k, IntPtr v, string f);
 [DllImport("user32.dll")] public static extern IntPtr GetWindow(IntPtr h, uint c); }
"@
$ini = "$env:LOCALAPPDATA\BeCompareAsm\settings.ini"
$srv = Start-Process python -ArgumentList "-m http.server 8099 --bind 127.0.0.1 --directory $env:TEMP\bcupd" -PassThru -WindowStyle Hidden
Start-Sleep 2
foreach ($f in 'new','cur','missing') {
  [WA]::WritePrivateProfileString("Update", "Url", "http://127.0.0.1:8099/$f.json", $ini) | Out-Null
  Start-App "" 2
  [U]::PostMessage($script:hw, 0x0111, [IntPtr]1070, [IntPtr]::Zero) | Out-Null
  Start-Sleep 3
  $d = [WA]::GetWindow($script:hw, 6)
  $txt = ""
  [U]::EnumChildWindows($d, { param($h,$l) $sb = New-Object System.Text.StringBuilder 512; [U]::SendMessage($h, 0x000D, [IntPtr]512, $sb) | Out-Null; if ($sb.Length) { $script:txt += "[" + $sb.ToString() + "]" }; $true }, [IntPtr]::Zero) | Out-Null
  "$f => $script:txt"
  Stop-App
}
Stop-Process -Id $srv.Id -Force
[WA]::WritePrivateProfileString("Update", [IntPtr]::Zero, [IntPtr]::Zero, $ini) | Out-Null
