<#----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------#>

# Build and verify one isolated native normal-mapping matrix entry.
[CmdletBinding()]
param(
    [ValidateSet('dx9', 'dx11')][string]$Backend = 'dx11',
    [ValidateSet(0, 1)][int]$Normal = 1,
    [ValidateRange(1, 4)][int]$Lights = 2,
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [Parameter(Mandatory = $true)][string]$Output,
    [string]$Python = 'py',
    [string]$MSBuild,
    [switch]$SkeletalParity,
    [switch]$Dx11Failure
)

$ErrorActionPreference = 'Stop'
if ($Dx11Failure -and $Backend -ne 'dx11') { throw '-Dx11Failure requires -Backend dx11' }
$repo = Split-Path $PSScriptRoot -Parent
$destination = [System.IO.Path]::GetFullPath($Output)
if (Test-Path -LiteralPath $destination) {
    throw "Output must be a new directory: $destination"
}
if (-not $MSBuild) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'vswhere not found; pass -MSBuild explicitly.' }
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'Visual Studio C++ tools not found.' }
    $MSBuild = Join-Path $installation 'MSBuild/Current/Bin/MSBuild.exe'
}
$pythonArgs = @()
if ([System.IO.Path]::GetFileNameWithoutExtension($Python) -eq 'py') { $pythonArgs += '-3' }
& $Python @pythonArgs --version
if ($LASTEXITCODE -ne 0) { throw 'Python 3 is required.' }
New-Item -ItemType Directory -Path $destination | Out-Null
$bin = Join-Path $destination 'bin'
# Evaluate project-specific paths inside MSBuild, not in a command-line global
# property (whose nested expansion differs between compiler and linker tasks).
$isolationProps = Join-Path $destination 'isolation.props'
@'
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <IntDir>$(MSBuildThisFileDirectory)obj\$(MSBuildProjectName)\</IntDir>
  </PropertyGroup>
</Project>
'@ | Set-Content -Encoding UTF8 $isolationProps
$nativeBackend = if ($Backend -eq 'dx11') { 'DirectX11' } else { 'DirectX9' }
$buildArgs = @(
    (Join-Path $PSScriptRoot 'mini-mbm.sln'), '/t:libTest;mini_mbm',
    "/p:Configuration=$Configuration", '/p:Platform=x86',
    "/p:MbmBackend=$nativeBackend", "/p:MbmUseNormalMapping3D=$Normal",
    "/p:MbmSupportedMaxLights=$Lights", '/p:MbmCoreFeatureDefines=AUDIO_ENGINE_PORT_AUDIO',
    "/p:OutDir=$bin\", "/p:ForceImportAfterCppProps=$isolationProps", '/m:4', '/v:minimal', '/nologo'
)
@{ backend=$Backend; normal=$Normal; lights=$Lights; configuration=$Configuration;
   platform='x86'; msbuild=$MSBuild; arguments=$buildArgs } |
    ConvertTo-Json -Depth 4 | Set-Content -Encoding UTF8 (Join-Path $destination 'build.json')
Write-Host "Building $Backend normal=$Normal lights=$Lights ($Configuration x86)..."
& $MSBuild @buildArgs *> (Join-Path $destination 'build.log')
if ($LASTEXITCODE -ne 0) {
    Get-Content (Join-Path $destination 'build.log') -Tail 40
    throw "Build failed. See $destination/build.log"
}
Get-ChildItem -LiteralPath $bin -File |
    Where-Object { $_.Extension -in '.exe', '.dll' } |
    Get-FileHash -Algorithm SHA256 | Select-Object Path, Hash |
    ConvertTo-Json | Set-Content -Encoding UTF8 (Join-Path $destination 'binary-hashes.json')
$runnerArgs = @(
    (Join-Path $repo 'src/test-lib/run-normal-map-tests.py'),
    '--test-lib', (Join-Path $bin 'libTest.exe'), '--engine', (Join-Path $bin 'mini_mbm.exe'),
    '--backend', $Backend, '--normal', "$Normal", '--lights', "$Lights",
    '--output', (Join-Path $destination 'results')
)
if ($Backend -eq 'dx11' -and $Configuration -eq 'Debug') {
    $runnerArgs += '--require-native-validation'
}
if ($SkeletalParity) { $runnerArgs += '--skeletal-parity' }
if ($Dx11Failure) { $runnerArgs += '--dx11-failure' }
& $Python @pythonArgs @runnerArgs
if ($LASTEXITCODE -ne 0) { throw "Tests failed. See $destination/results/report.json" }
Write-Host "Normal-mapping entry passed. Report: $destination/results/report.json"
