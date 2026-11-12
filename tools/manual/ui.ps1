# UI test driver: . tools\ui.ps1 ; Start-App "folder A B" ; Click 105 ; Shot out.png ; Stop-App
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class U {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, string l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr SendMessage(IntPtr h, int m, IntPtr w, StringBuilder l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, int m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr a, int x, int y, int w, int hh, uint f);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  public static IntPtr FindVisible(IntPtr root, int id) {
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(root, (h,l) => { if (GetDlgCtrlID(h)==id && IsWindowVisible(h)) { found = h; return false; } return true; }, IntPtr.Zero);
    return found;
  }
  public static string ClassOf(IntPtr h) { var s = new StringBuilder(64); GetClassName(h, s, 64); return s.ToString(); }
}
"@
$script:app = $null
function Start-App([string]$ArgList = "", [int]$Delay = 2) {
  $exe = "$PSScriptRoot\..\..\bcgui.exe"
  $script:app = if ($ArgList) { Start-Process -FilePath $exe -ArgumentList $ArgList -PassThru } else { Start-Process -FilePath $exe -PassThru }
  Start-Sleep -Seconds $Delay
  $script:app.Refresh()
  $script:hw = $script:app.MainWindowHandle
  [U]::SetWindowPos($script:hw, [IntPtr](-1), 20, 20, 1280, 800, 0x40) | Out-Null
  [U]::SetForegroundWindow($script:hw) | Out-Null
}
function Stop-App { if ($script:app -and -not $script:app.HasExited) { Stop-Process -Id $script:app.Id -Force } }
function Ctrl([int]$id) { [U]::FindVisible($script:hw, $id) }
function Click([int]$id, [int]$wait = 600) {
  $h = Ctrl $id; if ($h -eq [IntPtr]::Zero) { Write-Host "ctrl $id not found"; return }
  [U]::SendMessage($h, 0x00F5, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null   # BM_CLICK
  Start-Sleep -Milliseconds $wait
}
function SetText([int]$id, [string]$t) { $h = Ctrl $id; [U]::SendMessage($h, 0x000C, [IntPtr]::Zero, $t) | Out-Null }
function GetText([int]$id) { $h = Ctrl $id; $sb = New-Object System.Text.StringBuilder 4096; [U]::SendMessage($h, 0x000D, [IntPtr]4096, $sb) | Out-Null; $sb.ToString() }
function Msg([int]$id, [int]$m, $w = 0, $l = 0) { $h = Ctrl $id; [U]::SendMessage($h, $m, [IntPtr]$w, [IntPtr]$l) }
function Cmd([int]$id) { [U]::PostMessage($script:hw, 0x0111, [IntPtr]$id, [IntPtr]::Zero) | Out-Null; Start-Sleep -Milliseconds 600 }
function Shot([string]$out) {
  Start-Sleep -Milliseconds 500
  $r = New-Object U+RECT
  [U]::GetWindowRect($script:hw, [ref]$r) | Out-Null
  $w = $r.R - $r.L; $ht = $r.B - $r.T
  $bmp = New-Object System.Drawing.Bitmap $w, $ht
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.CopyFromScreen($r.L, $r.T, 0, 0, (New-Object System.Drawing.Size $w, $ht))
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
  $g.Dispose(); $bmp.Dispose()
}
