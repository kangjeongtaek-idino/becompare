. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "folder $t\bcext\A $t\bcext\B" 3
Shot "$sp\wr1.png"
[U]::SetWindowPos($script:hw, [IntPtr](-1), 20, 20, 760, 700, 0x40) | Out-Null
Start-Sleep 1
Shot "$sp\wr2.png"
Stop-App
Start-App "text $t\s1.c $t\s2.c" 3
[U]::SetWindowPos($script:hw, [IntPtr](-1), 20, 20, 760, 700, 0x40) | Out-Null
Start-Sleep 1
Shot "$sp\wr3.png"
Stop-App
