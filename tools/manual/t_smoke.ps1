$t = "$env:TEMP"
function Smoke($a) {
  $p = Start-Process -FilePath $PSScriptRoot\..\..\bcgui.exe -ArgumentList $a -PassThru
  Start-Sleep -Seconds 3
  if ($p.HasExited) { "CRASH/EXIT ($($p.ExitCode)): $a" } else { "ok: $a"; Stop-Process -Id $p.Id -Force }
}
Smoke "home"
Smoke "folder $t\bcext\A $t\bcext\B"
Smoke "text $t\s1.c $t\s2.c"
Smoke "table $t\c1.csv $t\c2.csv"
Smoke "table $t\x1.xlsx $t\x2.xlsx"
Smoke "hex $t\ha.bin $t\hb.bin"
Smoke "merge $t\m1.txt $t\m2.txt"
Smoke "version C:\Windows\System32\kernel32.dll C:\Windows\System32\ntdll.dll"
Smoke "registry HKCU\Software\BCTest\A HKCU\Software\BCTest\B"
Smoke "media $t\bcmedia\a.mp3 $t\bcmedia\b.mp3"
Smoke "edit $t\e1.c"
Smoke "foldermerge $t\bcfm\L $t\bcfm\R"
Smoke "picture C:\Windows\Web\Wallpaper\Windows\img0.jpg C:\Windows\Web\Wallpaper\Windows\img0.jpg"
Smoke "folder $t\bcgui_zip\crc.bcs"
