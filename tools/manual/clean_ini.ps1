Add-Type @"
using System; using System.Runtime.InteropServices;
public class W5 { [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s, IntPtr k, IntPtr v, string f); }
"@
$ini = "$env:LOCALAPPDATA\BeCompareAsm\settings.ini"
foreach ($s in 'Colors','Keys','Fonts','Shell','External') { [W5]::WritePrivateProfileString($s, [IntPtr]::Zero, [IntPtr]::Zero, $ini) | Out-Null }
Get-Content $ini
