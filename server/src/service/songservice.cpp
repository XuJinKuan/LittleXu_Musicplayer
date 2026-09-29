#include "songservice.h"

#include "onlineservice.h"
#include "util/http.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonValue>

namespace {

ServiceResult fail(int status, const QString &msg)
{
    ServiceResult r;
    r.httpStatus = status;
    r.body = http::result(status, msg);
    return r;
}

ServiceResult ok(const QJsonValue &data, const QString &msg = QStringLiteral("ok"))
{
    ServiceResult r;
    r.httpStatus = 200;
    r.body = http::result(0, msg, data);
    return r;
}

QJsonObject songToJson(const SongRecord &s, bool withFilePath)
{
    QJsonObject o;
    o.insert(QStringLiteral("songId"), s.songId);
    o.insert(QStringLiteral("title"), s.title);
    o.insert(QStringLiteral("duration"), s.duration);
    o.insert(QStringLiteral("bitrate"), s.bitrate);
    o.insert(QStringLiteral("year"), s.year);
    o.insert(QStringLiteral("genre"), s.genre);
    o.insert(QStringLiteral("playCount"), s.playCount);
    o.insert(QStringLiteral("albumId"), s.albumId);
    o.insert(QStringLiteral("albumName"), s.albumName);
    o.insert(QStringLiteral("artistNames"), s.artistNames);
    if (withFilePath) {
        o.insert(QStringLiteral("filePath"), s.filePath);
    }
    return o;
}

int intFromQuery(const QMap<QString, QString> &query, const QString &key, int fallback)
{
    if (!query.contains(key)) {
        return fallback;
    }
    bool ok = false;
    const int v = query.value(key).toInt(&ok);
    return ok ? v : fallback;
}

} // namespace

ServiceResult SongService::list(const QMap<QString, QString> &query)
{
    const QString keyword = query.value(QStringLiteral("keyword")).trimmed();
    const int limit = intFromQuery(query, QStringLiteral("limit"), 20);
    const int offset = intFromQuery(query, QStringLiteral("offset"), 0);

    QVector<SongRecord> songs;
    int total = 0;
    if (!m_songDao.list(keyword, limit, offset, &songs, &total)) {
        return fail(500, QStringLiteral("查询歌曲列表失败"));
    }

    QJsonArray arr;
    for (const SongRecord &s : songs) {
        arr.append(songToJson(s, false));
    }

    QJsonObject data;
    data.insert(QStringLiteral("total"), total);
    data.insert(QStringLiteral("limit"), (limit <= 0 || limit > 200) ? 20 : limit);
    data.insert(QStringLiteral("offset"), qMax(0, offset));
    data.insert(QStringLiteral("items"), arr);
    return ok(data);
}

ServiceResult SongService::detail(int songId)
{
    if (songId <= 0) {
        return fail(400, QStringLiteral("歌曲 ID 非法"));
    }

    SongRecord song;
    if (!m_songDao.detail(songId, &song)) {
        return fail(404, QStringLiteral("歌曲不存在"));
    }
    return ok(songToJson(song, true));
}

ServiceResult SongService::addSong(const QJsonObject &req)
{
    const QString title = req.value(QStringLiteral("title")).toString().trimmed();
    const int duration = req.value(QStringLiteral("duration")).toInt();
    const QString artist = req.value(QStringLiteral("artist")).toString().trimmed();
    const QString album = req.value(QStringLiteral("album")).toString().trimmed();
    const QString genre = req.value(QStringLiteral("genre")).toString().trimmed();

    if (title.isEmpty() || duration <= 0) {
        return fail(400, QStringLiteral("歌曲标题和时长必填"));
    }

    int newSongId = 0;
    if (!m_songDao.addSong(title, duration, artist, album, genre, &newSongId)) {
        return fail(500, QStringLiteral("添加歌曲失败"));
    }

    QJsonObject data;
    data.insert(QStringLiteral("songId"), newSongId);
    return ok(data, QStringLiteral("歌曲已添加到库"));
}

ServiceResult SongService::deleteSong(int songId)
{
    if (songId <= 0) {
        return fail(400, QStringLiteral("歌曲 ID 非法"));
    }

    if (!m_songDao.deleteSong(songId)) {
        return fail(500, QStringLiteral("删除歌曲失败"));
    }

    QJsonObject data;
    data.insert(QStringLiteral("songId"), songId);
    return ok(data, QStringLiteral("歌曲已从库中删除"));
}

ServiceResult SongService::play(const QJsonObject &req)
{
    const int userId = req.value(QStringLiteral("userId")).toInt();
    const int songId = req.value(QStringLiteral("songId")).toInt();
    const int playedSeconds = req.value(QStringLiteral("playedSeconds")).toInt();
    const bool completed = req.value(QStringLiteral("completed")).toBool(false);

    if (userId <= 0) {
        return fail(400, QStringLiteral("userId 必填"));
    }

    // songId > 0：本地歌曲；songId == 0：在线歌曲，需携带 online 信息
    if (songId > 0) {
        SongRecord song;
        if (!m_songDao.detail(songId, &song)) {
            return fail(404, QStringLiteral("歌曲不存在"));
        }

        if (!m_songDao.addPlayRecord(userId, songId, playedSeconds, completed)) {
            return fail(500, QStringLiteral("写入播放记录失败"));
        }

        QJsonObject data;
        data.insert(QStringLiteral("songId"), songId);
        data.insert(QStringLiteral("playCount"), song.playCount + 1);
        return ok(data, QStringLiteral("播放记录已写入"));
    }

    // 在线歌曲记录
    const QString onlineTitle = req.value(QStringLiteral("onlineTitle")).toString().trimmed();
    const QString onlineArtist = req.value(QStringLiteral("onlineArtist")).toString().trimmed();
    const QString onlineRid = req.value(QStringLiteral("onlineRid")).toString().trimmed();

    if (onlineTitle.isEmpty() || onlineRid.isEmpty()) {
        return fail(400, QStringLiteral("在线歌曲需提供 onlineTitle 与 onlineRid"));
    }

    if (!m_songDao.addOnlinePlayRecord(userId, onlineTitle, onlineArtist,
                                       onlineRid, playedSeconds, completed)) {
        return fail(500, QStringLiteral("写入在线播放记录失败"));
    }

    QJsonObject data;
    data.insert(QStringLiteral("songId"), 0);
    data.insert(QStringLiteral("onlineTitle"), onlineTitle);
    return ok(data, QStringLiteral("在线播放记录已写入"));
}

ServiceResult SongService::monthlyReport(const QMap<QString, QString> &query)
{
    const int userId = intFromQuery(query, QStringLiteral("userId"), 0);
    const int year = intFromQuery(query, QStringLiteral("year"), 0);
    const int month = intFromQuery(query, QStringLiteral("month"), 0);

    if (userId <= 0) {
        return fail(400, QStringLiteral("userId 必填"));
    }
    if (year <= 0 || month < 1 || month > 12) {
        return fail(400, QStringLiteral("year / month 非法"));
    }

    QVector<PlayRecordRow> rows;
    if (!m_songDao.monthlyReport(userId, year, month, &rows)) {
        return fail(500, QStringLiteral("生成月度报告失败"));
    }

    QJsonArray arr;
    int totalSeconds = 0;
    for (const PlayRecordRow &row : rows) {
        QJsonObject o;
        o.insert(QStringLiteral("songId"), row.songId);
        o.insert(QStringLiteral("title"), row.title);
        o.insert(QStringLiteral("genre"), row.genre);
        o.insert(QStringLiteral("playTimes"), row.playTimes);
        o.insert(QStringLiteral("totalSeconds"), row.totalSeconds);
        o.insert(QStringLiteral("isFavorite"), row.isFavorite);
        arr.append(o);
        totalSeconds += row.totalSeconds;
    }

    QJsonObject data;
    data.insert(QStringLiteral("userId"), userId);
    data.insert(QStringLiteral("year"), year);
    data.insert(QStringLiteral("month"), month);
    data.insert(QStringLiteral("songCount"), rows.size());
    data.insert(QStringLiteral("totalSeconds"), totalSeconds);
    data.insert(QStringLiteral("items"), arr);
    return ok(data);
}

ServiceResult SongService::onlineStream(int songId)
{
    if (songId <= 0) {
        return fail(400, QStringLiteral("歌曲 ID 非法"));
    }

    SongRecord song;
    if (!m_songDao.detail(songId, &song)) {
        return fail(404, QStringLiteral("歌曲不存在"));
    }

    // 用「歌名 + 歌手」去酷我搜索，取第一首作为在线音源
    OnlineService online;
    QMap<QString, QString> query;
    query.insert(QStringLiteral("keyword"), song.title + QStringLiteral(" ") + song.artistNames);

    ServiceResult searchResult = online.search(query);
    if (searchResult.httpStatus != 200) {
        return searchResult;
    }

    const QJsonObject searchData = searchResult.body
        .value(QStringLiteral("data")).toObject();
    const QJsonArray items = searchData.value(QStringLiteral("items")).toArray();

    if (items.isEmpty()) {
        return fail(404, QStringLiteral("在线音源未找到该歌曲"));
    }

    // 取第一首的 rid 去解析直链
    const QString rid = items.first().toObject().value(QStringLiteral("rid")).toString();
    if (rid.isEmpty()) {
        return fail(502, QStringLiteral("在线音源返回数据异常"));
    }

    QMap<QString, QString> urlQuery;
    urlQuery.insert(QStringLiteral("rid"), rid);
    ServiceResult urlResult = online.url(urlQuery);

    if (urlResult.httpStatus != 200) {
        return urlResult;
    }

    // 把直链和歌曲信息一起返回
    QJsonObject urlData = urlResult.body
        .value(QStringLiteral("data")).toObject();

    QJsonObject data;
    data.insert(QStringLiteral("songId"), songId);
    data.insert(QStringLiteral("title"), song.title);
    data.insert(QStringLiteral("artist"), song.artistNames);
    data.insert(QStringLiteral("url"), urlData.value(QStringLiteral("url")).toString());
    data.insert(QStringLiteral("rid"), rid);

    return ok(data);
}

ServiceResult SongService::stream(int songId, const QString &rangeHeader)
{
    if (songId <= 0) {
        return fail(400, QStringLiteral("歌曲 ID 非法"));
    }

    SongRecord song;
    if (!m_songDao.detail(songId, &song)) {
        return fail(404, QStringLiteral("歌曲不存在"));
    }
    if (song.filePath.isEmpty()) {
        return fail(404, QStringLiteral("该歌曲未登记音频文件"));
    }

    // 目录穿越防护：拼接并归一化后必须仍位于 mediaRoot 之内
    const QString root = QDir::cleanPath(m_mediaRoot);
    const QString full = QDir::cleanPath(root + QLatin1Char('/') + song.filePath);
    if (full != root && !full.startsWith(root + QLatin1Char('/'))) {
        return fail(403, QStringLiteral("音频路径非法"));
    }

    QFile file(full);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return fail(404, QStringLiteral("音频文件不存在：%1").arg(song.filePath));
    }

    const qint64 fileSize = file.size();
    qint64 start = 0;
    qint64 end = fileSize - 1;
    bool partial = false;

    const QString range = rangeHeader.trimmed();
    if (!range.isEmpty()) {
        // 只支持单段：bytes=start-end / bytes=start- / bytes=-suffix，多段时取第一段
        QString spec = range;
        if (spec.startsWith(QLatin1String("bytes="), Qt::CaseInsensitive)) {
            spec = spec.mid(6);
        }
        spec = spec.section(QLatin1Char(','), 0, 0).trimmed();

        const int dash = spec.indexOf(QLatin1Char('-'));
        bool ok = (dash >= 0);
        if (ok) {
            const QString first = spec.left(dash).trimmed();
            const QString second = spec.mid(dash + 1).trimmed();
            if (first.isEmpty() && !second.isEmpty()) {
                const qint64 suffix = second.toLongLong(&ok);   // 末尾 N 字节
                if (ok && suffix > 0) {
                    start = qMax<qint64>(0, fileSize - suffix);
                    end = fileSize - 1;
                }
            } else if (!first.isEmpty()) {
                start = first.toLongLong(&ok);
                if (ok) {
                    if (second.isEmpty()) {
                        end = fileSize - 1;
                    } else {
                        end = second.toLongLong(&ok);
                    }
                }
            } else {
                ok = false;
            }
        }

        if (!ok || start < 0 || start >= fileSize || end < start) {
            ServiceResult r;
            r.httpStatus = 416;
            r.binary = true;
            r.contentType = QByteArrayLiteral("text/plain; charset=utf-8");
            r.binaryBody = QByteArrayLiteral("Range Not Satisfiable");
            r.headers.append(qMakePair(QByteArrayLiteral("Accept-Ranges"),
                                       QByteArrayLiteral("bytes")));
            r.headers.append(qMakePair(QByteArrayLiteral("Content-Range"),
                                       QByteArrayLiteral("bytes */")
                                           + QByteArray::number(fileSize)));
            return r;
        }

        if (end >= fileSize) {
            end = fileSize - 1;
        }
        partial = true;
    }

    if (!file.seek(start)) {
        return fail(500, QStringLiteral("读取音频文件失败"));
    }
    const qint64 want = end - start + 1;
    const QByteArray chunk = file.read(want);
    if (chunk.size() != want) {
        return fail(500, QStringLiteral("读取音频文件失败"));
    }

    ServiceResult r;
    r.httpStatus = partial ? 206 : 200;
    r.binary = true;
    r.contentType = QByteArrayLiteral("audio/mpeg");
    r.binaryBody = chunk;
    r.headers.append(qMakePair(QByteArrayLiteral("Accept-Ranges"),
                               QByteArrayLiteral("bytes")));
    if (partial) {
        const QByteArray contentRange = QByteArrayLiteral("bytes ")
            + QByteArray::number(start) + '-' + QByteArray::number(end) + '/'
            + QByteArray::number(fileSize);
        r.headers.append(qMakePair(QByteArrayLiteral("Content-Range"), contentRange));
    }
    return r;
}