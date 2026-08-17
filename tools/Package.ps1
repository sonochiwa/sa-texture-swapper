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

$readme = @(
    "Texture Swapper v$Version"
    'Created by sonochiwa'
    ''
    'Replaces single textures inside .txd dictionaries from loose PNG files, while'
    'the game is running, without rebuilding the .txd or the .img holding them.'
    'Requires GTA San Andreas 1.0 US and an ASI loader.'
    ''
    'Installation'
    ''
    '1. Copy TextureSwapper.asi and TextureSwapper.ini into the game directory,'
    '   next to gta_sa.exe.'
    '2. Create a folder named swapper next to gta_sa.exe.'
    '3. Inside it, create one folder per TXD and put your PNG files there. The'
    '   folder holding a PNG names the TXD, the file name names the texture:'
    ''
    '     swapper\hud\fist.png  ->  texture "fist" in hud.txd'
    ''
    '   A .txd suffix on the folder is accepted too. Folders above it are'
    '   free-form and ignored when matching. Naming the folder after the .txd'
    '   file always works.'
    '4. Start the game.'
    ''
    'Notes'
    ''
    '- PNG is the only supported input format.'
    '- Edit a PNG while playing and the texture updates without a restart; delete'
    '  it and the original comes back.'
    '- Replacements apply on top of dictionaries supplied by other mods, including'
    '  Mod Loader, so an HD pack does not have to be rebuilt for a single texture.'
    '- If a replacement does not show up, set loggingEnabled=1 in'
    '  TextureSwapper.ini. TextureSwapper.log then lists every replacement and'
    '  every skipped file, with the reason.'
    ''
    'Source code: https://github.com/sonochiwa/sa-texture-swapper'
) -join "`r`n"

[System.IO.File]::WriteAllText((Join-Path $staging 'README.txt'), $readme + "`r`n",
    (New-Object System.Text.UTF8Encoding $false))

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
