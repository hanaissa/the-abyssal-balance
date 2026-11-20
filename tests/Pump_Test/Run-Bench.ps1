param([string]$Port = 'COM3')

$pumpSerial = [System.IO.Ports.SerialPort]::new($Port, 115200)
$pumpSerial.DtrEnable = $true
$pumpSerial.ReadTimeout = 200

try {
    $pumpSerial.Open()
    Write-Host 'Press f/d (50%), F/D (100%), s (stop), q (quit). No Enter needed.'
    Write-Host 'Each pulse stops automatically after 500 ms. Start with motor power OFF.'
    while ($true) {
        $received = $pumpSerial.ReadExisting()
        if ($received.Length -gt 0) { Write-Host -NoNewline $received }
        if ([Console]::KeyAvailable) {
            $pumpKey = [Console]::ReadKey($true).KeyChar.ToString()
            if ($pumpKey -ceq 'q') { break }
            if ($pumpKey -cmatch '^[fdFDs]$') { $pumpSerial.Write($pumpKey) }
        }
        Start-Sleep -Milliseconds 20
    }
} finally {
    try {
        if ($pumpSerial.IsOpen) { $pumpSerial.Write('s') }
    } finally {
        if ($pumpSerial.IsOpen) { $pumpSerial.Close() }
        $pumpSerial.Dispose()
    }
}
