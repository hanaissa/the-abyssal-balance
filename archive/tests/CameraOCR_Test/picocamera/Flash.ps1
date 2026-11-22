$ErrorActionPreference = 'Stop'
$firmware = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../../../build/picocamera-bench/PicoCamera_Test.uf2'))
if (-not (Test-Path -LiteralPath $firmware)) { throw 'Build PicoCamera_Test before flashing.' }
$picoDrive = [System.IO.DriveInfo]::GetDrives() |
    Where-Object { $_.IsReady -and $_.VolumeLabel -eq 'RPI-RP2' } |
    Select-Object -First 1
if (-not $picoDrive) { throw 'Unplug USB, hold BOOTSEL while reconnecting, release it, then retry.' }
Copy-Item -LiteralPath $firmware -Destination $picoDrive.RootDirectory.FullName
Write-Host 'Updated PicoCamera firmware copied. Pico restarting.'
