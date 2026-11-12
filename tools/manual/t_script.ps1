$p = Start-Process -FilePath $PSScriptRoot\..\..\bcgui.exe -ArgumentList "/script $env:TEMP\bcscr\job.bcscript" -PassThru
$p.WaitForExit(20000) | Out-Null
if ($p.HasExited) { "exit=" + $p.ExitCode } else { "still running"; Stop-Process -Id $p.Id -Force }
Get-Content $env:TEMP\bcscr\job.bcscript.log
