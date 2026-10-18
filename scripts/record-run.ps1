<#
.SYNOPSIS
Re-records the run AUTOTEST builds replay (tests/autotest_input.c), with scripts/bot.py.

.DESCRIPTION
Needed after a change that alters the game's behaviour for the same input: room templates, generation
rules, door positions, walking speed, entity timing. The bot plays -Rooms rooms, then lets Rush or Ambush
catch it and picks Try again, so the replay covers the death screen too. Steps: write an empty recording, build the AUTOTEST DMG ROM (which
then reads the joypad), let the bot play -Rooms rooms recording its buttons, rebuild, and check the replay
on both consoles.

.EXAMPLE
.\scripts\record-run.ps1
.\scripts\record-run.ps1 -Rooms 40
#>
param([int] $Rooms = 30)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$py = '.\tools\venv\Scripts\python.exe'
$make = @('F:\Programs\devkitPro\msys2\usr\bin\make.exe', 'C:\devkitPro\msys2\usr\bin\make.exe') |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $make) { $make = (Get-Command make).Source }

function Quiet([scriptblock] $b) { & $b 2>&1 | Select-String -NotMatch 'UserWarning|warnings.warn' }

Quiet { & $py scripts\bot.py --reset }
& $make AUTOTEST=1 dmg | Out-Null
if ($LASTEXITCODE) { throw 'build failed' }
Quiet { & $py scripts\bot.py build\dmg-autotest\doors.gb --rooms $Rooms --record --die-at-end }
if ($LASTEXITCODE) { throw 'the bot failed (see build/screens/bot-fail.png)' }
& $make AUTOTEST=1 | Out-Null
if ($LASTEXITCODE) { throw 'build failed' }
foreach ($r in 'build\dmg-autotest\doors.gb', 'build\gbc-autotest\doors.gbc') {
    Quiet { & $py scripts\bot.py $r --check $Rooms }
    if ($LASTEXITCODE) { throw "replay failed on $r" }
}
Write-Host "Recorded $Rooms rooms into tests\autotest_input.c"
