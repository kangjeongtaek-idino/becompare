$ErrorActionPreference = 'Stop'
Set-Location "$PSScriptRoot\gui"
New-Item -ItemType Directory -Force ..\build | Out-Null
$mods = @('util','main','home')
$optional = 'folder','text','table','hex','pic','merge','pathbar','engine','rich','textlib','inflate','vtab','props','fmerge','settings','script','shell','conv','update','tcextra','export','export_xml'
foreach ($extra in $optional) { if (Test-Path "$extra.asm") { $mods += $extra } }
$objs = @()
foreach ($m in $mods) {
  nasm -f win64 -I . "$m.asm" -o "..\build\$m.obj"
  if ($LASTEXITCODE) { exit $LASTEXITCODE }
  $objs += "..\build\$m.obj"
}
$defs = @()
foreach ($v in 'folder','text','table','hex','pic','merge') { if (-not (Test-Path "$v.asm")) { $defs += "-DSTUB_$v" } }
nasm -f win64 -I . @defs stubs.asm -o ..\build\stubs.obj
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$objs += '..\build\stubs.obj'
windres res\app.rc -O coff -o ..\build\app.res.o
if ($LASTEXITCODE) { exit $LASTEXITCODE }
$objs += '..\build\app.res.o'
gcc -mwindows @objs -o ..\bcgui.exe -luser32 -lgdi32 -lcomctl32 -lcomdlg32 -lshell32 -lole32 -ldwmapi -lwininet -lgdiplus -luxtheme -lshlwapi -lversion -ladvapi32 -lnormaliz
if ($LASTEXITCODE) { exit $LASTEXITCODE }
Write-Host 'built bcgui.exe'
