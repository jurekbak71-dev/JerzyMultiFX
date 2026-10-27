$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '"'
    $child = Start-Process powershell.exe -Verb RunAs -ArgumentList $arguments -Wait -PassThru
    exit $child.ExitCode
}
$source = Join-Path $PSScriptRoot 'Jerzy MultiFX.vst3'
if (-not (Test-Path -LiteralPath $source -PathType Container)) { throw 'Najpierw rozpakuj CALY ZIP do folderu.' }
$directory = Join-Path $env:CommonProgramFiles 'VST3'
$target = Join-Path $directory 'Jerzy MultiFX.vst3'
$stage = Join-Path $directory ('.JerzyMultiFX-' + [guid]::NewGuid().ToString('N'))
$backup = Join-Path $env:ProgramData ('JerzyMultiFX\kopie\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '\Jerzy MultiFX.vst3')
$parked = $false
try {
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $stage -Recurse
    if (Test-Path -LiteralPath $target) {
        New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
        Move-Item -LiteralPath $target -Destination $backup
        $parked = $true
    }
    Move-Item -LiteralPath $stage -Destination $target
    Write-Host 'GOTOWE. Otworz program muzyczny i wyszukaj Jerzy MultiFX.'
    if ($parked) { Write-Host ('Poprzednia wersja zachowana: ' + $backup) }
    Read-Host 'Nacisnij Enter, aby zamknac'
} catch {
    if ($parked -and -not (Test-Path -LiteralPath $target)) { Move-Item -LiteralPath $backup -Destination $target }
    Write-Host 'Nie udalo sie zainstalowac. Zamknij FL Studio / DAW i sprobuj ponownie.'
    Write-Host $_.Exception.Message
    Read-Host 'Nacisnij Enter, aby zamknac'
    exit 1
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
