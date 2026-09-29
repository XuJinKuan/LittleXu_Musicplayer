#pragma once

// 服务端配置。所有项都可用环境变量覆盖，方便换机器 / 换数据库密码时
// 不必改代码重新编译。

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

    return c;
}

} // namespace cfg