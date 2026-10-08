# 小徐爱听歌 · 音乐播放器

一个数据库课程设计项目：三层 C/S 架构的在线音乐播放器。桌面端 / Android 端共用同一套 Qt/QML 客户端代码，通过 HTTP/JSON 访问自研 C++ 服务端，服务端直连 MySQL 8.0。

## 功能一览

- **账号体系**：注册（QQ 邮箱验证码）、登录；密码以 `SHA-256(盐值 + 明文)` 存储
- **我的歌曲库**：按用户隔离，可查看歌词、在线播放、删除本人曲目
- **在线搜索**：接入酷我音源搜索，试听、加歌词、一键「添加到歌曲库」
- **播放器**：播放 / 暂停 / 上一首 / 下一首 / 音量调节，进度与歌词同步
- **听歌报告**：按月统计播放次数、收听时长等（调用存储过程 `sp_user_monthly_report`）
- **热度榜**：全站热门歌曲排行

> 说明：早期的「曲风分布 / 时段分布 / 相似推荐」统计分析功能已在迭代中移除，当前代码不含相关接口与页面。

## 技术栈

| 层 | 组成 |
| --- | --- |
| 客户端 | Qt 6.5.3（Quick / QML + C++）、QtMultimedia、QtNetwork |
| 服务端 | Qt 6.5.3（Core / Network）、自研 HTTP 服务、MySQL C API 直连 |
| 数据库 | MySQL 8.0（utf8mb4 / InnoDB），11 张表 |
| 构建 | CMake ≥ 3.16 + Ninja；Android 走 androiddeployqt + Gradle 8.0 |

服务端不用 `QSqlDatabase`（Qt 官方 Windows 包不含 QMYSQL 驱动），改为直连 `libmysql.dll`；HTTP 亦为自研（官方包不含 `Qt6HttpServer`）。

## 目录结构

```
database_project/
├── client/                 # 客户端（Windows + Android 共用）
│   ├── src/                # C++：main / apiclient / songlistmodel / appcontroller
│   ├── qml/                # QML 页面：Main / LoginPage / MainPage / LibraryPage /
│   │                       #          OnlinePage / ReportPage / PlayerBar / SongDetailPopup
│   ├── image/              # 图标与背景图
│   ├── android/            # AndroidManifest.xml（INTERNET 权限 + 允许明文 HTTP）
│   └── CMakeLists.txt
├── server/                 # 服务端
│   ├── src/
│   │   ├── main.cpp
│   │   ├── router/         # HTTP 监听与路由分发
│   │   ├── service/        # 业务逻辑（user / song / online）
│   │   ├── dao/            # 数据访问（user / song / verify）
│   │   └── util/           # config / http / mailer / mysqlpool
│   └── CMakeLists.txt
├── sql/                    # 建库脚本，按编号顺序执行
│   ├── 01_schema.sql       # 建库建表（11 张表）
│   ├── 02_views.sql        # 视图
│   ├── 03_triggers.sql     # 触发器
│   ├── 04_procedures.sql   # 存储过程
│   ├── 05_seed.sql         # 种子数据（可重复执行）
│   └── 06_email_verify.sql # 注册验证码表
├── PLAN.md                 # 项目计划书
├── NOTES.md                # 环境台账与阶段性进度记录
└── README.md
```

## 环境要求

- **Qt 6.5.3**（MinGW 套件），示例路径 `E:\Qt\6.5.3`
- **MinGW-w64**（提供 gendef / dlltool / g++），示例路径 `D:\work\mingw64`
- **MySQL Server 8.0**，默认根目录 `C:\Program Files\MySQL\MySQL Server 8.0`（CMake 按此查找头文件与 DLL，可用 `-DMYSQL_ROOT=...` 覆盖）
- **CMake ≥ 3.16** + **Ninja**
- **仅 Android**：JDK 17、Android SDK（platform 33 / build-tools 33.0.2）、NDK 25.1.8937393

## 数据库初始化

按编号顺序执行，缺一不可：

```powershell
mysql -u root -p < sql/01_schema.sql; mysql -u root -p < sql/02_views.sql; mysql -u root -p < sql/03_triggers.sql; mysql -u root -p < sql/04_procedures.sql; mysql -u root -p < sql/05_seed.sql; mysql -u root -p < sql/06_email_verify.sql
```

- 库名固定为 `music_app`
- `01_schema.sql` 中 `song.owner_user_id` 为 `NOT NULL`，**表结构变更后必须重新初始化数据库**，否则服务端写入会失败
- `05_seed.sql` 可重复执行：先 `TRUNCATE` 各表再写入
- 种子账号（密码统一 `123456`）：`xiaoxu`、`azhe`、`linxiaoyu`；其中 10 首种子歌曲归属 `xiaoxu`
- 若数据库中残留已移除的视图，手工执行 `DROP VIEW IF EXISTS v_hot_song;`

## 构建与运行

### 服务端

```powershell
$env:PATH="E:\Qt\6.5.3\mingw_64\bin;D:\work\mingw64\bin;$env:PATH"; cmake -S server -B server/build -G Ninja -DCMAKE_BUILD_TYPE=Release; cmake --build server/build
```

运行（数据库口令通过环境变量传入）：

```powershell
$env:PATH="E:\Qt\6.5.3\mingw_64\bin;D:\work\mingw64\bin;$env:PATH"; $env:MUSIC_DB_PASSWORD="你的MySQL口令"; .\server\build\music_server.exe
```

启动成功后可访问 `http://127.0.0.1:8080/api/health` 验活。

构建脚本已把运行期依赖自动复制到 exe 同目录：`libmysql.dll`、`libssl-1_1-x64.dll`、`libcrypto-1_1-x64.dll`。**缺失时进程会以 `0xC0000135` 立即退出**。

### 客户端（Windows）

```powershell
$env:PATH="E:\Qt\6.5.3\mingw_64\bin;D:\work\mingw64\bin;$env:PATH"; cmake -S client -B client/build -G Ninja -DCMAKE_BUILD_TYPE=Release; cmake --build client/build
```

分发时需重跑 `windeployqt`（QML 已打进 qrc，运行期以 `qrc:/qml/*.qml` 加载）。

### 客户端（Android）

```powershell
$env:JAVA_HOME="E:\JDK17"; $env:PATH="E:\JDK17\bin;E:\Qt\6.5.3\mingw_64\bin;D:\work\mingw64\bin;$env:PATH"; cmake --build client/build-android
```

- **必须使用 JDK 17**：AGP 7.4.1 内置的 R8 不支持 JDK 21，会在 `:dexBuilderRelease` 抛出 `NullPointerException`
- 未签名产物：`client/build-android/android-build/music_client.apk`
- 签名（keystore 与口令自行保管，不进版本库）：

```powershell
& "E:\Android\Sdk\build-tools\33.0.2\apksigner.bat" sign --ks "路径\music_client.keystore" --ks-key-alias musicclient --out "client\build-android\music_client-release.apk" "client\build-android\android-build\music_client.apk"
```

## 配置说明

### 客户端服务器地址

默认 `http://127.0.0.1:8080`，可在应用内修改并持久化（`QSettings` 键 `server/base`）。Android 真机需填写可访问的服务端地址（如 cpolar 内网穿透地址）；应用已允许明文 HTTP。

### 服务端环境变量

所有配置均可用环境变量覆盖，无需改代码重编。

| 变量 | 默认值 | 说明 |
| --- | --- | --- |
| `MUSIC_DB_HOST` | `127.0.0.1` | 数据库地址 |
| `MUSIC_DB_PORT` | `3306` | 数据库端口 |
| `MUSIC_DB_USER` | `root` | 数据库用户 |
| `MUSIC_DB_PASSWORD` | 空 | 数据库口令 |
| `MUSIC_DB_NAME` | `music_app` | 库名 |
| `MUSIC_DB_CHARSET` | `utf8mb4` | 字符集 |
| `MUSIC_DB_POOL_SIZE` | `4` | 连接池大小 |
| `MUSIC_DB_TIMEOUT_SEC` | `5` | 连接超时（秒） |
| `MUSIC_SERVER_BIND` | `0.0.0.0` | HTTP 监听地址 |
| `MUSIC_SERVER_PORT` | `8080` | HTTP 监听端口 |
| `MUSIC_MEDIA_ROOT` | exe 上两级目录 | 音频文件根目录 |
| `MUSIC_SMTP_HOST` | `smtp.qq.com` | 邮件服务器 |
| `MUSIC_SMTP_PORT` | `465` | 隐式 SSL 端口 |
| `MUSIC_SMTP_USER` / `MUSIC_SMTP_FROM` | 见 config.h | 认证账号 / 发件人（须一致） |
| `MUSIC_SMTP_FROM_NAME` | `小徐爱听歌` | 发件人昵称 |
| `MUSIC_SMTP_AUTH_CODE` | 空 | 邮箱**授权码**（非登录密码） |

### 邮件授权码（可选，但注册验证码依赖它）

优先级：环境变量 `MUSIC_SMTP_AUTH_CODE` > exe 同目录的 `mail.local.ini`（已在 `.gitignore` 忽略）。`mail.local.ini` 格式：

```ini
[smtp]
host=smtp.qq.com
port=465
user=你的QQ邮箱
from=你的QQ邮箱
from_name=小徐爱听歌
auth_code=邮箱授权码
```

未配置时服务端照常启动，仅注册验证码功能不可用。

## REST 接口

| 方法 | 路径 | 说明 |
| --- | --- | --- |
| GET | `/api/health` | 健康检查 |
| POST | `/api/email/code` | 发送注册验证码 |
| POST | `/api/register` | 注册 |
| POST | `/api/login` | 登录 |
| GET | `/api/songs` | 歌曲库列表（按 `userId` 过滤） |
| POST | `/api/songs` | 添加歌曲（写入归属用户） |
| GET | `/api/songs/{id}` | 歌曲详情 |
| DELETE | `/api/songs/{id}?userId=` | 删除歌曲（仅限本人） |
| GET | `/api/songs/{id}/stream` | 本地音频流（支持 Range） |
| GET | `/api/songs/{id}/online` | 曲库在线播放（转酷我直链） |
| GET | `/api/online/search` | 在线搜索 |
| GET | `/api/online/url` | 获取在线音频直链 |
| GET | `/api/online/lrc` | 获取在线歌词 |
| POST | `/api/play` | 上报播放记录 |
| GET | `/api/report/monthly` | 月度听歌报告 |

统一响应体：`{"code":0,"msg":"ok","data":{...}}`；`code != 0` 时 `msg` 为错误原因。

## 数据库对象

- **表（11）**：`user`、`artist`、`album`、`song`、`song_artist`、`playlist`、`playlist_song`、`play_record`、`favorite`、`tag`、`song_tag`
- **视图（2）**：`v_song_detail`、`v_user_play_summary`
- **触发器（2）**：`trg_play_insert`（累加 `play_count`）、`trg_playlist_song_bi`（歌单排序号自动编号）
- **存储过程（1）**：`sp_user_monthly_report`

## 已知问题

1. **Android 播放必须走系统原生后端**：APK 内置 FFmpeg 后端不含 TLS，无法播放 https 音频直链，故 `main.cpp` 在 Android 下强制 `QT_MEDIA_BACKEND=android`。
2. **APK 未打包 OpenSSL**：客户端 API 走 HTTP 不受影响；若后续需 Qt 自身发起 HTTPS 请求，需另行提供 `libssl` / `libcrypto`。
3. **签名变更需卸载重装**：更换 keystore 后，手机上必须先卸载旧 App 再安装新包。