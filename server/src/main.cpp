#include "router/router.h"
#include "util/config.h"
#include "util/mysqlpool.h"

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("music_server"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // 1) 数据库连接池
    const cfg::DbConfig dbConfig = cfg::loadDbConfig();
    QString error;
    if (!MySqlPool::instance().init(dbConfig, &error)) {
        qCritical().noquote() << QStringLiteral("数据库初始化失败：%1").arg(error);
        return 1;
    }

    // 2) HTTP 服务
    const cfg::ServerConfig serverConfig = cfg::loadServerConfig();
    Router router;
    if (!router.start(serverConfig.bindAddress, serverConfig.port, &error)) {
        qCritical().noquote() << QStringLiteral("HTTP 服务启动失败：%1").arg(error);
        MySqlPool::instance().shutdown();
        return 1;
    }

    qInfo().noquote() << QStringLiteral("小徐爱听歌 服务端已启动：http://%1:%2")
                             .arg(serverConfig.bindAddress)
                             .arg(serverConfig.port);
    qInfo().noquote() << QStringLiteral("健康检查：GET /api/health");

    const int rc = app.exec();

    MySqlPool::instance().shutdown();
    return rc;
}