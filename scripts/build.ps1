<#
.SYNOPSIS
    Cross-compiles the C Hello World demo for Windows, macOS, and Linux using zig cc.

.DESCRIPTION
    Iterates over target triples for Windows, Linux, and macOS, invoking `zig cc`
    to produce standalone executables in the `dist/` directory.

.EXAMPLE
    .\scripts\build.ps1
#>

[CmdletBinding()]
param(
    [string]$SourceFile,
    [string]$OutputDir
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Definition }
if (-not $scriptDir) { $scriptDir = (Get-Location).Path }

if (-not $SourceFile) {
    $SourceFile = Join-Path $scriptDir "..\src\main.c"
}
if (-not $OutputDir) {
    $OutputDir = Join-Path $scriptDir "..\dist"
}

# Function to discover a functioning zig executable
function Find-ZigExecutable {
    $candidates = @()

    # Check known paths
    if (Test-Path "C:\zig-x86_64-windows-0.16.0\zig.exe") {
        $candidates += "C:\zig-x86_64-windows-0.16.0\zig.exe"
    }

    # Check PATH entries
    $pathCommands = Get-Command zig -All -ErrorAction SilentlyContinue
    if ($pathCommands) {
        foreach ($cmd in $pathCommands) {
            $candidates += $cmd.Source
        }
    }

    foreach ($cand in $candidates) {
        try {
            $ver = & $cand version 2>$null
            if ($LASTEXITCODE -eq 0 -and $ver) {
                return $cand
            }
        } catch {
            # Continue trying next candidate
        }
    }

    return $null
}

$zigExe = Find-ZigExecutable
if (-not $zigExe) {
    Write-Error "A working 'zig' compiler executable was not found. Please ensure Zig is installed and accessible."
    exit 1
}

$zigVer = (& $zigExe version).Trim()
Write-Host "Found Zig: $zigExe"
Write-Host "Zig version: $zigVer"
Write-Host ""

# Ensure output directory exists
$resolvedOutputDir = [System.IO.Path]::GetFullPath($OutputDir)
if (-not (Test-Path -LiteralPath $resolvedOutputDir)) {
    New-Item -ItemType Directory -Path $resolvedOutputDir -Force | Out-Null
}

$resolvedSource = [System.IO.Path]::GetFullPath($SourceFile)
if (-not (Test-Path -LiteralPath $resolvedSource)) {
    Write-Error "Source file not found at: $resolvedSource"
    exit 1
}

# Define target matrix
$targets = @(
    @{ Triple = "x86_64-windows-gnu";  OutputFile = "hello-x86_64-windows.exe"; Platform = "Windows x86_64" },
    @{ Triple = "aarch64-windows-gnu"; OutputFile = "hello-aarch64-windows.exe"; Platform = "Windows ARM64" },
    @{ Triple = "x86_64-linux-musl";   OutputFile = "hello-x86_64-linux";       Platform = "Linux x86_64 (musl)" },
    @{ Triple = "aarch64-linux-musl";  OutputFile = "hello-aarch64-linux";      Platform = "Linux ARM64 (musl)" },
    @{ Triple = "x86_64-macos";        OutputFile = "hello-x86_64-macos";       Platform = "macOS x86_64 (Intel)" },
    @{ Triple = "aarch64-macos";       OutputFile = "hello-aarch64-macos";      Platform = "macOS aarch64 (Apple Silicon)" }
)

Write-Host "Building targets to: $resolvedOutputDir"
Write-Host "----------------------------------------------------"

$successCount = 0
$failCount = 0

foreach ($target in $targets) {
    $outPath = Join-Path $resolvedOutputDir $target.OutputFile
    Write-Host -NoNewline "Compiling [$($target.Platform)] ($($target.Triple))... "

    $cmdArgs = @("cc", "-target", $target.Triple, "-O2", "-Wall", "-Wextra", $resolvedSource, "-o", $outPath)
    
    & $zigExe @cmdArgs
    if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $outPath)) {
        $fileSize = (Get-Item -LiteralPath $outPath).Length
        Write-Host "SUCCESS ($fileSize bytes)"
        $successCount++
    } else {
        Write-Host "FAILED"
        $failCount++
    }
}

Write-Host "----------------------------------------------------"
Write-Host "Build Complete: $successCount succeeded, $failCount failed."

if ($failCount -gt 0) {
    exit 1
}
