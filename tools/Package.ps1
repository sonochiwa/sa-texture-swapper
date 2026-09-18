# Stages and archives a release from a completed Release|Win32 build.
#
#   powershell -ExecutionPolicy Bypass -File tools\Package.ps1 -Version 1.0.0
#
# Produces build\release\TextureSwapper-vX.Y.Z\ and build\TextureSwapper-vX.Y.Z.zip.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Version must be semantic, for example 1.0.0. Got: $Version"
}

$root      = Split-Path -Parent $PSScriptRoot
$build     = Join-Path $root 'build'
$binary    = Join-Path $build 'TextureSwapper.asi'
$canonical = Join-Path $root 'Config\TextureSwapper.ini'
$changelog = Join-Path $root 'CHANGELOG.md'

foreach ($required in @($binary, $canonical, $changelog)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required file: $required"
    }
}

# The version has to agree everywhere before anything is packaged.
$iniHeader = (Get-Content -LiteralPath $canonical -TotalCount 1)
if ($iniHeader -ne "# Texture Swapper v$Version") {
    throw "Config\TextureSwapper.ini header is '$iniHeader', expected '# Texture Swapper v$Version'"
}
if (-not (Select-String -LiteralPath $changelog -Pattern "^##[ \t]+$([regex]::Escape($Version))[ \t]*$" -Quiet)) {
    throw "CHANGELOG.md has no '## $Version' section"
}

$staging = Join-Path $build "release\TextureSwapper-v$Version"
$archive = Join-Path $build "TextureSwapper-v$Version.zip"

if (Test-Path -LiteralPath $archive) {
    if (-not $Force) {
        throw "$archive already exists. Re-run with -Force only when regenerating it deliberately."
    }
    Remove-Item -LiteralPath $archive -Force
}

if (Test-Path -LiteralPath $staging) {
    Remove-Item -LiteralPath $staging -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $staging | Out-Null

Copy-Item -LiteralPath $binary -Destination (Join-Path $staging 'TextureSwapper.asi')
Copy-Item -LiteralPath $canonical -Destination (Join-Path $staging 'TextureSwapper.ini')

Compress-Archive -LiteralPath (Get-ChildItem -LiteralPath $staging).FullName `
    -DestinationPath $archive -CompressionLevel Optimal

$hash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLower()
$name = Split-Path -Leaf $archive
[System.IO.File]::WriteAllText("$archive.sha256", "$hash  $name`n",
    (New-Object System.Text.UTF8Encoding $false))

# Windows PowerShell needs the assembly loaded; PowerShell 7 already has the type.
try { Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction Stop } catch {}
$zip = [System.IO.Compression.ZipFile]::OpenRead($archive)
try {
    Write-Host 'Archive entries:'
    $zip.Entries | ForEach-Object { Write-Host ("  {0} ({1} bytes)" -f $_.FullName, $_.Length) }
} finally {
    $zip.Dispose()
}

Write-Host ''
Write-Host "Archive: $archive"
Write-Host ("Size:    {0} bytes" -f (Get-Item -LiteralPath $archive).Length)
Write-Host "SHA-256: $hash"
