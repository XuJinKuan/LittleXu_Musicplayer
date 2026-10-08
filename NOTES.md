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

> ⚠️ 实测踩坑（2026-10-08）：系统 `JAVA_HOME=E:\JDK` 实为 **Oracle JDK 21**，用它跑 Android 打包会在 Gradle `:dexBuilderRelease` 触发 R8 NPE。构建前必须切到 `E:\JDK17`，详见第 8 节。

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

1. ~~**APK 真实编译**：整条 Android 构建链路从未跑通过一次。~~ **已完成（2026-10-08）：已产出签名 APK，见第 8 节**
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

---

## 6. 客户端第一版（2026-09-30 ~ 2026-10-08）

### 已完成

- 目录 `client/`：`src/`（main / apiclient / songlistmodel / appcontroller）+ `qml/`（Main / LoginPage / MainPage / LibraryPage / OnlinePage / ReportPage / StatsPage / PlayerBar / SongDetailPopup）
- 构建：`cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release`
  - PATH 前缀必须含：`E:\Qt\6.5.3\mingw_64\bin;D:\work\mingw64\bin;`
- 依赖模块：`Core Gui Widgets Quick QuickControls2 Network Multimedia Charts`
- QML 用 `qt_add_resources` 打进 qrc（**不用** `qt_add_qml_module`），运行期以 `qrc:/qml/*.qml` 加载
- `WIN32_EXECUTABLE OFF`，保留控制台便于看 qDebug
- 服务端 4 个统计接口 + 登录接口实测 `code=0`，数据自洽

### 关键问题：登录后约 2 秒闪退（✅ 已修复并复验通过）

**现象**：点登录后约 2 秒窗口闪退。

**定位过程**
1. gdb 抓栈 → SIGSEGV，崩溃帧符号 `QWidgetTextControl::setCursorWidth(int)`（Qt6Widgets.dll）
2. 判定该符号为 **release 版无符号导致的误报**（崩溃 PC 落在 Qt6Widgets 地址段，符号只是"最近的导出符号"）
3. 崩溃链：`AppController::onApiFinished` → `emit sessionChanged()` → Main.qml 的 `onSessionChanged` → `stack.replace(mainPageComponent)` → 实例化 MainPage 子树
4. MainPage 的 `StackLayout` **一次性实例化全部 4 个页面**，其中仅 `StatsPage.qml` 含 `ChartView`
   → 完美解释"登录前不崩、登录后必崩"（登录页无 ChartView）

**根因**：Qt 6 在 Windows 下，QML `ChartView`（QtCharts）配合 `QGuiApplication` 会硬崩溃，需改用 `QApplication`。

**依据**：Qt Forum 两个案例，崩溃符号一致且给出明确解法
- https://forum.qt.io/topic/163931/qt-6-application-crashes-when-using-qtcharts-chartview-in-qml/3 （提问者回复 "This resolved the issue completely"）
- https://forum.qt.io/topic/70575/qt-charts-crash

**修复（最小 diff，无破坏性）**
- `client/src/main.cpp`：`#include <QGuiApplication>` → `<QApplication>`；`QGuiApplication app(argc, argv)` → `QApplication app(argc, argv)`
- `client/CMakeLists.txt`：`find_package` 组件加 `Widgets`；`target_link_libraries` 加 `Qt6::Widgets`
- 副作用仅显式新增 Qt6Widgets 依赖（该 DLL 原已被 Qt6Charts 连带加载）

**状态**：已重新配置 + 构建通过（exit 0）；**2026-10-08 运行复验通过——登录后不再闪退，统计页 4 个图表正常渲染。结论：`QGuiApplication` → `QApplication` 即为本次修复。**

### 观察到的运行期日志

```
[mp3float @ 000001084b98eb40] Could not update timestamps for skipped samples.
```

- 来源：Qt6 Multimedia 的 FFmpeg 后端，解码 MP3 时跳过样本、时间戳无法更新
- 判断：无害；出现该日志说明已进入播放流程（**已确认登录未闪退**，且播放正常）

### 待办 / 隐患

1. ~~运行复验：登录是否还闪退、统计页图表是否正常渲染~~ **已完成（2026-10-08）：登录不闪退，4 个图表正常**
2. **`dist/` 未更新**：现有 `dist/music_client/` **缺 `Qt6Charts.dll` 与 `qml/QtCharts/`**，分发包运行到统计页会崩。需对新增的 Charts 依赖重跑 `windeployqt --qmldir client/qml`
3. 若本次修复无效 → 改用二分法：临时注释 `StatsPage.qml` 的 `ChartView`，确认崩溃点

---

## 7. 曲库用户个性化 + 统计精简（2026-10-08）

### 需求

1. 曲库按用户个性化：不同账号登录看到各自的曲库（此前所有账号共用一个曲库）
2. 统计分析只保留热度榜，删除曲风分布 / 时段分布 / 相似推荐

### 已确认决策

- **曲库方案**：同库 + 归属字段（`song.owner_user_id`），不采用"每用户独立数据库"
- **统计方案**：仅精简统计页（`StatsPage.qml`），"听歌报告"页保留不动
- **热度榜范围**：全站热度榜（不分用户），数据源 `v_hot_song`
- **种子归属**：10 首种子曲目全部归 `user1(xiaoxu)`；`azhe` / `linxiaoyu` 曲库初始为空，可自行在线搜索添加

### 改动清单

- **SQL**
  - `01_schema.sql`：`song` 表新增 `owner_user_id INT NOT NULL` + `KEY idx_song_owner` + 外键（`ON DELETE CASCADE`）
  - `05_seed.sql`：song INSERT 补 `owner_user_id`，10 首全为 `1`
  - `04_procedures.sql`：删除 `sp_similar_song`，仅保留 `sp_user_monthly_report`
- **服务端**
  - `dao/songdao.h/.cpp`：`list` 按 `owner_user_id` 过滤；`addSong` 写入归属；`deleteSong(songId,userId)` 仅限本人
  - `service/songservice.h/.cpp`：`list/addSong/deleteSong` 接入 `userId`（缺失返回 400；删除非本人返回 404）
  - `router/router.cpp`：DELETE 传 `userId`；删除 `/api/stats/genre|hours|similar` 三路由
  - `dao/statdao.h/.cpp`、`service/statservice.h/.cpp`：只留 `hotSongs` / `hot`，删除 3 个 struct 与 3 个接口
- **客户端**
  - `src/appcontroller.h/.cpp`：`loadSongs/addSong/deleteSong` 带 `userId`；删 3 个 loadStats 方法、3 个信号、`onApiFinished` 三分支
  - `qml/StatsPage.qml`：重写为仅热度榜单图铺满

### 破坏性 / 前置条件（重要）

- `song` 表结构已变更（新增 `NOT NULL` 列），**必须先重新初始化数据库**，否则旧表下服务端 INSERT 失败
- 重导顺序：`01_schema.sql` → `02_views.sql` → `03_triggers.sql` → `04_procedures.sql` → `05_seed.sql` → `06_email_verify.sql`

### 待办

1. ~~重新导入 SQL 并重建服务端 + 客户端~~ **已完成（2026-10-08）**：服务端已重编（含清理占用进程）、客户端已构建
2. `dist/` 分发包仍未更新（缺 `Qt6Charts.dll` 与 `qml/QtCharts/`），待重跑 `windeployqt --qmldir client/qml`

### 验证步骤（待批准后执行）

1. 重导 SQL，确认 `song` 表含 `owner_user_id` 且 10 首归属 `1`
2. 构建服务端与客户端
3. `xiaoxu` 登录 → 曲库 10 首；`azhe` 登录 → 曲库为空，可在线添加
4. 统计页仅剩热度榜一张图

---

## 8. Android 打包与签名（2026-10-08）

### 构建链路（已跑通）

```
CMake/Ninja → androiddeployqt → Gradle 8.0 (AGP 7.4.1) → APK
```

构建命令（**JAVA_HOME 必须先切 17**）：

```powershell
$env:JAVA_HOME="E:\JDK17"; $env:PATH="E:\JDK17\bin;E:\Qt\6.5.3\mingw_64\bin;D:\work\mingw64\bin;$env:PATH"; cmake --build g:\Projects\database_project\client\build-android
```

### 坑 1：JDK 21 导致 R8 崩溃（必记）

- **现象**：C++ 编译 12 步全过，卡在 Gradle `:dexBuilderRelease`
  `java.lang.NullPointerException: Cannot invoke "String.length()" because "<parameter1>" is null`（R8 4.0.48）
- **根因**：AGP 7.4.1 内置 R8 4.0.48 仅支持到 JDK 17；系统 `JAVA_HOME=E:\JDK` 是 **Oracle JDK 21**，不兼容
- **修复**：构建前把 `JAVA_HOME` 切到 `E:\JDK17`
- **已排除**：与项目源码无关（报错类为 Qt 自带 `QtLoader$*.class`）；也非 Gradle 中间产物损坏

### 坑 2：versionCode

- `client/CMakeLists.txt` 新增一行 `set_property(TARGET music_client PROPERTY QT_ANDROID_VERSION_CODE 2)`
- 目的：手机端识别为"更新"

### 签名：keystore 更换事件

- 旧 keystore `keys/xiaoxu.keystore`（2026-09-29 20:46:11 创建）**密码遗失且无法找回**
  - 已排查无果：PowerShell 历史、Android Studio（从未安装）、Windows 凭据管理器、Trae 会话/日志目录
- 已新建 `keys/music_client.keystore`（2026-10-08 13:42:35，2754 B），别名 `musicclient`，
  DN 沿用 `CN=XiaoXu, OU=WUST, O=WUST-DB-Course, L=Wuhan, ST=Hubei, C=CN`
- 证书 SHA-256 对比（**已确认不同**，签名确实更换）：

  | 包 | SHA-256 |
  | --- | --- |
  | 旧包 9/29 | `047a6e46…6285e858` |
  | 新包 10/08 | `882a5c24…7a8a287` |

- **后果**：手机上必须**先卸载旧 App 再装新包**（签名不一致），登录数据丢失；以后只能用新 keystore 更新
- **密码必须存到项目外**；`.gitignore` 已加 `*.keystore` / `*.jks` 防误提交

签名 / 验证命令：

```powershell
& "E:\Android\Sdk\build-tools\33.0.2\apksigner.bat" sign --ks "G:\Projects\database_project\keys\music_client.keystore" --ks-key-alias musicclient --out "G:\Projects\database_project\client\build-android\music_client-release-v2.apk" "G:\Projects\database_project\client\build-android\android-build\music_client.apk"
```

```powershell
& "E:\Android\Sdk\build-tools\33.0.2\apksigner.bat" verify --print-certs "G:\Projects\database_project\client\build-android\music_client-release-v2.apk"
```

### 产物

| 文件 | 说明 |
| --- | --- |
| `client/build-android/android-build/music_client.apk` | 未签名（androiddeployqt 输出） |
| `client/build-android/music_client-release-v2.apk` | **已签名，可装手机**（versionCode=2，26,123,979 B） |
| `client/build-android/music_client-release.apk` | 9/29 旧包，旧签名，保留未删 |

### 备注

- `apksigner` 输出的 `WARNING: META-INF/.../app-metadata.properties not protected by signature` 是 AGP 打包常规提示，可忽略