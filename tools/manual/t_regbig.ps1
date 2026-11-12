. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$sw = [Diagnostics.Stopwatch]::StartNew()
Start-App 'registry HKCU\Software HKCU\Software\Classes' 1
Start-Sleep -Seconds 8
$script:app.Refresh()
"alive=" + (-not $script:app.HasExited) + " ws(MB)=" + [int]($script:app.WorkingSet64/1MB) + " elapsed=" + $sw.Elapsed.TotalSeconds
$lbl = GetText 702
$lbl
Shot "$sp\regbig.png"
Stop-App
