param(
    [string]$Port = 'COM3',
    [string]$Output = 'build/camera-frame.bmp',
    [ValidateSet('QVGA', 'VGA')][string]$Resolution = 'QVGA',
    [ValidateSet('Auto', 'Short')][string]$Exposure = 'Auto',
    [switch]$ColorBars,
    [switch]$ByteCheck,
    [switch]$UV
)

$ErrorActionPreference = 'Stop'
if ($ColorBars -and $UV) { throw 'Choose ColorBars or UV, not both.' }
if ($ColorBars -and $Exposure -ne 'Auto') { throw 'Use automatic exposure for the internal bar test.' }
if ($ByteCheck -and ($UV -or $ColorBars -or $Exposure -ne 'Auto')) { throw 'ByteCheck uses a visible scene with automatic exposure.' }
. (Join-Path $PSScriptRoot 'Frame-Tools.ps1')
$serial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$serial.DtrEnable = $true
$serial.ReadTimeout = 30000
$serial.WriteTimeout = 3000
try {
    $serial.Open()
    # The firmware accepts commands continuously after initialization. Allow
    # startup to complete, then remove its banner before the framed response.
    Start-Sleep -Seconds 4
    $startup = [System.Text.StringBuilder]::new()
    while ($serial.BytesToRead -gt 0 -and $startup.Length -lt 4096) {
        [void]$startup.Append([char]$serial.BaseStream.ReadByte())
    }
    if ($startup.ToString() -match '(?m)^ERROR[^\r\n]*') { throw $Matches[0] }
    $command = 'c'
    if ($ColorBars) { $command = 't' }
    if ($UV) { $command = 'u' }
    if ($Exposure -eq 'Short') {
        $command = 's'
        if ($UV) { $command = 'v' }
    }
    if ($Resolution -eq 'VGA') { $command = $command.ToUpperInvariant() }
    if ($ByteCheck) { $command = 'j' }
    Write-Host "Requesting frame on $Port (command $command)..."
    $serial.Write($command)
    # BaseStream is used exclusively for reads: do not mix ReadLine/ReadExisting
    # with binary image reads. Serial timeouts bound disconnected/stuck captures.
    $serial.BaseStream.ReadTimeout = 30000
    $frame = Read-CameraFrame -Stream $serial.BaseStream
    $outputPath = [System.IO.Path]::GetFullPath($Output)
    [void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($outputPath))
    Write-CameraBitmap -Frame $frame -Path $outputPath
    Write-Host "Saved $($frame.Width) x $($frame.Height) grayscale image: $outputPath"
} finally {
    if ($serial.IsOpen) { $serial.Close() }
    $serial.Dispose()
}
