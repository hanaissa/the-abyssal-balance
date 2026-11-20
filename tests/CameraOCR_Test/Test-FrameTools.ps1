$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Frame-Tools.ps1')
$payload = [byte[]]::new(320 * 240)
[uint32]$sum = 0
for ($i = 0; $i -lt $payload.Length; ++$i) {
    $payload[$i] = $i % 256 # Includes newlines and all binary byte values.
    $sum += $payload[$i]
}
function New-TestStream([uint32]$Checksum, [int]$Length, [string]$Trailer = "`nEND_FRAME`n") {
    $stream = [System.IO.MemoryStream]::new()
    $header = [System.Text.Encoding]::ASCII.GetBytes("READY`nABYSSAL_FRAME 320 240 76800 $Checksum`n")
    $stream.Write($header, 0, $header.Length)
    $stream.Write($payload, 0, $Length)
    $ending = [System.Text.Encoding]::ASCII.GetBytes($Trailer)
    $stream.Write($ending, 0, $ending.Length)
    $stream.Position = 0
    return ,$stream
}
foreach ($ending in @("`nEND_FRAME`n", "`nEND_FRAME`r`n")) {
    $stream = New-TestStream $sum $payload.Length $ending
    try {
        $frame = Read-CameraFrame $stream
        if ($frame.Pixels.Length -ne 76800 -or $frame.Pixels[255] -ne 255 -or
            $frame.Pixels[256] -ne 0) { throw 'Payload round-trip failed.' }
    } finally { $stream.Dispose() }
}
foreach ($case in @('checksum', 'truncated', 'trailer')) {
    $checksum = $sum
    $length = $payload.Length
    $trailer = "`nEND_FRAME`n"
    if ($case -eq 'checksum') { $checksum += 1 }
    if ($case -eq 'truncated') { $length = 50 }
    if ($case -eq 'trailer') { $trailer = "`nBROKEN`n" }
    $stream = New-TestStream $checksum $length $trailer
    $rejected = $false
    try { $null = Read-CameraFrame $stream } catch { $rejected = $true }
    finally { $stream.Dispose() }
    if (-not $rejected) { throw "Failed to reject $case input." }
}
# Real-size VGA fixture checks packed nibble order and dimensions.
$stream = [System.IO.MemoryStream]::new()
try {
    $packed = [byte[]]::new(640 * 480 / 2)
    $packed[0] = 0x0F
    $packed[1] = 0xA5
    $header = [System.Text.Encoding]::ASCII.GetBytes("FRAME_CHECK lines=480`nABYSSAL_FRAME 640 480 153600 180 4`n")
    $stream.Write($header, 0, $header.Length)
    $stream.Write($packed, 0, $packed.Length)
    $trailer = [System.Text.Encoding]::ASCII.GetBytes("`nEND_FRAME`n")
    $stream.Write($trailer, 0, $trailer.Length)
    $stream.Position = 0
    $frame = Read-CameraFrame $stream
    if ($frame.Width -ne 640 -or $frame.Height -ne 480 -or $frame.Bits -ne 4 -or
        $frame.Pixels.Length -ne 307200 -or $frame.Pixels[0] -ne 0 -or
        $frame.Pixels[1] -ne 255 -or $frame.Pixels[2] -ne 170 -or $frame.Pixels[3] -ne 85) {
        throw 'VGA nibble unpacking or resolution failed.'
    }
} finally { $stream.Dispose() }
# Receiver must not save a malformed frame reported by firmware.
$stream = [System.IO.MemoryStream]::new()
try {
    $comparison = [byte[]]::new(640 * 237)
    $comparison[320] = 255
    $header = [System.Text.Encoding]::ASCII.GetBytes("BYTE_CHECK test`nABYSSAL_FRAME 640 237 151680 255 8`n")
    $stream.Write($header, 0, $header.Length)
    $stream.Write($comparison, 0, $comparison.Length)
    $ending = [System.Text.Encoding]::ASCII.GetBytes("`nEND_FRAME`n")
    $stream.Write($ending, 0, $ending.Length)
    $stream.Position = 0
    $frame = Read-CameraFrame $stream
    if ($frame.Width -ne 640 -or $frame.Height -ne 237 -or $frame.Bits -ne 8 -or
        $frame.Pixels[320] -ne 255) { throw 'Byte-phase comparison was misdecoded.' }
} finally { $stream.Dispose() }
$stream = [System.IO.MemoryStream]::new()
try {
    $shortPixels = [byte[]]::new(152640)
    $header = [System.Text.Encoding]::ASCII.GetBytes("ABYSSAL_FRAME 640 477 152640 0 4`n")
    $stream.Write($header, 0, $header.Length)
    $stream.Write($shortPixels, 0, $shortPixels.Length)
    $ending = [System.Text.Encoding]::ASCII.GetBytes("`nEND_FRAME`n")
    $stream.Write($ending, 0, $ending.Length)
    $stream.Position = 0
    $frame = Read-CameraFrame $stream
    if ($frame.Width -ne 640 -or $frame.Height -ne 477 -or
        $frame.Pixels.Length -ne (640 * 477)) { throw 'Short-frame dimensions were padded or lost.' }
} finally { $stream.Dispose() }
$stream = [System.IO.MemoryStream]::new([System.Text.Encoding]::ASCII.GetBytes(
    "ERROR frame geometry: lines=120 expected=240`n"))
$rejected = $false
try { $null = Read-CameraFrame $stream } catch { $rejected = $true }
finally { $stream.Dispose() }
if (-not $rejected) { throw 'Frame geometry failure was not rejected.' }
# Small fixture validates BMP header, bottom-up row order and padding.
$bmpPath = Join-Path ([System.IO.Path]::GetTempPath()) ('abyssal-frame-' + [guid]::NewGuid() + '.bmp')
try {
    Write-CameraBitmap @{ Width=2; Height=2; Pixels=[byte[]](0,255,64,128) } $bmpPath
    $bmp = [System.IO.File]::ReadAllBytes($bmpPath)
    if ($bmp.Length -ne 70 -or $bmp[0] -ne 66 -or $bmp[54] -ne 64 -or
        $bmp[57] -ne 128 -or $bmp[62] -ne 0 -or $bmp[65] -ne 255) {
        throw 'BMP dimensions, padding or row orientation failed.'
    }
} finally { if (Test-Path -LiteralPath $bmpPath) { Remove-Item -LiteralPath $bmpPath } }
Write-Host 'PASS: binary payload, VGA nibbles, frame-error/checksum/truncation rejection and BMP layout.'
