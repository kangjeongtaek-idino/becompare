. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W9 { [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s, string k, string v, string f); }
"@
$ini = "$env:LOCALAPPDATA\BeCompareAsm\settings.ini"
Remove-Item $env:TEMP\marker.txt -ErrorAction SilentlyContinue
[W9]::WritePrivateProfileString("Convert", ".srt", "cmd.exe /c echo %1 %2 > $env:TEMP\marker.txt", $ini) | Out-Null
Start-App "text $env:TEMP\bcrule\a.srt $env:TEMP\bcrule\b.srt" 3
Stop-App
Get-Content $env:TEMP\marker.txt
