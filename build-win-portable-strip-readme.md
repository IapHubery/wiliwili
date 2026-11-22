# Windows 便携版体积优化与打包教程（strip）

本文解释为什么上游 wiliwili 的 Windows 便携包只有约 26 MB（解压后 50+ MB），而本地打包出来偏大；并给出体积对齐上游的打包步骤。

> 单 exe 的完整构建步骤见 [build-win-portable-readme.md](build-win-portable-readme.md)，本文只讲“体积为什么大 / 怎么减小”。

---

## 1. 体积对比

| 产物 | exe | zip |
|---|---|---|
| 上游 wiliwili 便携包 | 54.8 MB | 26 MB |
| 本项目（优化前，未 strip） | 72.4 MB | 30.2 MB |
| 本项目（strip 后） | **60.8 MB** | **28.7 MB** |

zip 压缩率两者基本一致（约 0.47），所以体积差异**全部来自 exe 本身**。

---

## 2. exe 体积构成

用 `objdump -h` 查看节区（以本项目为例）：

| 节区 | 大小 | 内容 |
|---|---|---|
| `.rsrc` | 35.3 MB | 内嵌的静态 `libmpv-2.dll`（35,219,271 字节，只依赖系统 DLL） |
| `.data` | 12.8 MB | libromfs 内嵌的 `resources/`（字体、图片、XML、i18n 等） |
| `.text` | 13.1 MB | 程序与静态库代码（curl、webp、OpenCC、borealis、nanovg…） |
| `.rdata/.pdata/.xdata/.reloc` 等 | ~2 MB | 只读数据、异常表、重定位 |
| **COFF 符号表** | **~11.6 MB** | MinGW 链接器默认保留的调试/链接符号（约 15 万个） |

上游 exe 的节区几乎相同：

- `.rsrc` ≈ 35.3 MB（同一份静态 mpv）
- `.data` ≈ 12.7 MB（同一套资源）
- `.text` ≈ 7.6 MB
- 文件末尾**没有符号表**

也就是说，两者内嵌的 **mpv 和 resources 完全一样**，差异只有两块：

1. **符号表**：未 strip 时多出约 11.6 MB（这是本地 72.4 MB 与上游 54.8 MB 差距的主要部分）
2. **代码体积**：上游 `.text` 约 7.6 MB，本项目约 13.1 MB。原因是上游为 D3D11 后端、代码版本更旧、功能更少；本项目分支功能更多且使用 OpenGL/GLFW 后端。这部分压缩后仍会差约 2~3 MB。

结论：**上游便携包小，不是打包方式不同，而是它的 exe 已经 strip 过，且代码本身更小。**

---

## 3. 打包步骤（strip 版）

### 3.1 构建单 exe

按 [build-win-portable-readme.md](build-win-portable-readme.md) 配置并构建：

```powershell
$ROOT = "D:\src\wiliwili"
$MSYS = "C:\msys64\ucrt64"

cmake --build build-portable
```

产物：`build-portable\wiliwili.exe`（约 72 MB，**保留符号，便于调试**）。

### 3.2 用脚本打包（推荐）

`scripts/package_win_single_strip_exe.ps1` 会对发布副本执行 strip（不会动 `build-portable` 里的调试产物）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\package_win_single_strip_exe.ps1 -Version 1.6.1
```

输出示例：

```
==> Stripped wiliwili.exe: 72.4 MB -> 60.8 MB
==> Single-exe package ready: dist/wiliwili-Windows-x64-single-exe (60.9 MB)
==> Zip ready: dist/wiliwili-Windows-x64-single-exe-1.6.1.zip (28.7 MB)
```

参数说明：

| 参数 | 作用 |
|---|---|
| `-NoStrip` | 不 strip，保留符号（排查崩溃时用） |
| `-NoZip` | 只生成目录，不打 zip |
| `-Version` | zip 文件名里的版本号 |

### 3.3 手动 strip + 打包

不使用脚本时：

```powershell
$ROOT = "D:\src\wiliwili"
$MSYS = "C:\msys64\ucrt64"

# 1. 复制一份发布用的 exe（不要直接 strip 调试产物）
Copy-Item "$ROOT\build-portable\wiliwili.exe" "$ROOT\dist\wiliwili.exe"

# 2. 去掉 COFF 符号表
& "$MSYS\bin\strip.exe" "$ROOT\dist\wiliwili.exe"

# 3. 连同文档一起压缩
Compress-Archive -Path "$ROOT\dist\wiliwili.exe", "$ROOT\LICENSE", "$ROOT\README.md" `
                 -DestinationPath "$ROOT\dist\wiliwili-Windows-x64-portable.zip" -Force
```

---

## 4. 验证

### 4.1 确认符号表已移除

```powershell
$exe = "dist\wiliwili.exe"

# 文件末尾不再有 COFF 符号表，大小应接近 60 MB
(Get-Item $exe).Length / 1MB

# 已定义符号数量应为 0
(& "$MSYS\bin\nm.exe" --defined-only $exe 2>$null | Measure-Object).Count
```

### 4.2 确认运行正常

strip 只删除符号信息，不影响执行。干净环境启动：

```powershell
$env:PATH = "C:\Windows\System32;C:\Windows"
.\dist\wiliwili.exe -d -o run.log
```

日志出现 `createWindow done`，打开视频出现 `Load bundled libmpv-2.dll, size: 35219271` 即可。

---

## 5. 还能再小吗

按收益从大到小：

| 方式 | 预估收益 | 说明 |
|---|---|---|
| `strip` | **~11.6 MB** | 只删符号，零风险；发布必做 |
| `-DCMAKE_BUILD_TYPE=MinSizeRel`（`-Os`） | 约 1~3 MB | 需重新完整构建；性能略降 |
| `-ffunction-sections -Wl,--gc-sections` | 约 0.5~1.5 MB | 去掉未引用函数 |
| `-DDISABLE_OPENCC=ON` | 约 1 MB | 失去繁简转换 |
| `-DDISABLE_WEBP=ON` | 约 1 MB | WebP 图改用 jpg，清晰度略降 |
| 更换更小的 mpv 构建 | 几 MB~十几 MB | 需要自己裁剪 FFmpeg/mpv，成本最高 |

注意：内嵌 mpv（35.3 MB）+ resources（12.8 MB）约占 exe 的 80%，这部分由 `MPV_BUNDLE_DLL` 和 `USE_LIBROMFS` 决定，单纯改编译参数无法显著缩小。

---

## 6. 常见问题

**Q: strip 后应用还能正常跑吗？**
能。strip 只移除符号表、调试段，不改代码和数据；崩溃时的符号化调试请使用未 strip 的 `build-portable\wiliwili.exe` 或保留一份 `-NoStrip` 产物。

**Q: 为什么 strip 后仍比上游大 6 MB 左右？**
内嵌的 mpv/resources 相同，差异在 `.text`：上游是 D3D11 后端、代码版本更旧、功能更少；本项目分支功能更多且使用 OpenGL/GLFW。属于代码差异，不是打包问题。

**Q: 直接对 build-portable\wiliwili.exe 执行 strip 有什么问题？**
会丢失该产物的符号，之后排查崩溃（如用 gdb/addr2line）会困难。建议始终 strip 复制出来的发布副本。

**Q: 上游为什么是 26 MB，而 strip 后我们是 28.7 MB？**
zip 压缩率一致（约 0.47），差距来自 exe：60.8 MB vs 上游 54.8 MB，主要是 `.text` 代码体积差。

---

## 7. 相关文件

| 文件 | 说明 |
|---|---|
| `scripts/package_win_single_strip_exe.ps1` | 单 exe 打包并 strip 发布副本（体积更小，推荐发布用） |
| `scripts/package_win_single_exe.ps1` | 单 exe 打包，保留符号（排查问题用） |
| `build-win-portable-readme.md` | 单 exe 构建（静态 mpv / 静态 curl / libromfs）完整教程 |
| `wiliwili/app_win32.rc.in` | 把 `libmpv-2.dll` 作为 `MPV RCDATA` 嵌入 exe |
| `wiliwili/source/view/mpv_core.cpp` | 运行时从 exe 资源加载内嵌 mpv |
