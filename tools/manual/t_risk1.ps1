$t = "$env:TEMP\bcrisk"
Remove-Item -Recurse -Force $t -ErrorAction SilentlyContinue
foreach ($d in "A\ok","A\locked","B\ok","B\locked") { New-Item -ItemType Directory -Force "$t\$d" | Out-Null }
"x" | Set-Content "$t\A\ok\f.txt"; "x" | Set-Content "$t\B\ok\f.txt"
"y" | Set-Content "$t\A\locked\g.txt"; "y" | Set-Content "$t\B\locked\g.txt"
"extra" | Set-Content "$t\B\only.txt"
icacls "$t\A\locked" /deny "$($env:USERNAME):(OI)(CI)(RX)" | Out-Null
$p = Start-Process $PSScriptRoot\..\..\bcgui.exe -ArgumentList "folder $t\A $t\B /sync mirror" -PassThru
if (-not $p.WaitForExit(15000)) { "HUNG"; Stop-Process -Id $p.Id -Force } else { "exit=" + $p.ExitCode }
"B still has: " + ((Get-ChildItem $t\B -Recurse -File | ForEach-Object Name) -join ",")
icacls "$t\A\locked" /reset | Out-Null
