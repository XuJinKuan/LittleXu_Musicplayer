#pragma once

#include "service/serviceresult.h"
#include "service/songservice.h"
#include "service/userservice.h"
#include "util/http.h"

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTcpServer>

class QTcpSocket;

// 极简路由：QTcpServer + 单请求短连接。
// 刻意不使用 Q_OBJECT（不需要自定义信号槽），成员函数指针的 connect 即可工作。
class Router : public QObject
{
public:
    explicit Router(QObject *parent = nullptr);

    bool start(const QString &bindAddress, quint16 port, QString *error);

private:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

    void cleanup(QTcpSocket *socket);
    ServiceResult route(const HttpRequest &req);

    QTcpServer m_server;
    QHash<QTcpSocket *, QByteArray> m_buffers;

    UserService m_userService;
    SongService m_songService;
};