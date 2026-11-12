$d = "$env:TEMP\bcscr"
$p = Start-Process -FilePath $PSScriptRoot\..\..\bcgui.exe -ArgumentList "folder $d\A $d\B /report $d\out.html" -PassThru
if (-not $p.WaitForExit(15000)) { "HUNG"; Stop-Process -Id $p.Id -Force } else { "exit=" + $p.ExitCode }
Get-ChildItem $d -Recurse | Select-Object FullName, Length | Format-Table -HideTableHeaders
Get-Content "$d\out.html" -Encoding UTF8 | Select-String "f.txt"
Get-Content "$d\out_files\*.html" -Encoding UTF8 | Select-String "<tr"
