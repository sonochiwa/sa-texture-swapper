# Generates the embedded default-configuration template from the canonical INI.
#
# Config\TextureSwapper.ini is the single source of truth. The plugin writes this
# byte array verbatim when the INI is missing, so the generated file is always
# byte-for-byte identical to the canonical one.

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$SourceIni,
    [Parameter(Mandatory = $true)][string]$OutputHeader
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $SourceIni)) {
    throw "Canonical configuration not found: $SourceIni"
}

$bytes = [System.IO.File]::ReadAllBytes($SourceIni)
if ($bytes.Length -eq 0) {
    throw "Canonical configuration is empty: $SourceIni"
}

$lines = New-Object System.Collections.Generic.List[string]
$lines.Add('// Generated from Config\TextureSwapper.ini by tools\GenerateConfigTemplate.ps1.')
$lines.Add('// Do not edit by hand; edit the canonical INI instead.')
$lines.Add('#pragma once')
$lines.Add('')
$lines.Add('// Byte-for-byte copy of the canonical configuration file.')
$lines.Add('inline const unsigned char kDefaultConfigIni[] = {')

for ($i = 0; $i -lt $bytes.Length; $i += 12) {
    $chunk = $bytes[$i..([Math]::Min($i + 11, $bytes.Length - 1))]
    $text = ($chunk | ForEach-Object { '0x{0:X2},' -f $_ }) -join ' '
    $lines.Add('    ' + $text)
}

$lines.Add('};')

$content = ($lines -join "`r`n") + "`r`n"

$outputDir = Split-Path -Parent $OutputHeader
if ($outputDir -and -not (Test-Path -LiteralPath $outputDir)) {
    New-Item -ItemType Directory -Force -Path $outputDir | Out-Null
}

# Only rewrite when the content changed, so MSBuild does not rebuild every time.
$existing = $null
if (Test-Path -LiteralPath $OutputHeader) {
    $existing = [System.IO.File]::ReadAllText($OutputHeader)
}

if ($existing -ne $content) {
    [System.IO.File]::WriteAllText($OutputHeader, $content, (New-Object System.Text.UTF8Encoding $false))
    Write-Host "Generated $OutputHeader ($($bytes.Length) bytes of configuration)"
} else {
    Write-Host "$OutputHeader is up to date"
}
