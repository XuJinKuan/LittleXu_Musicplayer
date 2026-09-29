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
    ServiceResult play(const QJsonObject &req);
    ServiceResult monthlyReport(const QMap<QString, QString> &query);

private:
    SongDao m_songDao;
};