#pragma once

#include "dao/songdao.h"
#include "serviceresult.h"

#include <QJsonObject>
#include <QMap>
#include <QString>

class SongService
{
public:
    ServiceResult list(const QMap<QString, QString> &query);
    ServiceResult detail(int songId);
    ServiceResult addSong(const QJsonObject &req);
    ServiceResult deleteSong(int songId);
    ServiceResult play(const QJsonObject &req);
    ServiceResult monthlyReport(const QMap<QString, QString> &query);

    // 音频流：支持单段 Range 请求，命中时返回 206 Partial Content
    ServiceResult stream(int songId, const QString &rangeHeader);

    // 根据数据库歌曲信息在线搜索并返回酷我直链（本地无 mp3 时使用）
    ServiceResult onlineStream(int songId);

    void setMediaRoot(const QString &root) { m_mediaRoot = root; }

private:
    SongDao m_songDao;
    QString m_mediaRoot;
};