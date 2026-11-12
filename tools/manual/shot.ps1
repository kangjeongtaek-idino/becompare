# usage: shot.ps1 -Out file.png [-ArgList "dir A B"] [-Delay 2] [-Keep]
param([string]$Out, [string]$ArgList = "", [int]$Delay = 2, [string]$Exe = "$PSScriptRoot\..\bcgui.exe", [switch]$Keep)
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W {
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
}
"@
$p = if ($ArgList) { Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru } else { Start-Process -FilePath $Exe -PassThru }
Start-Sleep -Seconds $Delay
$p.Refresh()
$h = $p.MainWindowHandle
if ($h -eq [IntPtr]::Zero) { Write-Host "no window (exited=$($p.HasExited))"; exit 1 }
[W]::SetWindowPos($h, [IntPtr](-1), 20, 20, 1280, 800, 0x40) | Out-Null   # topmost, fixed size
[W]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 700
$r = New-Object W+RECT
[W]::GetWindowRect($h, [ref]$r) | Out-Null
$w = $r.R - $r.L; $ht = $r.B - $r.T
$bmp = New-Object System.Drawing.Bitmap $w, $ht
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $ht))
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
if (-not $Keep) { Stop-Process -Id $p.Id -Force }
Write-Host "saved $Out ($w x $ht)"
