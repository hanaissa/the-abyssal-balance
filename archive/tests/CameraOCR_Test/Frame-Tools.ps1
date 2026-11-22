function Read-CameraFrame {
    param([System.IO.Stream]$Stream)
    # Stream must provide a read timeout; also bound the complete transfer.
    $deadline = [DateTime]::UtcNow.AddSeconds(30)
    while ($true) {
        $line = [System.Text.StringBuilder]::new()
        while ($true) {
            if ([DateTime]::UtcNow -gt $deadline) { throw 'Camera header timeout.' }
            $next = $Stream.ReadByte()
            if ($next -lt 0) { throw 'Camera stream ended before a frame header.' }
            if ($next -eq 10) { break }
            if ($line.Length -ge 512) { throw 'Oversized camera header.' }
            if ($next -ne 13) { [void]$line.Append([char]$next) }
        }
        $header = $line.ToString()
        if ($header.StartsWith('ERROR')) { throw $header }
        if ($header -match '^ABYSSAL_FRAME (\d+) (\d+) (\d+) (\d+)(?: (4|8))?$') { break }
        if ($header.Length -gt 0) { Write-Host $header }
    }
    $width = [int]$Matches[1]
    $height = [int]$Matches[2]
    $count = [int]$Matches[3]
    $expectedSum = [uint32]$Matches[4]
    $bits = 8
    if ($Matches.ContainsKey(5) -and $Matches[5]) { $bits = [int]$Matches[5] }
    $validMode = ($width -eq 320 -and $height -ge 232 -and $height -le 240 -and $bits -eq 8) -or
                 ($width -eq 640 -and $height -ge 232 -and $height -le 240 -and $bits -eq 8) -or
                 ($width -eq 640 -and $height -ge 472 -and $height -le 480 -and $bits -eq 4)
    if (-not $validMode -or $count -ne ($width * $height * $bits / 8)) {
        throw 'Unexpected image dimensions or payload length.'
    }
    $pixels = [byte[]]::new($count)
    $offset = 0
    while ($offset -lt $count) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Camera payload timeout.' }
        $read = $Stream.Read($pixels, $offset, $count - $offset)
        if ($read -le 0) { throw 'Truncated camera image.' }
        $offset += $read
    }
    [uint32]$sum = 0
    foreach ($pixel in $pixels) { $sum += $pixel }
    if ($sum -ne $expectedSum) { throw 'Image checksum mismatch; retry the capture.' }
    # Read the exact trailer as bytes, never decode pixel data as text.
    foreach ($expected in [System.Text.Encoding]::ASCII.GetBytes("`nEND_FRAME`r`n")) {
        # Firmware SDK may translate the final newline to CRLF. Accept LF too.
        $actual = $Stream.ReadByte()
        if ($expected -eq 13 -and $actual -eq 10) { break }
        if ($actual -ne $expected) { throw 'Invalid camera image trailer.' }
    }
    if ($bits -eq 4) {
        $expanded = [byte[]]::new($width * $height)
        for ($i = 0; $i -lt $pixels.Length; ++$i) {
            $expanded[$i * 2] = ($pixels[$i] -shr 4) * 17
            $expanded[$i * 2 + 1] = ($pixels[$i] -band 15) * 17
        }
        $pixels = $expanded
    }
    return @{ Width = $width; Height = $height; Pixels = $pixels; Bits = $bits }
}

function Write-CameraBitmap {
    param($Frame, [string]$Path)
    $width = [int]$Frame.Width
    $height = [int]$Frame.Height
    $rowBytes = ($width * 3 + 3) -band (-bnot 3)
    $file = [System.IO.File]::Create($Path)
    $writer = [System.IO.BinaryWriter]::new($file)
    try {
        $writer.Write([byte[]](66, 77)) # BMP signature
        $writer.Write([uint32](54 + $rowBytes * $height))
        $writer.Write([uint32]0)
        $writer.Write([uint32]54)
        $writer.Write([uint32]40)
        $writer.Write([int32]$width)
        $writer.Write([int32]$height)
        $writer.Write([uint16]1)
        $writer.Write([uint16]24)
        $writer.Write([uint32]0)
        $writer.Write([uint32]($rowBytes * $height))
        $writer.Write([int32]2835)
        $writer.Write([int32]2835)
        $writer.Write([uint32]0)
        $writer.Write([uint32]0)
        $row = [byte[]]::new($rowBytes)
        for ($y = $height - 1; $y -ge 0; --$y) {
            for ($x = 0; $x -lt $width; ++$x) {
                $gray = $Frame.Pixels[$y * $width + $x]
                $row[$x * 3] = $gray
                $row[$x * 3 + 1] = $gray
                $row[$x * 3 + 2] = $gray
            }
            $writer.Write($row)
        }
    } finally {
        $writer.Dispose()
    }
}
