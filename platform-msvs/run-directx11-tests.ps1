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

[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",

    [ValidateSet("x86", "x64")]
    [string]$Platform = "x86",

    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$solutionPath = Join-Path $PSScriptRoot "mini-mbm.sln"
$testDirectory = Join-Path $PSScriptRoot $Configuration
$testExecutable = Join-Path $testDirectory "libTest.exe"

function Find-MSBuild {
    $visualStudioRoot = Join-Path $env:ProgramFiles "Microsoft Visual Studio"
    $installed = @(Get-ChildItem -Path (Join-Path $visualStudioRoot "*\*\MSBuild\Current\Bin\MSBuild.exe") `
        -File -ErrorAction SilentlyContinue | Sort-Object `
            { $_.VersionInfo.FileMajorPart }, { $_.VersionInfo.FileMinorPart } -Descending)
    if ($installed.Count -gt 0) {
        return $installed[0].FullName
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $installationJson = & $vswhere -all -products * -requires Microsoft.Component.MSBuild -format json
        $installations = ConvertFrom-Json -InputObject ($installationJson -join "`n")
        $installations = @($installations | Sort-Object { [version]$_.installationVersion } -Descending)
        foreach ($installation in $installations) {
            $candidate = Join-Path $installation.installationPath "MSBuild\Current\Bin\MSBuild.exe"
            if (Test-Path -LiteralPath $candidate) {
                return $candidate
            }
        }
    }

    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    throw "MSBuild was not found. Install the Visual Studio C++ workload described in platform-msvs/README.md."
}

if (-not $SkipBuild) {
    $msbuild = Find-MSBuild
    Write-Host "Building libTest ($Configuration|$Platform, DirectX11)..."
    & $msbuild $solutionPath /t:libTest "/p:Configuration=$Configuration" "/p:Platform=$Platform" `
        /p:MbmBackend=DirectX11 "/p:SolutionDir=$PSScriptRoot\" /m /v:minimal
    if ($LASTEXITCODE -ne 0) {
        throw "DirectX 11 libTest build failed with exit code $LASTEXITCODE."
    }
}

if (-not (Test-Path -LiteralPath $testExecutable)) {
    throw "libTest was not found at $testExecutable. Run without -SkipBuild first."
}

$tests = @(
    "--directx11-foundation-test",
    "--directx11-shader-profile-test",
    "--directx11-texture-failure-test",
    "--directx11-screen-size-test",
    "--directx11-resize-test",
    "--directx11-skeletal-parity-test",
    "--directx11-lighting-test",
    "--directx11-custom-lighting-test",
    "--directx11-mesh-readback-test",
    "--directx11-rasterizer-test",
    "--directx11-depth-state-test",
    "--directx11-blend-state-test",
    "--directx11-sampler-state-test",
    "--directx11-texture-upload-test",
    "--directx11-texture-stage-test",
    "--directx11-builtin-shader-test"
)

$passed = 0
Push-Location $testDirectory
try {
    foreach ($test in $tests) {
        Write-Host "`n[$($passed + 1)/$($tests.Count)] $test"
        & $testExecutable $test
        if ($LASTEXITCODE -ne 0) {
            throw "$test failed with exit code $LASTEXITCODE ($passed of $($tests.Count) tests passed)."
        }
        ++$passed
    }
}
finally {
    Pop-Location
}

Write-Host "`nDirectX 11 regression suite passed: $passed/$($tests.Count) tests."
