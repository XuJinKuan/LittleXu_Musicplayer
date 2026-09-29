#include "userdao.h"

namespace {

QString mapUserSql()
{
    return QStringLiteral(
        "SELECT user_id, username, nickname, IFNULL(email, '') AS email, "
        "       salt, password_hash, IFNULL(avatar_path, '') AS avatar_path "
        "FROM `user` ");
}

void fillUser(const SqlResult &res, int row, UserRecord *out)
{
    out->userId = res.at(row, QStringLiteral("user_id")).toInt();
    out->username = res.at(row, QStringLiteral("username"));
    out->nickname = res.at(row, QStringLiteral("nickname"));
    out->email = res.at(row, QStringLiteral("email"));
    out->salt = res.at(row, QStringLiteral("salt"));
    out->passwordHash = res.at(row, QStringLiteral("password_hash"));
    out->avatarPath = res.at(row, QStringLiteral("avatar_path"));
}

} // namespace

bool UserDao::findByUsername(const QString &username, UserRecord *out)
{
    return findByUsernameWithPassword(username, out);
}

bool UserDao::findByUsernameWithPassword(const QString &username, UserRecord *out)
{
    if (!out) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        mapUserSql() + QStringLiteral("WHERE username = ? LIMIT 1"),
        {username});

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }
    if (res.isEmpty()) {
        return false;
    }

    fillUser(res, 0, out);
    return true;
}

bool UserDao::existsUsername(const QString &username, bool *exists)
{
    if (!exists) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("SELECT COUNT(*) AS c FROM `user` WHERE username = ?"),
        {username});

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }

    *exists = (!res.isEmpty() && res.at(0, QStringLiteral("c")).toInt() > 0);
    return true;
}

bool UserDao::existsEmail(const QString &email, bool *exists)
{
    if (!exists) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("SELECT COUNT(*) AS c FROM `user` WHERE email = ?"),
        {email});

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }

    *exists = (!res.isEmpty() && res.at(0, QStringLiteral("c")).toInt() > 0);
    return true;
}

bool UserDao::insert(const QString &username, const QString &nickname, const QString &email,
                     const QString &salt, const QString &passwordHash, int *newUserId)
{
    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("INSERT INTO `user` (username, nickname, email, salt, password_hash) "
                       "VALUES (?, ?, ?, ?, ?)"),
        {username, nickname, email, salt, passwordHash});

    QString err;
    if (!lease->exec(sql, &err)) {
        return false;
    }

    if (newUserId) {
        *newUserId = static_cast<int>(lease->lastInsertId());
    }
    return true;
}