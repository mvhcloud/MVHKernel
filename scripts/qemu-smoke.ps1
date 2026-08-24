param(
    [string]$Image = "build\mvh-kernel.iso",
    [string]$Qemu = "D:\qemu\qemu-system-x86_64.exe",
    [string]$Machine = "pc",
    [int]$MemoryMiB = 128,
    [int]$Cpus = 1,
    [int]$TimeoutSeconds = 15
)

$ErrorActionPreference = "Stop"
$resolvedImage = (Resolve-Path -LiteralPath $Image).Path
$resolvedQemu = (Resolve-Path -LiteralPath $Qemu).Path
$runDirectory = Join-Path ([System.IO.Path]::GetTempPath()) ("mvh-qemu-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $runDirectory | Out-Null
$serialLog = Join-Path $runDirectory "serial.log"
$errorLog = Join-Path $runDirectory "qemu.err"

try {
    $arguments = @(
        "-accel", "tcg", "-machine", $Machine, "-cpu", "qemu64",
        "-m", $MemoryMiB, "-smp", $Cpus, "-display", "none",
        "-monitor", "none", "-no-reboot", "-no-shutdown",
        "-serial", "file:$serialLog", "-cdrom", $resolvedImage, "-boot", "d"
    )
    $process = Start-Process -FilePath $resolvedQemu -ArgumentList $arguments -PassThru `
        -WindowStyle Hidden -RedirectStandardError $errorLog
    if (-not $process.WaitForExit($TimeoutSeconds * 1000)) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    } elseif ($process.ExitCode -ne 0) {
        throw "QEMU exited with code $($process.ExitCode): $(Get-Content -Raw $errorLog)"
    }
    $serial = Get-Content -Raw -LiteralPath $serialLog
    foreach ($marker in @("MVH Kernel 1.1.8/2 build", "MVH kernel ready", "mvh>")) {
        if (-not $serial.Contains($marker)) { throw "Missing serial marker: $marker" }
    }
    Write-Output "QEMU smoke passed: machine=$Machine memory=${MemoryMiB}MiB cpus=$Cpus"
} finally {
    if (Test-Path -LiteralPath $runDirectory) {
        Remove-Item -LiteralPath $runDirectory -Recurse -Force
    }
}
