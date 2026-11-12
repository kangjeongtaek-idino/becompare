. $PSScriptRoot\ui.ps1
$t = "$env:TEMP\bctc7"
New-Item -ItemType Directory -Force $t | Out-Null
[System.IO.File]::WriteAllText("$t\a.c", "int foo = 1;`r`nint bar = foo + 2;`r`ncolor: red`r`n")
[System.IO.File]::WriteAllText("$t\b.c", "int xyz = 1;`r`nint baz = xyz + 2;`r`ncolour: red`r`n")
Start-App "text $t\a.c $t\b.c" 3
"plain:    " + (GetText 250)
Click 216 1500
"idents:   " + (GetText 250)
SetText 232 "color=colour"
Click 105 1500
"idents+rules: " + (GetText 250)
Click 216 1500
"rules only:   " + (GetText 250)
Stop-App
