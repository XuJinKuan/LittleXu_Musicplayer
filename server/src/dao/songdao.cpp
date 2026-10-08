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

bool SongDao::list(int userId, const QString &keyword, int limit, int offset,
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

    // 曲库按用户隔离：只查归属当前用户的歌曲
    QString where = QStringLiteral("WHERE s.owner_user_id = ? ");
    QVariantList args;
    args << userId;
    if (!keyword.isEmpty()) {
        // 歌曲名 / 专辑名 / 歌手名 任一命中即可。歌手是多对多，
        // 用 EXISTS 避免 JOIN 造成同一首歌重复出行。
        where += QStringLiteral(
            "AND (s.title LIKE ? OR al.name LIKE ? "
            "   OR EXISTS (SELECT 1 FROM song_artist sa "
            "                JOIN artist ar ON sa.artist_id = ar.artist_id "
            "               WHERE sa.song_id = s.song_id AND ar.name LIKE ?)) ");
        const QString like = QStringLiteral("%") + keyword + QStringLiteral("%");
        args << like << like << like;
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

bool SongDao::addSong(const QString &title, int duration, const QString &artistName,
                      const QString &albumName, const QString &genre, int ownerUserId,
                      int *newSongId)
{
    if (newSongId) {
        *newSongId = 0;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    // 1. 确保 artist 存在，取 artist_id
    int artistId = 0;
    if (!artistName.isEmpty()) {
        const QString findArtist = MySqlPool::buildSql(
            QStringLiteral("SELECT artist_id FROM artist WHERE name = ? LIMIT 1"),
            {artistName});
        SqlResult artistRes;
        QString err;
        if (lease->select(findArtist, &artistRes, &err) && !artistRes.isEmpty()) {
            artistId = artistRes.at(0, QStringLiteral("artist_id")).toInt();
        } else {
            const QString insArtist = MySqlPool::buildSql(
                QStringLiteral("INSERT INTO artist (name) VALUES (?)"),
                {artistName});
            if (!lease->exec(insArtist, &err)) {
                return false;
            }
            // 重新查询取 id
            if (lease->select(findArtist, &artistRes, &err) && !artistRes.isEmpty()) {
                artistId = artistRes.at(0, QStringLiteral("artist_id")).toInt();
            }
        }
    }

    // 2. 确保 album 存在，取 album_id
    int albumId = 0;
    if (!albumName.isEmpty()) {
        const QString findAlbum = MySqlPool::buildSql(
            QStringLiteral("SELECT album_id FROM album WHERE name = ? LIMIT 1"),
            {albumName});
        SqlResult albumRes;
        QString err;
        if (lease->select(findAlbum, &albumRes, &err) && !albumRes.isEmpty()) {
            albumId = albumRes.at(0, QStringLiteral("album_id")).toInt();
        } else {
            const QString insAlbum = MySqlPool::buildSql(
                QStringLiteral("INSERT INTO album (name, artist_id) VALUES (?, ?)"),
                {albumName, artistId > 0 ? artistId : QVariant()});
            if (!lease->exec(insAlbum, &err)) {
                return false;
            }
            if (lease->select(findAlbum, &albumRes, &err) && !albumRes.isEmpty()) {
                albumId = albumRes.at(0, QStringLiteral("album_id")).toInt();
            }
        }
    }

    // 3. 插入 song
    const QString insSong = MySqlPool::buildSql(
        QStringLiteral("INSERT INTO song (title, duration, file_path, genre, album_id, owner_user_id) "
                       "VALUES (?, ?, NULL, ?, ?, ?)"),
        {title, qMax(1, duration), genre.isEmpty() ? QStringLiteral("其他") : genre,
         albumId > 0 ? albumId : QVariant(), ownerUserId});
    QString err;
    if (!lease->exec(insSong, &err)) {
        return false;
    }

    // 4. 取刚插入的 song_id
    const QString lastIdSql = QStringLiteral("SELECT LAST_INSERT_ID() AS id");
    SqlResult idRes;
    if (!lease->select(lastIdSql, &idRes, &err) || idRes.isEmpty()) {
        return false;
    }
    const int songId = idRes.at(0, QStringLiteral("id")).toInt();
    if (songId <= 0) {
        return false;
    }

    // 5. 关联 artist
    if (artistId > 0) {
        const QString insSA = MySqlPool::buildSql(
            QStringLiteral("INSERT INTO song_artist (song_id, artist_id, role) VALUES (?, ?, '主唱')"),
            {songId, artistId});
        if (!lease->exec(insSA, &err)) {
            return false;
        }
    }

    if (newSongId) {
        *newSongId = songId;
    }
    return true;
}

bool SongDao::deleteSong(int songId, int userId)
{
    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    // 仅允许删除归属本人的歌曲；song_artist / play_record / favorite /
    // playlist_song 都是 ON DELETE CASCADE，只需删 song 主表
    const QString sql = MySqlPool::buildSql(
        QStringLiteral("DELETE FROM song WHERE song_id = ? AND owner_user_id = ?"),
        {songId, userId});

    QString err;
    if (!lease->exec(sql, &err)) {
        return false;
    }
    // 受影响行数为 0 说明该歌不存在或不属于当前用户
    return lease->affectedRows() > 0;
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

bool SongDao::addOnlinePlayRecord(int userId, const QString &title, const QString &artist,
                                  const QString &rid, int playedSeconds, bool completed)
{
    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    // 在线歌曲 song_id 为 NULL，使用 online_title / online_artist / online_rid 记录
    const QString sql = MySqlPool::buildSql(
        QStringLiteral("INSERT INTO play_record "
                       "(user_id, song_id, online_title, online_artist, online_rid, "
                       " played_seconds, is_completed) "
                       "VALUES (?, NULL, ?, ?, ?, ?, ?)"),
        {userId, title, artist, rid, qMax(0, playedSeconds), completed});

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