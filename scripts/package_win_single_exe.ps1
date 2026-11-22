<#
.SYNOPSIS
    Package the single-executable Windows build (resources + static mpv embedded).

.DESCRIPTION
    The single-exe build is produced with:
      -DUSE_LIBROMFS=ON
      -DMPV_BUNDLE_DLL=<static libmpv-2.dll>
      -DUSE_SYSTEM_CURL=OFF (static Schannel curl) + static libwebp

    Such an executable only imports system DLLs, so the portable package is just
    wiliwili.exe + documents (the same layout as the GitHub Actions artifact).

.EXAMPLE
    pwsh -File scripts/package_win_single_exe.ps1
    pwsh -File scripts/package_win_single_exe.ps1 -Version 1.6.1
#>
[CmdletBinding()]
param(
    [string]$BuildDir = "build-portable",
    [string]$OutDir = "dist/wiliwili-Windows-x64-single-exe",
    [string]$MsysRoot = "C:\msys64\ucrt64",
    [string]$Version = "1.6.1",
    [switch]$NoZip
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
Set-Location $repoRoot

$exePath = Join-Path $repoRoot (Join-Path $BuildDir "wiliwili.exe")
$objdump = Join-Path $MsysRoot "bin\objdump.exe"
$system32 = Join-Path $env:WINDIR "System32"

if (-not (Test-Path $exePath)) { throw "Built executable not found: $exePath" }
if (-not (Test-Path $objdump)) { throw "objdump not found: $objdump" }

$exe = Get-Item $exePath
if ($exe.Length -lt 40MB) {
    Write-Warning ("wiliwili.exe is only {0:N1} MB - USE_LIBROMFS / MPV_BUNDLE_DLL may not be enabled." -f ($exe.Length / 1MB))
}

# A single-exe build must not import third-party DLLs.
Write-Host "==> Checking imports of $exePath"
$thirdParty = & $objdump -p $exePath 2>$null |
    Select-String "DLL Name:\s*(\S+)" |
    ForEach-Object { $_.Matches[0].Groups[1].Value } |
    Where-Object { $_ -notlike 'api-ms-win-*' -and $_ -notlike 'ext-ms-win-*' } |
    Where-Object { -not (Test-Path (Join-Path $system32 $_)) } |
    Sort-Object -Unique

if ($thirdParty) {
    Write-Warning ("Non-system DLL imports found: " + ($thirdParty -join ", "))
    Write-Warning "This is not a self-contained build - do not publish it as single exe."
}

if (Test-Path $OutDir) {
    Write-Host "==> Removing previous output $OutDir"
    Remove-Item -LiteralPath $OutDir -Recurse -Force
}
New-Item -ItemType Directory -Path $OutDir -Force | Out-Null

Write-Host "==> Copying executable and documents"
Copy-Item -LiteralPath $exePath -Destination (Join-Path $OutDir "wiliwili.exe")
Copy-Item -LiteralPath (Join-Path $repoRoot "LICENSE") -Destination $OutDir
if (Test-Path (Join-Path $repoRoot "README.md")) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "README.md") -Destination $OutDir
}

$guidance = @"
wiliwili $Version - Windows x64 便携版（单文件）
==================================================

运行方式
  * 双击 wiliwili.exe 即可，无需安装 MSYS2 或其它运行库。
  * 本版本已把界面资源与 libmpv/FFmpeg 嵌入 exe，目录中不需要 resources 或 *.dll。

系统要求
  * Windows 10 / 11 x64。
  * 支持 OpenGL 3.2+ 或 D3D11 的显卡驱动。

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
Write-Host ("==> Single-exe package ready: {0} ({1:N1} MB)" -f $OutDir, ($totalBytes / 1MB))

if (-not $NoZip) {
    $zipPath = Join-Path $repoRoot ("dist/wiliwili-Windows-x64-single-exe-{0}.zip" -f $Version)
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
