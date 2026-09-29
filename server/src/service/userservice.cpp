#include "userservice.h"

#include "util/http.h"
#include "util/mailer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QUuid>

namespace {

constexpr int kMinUsernameLen = 3;
constexpr int kMaxUsernameLen = 50;
constexpr int kMinPasswordLen = 6;
constexpr int kMaxPasswordLen = 64;

constexpr int kMaxEmailLen = 100;
constexpr int kCodeTtlSeconds = 300;    // 验证码有效期 5 分钟
constexpr int kResendCooldownSec = 60;  // 同一邮箱两次发信的最小间隔

ServiceResult fail(int status, const QString &msg)
{
    ServiceResult r;
    r.httpStatus = status;
    r.body = http::result(status, msg);
    return r;
}

bool isValidEmail(const QString &email)
{
    if (email.isEmpty() || email.size() > kMaxEmailLen) {
        return false;
    }
    static const QRegularExpression re(
        QStringLiteral("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$"));
    return re.match(email).hasMatch();
}

QString formatDateTime(const QDateTime &dt)
{
    return dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

} // namespace

QString UserService::makeSalt()
{
    // 取 UUID 去掉大括号和连字符后的前 16 位，足够避免彩虹表
    const QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString compact;
    compact.reserve(uuid.size());
    for (const QChar ch : uuid) {
        if (ch != QLatin1Char('-')) {
            compact += ch;
        }
    }
    return compact.left(16);
}

QString UserService::hashPassword(const QString &salt, const QString &password)
{
    const QByteArray raw = (salt + password).toUtf8();
    const QByteArray digest = QCryptographicHash::hash(raw, QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

ServiceResult UserService::signUp(const QJsonObject &req)
{
    const QString username = req.value(QStringLiteral("username")).toString().trimmed();
    const QString password = req.value(QStringLiteral("password")).toString();
    QString nickname = req.value(QStringLiteral("nickname")).toString().trimmed();
    const QString email = req.value(QStringLiteral("email")).toString().trimmed();
    const QString code = req.value(QStringLiteral("code")).toString().trimmed();

    if (username.size() < kMinUsernameLen || username.size() > kMaxUsernameLen) {
        return fail(400, QStringLiteral("用户名长度需在 %1~%2 之间")
                             .arg(kMinUsernameLen)
                             .arg(kMaxUsernameLen));
    }
    if (password.size() < kMinPasswordLen || password.size() > kMaxPasswordLen) {
        return fail(400, QStringLiteral("密码长度需在 %1~%2 之间")
                             .arg(kMinPasswordLen)
                             .arg(kMaxPasswordLen));
    }
    if (nickname.isEmpty()) {
        nickname = username;
    }
    if (nickname.size() > kMaxUsernameLen) {
        return fail(400, QStringLiteral("昵称过长"));
    }
    if (!isValidEmail(email)) {
        return fail(400, QStringLiteral("请填写正确的邮箱地址"));
    }
    if (code.size() != 4) {
        return fail(400, QStringLiteral("请输入 4 位验证码"));
    }

    bool exists = false;
    if (!m_userDao.existsUsername(username, &exists)) {
        return fail(500, QStringLiteral("数据库查询失败"));
    }
    if (exists) {
        return fail(409, QStringLiteral("用户名已被占用"));
    }

    if (!m_userDao.existsEmail(email, &exists)) {
        return fail(500, QStringLiteral("数据库查询失败"));
    }
    if (exists) {
        return fail(409, QStringLiteral("该邮箱已被注册"));
    }

    VerifyRecord record;
    if (!m_verifyDao.latestValid(email, &record)) {
        return fail(400, QStringLiteral("验证码已过期或不存在，请重新获取"));
    }
    if (record.code != code) {
        return fail(400, QStringLiteral("验证码错误"));
    }

    const QString salt = makeSalt();
    const QString hash = hashPassword(salt, password);

    int newUserId = 0;
    if (!m_userDao.insert(username, nickname, email, salt, hash, &newUserId)) {
        return fail(500, QStringLiteral("注册失败，请稍后重试"));
    }

    // 注册成功后作废该验证码，防止一码多用
    m_verifyDao.markUsed(record.verifyId);

    QJsonObject data;
    data.insert(QStringLiteral("userId"), newUserId);
    data.insert(QStringLiteral("username"), username);
    data.insert(QStringLiteral("nickname"), nickname);
    data.insert(QStringLiteral("email"), email);

    ServiceResult r;
    r.httpStatus = 200;
    r.body = http::result(0, QStringLiteral("注册成功"), data);
    return r;
}

ServiceResult UserService::sendEmailCode(const QJsonObject &req)
{
    const QString email = req.value(QStringLiteral("email")).toString().trimmed();

    if (!isValidEmail(email)) {
        return fail(400, QStringLiteral("请填写正确的邮箱地址"));
    }

    bool exists = false;
    if (!m_userDao.existsEmail(email, &exists)) {
        return fail(500, QStringLiteral("数据库查询失败"));
    }
    if (exists) {
        return fail(409, QStringLiteral("该邮箱已被注册"));
    }

    // 限流：同一邮箱 60 秒内只允许发一次
    qint64 lastEpoch = 0;
    if (!m_verifyDao.lastSendEpoch(email, &lastEpoch)) {
        return fail(500, QStringLiteral("数据库查询失败"));
    }
    if (lastEpoch > 0) {
        const qint64 elapsed = QDateTime::currentSecsSinceEpoch() - lastEpoch;
        if (elapsed >= 0 && elapsed < kResendCooldownSec) {
            return fail(429, QStringLiteral("发送过于频繁，请 %1 秒后重试")
                                 .arg(kResendCooldownSec - elapsed));
        }
    }

    const int value = QRandomGenerator::global()->bounded(1000, 10000);
    const QString code = QString::number(value);
    const QString expireTime =
        formatDateTime(QDateTime::currentDateTime().addSecs(kCodeTtlSeconds));

    int verifyId = 0;
    if (!m_verifyDao.insert(email, code, expireTime, &verifyId)) {
        return fail(500, QStringLiteral("验证码保存失败，请稍后重试"));
    }

    const QString subject = QStringLiteral("【小徐爱听歌】注册验证码");
    const QString body = QStringLiteral(
        "您的注册验证码是：%1，%2 分钟内有效。\n"
        "请勿将验证码告知他人；如非本人操作，忽略本邮件即可。")
        .arg(code)
        .arg(kCodeTtlSeconds / 60);

    QString mailError;
    if (!Mailer::send(email, subject, body, &mailError)) {
        // 发信失败时删除刚写入的记录，避免白白占用 60 秒冷却时间
        m_verifyDao.remove(verifyId);
        qWarning().noquote() << QStringLiteral("发送验证码邮件失败：%1").arg(mailError);
        return fail(500, QStringLiteral("验证码邮件发送失败：%1").arg(mailError));
    }

    QJsonObject data;
    data.insert(QStringLiteral("email"), email);
    data.insert(QStringLiteral("expireSeconds"), kCodeTtlSeconds);
    data.insert(QStringLiteral("resendAfter"), kResendCooldownSec);

    ServiceResult r;
    r.httpStatus = 200;
    r.body = http::result(0, QStringLiteral("验证码已发送，请查收邮件"), data);
    return r;
}

ServiceResult UserService::signIn(const QJsonObject &req)
{
    const QString username = req.value(QStringLiteral("username")).toString().trimmed();
    const QString password = req.value(QStringLiteral("password")).toString();

    if (username.isEmpty() || password.isEmpty()) {
        return fail(400, QStringLiteral("用户名和密码不能为空"));
    }

    UserRecord user;
    if (!m_userDao.findByUsername(username, &user)) {
        return fail(401, QStringLiteral("用户名或密码错误"));
    }

    const QString hash = hashPassword(user.salt, password);
    if (hash.compare(user.passwordHash, Qt::CaseInsensitive) != 0) {
        return fail(401, QStringLiteral("用户名或密码错误"));
    }

    QJsonObject data;
    data.insert(QStringLiteral("userId"), user.userId);
    data.insert(QStringLiteral("username"), user.username);
    data.insert(QStringLiteral("nickname"), user.nickname);
    data.insert(QStringLiteral("avatarPath"), user.avatarPath);

    ServiceResult r;
    r.httpStatus = 200;
    r.body = http::result(0, QStringLiteral("登录成功"), data);
    return r;
}