#pragma once

#include "dao/userdao.h"
#include "serviceresult.h"

#include <QJsonObject>
#include <QString>

class UserService
{
public:
    ServiceResult signUp(const QJsonObject &req);
    ServiceResult signIn(const QJsonObject &req);

    // SHA-256(salt + 明文)，十六进制小写
    static QString hashPassword(const QString &salt, const QString &password);

private:
    static QString makeSalt();

    UserDao m_userDao;
};