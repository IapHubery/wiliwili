# Windows 便携版构建与打包教程

本文说明如何在本地构建并打包出 Windows 10 x64 便携版，并解释为什么 GitHub Actions 的便携包只有约 30 MB（解压后约 73 MB，仅一个 exe + README），而本地默认构建打包出来却有几百 MB。

---

## 1. 为什么体积差这么多

GitHub Actions 的便携包（`.github/workflows/build-win-portable.yml`）做了四件事，本地开发构建默认一件都没做：

| 环节 | CI 便携包 | 本地默认构建 |
|---|---|---|
| 界面资源 | `-DUSE_LIBROMFS=ON`，`resources/`（约 12 MB）编译进 exe | 磁盘上的 `resources/` 目录 |
| libmpv | 使用 wiliwili 官方提供的 **静态 mpv** 包：`libmpv-2.dll` 约 35 MB，FFmpeg/OpenSSL 等全部静态链接，只依赖系统 DLL | 使用 MSYS2 的 `libmpv-2.dll`（约 3 MB）+ 动态 FFmpeg，递归依赖 **139 个 DLL、约 184 MB** |
| libcurl | 源码静态编译，Schannel 后端，无第三方 DLL | 动态 `libcurl-4.dll` + OpenSSL/nghttp2 等一批 DLL |
| libwebp | 静态 `libwebp.a` | 动态 `libwebp-7.dll` |

所以：

- **CI**：`wiliwili.exe` 约 73 MB（资源 + 内嵌 mpv 都在 exe 里），zip 约 30 MB，目录里只有 `wiliwili.exe` 和 `README.md`。
- **本地目录版**：exe 约 27 MB + 139 个 DLL（184 MB）+ `resources/`（12 MB），未压缩约 222 MB，zip 约 88 MB。

> 结论：不是打包脚本有问题，而是**本地构建配置**没有用 CI 的「静态 mpv + 资源嵌入 + 静态 curl/webp」。按下面的方式重新配置即可得到同样的单 exe 便携版。

---

## 2. 前置条件

- Windows 10/11 x64
- MSYS2（UCRT64 环境），路径示例：`C:\msys64\ucrt64`
  - 已安装：`cmake`、`ninja`、`gcc/g++`、`pkgconf`
  - 已安装 `libmpv`（仅用于头文件）、`libwebp`
- 源码仓库已递归拉取子模块（`library/borealis` 等）
- 需要两个额外文件（见第 3 步）：
  1. wiliwili 官方静态 mpv 包里的 `libmpv-2.dll`
  2. curl 源码（8.7.1）

下文用两个变量表示路径（按自己的实际路径替换）：

```powershell
$ROOT = "D:\src\wiliwili"
$MSYS = "C:\msys64\ucrt64"
```

---

## 3. 准备依赖

### 3.1 静态 mpv（约 12 MB 压缩包，解出 35 MB DLL）

从 wiliwili 官方 Release 下载：

```url
https://github.com/xfangfang/wiliwili/releases/download/v0.1.0/mingw-w64-x86_64-mpv-0.40.0-2-any.pkg.tar.zst
```

国内访问 GitHub 慢时可用镜像（在链接前加 `https://ghfast.top/`）：

```url
https://ghfast.top/xfangfang/wiliwili/releases/download/v0.1.0/mingw-w64-x86_64-mpv-0.40.0-2-any.pkg.tar.zst
```

解包（PowerShell 的 `tar` 支持 zstd）：

```powershell
New-Item -ItemType Directory "$ROOT\build-portable-deps" -Force | Out-Null
tar -xf .\mingw-w64-x86_64-mpv-0.40.0-2-any.pkg.tar.zst -C "$ROOT\build-portable-deps\mpv"
Copy-Item "$ROOT\build-portable-deps\mpv\mingw64\bin\libmpv-2.dll" "$ROOT\build-portable-deps\libmpv-2-static.dll"
```

校验（应该只看到系统 DLL）：

```powershell
& "$MSYS\bin\objdump.exe" -p "$ROOT\build-portable-deps\libmpv-2-static.dll" |
  Select-String "DLL Name:"
```

### 3.2 curl 源码

下载并解压到 `build-portable-deps\curl-8.7.1`：

```
https://curl.se/download/curl-8.7.1.tar.xz
```

> 网络慢时建议用下载工具（NDM/IDM）下载后再解压；构建时不会联网。

`.tar.xz` 解压如遇 `tar` 卡住（部分环境会调用外部 `xz` 过滤器），可用 7-Zip 或 Python：

```powershell
python -c "import tarfile; tarfile.open(r'curl-8.7.1.tar.xz').extractall(r'build-portable-deps')"
```

最终目录结构：

```
build-portable-deps/
├── curl-8.7.1/                 # curl 源码（含 CMakeLists.txt）
└── libmpv-2-static.dll         # 静态 mpv，约 35 MB
```

---

## 4. 配置（单 exe，CI 同款）

在仓库根目录执行：

```powershell
$env:PATH = "$MSYS\bin;" + $env:PATH

cmake -B build-portable -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DPLATFORM_DESKTOP=ON `
  -DWIN32_TERMINAL=OFF `
  -DUSE_LIBROMFS=ON `
  -DMPV_BUNDLE_DLL="$ROOT/build-portable-deps/libmpv-2-static.dll" `
  -DUSE_SYSTEM_CURL=OFF `
  -DFETCHCONTENT_SOURCE_DIR_CURL="$ROOT/build-portable-deps/curl-8.7.1" `
  -DCPR_FORCE_WINSSL_BACKEND=ON `
  -DCURL_ZLIB=OFF `
  -DWebP_LIBRARY="$MSYS/lib/libwebp.a" `
  -DWebP_INCLUDE_DIR="$MSYS/include" `
  -DCURL_BROTLI=OFF -DCURL_ZSTD=OFF `
  -DCURL_USE_LIBPSL=OFF -DCURL_USE_LIBSSH2=OFF `
  -DUSE_LIBIDN2=OFF -DUSE_WIN32_IDN=ON
```

关键参数说明：

| 参数 | 作用 |
|---|---|
| `USE_LIBROMFS=ON` | 把 `resources/` 编进 exe（libromfs），不再需要 resources 目录 |
| `MPV_BUNDLE_DLL=...` | 把静态 `libmpv-2.dll` 以 RCDATA 资源嵌入 exe，运行时用 MemoryModule 从内存加载 |
| `USE_SYSTEM_CURL=OFF` + `FETCHCONTENT_SOURCE_DIR_CURL` | 用本地 curl 源码构建静态 curl，避免从 GitHub 拉取 |
| `CPR_FORCE_WINSSL_BACKEND=ON` | curl 使用 Schannel（Windows 原生证书库），不依赖 OpenSSL/CA 文件 |
| `CURL_ZLIB=OFF` | 不额外拉取 zlib；HTTP 功能不受影响 |
| `CURL_BROTLI/ZSTD/PSL/SSH2=OFF`、`USE_LIBIDN2=OFF` | 关掉会引入第三方 DLL 的可选特性 |
| `WebP_LIBRARY=.../libwebp.a` | 静态链接 webp，避免 `libwebp-7.dll` |

> **重要**：`MPV_BUNDLE_DLL` 的路径必须用**正斜杠**。如果写成反斜杠，`windres` 会把路径里的 `\b` 当成转义序列，报
> `unrecognized escape sequence`，并把路径截断（例如 `wiliwiliuild-portable-deps`）。

配置成功的关键日志：

```
-- Using WinSSL.
-- Configuring built-in curl...
-- curl version=[8.7.1]
-- Enabled SSL backends: Schannel
-- USE LIBROMFS
-- Found webp: ... C:/msys64/ucrt64/lib/libwebp.a
```

---

## 5. 构建

```powershell
cmake --build build-portable
```

产物：`build-portable\wiliwili.exe`，约 72 MB（含 35 MB mpv 与 12 MB 资源）。

自检（只应出现系统 DLL）：

```powershell
& "$MSYS\bin\objdump.exe" -p .\build-portable\wiliwili.exe |
  Select-String "DLL Name:"
```

如果出现 `libcurl-4.dll`、`libmpv-2.dll`、`libwebp-7.dll` 等，说明上一步参数没生效，重新删除 `build-portable` 后配置。

---

## 6. 打包

使用仓库内脚本（会校验导入表、生成 zip）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package_win_single_exe.ps1 -Version 1.6.0
```

输出：

```
dist/wiliwili-Windows-x64-single-exe/           # wiliwili.exe + README.md + LICENSE + 使用说明.txt
dist/wiliwili-Windows-x64-single-exe-1.6.0.zip  # 约 28~30 MB
```

脚本会：

1. 检查 exe 是否存在、体积是否正常（< 40 MB 会警告，通常说明 mpv/资源没嵌进去）
2. 用 `objdump` 检查是否还依赖非系统 DLL
3. 复制 exe 与许可证/说明
4. 压缩为 zip

如只想生成目录不加压缩包，加 `-NoZip`。

---

## 7. 验证

### 7.1 干净环境启动

```powershell
$env:PATH = "C:\Windows\System32;C:\Windows"   # 故意去掉 MSYS2
cd dist\wiliwili-Windows-x64-single-exe
.\wiliwili.exe -d -o run.log
```

日志里应看到：

```
createWindow done
```

以及打开视频时：

```
Load bundled libmpv-2.dll, size: 35219271
MPV Version: mpv v0.40.0
```

### 7.2 播放验证

随便点开一个视频，日志应出现 `onVideoPlayUrl quality: ...`、`Video type: dash`、`MPV_EVENT_START_FILE`，画面正常。

### 7.3 体积对照

| 产物 | exe | 解压后 | zip |
|---|---|---|---|
| 单 exe（CI 同款） | ~72 MB | ~72 MB | ~30 MB |
| 目录版（带 DLL） | ~27 MB | ~222 MB | ~88 MB |

---

## 8. 目录版（可选，体积大但构建简单）

如果不想准备静态 mpv / curl 源码，可以继续用目录版：动态链接所有依赖，再把 DLL 和 `resources/` 一起打包。

```powershell
cmake -B build-release -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DPLATFORM_DESKTOP=ON `
  -DWIN32_TERMINAL=OFF `
  -DUSE_SYSTEM_CURL=ON

cmake --build build-release

powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package_win_portable.ps1 -Version 1.6.0
```

输出：

```
dist/wiliwili-Windows-x64-portable/           # exe + 139 个 DLL + resources + ca-bundle.crt + 文档
dist/wiliwili-Windows-x64-portable-1.6.0.zip  # 约 88 MB
```

脚本会：

1. 按 exe 的 PE 导入表**递归**收集所有非系统 DLL（从 `C:\msys64\ucrt64\bin`）
2. 复制 `resources/`、`LICENSE`、`README.md`
3. 复制 `ca-bundle.crt`（MSYS2 的 libcurl 默认按 DLL 相对路径 `../etc/ssl/certs` 找 CA，换目录后会失效）
4. 生成 zip

---

## 9. 常见问题

**Q: 直接双击启动会连续弹「无法定位程序输入点 ... 于动态链接库 ...\libopenal-1.dll 上」？**
旧代码会优先 `LoadLibraryW("libmpv-2.dll")`，沿 PATH 找到 MSYS2 的 mpv；它的依赖（libopenal 等）又可能从 PATH 里更靠前的 Git/MinGW `libstdc++-6.dll` 取符号，版本不匹配就弹框。当前构建只从 **exe 同目录** 加载外部 `libmpv-2.dll`，没有则直接用内嵌 mpv，不会再搜索 PATH。如果仍出现，检查 exe 旁边是否有不匹配的 `libmpv-2.dll`。

**Q: 点开设置页直接闪退，日志里有 `Fatal error: Unknown XML tag "brls:HBox"`？**
borealis 没有 `brls:HBox`，横向布局请用 `brls:Box`（默认 `axis="row"`，纵向写 `axis="column"`）。自定义 XML 里的标签必须是 borealis 内置控件，或已在 `Register::initCustomView()` 里注册的控件，否则 `createFromXMLElement` 会抛异常导致崩溃。

**Q: 构建时报 `windres: unrecognized escape sequence`，路径里出现 `wiliwiliuild-portable-deps`？**
`MPV_BUNDLE_DLL` 用了反斜杠。改成正斜杠路径：

```powershell
-DMPV_BUNDLE_DLL="E:/path/to/libmpv-2-static.dll"
```

**Q: 报错 `cmake_dependent_option` / `MPV_BUNDLE_DLL` 被忽略？**
`MPV_BUNDLE_DLL` 依赖 `USE_LIBROMFS=ON` 且仅 Windows 生效。确认两个参数同时存在。

**Q: 配置时仍然去 GitHub 下载 curl/zlib？**
- 确认传了 `-DFETCHCONTENT_SOURCE_DIR_CURL=<本地 curl 源码>`；
- 确认 `-DCURL_ZLIB=OFF`；
- 如果 curl 源码路径不对，CMake 会回退到 FetchContent 联网下载。

**Q: exe 只有 20 多 MB，打包后发现还要 DLL？**
说明 `USE_LIBROMFS`/`MPV_BUNDLE_DLL` 没生效，或链接的仍是动态 mpv/curl。删除 `build-portable` 重新配置，并检查第 4 节的关键日志。

**Q: 运行时报 `schannel: AcquireCredentialsHandle failed: SEC_E_NO_CREDENTIALS`？**
Schannel 需要访问当前用户的凭据/证书库。在受限制令牌、沙箱或部分安全软件环境下会失败；正常双击运行时不会出现。若必须在这种环境运行，可改用 OpenSSL 静态 curl（`CPR_FORCE_OPENSSL_BACKEND=ON`），并随包提供 `ca-bundle.crt`。

**Q: 检查更新报 `CRYPT_E_NO_REVOCATION_CHECK` / `unable to get local issuer certificate`？**
这是访问 `api.github.com` 时的证书吊销检查/网络拦截问题，不影响视频、搜索等功能。换网络或使用代理后重试即可。

**Q: 配置保存在哪？**
Release 构建：`%LOCALAPPDATA%\xfangfang\wiliwili`。删除该目录即可恢复初始状态。

**Q: 如何更新版本号？**
`CMakeLists.txt` 顶部的 `VERSION_MAJOR/MINOR/REVISION`；打包脚本传入 `-Version` 只影响 zip 文件名。

---

## 10. 相关文件

| 文件 | 说明 |
|---|---|
| `.github/workflows/build-win-portable.yml` | 官方单 exe 便携版 CI 流程 |
| `scripts/package_win_single_exe.ps1` | 单 exe 打包脚本（校验导入表 + zip） |
| `scripts/package_win_portable.ps1` | 目录版打包脚本（递归收集 DLL + 资源 + CA） |
| `CMakeLists.txt` | `USE_LIBROMFS`、`MPV_BUNDLE_DLL`、静态链接等选项定义 |
| `wiliwili/app_win32.rc.in` | 把 `libmpv-2.dll` 作为 `MPV RCDATA` 资源嵌入 |
| `wiliwili/source/view/mpv_core.cpp` | 运行时优先加载外部 `libmpv-2.dll`，找不到时从 exe 资源加载 |
