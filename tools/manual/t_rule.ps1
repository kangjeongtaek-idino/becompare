. $PSScriptRoot\ui.ps1
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W9 { [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s, string k, string v, string f);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool WritePrivateProfileString(string s, IntPtr k, IntPtr v, string f); }
"@
$ini = "$env:LOCALAPPDATA\BeCompareAsm\settings.ini"
$t = "$env:TEMP\bcrule"
New-Item -ItemType Directory -Force $t | Out-Null
[System.IO.File]::WriteAllText("$t\a.srt", "pear`r`napple`r`nmango`r`n")
[System.IO.File]::WriteAllText("$t\b.srt", "mango`r`napple`r`nkiwi`r`n")
[W9]::WritePrivateProfileString("Convert", ".srt", "cmd.exe /c sort < `%1 > `%2", $ini) | Out-Null
Start-App "text $t\a.srt $t\b.srt" 3
"A: " + ((GetText 201) -replace "`r", " | ")
"B: " + ((GetText 202) -replace "`r", " | ")
Stop-App

