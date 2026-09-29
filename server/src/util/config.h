#pragma once

// 服务端配置。所有项都可用环境变量覆盖，方便换机器 / 换数据库密码时
// 不必改代码重新编译。

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QString>
#include <QtGlobal>

namespace cfg {

struct DbConfig {
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 3306;
    QString user = QStringLiteral("root");
    QString password;
    QString database = QStringLiteral("music_app");
    QString charset = QStringLiteral("utf8mb4");
    int poolSize = 4;
    int connectTimeoutSec = 5;
};

struct ServerConfig {
    QString bindAddress = QStringLiteral("0.0.0.0");
    quint16 port = 8080;
    QString mediaRoot;   // 音频文件根目录，song.file_path 相对此目录解析
};

// QQ 邮箱 SMTP：必须用「授权码」而不是登录密码，走隐式 SSL 的 465 端口。
struct MailConfig {
    QString host = QStringLiteral("smtp.qq.com");
    quint16 port = 465;
    QString user = QStringLiteral("363161953@qq.com");   // 认证账号
    QString from = QStringLiteral("363161953@qq.com");   // 发件人（须与认证账号一致）
    QString fromName = QStringLiteral("小徐爱听歌");
    QString authCode;                                    // 授权码，非登录密码
    int timeoutMs = 15000;

    bool configured() const { return !authCode.isEmpty(); }
};

inline QString envOr(const char *key, const QString &fallback)
{
    const QString v = qEnvironmentVariable(key);
    return v.isEmpty() ? fallback : v;
}

inline DbConfig loadDbConfig()
{
    DbConfig c;
    c.host = envOr("MUSIC_DB_HOST", c.host);
    c.user = envOr("MUSIC_DB_USER", c.user);
    // 空口令是合法配置，这里不能把「未设置」和「设置为空」混为一谈
    c.password = qEnvironmentVariable("MUSIC_DB_PASSWORD", c.password);
    c.database = envOr("MUSIC_DB_NAME", c.database);
    c.charset = envOr("MUSIC_DB_CHARSET", c.charset);

    bool ok = false;
    const int port = envOr("MUSIC_DB_PORT", QString::number(c.port)).toInt(&ok);
    if (ok && port > 0 && port <= 65535) {
        c.port = static_cast<quint16>(port);
    }

    ok = false;
    const int pool = envOr("MUSIC_DB_POOL_SIZE", QString::number(c.poolSize)).toInt(&ok);
    if (ok && pool > 0) {
        c.poolSize = pool;
    }

    ok = false;
    const int timeout = envOr("MUSIC_DB_TIMEOUT_SEC",
                              QString::number(c.connectTimeoutSec)).toInt(&ok);
    if (ok && timeout > 0) {
        c.connectTimeoutSec = timeout;
    }

    return c;
}

inline ServerConfig loadServerConfig()
{
    ServerConfig c;
    c.bindAddress = envOr("MUSIC_SERVER_BIND", c.bindAddress);

    bool ok = false;
    const int port = envOr("MUSIC_SERVER_PORT", QString::number(c.port)).toInt(&ok);
    if (ok && port > 0 && port <= 65535) {
        c.port = static_cast<quint16>(port);
    }

    // 音频根目录：默认取可执行文件目录的上两级（exe 在 server/build/，上两级即工程根目录），
    // 与 DB 里的 media/song/xxx.mp3 拼接即为完整路径。
    c.mediaRoot = envOr("MUSIC_MEDIA_ROOT",
                        QDir(QCoreApplication::applicationDirPath())
                            .absoluteFilePath(QStringLiteral("../..")));
    c.mediaRoot = QDir::cleanPath(c.mediaRoot);

    return c;
}

// 授权码属于私密信息，不进版本库：优先读环境变量，其次读可执行文件同目录的
// mail.local.ini（该文件已在 .gitignore 中忽略）。
inline MailConfig loadMailConfig()
{
    MailConfig c;

    QSettings ini(QCoreApplication::applicationDirPath() + QStringLiteral("/mail.local.ini"),
                  QSettings::IniFormat);
    ini.beginGroup(QStringLiteral("smtp"));
    c.host = ini.value(QStringLiteral("host"), c.host).toString();
    c.user = ini.value(QStringLiteral("user"), c.user).toString();
    c.from = ini.value(QStringLiteral("from"), c.from).toString();
    c.fromName = ini.value(QStringLiteral("from_name"), c.fromName).toString();
    c.authCode = ini.value(QStringLiteral("auth_code"), c.authCode).toString();

    bool ok = false;
    const int port = ini.value(QStringLiteral("port"), c.port).toInt(&ok);
    if (ok && port > 0 && port <= 65535) {
        c.port = static_cast<quint16>(port);
    }
    ini.endGroup();

    c.host = envOr("MUSIC_SMTP_HOST", c.host);
    c.user = envOr("MUSIC_SMTP_USER", c.user);
    c.from = envOr("MUSIC_SMTP_FROM", c.from);
    c.fromName = envOr("MUSIC_SMTP_FROM_NAME", c.fromName);
    c.authCode = qEnvironmentVariable("MUSIC_SMTP_AUTH_CODE", c.authCode);

    ok = false;
    const int envPort = envOr("MUSIC_SMTP_PORT", QString::number(c.port)).toInt(&ok);
    if (ok && envPort > 0 && envPort <= 65535) {
        c.port = static_cast<quint16>(envPort);
    }

    return c;
}

} // namespace cfg