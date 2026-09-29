#pragma once

#include "serviceresult.h"

#include <QMap>
#include <QNetworkAccessManager>
#include <QString>

// 在线音源（酷我）。
//
// 重要约束：服务端是单线程同步模型，这里用「嵌套 QEventLoop」把网络请求伪同步化，
// 请求期间整个服务端会被阻塞（上限 8 秒）。单人自用可接受，多人并发搜索会被拖慢。
class OnlineService
{
public:
    // GET /api/online/search?keyword=xxx
    ServiceResult search(const QMap<QString, QString> &query);

    // GET /api/online/url?rid=MUSIC_xxx
    // 酷我直链 URL 内含 hash 且有时效，必须每次实时获取，不可缓存。
    ServiceResult url(const QMap<QString, QString> &query);

    // GET /api/online/lrc?rid=MUSIC_xxx
    // 返回酷我歌词文本（LRC 格式，含时间轴）。
    ServiceResult lrc(const QMap<QString, QString> &query);

private:
    // 同步 GET：成功返回响应体，失败返回空并写入 error
    QByteArray syncGet(const QString &url, QString *error);

    QNetworkAccessManager m_nam;
};