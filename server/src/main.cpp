#include "router/router.h"
#include "util/config.h"
#include "util/mailer.h"
#include "util/mysqlpool.h"

#include <QCoreApplication>
#include <QDebug>
#include <QSslSocket>

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

    // 2) 邮件服务（注册验证码）
    const cfg::MailConfig mailConfig = cfg::loadMailConfig();
    Mailer::init(mailConfig);
    if (Mailer::isReady()) {
        qInfo().noquote() << QStringLiteral("邮件服务已就绪：%1:%2（发件人 %3）")
                                 .arg(mailConfig.host)
                                 .arg(mailConfig.port)
                                 .arg(mailConfig.user);
        if (!QSslSocket::supportsSsl()) {
            qWarning().noquote() << QStringLiteral(
                "当前环境 SSL 不可用，发信会失败：请确认 libssl-1_1-x64.dll 与 "
                "libcrypto-1_1-x64.dll 与可执行文件同目录");
        }
    } else {
        qWarning().noquote() << QStringLiteral(
            "未配置 SMTP 授权码，注册验证码功能不可用：请设置环境变量 "
            "MUSIC_SMTP_AUTH_CODE，或在可执行文件同目录创建 mail.local.ini");
    }

    // 3) HTTP 服务
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