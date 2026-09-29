#include "songservice.h"

#include "util/http.h"

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

ServiceResult SongService::play(const QJsonObject &req)
{
    const int userId = req.value(QStringLiteral("userId")).toInt();
    const int songId = req.value(QStringLiteral("songId")).toInt();
    const int playedSeconds = req.value(QStringLiteral("playedSeconds")).toInt();
    const bool completed = req.value(QStringLiteral("completed")).toBool(false);

    if (userId <= 0 || songId <= 0) {
        return fail(400, QStringLiteral("userId 与 songId 必填"));
    }

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