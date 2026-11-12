function RunArg($a) {
  $p = Start-Process -FilePath $PSScriptRoot\..\..\bcgui.exe -ArgumentList $a -PassThru
  if (-not $p.WaitForExit(8000)) { "HUNG: $a"; Stop-Process -Id $p.Id -Force } else { "ok $a exit=" + $p.ExitCode }
}
RunArg "/shellleft $env:TEMP\s1.c"
RunArg "/shelluninstall"
