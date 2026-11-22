<#
.SYNOPSIS
    Build a Windows x64 portable folder (wiliwili.exe + resources + runtime DLLs) and zip it.

.DESCRIPTION
    Collects the full non-system DLL dependency closure of the freshly built
    wiliwili.exe from the UCRT64 MSYS2 prefix, copies resources and the GPL
    license, and produces a zip that runs on a clean Windows 10 machine
    (no MSYS2 installation required).

.EXAMPLE
    pwsh -File scripts/package_win_portable.ps1
    pwsh -File scripts/package_win_portable.ps1 -BuildDir build-release -Version 1.6.1
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "build-release",
    [string]$OutDir = "dist/wiliwili-Windows-x64-portable",
    [string]$MsysRoot = "C:\msys64\ucrt64",
    [string]$Version = "1.6.1",
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $repoRoot

$buildPath = Join-Path $repoRoot $BuildDir
$exePath = Join-Path $buildPath "wiliwili.exe"
$resourcePath = Join-Path $buildPath "resources"
$objdump = Join-Path $MsysRoot "bin\objdump.exe"
$ucrtBin = Join-Path $MsysRoot "bin"
$system32 = Join-Path $env:WINDIR "System32"

if (-not (Test-Path $exePath)) { throw "Built executable not found: $exePath" }
if (-not (Test-Path $objdump)) { throw "objdump not found: $objdump" }
if (-not (Test-Path $resourcePath)) { throw "Built resources not found: $resourcePath" }

function Get-PeImports {
    param([Parameter(Mandatory)][string]$Path)
    & $objdump -p $Path 2>$null |
        Select-String "DLL Name:\s*(\S+)" |
        ForEach-Object { $_.Matches[0].Groups[1].Value }
}

function Resolve-DllClosure {
    param([Parameter(Mandatory)][string]$Exe)

    $toCopy = New-Object System.Collections.Generic.HashSet[string]
    $visited = New-Object System.Collections.Generic.HashSet[string]
    $missing = New-Object System.Collections.Generic.HashSet[string]
    $queue = New-Object System.Collections.Generic.Queue[string]
    $queue.Enqueue((Resolve-Path $Exe).Path)

    while ($queue.Count -gt 0) {
        $file = $queue.Dequeue()
        if (-not $visited.Add($file.ToLowerInvariant())) { continue }

        foreach ($dll in (Get-PeImports $file)) {
            # Windows API sets are provided by the OS.
            if ($dll -like 'api-ms-win-*' -or $dll -like 'ext-ms-win-*') { continue }

            # Anything shipped by Windows itself stays a system DLL (drivers,
            # DXGI/D3D11/Vulkan loader, msvcrt, ...).
            if (Test-Path (Join-Path $system32 $dll)) { continue }

            $candidate = Join-Path $ucrtBin $dll
            if (Test-Path $candidate) {
                if ($toCopy.Add($candidate)) { $queue.Enqueue($candidate) }
            } else {
                [void]$missing.Add("$dll (needed by $(Split-Path $file -Leaf))")
            }
        }
    }

    return [pscustomobject]@{ Files = @($toCopy | Sort-Object); Missing = @($missing | Sort-Object) }
}

Write-Host "==> Collecting dependency closure of $exePath"
$closure = Resolve-DllClosure -Exe $exePath
if ($closure.Missing.Count -gt 0) {
    Write-Warning ("Unresolved DLLs:`n  " + ($closure.Missing -join "`n  "))
}

if (Test-Path $OutDir) {
    Write-Host "==> Removing previous output $OutDir"
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

Write-Host "==> Copying executable, resources, license"
Copy-Item -LiteralPath $exePath -Destination (Join-Path $OutDir "wiliwili.exe")
Copy-Item -LiteralPath $resourcePath -Destination $OutDir -Recurse
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $OutDir
if (Test-Path (Join-Path $repoRoot "README.md")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination $OutDir
}

Write-Host "==> Copying $($closure.Files.Count) runtime DLLs"
foreach ($dll in $closure.Files) {
    Copy-Item -LiteralPath $dll -Destination $OutDir
}

# MSYS2 的 libcurl 默认按 DLL 相对路径查找 CA 证书，复制出来后必须自带一份
$caBundle = Join-Path $MsysRoot "etc\ssl\certs\ca-bundle.crt"
if (Test-Path $caBundle) {
    Write-Host "==> Copying CA bundle"
    Copy-Item -LiteralPath $caBundle -Destination (Join-Path $OutDir "ca-bundle.crt")
} else {
    Write-Warning "CA bundle not found: $caBundle"
}

$guidance = @"
wiliwili $Version - Windows x64 便携版
==================================================

运行方式
  * 双击 wiliwili.exe 即可，无需安装 MSYS2 或其它运行库。
  * 请保持整个文件夹结构完整（wiliwili.exe、resources、同目录的 *.dll）。

系统要求
  * Windows 10 / 11 x64。
  * 支持 OpenGL 3.2+ 的显卡驱动（NVIDIA / AMD / Intel 均可）。

数据保存位置
  * 配置、Cookie、播放记录：%LOCALAPPDATA%\xfangfang\wiliwili
  * 删除该目录即可恢复到初始状态。

启动参数（可选）
  * -d        输出调试日志
  * -v        显示界面调试边框
  * -t        在终端输出 mpv 日志
  * -o 文件   把日志写入指定文件，例如: wiliwili.exe -d -o wiliwili.log

许可证
  * 本项目基于 GPL-3.0，详见 LICENSE。
"@
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText((Join-Path $OutDir "ReadMe.txt"), $guidance, $utf8NoBom)

$totalBytes = (Get-ChildItem $OutDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
Write-Host ("==> Portable folder ready: {0} ({1:N1} MB)" -f $OutDir, ($totalBytes / 1MB))

if (-not $NoZip) {
    $zipPath = Join-Path $repoRoot ("dist/wiliwili-Windows-x64-portable-{0}.zip" -f $Version)
    if (Test-Path $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Write-Host "==> Creating $zipPath"
    $zipDir = Split-Path -Parent $zipPath
    $leaf = Split-Path -Leaf $OutDir
    Push-Location $zipDir
    try {
        tar.exe -a -c -f $zipPath $leaf
        if ($LASTEXITCODE -ne 0) { throw "tar.exe failed with exit code $LASTEXITCODE" }
    } finally {
        Pop-Location
    }
    Write-Host ("==> Zip ready: {0} ({1:N1} MB)" -f $zipPath, ((Get-Item $zipPath).Length / 1MB))
}
