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
$buildInfoPath = Join-Path $PSScriptRoot 'BUILD-INFO.json'
if (-not (Test-Path -LiteralPath $buildInfoPath -PathType Leaf)) { throw 'Brak BUILD-INFO.json. Rozpakuj ponownie CALY ZIP.' }
$buildInfo = Get-Content -LiteralPath $buildInfoPath -Raw | ConvertFrom-Json
$sourceDll = Join-Path $source 'Contents\x86_64-win\Jerzy MultiFX.vst3'
if (-not (Test-Path -LiteralPath $sourceDll -PathType Leaf)) { throw 'Paczka nie zawiera binarnego pliku VST3 x64.' }
$sourceHash = (Get-FileHash -LiteralPath $sourceDll -Algorithm SHA256).Hash
$directory = Join-Path $env:CommonProgramFiles 'VST3'
$target = Join-Path $directory 'Jerzy MultiFX.vst3'
$targetDll = Join-Path $target 'Contents\x86_64-win\Jerzy MultiFX.vst3'
$stage = Join-Path $directory ('.JerzyMultiFX-' + [guid]::NewGuid().ToString('N'))
$backup = Join-Path $env:ProgramData ('JerzyMultiFX\kopie\' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '\Jerzy MultiFX.vst3')
$parked = $false
$installed = $false
try {
    Write-Host ('Instalowana wersja: ' + $buildInfo.version)
    Write-Host ('Commit: ' + $buildInfo.commit)
    Write-Host ('SHA256: ' + $sourceHash)
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $stage -Recurse
    if (Test-Path -LiteralPath $target) {
        New-Item -ItemType Directory -Path (Split-Path $backup) -Force | Out-Null
        Move-Item -LiteralPath $target -Destination $backup
        $parked = $true
    }
    Move-Item -LiteralPath $stage -Destination $target
    $installed = $true
    $installedHash = (Get-FileHash -LiteralPath $targetDll -Algorithm SHA256).Hash
    if ($installedHash -ne $sourceHash) { throw 'Suma zainstalowanego pliku nie zgadza sie z paczka.' }
    $receipt = @(
        ('Wersja: ' + $buildInfo.version),
        ('Commit: ' + $buildInfo.commit),
        ('SHA256: ' + $installedHash),
        ('Sciezka: ' + $target),
        ('Data UTC: ' + [DateTime]::UtcNow.ToString('o'))
    ) -join [Environment]::NewLine
    Set-Content -LiteralPath (Join-Path $target 'ZAINSTALOWANA-WERSJA.txt') -Value $receipt -Encoding UTF8
    Write-Host ('GOTOWE. Zainstalowano: ' + $buildInfo.version)
    Write-Host ('Dokladna sciezka: ' + $target)
    Write-Host ('Potwierdzona suma SHA256: ' + $installedHash)
    Write-Host 'W GUI numer tej wersji jest widoczny obok napisu BASS GUITAR EFFECTS STUDIO.'
    Write-Host 'Po uruchomieniu FL Studio wykonaj pelne wyszukiwanie wtyczek, aby odswiezyc cache.'
    if ($parked) { Write-Host ('Poprzednia wersja zachowana: ' + $backup) }
    Read-Host 'Nacisnij Enter, aby zamknac'
} catch {
    if ($installed -and (Test-Path -LiteralPath $target)) { Remove-Item -LiteralPath $target -Recurse -Force }
    if ($parked -and -not (Test-Path -LiteralPath $target)) { Move-Item -LiteralPath $backup -Destination $target }
    Write-Host 'Nie udalo sie zainstalowac. Zamknij FL Studio / DAW i sprobuj ponownie.'
    Write-Host $_.Exception.Message
    Read-Host 'Nacisnij Enter, aby zamknac'
    exit 1
} finally {
    if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
}
