#include "router.h"

#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>

namespace {

constexpr int kMaxBufferBytes = 1 * 1024 * 1024; // 单个请求上限 1 MB

ServiceResult notFound()
{
    ServiceResult r;
    r.httpStatus = 404;
    r.body = http::result(404, QStringLiteral("接口不存在"));
    return r;
}

ServiceResult methodNotAllowed()
{
    ServiceResult r;
    r.httpStatus = 405;
    r.body = http::result(405, QStringLiteral("请求方法不允许"));
    return r;
}

ServiceResult badRequest(const QString &msg)
{
    ServiceResult r;
    r.httpStatus = 400;
    r.body = http::result(400, msg);
    return r;
}

} // namespace

Router::Router(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QTcpServer::newConnection, this, &Router::onNewConnection);
}

bool Router::start(const QString &bindAddress, quint16 port, QString *error)
{
    const QHostAddress address(bindAddress);
    if (!m_server.listen(address, port)) {
        if (error) {
            *error = QStringLiteral("监听 %1:%2 失败：%3")
                         .arg(bindAddress)
                         .arg(port)
                         .arg(m_server.errorString());
        }
        return false;
    }
    return true;
}

void Router::onNewConnection()
{
    while (QTcpSocket *socket = m_server.nextPendingConnection()) {
        m_buffers.insert(socket, QByteArray());
        connect(socket, &QTcpSocket::readyRead, this, &Router::onReadyRead);
        connect(socket, &QTcpSocket::disconnected, this, &Router::onDisconnected);
    }
}

void Router::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket || !m_buffers.contains(socket)) {
        return;
    }

    QByteArray &buffer = m_buffers[socket];
    buffer += socket->readAll();

    if (buffer.size() > kMaxBufferBytes) {
        socket->write(http::makeResponse(413, QByteArrayLiteral("{\"code\":413,\"msg\":\"请求过大\"}")));
        socket->disconnectFromHost();
        return;
    }

    HttpRequest req;
    const bool parsed = http::parseRequest(buffer, &req);

    if (!parsed && !req.complete) {
        return; // 数据未收全，继续等
    }

    QByteArray response;
    int status = 200;
    if (!req.valid) {
        status = 400;
        response = http::jsonResponse(http::result(400, req.error), status);
    } else if (req.method == QLatin1String("OPTIONS")) {
        status = 204;
        response = http::makeResponse(204, QByteArray());
    } else {
        const ServiceResult result = route(req);
        status = result.httpStatus;
        if (result.binary) {
            response = http::makeResponse(result.httpStatus, result.binaryBody,
                                          result.contentType, result.headers);
        } else {
            response = http::jsonResponse(result.body, result.httpStatus);
        }
    }

    // route() 内部可能进入嵌套事件循环（在线音源是伪同步请求）。等待期间客户端可能
    // 已断开并触发 cleanup()，socket 已被移出 m_buffers 并 deleteLater，不能再访问。
    if (!m_buffers.contains(socket)) {
        return;
    }

    QString logLine = QStringLiteral("%1 %2 %3 <- %4");
    logLine = logLine.arg(req.method.isEmpty() ? QStringLiteral("-") : req.method)
                     .arg(req.path.isEmpty() ? QStringLiteral("-") : req.path)
                     .arg(status)
                     .arg(socket->peerAddress().toString());
    qInfo().noquote() << logLine;

    m_buffers[socket].clear();
    socket->write(response);
    socket->disconnectFromHost();
}

void Router::onDisconnected()
{
    cleanup(qobject_cast<QTcpSocket *>(sender()));
}

void Router::cleanup(QTcpSocket *socket)
{
    if (!socket) {
        return;
    }
    if (!m_buffers.remove(socket)) {
        return; // 已经清理过，避免 deleteLater 重复投递
    }
    socket->deleteLater();
}

ServiceResult Router::route(const HttpRequest &req)
{
    const QString &path = req.path;
    const bool isGet = (req.method == QLatin1String("GET"));
    const bool isPost = (req.method == QLatin1String("POST"));
    const bool isDelete = (req.method == QLatin1String("DELETE"));

    if (path == QLatin1String("/api/health")) {
        if (!isGet) {
            return methodNotAllowed();
        }
        ServiceResult r;
        r.httpStatus = 200;
        QJsonObject data;
        data.insert(QStringLiteral("status"), QStringLiteral("up"));
        data.insert(QStringLiteral("serverTime"),
                    QDateTime::currentDateTime().toString(Qt::ISODate));
        r.body = http::result(0, QStringLiteral("ok"), data);
        return r;
    }

    if (path == QLatin1String("/api/email/code")) {
        if (!isPost) {
            return methodNotAllowed();
        }
        return m_userService.sendEmailCode(req.json);
    }

    if (path == QLatin1String("/api/register")) {
        if (!isPost) {
            return methodNotAllowed();
        }
        return m_userService.signUp(req.json);
    }

    if (path == QLatin1String("/api/login")) {
        if (!isPost) {
            return methodNotAllowed();
        }
        return m_userService.signIn(req.json);
    }

    if (path == QLatin1String("/api/online/search")) {
        if (!isGet) {
            return methodNotAllowed();
        }
        return m_onlineService.search(req.query);
    }

    if (path == QLatin1String("/api/online/url")) {
        if (!isGet) {
            return methodNotAllowed();
        }
        return m_onlineService.url(req.query);
    }

    if (path == QLatin1String("/api/online/lrc")) {
        if (!isGet) {
            return methodNotAllowed();
        }
        return m_onlineService.lrc(req.query);
    }

    if (path == QLatin1String("/api/songs")) {
        if (isGet) {
            return m_songService.list(req.query);
        }
        if (isPost) {
            return m_songService.addSong(req.json);
        }
        return methodNotAllowed();
    }

    // 必须放在 /api/songs/{id} 之前：否则 path.mid(11) 会拿到 "1/stream"
    if (path.startsWith(QLatin1String("/api/songs/")) && path.endsWith(QLatin1String("/stream"))) {
        if (!isGet) {
            return methodNotAllowed();
        }
        const QString idStr = path.mid(11, path.size() - 11 - 7);
        bool ok = false;
        const int songId = idStr.toInt(&ok);
        if (!ok || songId <= 0) {
            return badRequest(QStringLiteral("歌曲 ID 必须是整数"));
        }
        return m_songService.stream(songId, req.headers.value(QStringLiteral("range")));
    }

    // 歌曲库在线播放：根据 songId 查数据库后转发酷我搜索取直链
    if (path.startsWith(QLatin1String("/api/songs/")) && path.endsWith(QLatin1String("/online"))) {
        if (!isGet) {
            return methodNotAllowed();
        }
        const QString idStr = path.mid(11, path.size() - 11 - 7);
        bool ok = false;
        const int songId = idStr.toInt(&ok);
        if (!ok || songId <= 0) {
            return badRequest(QStringLiteral("歌曲 ID 必须是整数"));
        }
        return m_songService.onlineStream(songId);
    }

    if (path.startsWith(QLatin1String("/api/songs/"))) {
        const QString idStr = path.mid(11);
        bool ok = false;
        const int songId = idStr.toInt(&ok);
        if (!ok) {
            return badRequest(QStringLiteral("歌曲 ID 必须是整数"));
        }
        if (isGet) {
            return m_songService.detail(songId);
        }
        if (isDelete) {
            return m_songService.deleteSong(songId);
        }
        return methodNotAllowed();
    }

    if (path == QLatin1String("/api/play")) {
        if (!isPost) {
            return methodNotAllowed();
        }
        return m_songService.play(req.json);
    }

    if (path == QLatin1String("/api/report/monthly")) {
        if (!isGet) {
            return methodNotAllowed();
        }
        return m_songService.monthlyReport(req.query);
    }

    return notFound();
}