#pragma once

#include "util/mysqlpool.h" // 必须最先包含

#include <QString>

struct UserRecord {
    int userId = 0;
    QString username;
    QString nickname;
    QString email;
    QString salt;
    QString passwordHash;
    QString avatarPath;
};

class UserDao
{
public:
    bool findByUsername(const QString &username, UserRecord *out);
    bool findByUsernameWithPassword(const QString &username, UserRecord *out);
    bool existsUsername(const QString &username, bool *exists);
    bool existsEmail(const QString &email, bool *exists);

    // 成功时通过 newUserId 返回自增主键
    bool insert(const QString &username, const QString &nickname, const QString &email,
                const QString &salt, const QString &passwordHash, int *newUserId);
};