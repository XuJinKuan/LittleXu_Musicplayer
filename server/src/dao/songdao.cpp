#include "songdao.h"

namespace {

// 歌曲基础查询：一首歌一行，歌手用 GROUP_CONCAT 聚合成一列
QString songSelectSql()
{
    return QStringLiteral(
        "SELECT s.song_id, s.title, s.duration, s.bitrate, s.year, s.genre, "
        "       s.play_count, s.file_path, IFNULL(s.album_id, 0) AS album_id, "
        "       IFNULL(al.name, '') AS album_name, "
        "       IFNULL((SELECT GROUP_CONCAT(ar.name ORDER BY sa.role SEPARATOR '/') "
        "                 FROM song_artist sa JOIN artist ar ON sa.artist_id = ar.artist_id "
        "                WHERE sa.song_id = s.song_id), '') AS artist_names "
        "FROM song s "
        "LEFT JOIN album al ON s.album_id = al.album_id ");
}

void fillSong(const SqlResult &res, int row, SongRecord *out)
{
    out->songId = res.at(row, QStringLiteral("song_id")).toInt();
    out->title = res.at(row, QStringLiteral("title"));
    out->duration = res.at(row, QStringLiteral("duration")).toInt();
    out->bitrate = res.at(row, QStringLiteral("bitrate")).toInt();
    out->year = res.at(row, QStringLiteral("year")).toInt();
    out->genre = res.at(row, QStringLiteral("genre"));
    out->playCount = res.at(row, QStringLiteral("play_count")).toInt();
    out->filePath = res.at(row, QStringLiteral("file_path"));
    out->albumId = res.at(row, QStringLiteral("album_id")).toInt();
    out->albumName = res.at(row, QStringLiteral("album_name"));
    out->artistNames = res.at(row, QStringLiteral("artist_names"));
}

} // namespace

bool SongDao::list(const QString &keyword, int limit, int offset,
                   QVector<SongRecord> *out, int *total)
{
    if (!out) {
        return false;
    }
    out->clear();
    if (total) {
        *total = 0;
    }

    const int safeLimit = (limit <= 0 || limit > 200) ? 20 : limit;
    const int safeOffset = qMax(0, offset);

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    QString where;
    QVariantList args;
    if (!keyword.isEmpty()) {
        where = QStringLiteral("WHERE s.title LIKE ? OR al.name LIKE ? ");
        const QString like = QStringLiteral("%") + keyword + QStringLiteral("%");
        args << like << like;
    }

    if (total) {
        const QString countSql = MySqlPool::buildSql(
            QStringLiteral("SELECT COUNT(*) AS c FROM song s "
                           "LEFT JOIN album al ON s.album_id = al.album_id ") + where,
            args);
        SqlResult countRes;
        QString err;
        if (!lease->select(countSql, &countRes, &err)) {
            return false;
        }
        if (!countRes.isEmpty()) {
            *total = countRes.at(0, QStringLiteral("c")).toInt();
        }
    }

    QVariantList listArgs = args;
    listArgs << safeLimit << safeOffset;
    const QString sql = MySqlPool::buildSql(
        songSelectSql() + where
            + QStringLiteral("ORDER BY s.song_id ASC LIMIT ? OFFSET ?"),
        listArgs);

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }

    out->reserve(res.rowCount());
    for (int i = 0; i < res.rowCount(); ++i) {
        SongRecord rec;
        fillSong(res, i, &rec);
        out->append(rec);
    }
    return true;
}

bool SongDao::detail(int songId, SongRecord *out)
{
    if (!out) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        songSelectSql() + QStringLiteral("WHERE s.song_id = ? LIMIT 1"),
        {songId});

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }
    if (res.isEmpty()) {
        return false;
    }

    fillSong(res, 0, out);
    return true;
}

bool SongDao::addPlayRecord(int userId, int songId, int playedSeconds, bool completed)
{
    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    // song.play_count 由触发器 trg_play_insert 维护，这里不手工累加
    const QString sql = MySqlPool::buildSql(
        QStringLiteral("INSERT INTO play_record (user_id, song_id, played_seconds, is_completed) "
                       "VALUES (?, ?, ?, ?)"),
        {userId, songId, qMax(0, playedSeconds), completed});

    QString err;
    return lease->exec(sql, &err);
}

bool SongDao::monthlyReport(int userId, int year, int month, QVector<PlayRecordRow> *out)
{
    if (!out) {
        return false;
    }
    out->clear();

    if (year <= 0 || month < 1 || month > 12) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    // CALL 会产生多结果集，MySqlConn::select 只取第一个有行的结果集
    const QString sql = MySqlPool::buildSql(
        QStringLiteral("CALL sp_user_monthly_report(?, ?, ?)"),
        {userId, year, month});

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }

    out->reserve(res.rowCount());
    for (int i = 0; i < res.rowCount(); ++i) {
        PlayRecordRow row;
        row.songId = res.at(i, QStringLiteral("song_id")).toInt();
        row.title = res.at(i, QStringLiteral("title"));
        row.genre = res.at(i, QStringLiteral("genre"));
        row.playTimes = res.at(i, QStringLiteral("play_times")).toInt();
        row.totalSeconds = res.at(i, QStringLiteral("total_seconds")).toInt();
        row.isFavorite = res.at(i, QStringLiteral("is_favorite")).toInt() != 0;
        out->append(row);
    }
    return true;
}