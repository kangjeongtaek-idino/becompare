. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$r = "$env:TEMP\bcfm3"
Remove-Item -Recurse -Force $r -ErrorAction SilentlyContinue
foreach ($d in 'L','R','B','O') { New-Item -ItemType Directory -Force "$r\$d" | Out-Null }
function W($p, $t) { [System.IO.File]::WriteAllText($p, $t) }
W "$r\B\c1.txt" "base"; W "$r\L\c1.txt" "L"; W "$r\R\c1.txt" "R"
W "$r\B\c2.txt" "base"; W "$r\L\c2.txt" "L2"; W "$r\R\c2.txt" "R2"
W "$r\O\c1.txt" "stale old merge"
(Get-Item "$r\O\c1.txt").LastWriteTime = (Get-Date).AddDays(-3)
Start-Sleep 1
W "$r\O\c2.txt" "fresh merge after inputs"
Start-App "foldermerge $r\L $r\R" 3
SetText 107 "$r\B"
SetText 109 "$r\O"
Click 105 1500
Click 710 2000
Shot "$sp\fm4.png"
Stop-App
