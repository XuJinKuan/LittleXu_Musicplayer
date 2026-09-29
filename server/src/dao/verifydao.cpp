#include "verifydao.h"

bool VerifyDao::insert(const QString &email, const QString &code,
                       const QString &expireTime, int *newVerifyId)
{
    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("INSERT INTO email_verify (email, code, expire_time) "
                       "VALUES (?, ?, ?)"),
        {email, code, expireTime});

    QString err;
    if (!lease->exec(sql, &err)) {
        return false;
    }

    if (newVerifyId) {
        *newVerifyId = static_cast<int>(lease->lastInsertId());
    }
    return true;
}

bool VerifyDao::latestValid(const QString &email, VerifyRecord *out)
{
    if (!out) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("SELECT verify_id, email, code, expire_time FROM email_verify "
                       "WHERE email = ? AND used = 0 AND expire_time > NOW() "
                       "ORDER BY verify_id DESC LIMIT 1"),
        {email});

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }
    if (res.isEmpty()) {
        return false;
    }

    out->verifyId = res.at(0, QStringLiteral("verify_id")).toInt();
    out->email = res.at(0, QStringLiteral("email"));
    out->code = res.at(0, QStringLiteral("code"));
    out->expireTime = res.at(0, QStringLiteral("expire_time"));
    return true;
}

bool VerifyDao::markUsed(int verifyId)
{
    if (verifyId <= 0) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("UPDATE email_verify SET used = 1 WHERE verify_id = ?"),
        {verifyId});

    QString err;
    return lease->exec(sql, &err);
}

bool VerifyDao::remove(int verifyId)
{
    if (verifyId <= 0) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("DELETE FROM email_verify WHERE verify_id = ?"),
        {verifyId});

    QString err;
    return lease->exec(sql, &err);
}

bool VerifyDao::lastSendEpoch(const QString &email, qint64 *epochSeconds)
{
    if (!epochSeconds) {
        return false;
    }

    MySqlPool::Lease lease = MySqlPool::instance().acquire();
    if (!lease.valid()) {
        return false;
    }

    const QString sql = MySqlPool::buildSql(
        QStringLiteral("SELECT UNIX_TIMESTAMP(MAX(send_time)) AS ts "
                       "FROM email_verify WHERE email = ?"),
        {email});

    SqlResult res;
    QString err;
    if (!lease->select(sql, &res, &err)) {
        return false;
    }

    // 无记录时 MAX() 为 NULL，取出来是空字符串
    const QString ts = res.isEmpty() ? QString() : res.at(0, QStringLiteral("ts"));
    *epochSeconds = ts.isEmpty() ? 0 : ts.toLongLong();
    return true;
}