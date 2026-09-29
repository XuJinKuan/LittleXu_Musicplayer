#pragma once

#include "util/mysqlpool.h" // 必须最先包含

#include <QString>

struct VerifyRecord {
    int verifyId = 0;
    QString email;
    QString code;
    QString expireTime;
};

// email_verify 表的访问对象：注册验证码的落库、校验、作废。
class VerifyDao
{
public:
    // 成功时通过 newVerifyId 返回自增主键
    bool insert(const QString &email, const QString &code,
                const QString &expireTime, int *newVerifyId);

    // 取该邮箱最新的一条「未使用且未过期」的验证码
    bool latestValid(const QString &email, VerifyRecord *out);

    bool markUsed(int verifyId);
    bool remove(int verifyId);

    // 该邮箱最后一次发信的时间（Unix 秒）。从未发过返回 0 且返回 true。
    bool lastSendEpoch(const QString &email, qint64 *epochSeconds);
};