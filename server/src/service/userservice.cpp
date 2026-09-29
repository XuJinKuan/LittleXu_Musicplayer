#include "userservice.h"

#include "util/http.h"

#include <QCryptographicHash>
#include <QJsonValue>
#include <QUuid>

namespace {

constexpr int kMinUsernameLen = 3;
constexpr int kMaxUsernameLen = 50;
constexpr int kMinPasswordLen = 6;
constexpr int kMaxPasswordLen = 64;

ServiceResult fail(int status, const QString &msg)
{
    ServiceResult r;
    r.httpStatus = status;
    r.body = http::result(status, msg);
    return r;
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

    bool exists = false;
    if (!m_userDao.existsUsername(username, &exists)) {
        return fail(500, QStringLiteral("数据库查询失败"));
    }
    if (exists) {
        return fail(409, QStringLiteral("用户名已被占用"));
    }

    const QString salt = makeSalt();
    const QString hash = hashPassword(salt, password);

    int newUserId = 0;
    if (!m_userDao.insert(username, nickname, salt, hash, &newUserId)) {
        return fail(500, QStringLiteral("注册失败，请稍后重试"));
    }

    QJsonObject data;
    data.insert(QStringLiteral("userId"), newUserId);
    data.insert(QStringLiteral("username"), username);
    data.insert(QStringLiteral("nickname"), nickname);

    ServiceResult r;
    r.httpStatus = 200;
    r.body = http::result(0, QStringLiteral("注册成功"), data);
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