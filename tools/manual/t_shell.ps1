function RunArg($a) {
  $p = Start-Process -FilePath $PSScriptRoot\..\..\bcgui.exe -ArgumentList $a -PassThru
  if (-not $p.WaitForExit(8000)) { "HUNG: $a"; Stop-Process -Id $p.Id -Force } else { "ok $a exit=" + $p.ExitCode }
}
RunArg "/shellinstall"
(Get-ItemProperty "HKCU:\Software\Classes\*\shell\BeCompareAsmLeft\command")."(default)"
(Get-ItemProperty "HKCU:\Software\Classes\Directory\shell\BeCompareAsmCompare")."(default)"
RunArg "/shellleft $env:TEMP\s1.c"
Get-Content "$env:LOCALAPPDATA\BeCompareAsm\settings.ini" | Select-String -Pattern "Shell" -Context 0,1
RunArg "/shelluninstall"
Test-Path "HKCU:\Software\Classes\*\shell\BeCompareAsmLeft"
