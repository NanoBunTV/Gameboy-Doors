<#
.SYNOPSIS
Builds the ROMs with GNU make and checks their headers.

.EXAMPLE
.\build.ps1           # both ROMs: build\gbc\doors.gbc and build\dmg\doors.gb
.\build.ps1 gbc       # one target (gbc, dmg, all, clean)
.\build.ps1 -Test     # also build the AUTOTEST ROMs (the player walks a fixed route by itself) and run
                      # the BGB hardware-rule check and screenshot on them
#>
param(
    [ValidateSet('all', 'gbc', 'dmg', 'clean')] [string] $Target = 'all',
    [switch] $Test
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

# GNU make: devkitPro's MSYS2 on the owner's PC, else whatever is on PATH
$make = @('F:\Programs\devkitPro\msys2\usr\bin\make.exe', 'C:\devkitPro\msys2\usr\bin\make.exe') |
    Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $make) { $make = (Get-Command make -ErrorAction SilentlyContinue).Source }
if (-not $make) { throw 'GNU make not found (see Docs/BUILDING.md)' }
if (-not (Test-Path 'tools\gbdk\bin\lcc.exe')) { throw 'GBDK-2020 missing: tools\gbdk (see Docs/BUILDING.md)' }

& $make $Target
if ($LASTEXITCODE) { exit $LASTEXITCODE }
if ($Target -eq 'clean') { exit 0 }

$roms = @{ gbc = @('build\gbc\doors.gbc'); dmg = @('build\dmg\doors.gb'); all = @('build\gbc\doors.gbc', 'build\dmg\doors.gb') }[$Target]
& .\tools\venv\Scripts\python.exe scripts\check-rom.py @roms
if ($LASTEXITCODE) { exit $LASTEXITCODE }

if ($Test) {
    # The AUTOTEST build walks around on its own, so the check covers scrolling, not just a still screen
    & $make AUTOTEST=1 $Target
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    $failed = $false
    foreach ($r in ($roms -replace '\\(gbc|dmg)\\', '\$1-autotest\')) {
        if ($r -like '*.gbc') { $modes = @('gbc') } else { $modes = @('dmg', 'gbc') }
        foreach ($m in $modes) {
            & .\scripts\bgb-check.ps1 $r -Mode $m -Frames 900 -Seconds 15
            if ($LASTEXITCODE) { $failed = $true }
        }
        # PyBoy: is every background pixel right while scrolling, and does the VRAM work fit in VBlank?
        # (PyBoy prints a harmless SDL2 UserWarning on start; filtered out.)
        & .\tools\venv\Scripts\python.exe scripts\scroll-test.py $r 14000 --every 2 2>&1 | Select-String -NotMatch 'UserWarning|warnings.warn'
        if ($LASTEXITCODE) { $failed = $true }
        & .\tools\venv\Scripts\python.exe scripts\vblank-budget.py $r 14000 2>&1 | Select-String -NotMatch 'UserWarning|warnings.warn'
        if ($LASTEXITCODE) { $failed = $true }
        # The recorded run (tests/autotest_input.c) must still play through: a changed room or rule that
        # breaks it shows up here. Re-record with scripts/record-run.ps1 after deliberate changes.
        & .\tools\venv\Scripts\python.exe scripts\bot.py $r --check 30 2>&1 | Select-String -NotMatch 'UserWarning|warnings.warn'
        if ($LASTEXITCODE) { $failed = $true }
        # Sheets are loaded per room (sprites.c): every drawn sprite's sheet must be in VRAM, through the
        # whole replay and then the Library
        & .\tools\venv\Scripts\python.exe scripts\sprite-test.py $r 2>&1 | Select-String -NotMatch 'UserWarning|warnings.warn'
        if ($LASTEXITCODE) { $failed = $true }
    }
    # The Library (room 50) and Seek's chase (30-36), which the recorded run doesn't play: a DEBUG build warps
    # there (game_warp)
    & $make DEBUG=1 $Target
    if ($LASTEXITCODE) { exit $LASTEXITCODE }
    foreach ($r in ($roms -replace '\\(gbc|dmg)\\', '\$1-debug\')) {
        & .\tools\venv\Scripts\python.exe scripts\library-test.py $r 2>&1 | Select-String -NotMatch 'UserWarning|warnings.warn'
        if ($LASTEXITCODE) { $failed = $true }
        & .\tools\venv\Scripts\python.exe scripts\seek-test.py $r 2>&1 | Select-String -NotMatch 'UserWarning|warnings.warn'
        if ($LASTEXITCODE) { $failed = $true }
    }
    if ($failed) { Write-Host 'TESTS FAILED'; exit 1 }
    Write-Host 'All tests passed'
}
