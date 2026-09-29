#pragma once

#include "util/mysqlpool.h" // 必须最先包含

#include <QString>
#include <QVector>

struct SongRecord {
    int songId = 0;
    QString title;
    int duration = 0;        // 秒
    int bitrate = 0;
    int year = 0;
    QString genre;
    int playCount = 0;
    QString filePath;
    int albumId = 0;
    QString albumName;
    QString artistNames;     // 多个歌手用 '/' 连接
};

struct PlayRecordRow {
    int songId = 0;
    QString title;
    QString genre;
    int playTimes = 0;
    int totalSeconds = 0;
    bool isFavorite = false;
};

class SongDao
{
public:
    // keyword 为空表示不过滤
    bool list(const QString &keyword, int limit, int offset,
              QVector<SongRecord> *out, int *total);

    bool detail(int songId, SongRecord *out);

    // 添加歌曲（在线音源入库，filePath 为空）
    bool addSong(const QString &title, int duration, const QString &artistName,
                 const QString &albumName, const QString &genre, int *newSongId);

    // 删除歌曲（级联删除 song_artist / play_record / favorite 等）
    bool deleteSong(int songId);

    bool addPlayRecord(int userId, int songId, int playedSeconds, bool completed);

    // 在线歌曲播放记录：songId 为 0，记录标题/歌手/酷我 rid
    bool addOnlinePlayRecord(int userId, const QString &title, const QString &artist,
                             const QString &rid, int playedSeconds, bool completed);

    // 调用存储过程 sp_user_monthly_report
    bool monthlyReport(int userId, int year, int month, QVector<PlayRecordRow> *out);
};