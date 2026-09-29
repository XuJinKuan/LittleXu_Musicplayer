# 进度记录 · 环境配置阶段

> 建立时间：2026-09-28
> 用途：记录已确认的环境结论与未决项，供跨会话延续参考。
> 注意：本目录已完成 `git init`，具备版本回退能力。

---

## 1. 环境版本台账（已实测确认）

| 组件 | 版本 | 路径 |
| --- | --- | --- |
| Qt | 6.5.3 | `E:\Qt\6.5.3` |
| Qt Creator | 19.0.1 | 独立于离线包（在线安装器安装） |
| JDK | 17 | `E:\JDK17` |
| Android SDK | — | `E:\Android\Sdk` |
| sdktool | — | `E:\Qt\Tools\sdktool\bin\sdktool.exe` |

### Android SDK 已装包

- `cmdline-tools;latest`（22.0）
- `platform-tools`
- `platforms;android-33`
- `build-tools;33.0.2`
- `ndk;25.1.8937393`（本次新装，对齐 Qt 声明）
- `ndk;25.2.9519653`（原有，并存未删）
- `emulator`
- `extras;google;usb_driver`

### Qt for Android 四个 ABI 安装目录（qmake 为 `.bat`，无 `qmake.exe`）

- `E:\Qt\6.5.3\android_arm64_v8a\bin\qmake.bat`
- `E:\Qt\6.5.3\android_armv7\bin\qmake.bat`
- `E:\Qt\6.5.3\android_x86\bin\qmake.bat`
- `E:\Qt\6.5.3\android_x86_64\bin\qmake.bat`

### Gradle / JDK 硬约束（不可变）

Qt 6.5.3 锁定 **Gradle 8.0** → Gradle 8.0 官方支持 JDK 上限为 **Java 19** → 本项目必须使用 **JDK 17**。

---

## 2. 关键决策：路线 A（补装 NDK，而非绕过）

### 问题现象

Qt Creator 套件列表中没有 Android 套件，Qt 版本列表中 4 条 Android 版本均显示红色告警：
「无法检测 Qt 版本所使用的 ABI。检查设备 → 安卓中的设置是否有误。」

### 根因（Qt Creator 19.0 源码实证）

`AndroidQtVersion::detectQtAbis()` 存在一个**检测闸门**：

1. 先尝试 `qtAbisFromJson()`；本项目四个 `Core.json` 均无 ABI 字段 → 返回空；
2. 仅当 `AndroidConfig::sdkFullyConfigured()` 为真时，才继续解析 mkspecs 补全 ABI；
3. `sdkFullyConfigured()` **不是实时计算，而是持久化配置项**：settings group `AndroidConfigurations`，key `AllEssentialsInstalled`，默认 false。

因此 ABI 为空 → `invalidReason()` 命中最后一条 → 报上述告警 → 不生成 Kit。

### 两条可选路线

- **路线 A（已采用）**：补齐 `allEssentialsInstalled()` 所要求的全部包，让闸门翻 true。
  - 判定集合 = `commonEssentials()`（Windows：platform-tools、cmdline-tools;latest、emulator、extras;google;usb_driver）+ 各 Android Qt 版本的 `essentialsFromQtVersion()`，合并去重后与 sdkmanager 已装包比对。
  - 四个 `Core.json` 内容一致（仅 `compiler_target` 的 ABI 前缀不同）→ 装一次即可让 4 条 Qt 版本全部转有效。
  - 推导出的缺失/需装项：`ndk;25.1.8937393`（Core.json 声明值）+ `platforms;android-33` + `build-tools;33.x` + `emulator` + `extras;google;usb_driver`。
- **路线 B（未采用）**：在设置中指定 Default NDK，绕过对 `ndk;*` 的校验。
  - 源码依据：`allEssentialsInstalled()` 在设置了 default NDK 时会**剔除所有 `ndk;*` 项**，从而省下约 1 GB 下载。
  - 未采用原因：绕过后与 Qt 声明的 NDK 版本不一致，可能引入后续编译隐患。

### 执行结果

- 执行命令（exit code 0）：

```powershell
$env:JAVA_HOME='E:\JDK17'; $y = ("y`r`n" * 30); $y | & "E:\Android\Sdk\cmdline-tools\latest\bin\sdkmanager.bat" --sdk_root="E:\Android\Sdk" "ndk;25.1.8937393" "emulator" "extras;google;usb_driver"
```

- 三个包全部落地，`.temp` 已清空。
- 闸门翻转：`C:\Users\Jack\AppData\Roaming\QtProject\QtCreator.ini` 的 `[AndroidConfigurations] AllEssentialsInstalled=true`。
- **无需重启 Qt Creator**：进入设置页点「确定」即触发闸门重算（实测 ini 已刷新而 qtcreator 进程数仍为 1）。
- 自动生成结果：
  - `qtcreator\toolchains.xml`：8 条 Android Clang 工具链（4 ABI × clang / clang++），公共前缀 `E:/Android/Sdk/ndk/25.1.8937393/toolchains/llvm/prebuilt/windows-x86_64/bin/`。
  - `qtcreator\profiles.xml`：4 个 Android Kit（arm64-v8a / armeabi-v7a / x86 / x86_64），均含 `Android.SDK`、`Android.NDK`。
  - `qtcreator\qtversion.xml`：4 条 Android Qt 版本，`isAutodetected=true`。
  - **注意**：`qtversion.xml` 中**不含 `Abis` 字段**，ABI 为运行时检测结果、不持久化，故不能用该文件判断闸门状态。

---

## 3. 遗留隐患（不阻塞，待处理）

1. **`android_openssl` 目录不存在**
   `QtCreator.ini` 中 `OpenSSLPriLocation=E:/Android/Sdk/android_openssl`，但该目录实际不存在。仅影响后续 HTTPS 请求 / APK 签名相关构建。
2. **`QtHttpServer` 模块缺失**
   当前 Qt 6.5.3 安装中无该模块。按 `PLAN.md` 既定方案用 **cpp-httplib** 兜底，不额外补装。
3. **多余的 Android Qt 版本**
   Qt 版本列表中另有 ARMv7 / x86 / x86_64 三条，本项目实际只需 **ARM64-v8a**。是否卸载待定，暂未改动。

---

## 4. 未验证项（需用户批准后才测）

1. **APK 真实编译**：整条 Android 构建链路从未跑通过一次。
2. **真机 adb 识别**：USB 调试 + 数据线连接从未验证。

---

## 5. 服务端第一版（已完成 2026-09-29）

- 目录：`server/`，18 个文件，分层 main → Router → Service → Dao → MySqlPool
- 接口：GET `/api/health`、POST `/api/register`、POST `/api/login`、
        GET `/api/songs`、GET `/api/songs/{id}`、POST `/api/play`、
        GET `/api/report/monthly`
- 关键决策：
  - HTTP 自研（Qt 官方 Windows 包不含 Qt6HttpServer）
  - 直连 MySQL C API（无 qsqlmysql.dll）
  - MinGW 用 gendef + dlltool 生成 libmysql.dll.a（实测可行）
  - 密码 SHA-256(salt + password)，与 05_seed.sql 一致
- 运行期依赖（必须与 exe 同目录，post-build 已自动复制）：
  libmysql.dll（lib/）、libssl-1_1-x64.dll + libcrypto-1_1-x64.dll（bin/）
  缺失症状：进程立即退出，退出码 0xC0000135
- 启动：设 PATH 加 Qt 与 MinGW，MUSIC_DB_PASSWORD 传 root 口令
- 冒烟结果：health / songs / detail / login(200+401) / monthly / 404 / 405 全通过
- 先前的"后续阶段"链条中，**建库、schema、服务端骨架均已完成**，E-R 图已放弃；下一阶段为客户端第一版