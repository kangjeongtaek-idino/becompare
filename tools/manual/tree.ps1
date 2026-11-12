param([string]$ArgList = "", [int]$Delay = 2, [string]$Exe = "$PSScriptRoot\..\bcgui.exe")
Add-Type @"
using System; using System.Text; using System.Collections.Generic; using System.Runtime.InteropServices;
public class T {
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc cb, IntPtr l);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetParent(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
  public static List<string> Dump(IntPtr root) {
    var res = new List<string>();
    EnumChildWindows(root, (h,l) => {
      var c = new StringBuilder(64); GetClassName(h,c,64);
      var t = new StringBuilder(128); GetWindowText(h,t,128);
      RECT r; GetWindowRect(h, out r);
      res.Add(string.Format("{0:X} par={1:X} {2} vis={3} [{4},{5} {6}x{7}] '{8}'", h.ToInt64(), GetParent(h).ToInt64(), c, IsWindowVisible(h), r.L, r.T, r.R-r.L, r.B-r.T, t));
      return true; }, IntPtr.Zero);
    return res;
  }
}
"@
$p = if ($ArgList) { Start-Process -FilePath $Exe -ArgumentList $ArgList -PassThru } else { Start-Process -FilePath $Exe -PassThru }
Start-Sleep -Seconds $Delay
$p.Refresh()
"main=$($p.MainWindowHandle.ToInt64().ToString('X'))"
[T]::Dump($p.MainWindowHandle) | ForEach-Object { $_ }
Stop-Process -Id $p.Id -Force
