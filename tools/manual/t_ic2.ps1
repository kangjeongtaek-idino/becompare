. $PSScriptRoot\ui.ps1
$sp = "$env:TEMP\bc-manual"
$t = "$env:TEMP"
Start-App "picture C:\Windows\Web\Wallpaper\Windows\img0.jpg C:\Windows\Web\Wallpaper\Windows\img0.jpg" 3
Shot "$sp\ic_pic.png"; Stop-App
Start-App "merge $t\bcfm\L\conf.txt $t\bcfm\R\conf.txt $t\bcfm\B\conf.txt" 3
Shot "$sp\ic_mrg.png"; Stop-App
