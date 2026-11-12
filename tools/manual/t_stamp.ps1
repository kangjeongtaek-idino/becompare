. $PSScriptRoot\ui.ps1
$t = "$env:TEMP\bcstamp"
Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
foreach ($d in 'A','B') { New-Item -ItemType Directory -Force "$t\$d" | Out-Null; [System.IO.File]::WriteAllText("$t\$d\f.txt", "same") }
(Get-Item "$t\A\f.txt").LastWriteTime = [datetime]'2020-01-02 03:04:05'
(Get-Item "$t\A\f.txt").Attributes = 'ReadOnly'
Start-App "folder $t\A $t\B" 3
$l = Ctrl 150
[U]::PostMessage($l, 0x100, [IntPtr]0x28, [IntPtr]0) | Out-Null
Start-Sleep -Milliseconds 600
Click 171 1500
"B time: " + (Get-Item "$t\B\f.txt").LastWriteTime + "  attrs: " + (Get-Item "$t\B\f.txt").Attributes
Stop-App
(Get-Item "$t\A\f.txt").Attributes = 'Normal'; (Get-Item "$t\B\f.txt").Attributes = 'Normal'
