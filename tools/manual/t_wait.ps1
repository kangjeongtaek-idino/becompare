param([string]$a, [int]$s = 10)
$p = Start-Process -FilePath $PSScriptRoot\..\..\bcgui.exe -ArgumentList $a -PassThru
for ($i = 1; $i -le $s; $i++) { Start-Sleep 1; if ($p.HasExited) { "exited at ${i}s code " + $p.ExitCode; break } }
if (-not $p.HasExited) { "alive after ${s}s"; Stop-Process -Id $p.Id -Force }
