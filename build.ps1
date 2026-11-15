# Builds pocketpkmn.3dsx (and, with "cia", pocketpkmn.cia) - the compile runs inside the devkitpro/devkitarm Docker
# image in WSL; the project folder is mounted at /work so the path-with-a-space never reaches make.
# "cia" then packs the .cia in WSL Ubuntu-24.04 itself with cia/make_cia.sh (makerom + bannertool in
# ~/3ds-cia-tools there; docs/SESSION_CIA.md).
# Usage:  .\build.ps1          (build the .3dsx)
#         .\build.ps1 cia      (build the .3dsx, then the .cia)
#         .\build.ps1 clean    (remove build output)
param([string]$Target = 'all')

$win = (Resolve-Path $PSScriptRoot).Path
$wsl = '/mnt/' + $win.Substring(0, 1).ToLower() + ($win.Substring(2) -replace '\\', '/')

$make = if ($Target -eq 'cia') { 'all' } else { $Target }
wsl -d Ubuntu-24.04 -- docker run --rm -v "${wsl}:/work" -w /work devkitpro/devkitarm:latest make $make
if ($LASTEXITCODE -ne 0 -or $Target -ne 'cia') { exit $LASTEXITCODE }

wsl -d Ubuntu-24.04 --cd "$wsl" -- bash cia/make_cia.sh
exit $LASTEXITCODE
