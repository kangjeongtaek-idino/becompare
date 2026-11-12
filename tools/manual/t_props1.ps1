. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
Start-App "version C:\Windows\System32\kernel32.dll C:\Windows\System32\ntdll.dll" 3
Shot "$sp\ver1.png"
Stop-App
Start-App 'registry HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Advanced HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\Serialize' 3
Shot "$sp\reg1.png"
Stop-App
