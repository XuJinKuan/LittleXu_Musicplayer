#include "appcontroller.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QUrlQuery>

#include "apiclient.h"

namespace {

constexpr int kDefaultLimit = 20;

QString formatSeconds(int seconds)
{
    if (seconds <= 0)
        return QStringLiteral("--:--");

    return QStringLiteral("%1:%2")
        .arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

AppController::AppController(QObject *parent)
    : QObject(parent)
    , m_api(new ApiClient(this))
    , m_songs(new SongListModel(this))
{
    connect(m_api, &ApiClient::finished, this, &AppController::onApiFinished);
}

QString AppController::serverBase() const
{
    return m_api->baseUrl();
}

void AppController::setServerBase(const QString &base)
{
    if (base.trimmed() == m_api->baseUrl())
        return;

    m_api->setBaseUrl(base);
    emit serverBaseChanged();
}

bool AppController::loggedIn() const
{
    return m_userId > 0;
}

int AppController::userId() const
{
    return m_userId;
}

QString AppController::username() const
{
    return m_username;
}

QString AppController::nickname() const
{
    return m_nickname;
}

bool AppController::busy() const
{
    return m_busy;
}

QString AppController::lastError() const
{
    return m_lastError;
}

SongListModel *AppController::songs()
{
    return m_songs;
}

int AppController::songTotal() const
{
    return m_songTotal;
}

int AppController::songLimit() const
{
    return m_songLimit;
}

int AppController::currentPage() const
{
    return m_currentPage;
}

void AppController::setBusy(bool busy)
{
    if (m_busy == busy)
        return;

    m_busy = busy;
    emit busyChanged();
}

void AppController::fail(const QString &message, bool notify)
{
    m_lastError = message;
    emit lastErrorChanged();

    if (notify)
        emit toast(message);
}

void AppController::login(const QString &username, const QString &password)
{
    const QString name = username.trimmed();
    if (name.isEmpty() || password.isEmpty()) {
        fail(QStringLiteral("用户名和密码不能为空"));
        return;
    }

    m_lastError.clear();
    emit lastErrorChanged();
    setBusy(true);

    QJsonObject body;
    body.insert(QStringLiteral("username"), name);
    body.insert(QStringLiteral("password"), password);

    m_api->post(QStringLiteral("login"), QStringLiteral("/api/login"), body);
}

void AppController::registerUser(const QString &username, const QString &password,
                                const QString &nickname, const QString &email,
                                const QString &code)
{
    const QString name = username.trimmed();
    if (name.isEmpty() || password.isEmpty()) {
        fail(QStringLiteral("用户名和密码不能为空"));
        return;
    }
    if (email.trimmed().isEmpty()) {
        fail(QStringLiteral("请填写邮箱"));
        return;
    }
    if (code.trimmed().size() != 4) {
        fail(QStringLiteral("请填写 4 位邮箱验证码"));
        return;
    }

    m_lastError.clear();
    emit lastErrorChanged();
    setBusy(true);

    QJsonObject body;
    body.insert(QStringLiteral("username"), name);
    body.insert(QStringLiteral("password"), password);
    body.insert(QStringLiteral("nickname"), nickname.trimmed());
    body.insert(QStringLiteral("email"), email.trimmed());
    body.insert(QStringLiteral("code"), code.trimmed());

    m_api->post(QStringLiteral("register"), QStringLiteral("/api/register"), body);
}

void AppController::sendEmailCode(const QString &email)
{
    const QString target = email.trimmed();
    if (target.isEmpty()) {
        fail(QStringLiteral("请先填写邮箱"));
        return;
    }

    m_lastError.clear();
    emit lastErrorChanged();
    setBusy(true);

    QJsonObject body;
    body.insert(QStringLiteral("email"), target);

    m_api->post(QStringLiteral("emailCode"), QStringLiteral("/api/email/code"), body);
}

void AppController::logout()
{
    m_userId = 0;
    m_username.clear();
    m_nickname.clear();

    m_songs->clear();
    m_songTotal = 0;
    m_currentPage = 1;

    m_lastError.clear();
    emit lastErrorChanged();
    emit sessionChanged();
    emit songsChanged();
}

void AppController::loadSongs(const QString &keyword, int page)
{
    if (page < 1)
        page = 1;

    QUrlQuery query;
    const QString key = keyword.trimmed();
    if (!key.isEmpty())
        query.addQueryItem(QStringLiteral("keyword"), key);
    query.addQueryItem(QStringLiteral("limit"), QString::number(m_songLimit));
    query.addQueryItem(QStringLiteral("offset"), QString::number((page - 1) * m_songLimit));

    m_currentPage = page;
    setBusy(true);

    m_api->get(QStringLiteral("songs"), QStringLiteral("/api/songs"), query);
}

void AppController::loadSongDetail(int songId)
{
    if (songId <= 0)
        return;

    setBusy(true);
    m_api->get(QStringLiteral("songDetail"), QStringLiteral("/api/songs/%1").arg(songId));
}

void AppController::recordPlay(int songId, int playedSeconds)
{
    if (songId <= 0)
        return;

    if (!loggedIn()) {
        fail(QStringLiteral("请先登录后再记录播放"));
        return;
    }

    setBusy(true);

    QJsonObject body;
    body.insert(QStringLiteral("userId"), m_userId);
    body.insert(QStringLiteral("songId"), songId);
    body.insert(QStringLiteral("playedSeconds"), playedSeconds);
    body.insert(QStringLiteral("completed"), false);

    m_api->post(QStringLiteral("play"), QStringLiteral("/api/play"), body);
}

void AppController::loadMonthlyReport(int year, int month)
{
    if (year <= 0 || month < 1 || month > 12)
        return;

    if (!loggedIn()) {
        fail(QStringLiteral("请先登录后再查看报告"));
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("userId"), QString::number(m_userId));
    query.addQueryItem(QStringLiteral("year"), QString::number(year));
    query.addQueryItem(QStringLiteral("month"), QString::number(month));

    setBusy(true);

    m_api->get(QStringLiteral("report"), QStringLiteral("/api/report/monthly"), query);
}

void AppController::onApiFinished(const QString &tag, bool ok, int code,
                                  const QJsonValue &data, const QString &msg)
{
    setBusy(false);

    const QString fallback = msg.isEmpty()
        ? QStringLiteral("请求失败（code=%1）").arg(code)
        : msg;

    if (tag == QLatin1String("emailCode")) {
        if (!ok) {
            fail(fallback);
            return;
        }

        m_lastError.clear();
        emit lastErrorChanged();
        emit emailCodeSent();
        emit toast(msg.isEmpty() ? QStringLiteral("验证码已发送") : msg);
        return;
    }

    if (tag == QLatin1String("login") || tag == QLatin1String("register")) {
        if (!ok) {
            fail(fallback);
            return;
        }

        const QJsonObject obj = data.toObject();
        m_userId = obj.value(QStringLiteral("userId")).toInt();
        m_username = obj.value(QStringLiteral("username")).toString();
        m_nickname = obj.value(QStringLiteral("nickname")).toString();

        m_lastError.clear();
        emit lastErrorChanged();
        emit sessionChanged();
        emit toast(msg.isEmpty() ? QStringLiteral("操作成功") : msg);
        return;
    }

    if (tag == QLatin1String("songs")) {
        if (!ok) {
            m_songs->clear();
            m_songTotal = 0;
            emit songsChanged();
            fail(fallback, false);
            return;
        }

        handleSongs(data);
        m_lastError.clear();
        emit lastErrorChanged();
        emit songsChanged();
        return;
    }

    if (tag == QLatin1String("songDetail")) {
        if (!ok) {
            fail(fallback);
            return;
        }
        handleSongDetail(data);
        return;
    }

    if (tag == QLatin1String("play")) {
        if (!ok) {
            fail(fallback);
            return;
        }
        emit toast(msg.isEmpty() ? QStringLiteral("播放记录已写入") : msg);
        return;
    }

    if (tag == QLatin1String("report")) {
        if (!ok) {
            emit reportReady(QVariantMap());
            fail(fallback, false);
            return;
        }
        handleReport(data);
        return;
    }
}

void AppController::handleSongs(const QJsonValue &data)
{
    const QJsonObject obj = data.toObject();
    m_songTotal = obj.value(QStringLiteral("total")).toInt();
    m_songLimit = obj.value(QStringLiteral("limit")).toInt(m_songLimit);

    const QJsonArray arr = obj.value(QStringLiteral("items")).toArray();
    QVector<SongItem> items;
    items.reserve(arr.size());

    for (const QJsonValue &value : arr) {
        const QJsonObject row = value.toObject();

        SongItem item;
        item.songId = row.value(QStringLiteral("songId")).toInt();
        item.title = row.value(QStringLiteral("title")).toString();
        item.artist = row.value(QStringLiteral("artistNames")).toString();
        item.album = row.value(QStringLiteral("albumName")).toString();
        item.genre = row.value(QStringLiteral("genre")).toString();
        item.duration = row.value(QStringLiteral("duration")).toInt();
        item.playCount = row.value(QStringLiteral("playCount")).toInt();

        items.append(item);
    }

    m_songs->setSongs(items);
}

void AppController::handleSongDetail(const QJsonValue &data)
{
    const QJsonObject obj = data.toObject();
    const int duration = obj.value(QStringLiteral("duration")).toInt();

    QVariantMap detail;
    detail.insert(QStringLiteral("songId"), obj.value(QStringLiteral("songId")).toInt());
    detail.insert(QStringLiteral("title"), obj.value(QStringLiteral("title")).toString());
    detail.insert(QStringLiteral("artistNames"), obj.value(QStringLiteral("artistNames")).toString());
    detail.insert(QStringLiteral("albumName"), obj.value(QStringLiteral("albumName")).toString());
    detail.insert(QStringLiteral("genre"), obj.value(QStringLiteral("genre")).toString());
    detail.insert(QStringLiteral("year"), obj.value(QStringLiteral("year")).toInt());
    detail.insert(QStringLiteral("duration"), duration);
    detail.insert(QStringLiteral("durationText"), formatSeconds(duration));
    detail.insert(QStringLiteral("bitrate"), obj.value(QStringLiteral("bitrate")).toInt());
    detail.insert(QStringLiteral("playCount"), obj.value(QStringLiteral("playCount")).toInt());
    detail.insert(QStringLiteral("filePath"), obj.value(QStringLiteral("filePath")).toString());

    emit songDetailReady(detail);
}

void AppController::handleReport(const QJsonValue &data)
{
    const QJsonObject obj = data.toObject();
    const int totalSeconds = obj.value(QStringLiteral("totalSeconds")).toInt();

    QVariantMap report;
    report.insert(QStringLiteral("userId"), obj.value(QStringLiteral("userId")).toInt());
    report.insert(QStringLiteral("year"), obj.value(QStringLiteral("year")).toInt());
    report.insert(QStringLiteral("month"), obj.value(QStringLiteral("month")).toInt());
    report.insert(QStringLiteral("songCount"), obj.value(QStringLiteral("songCount")).toInt());
    report.insert(QStringLiteral("totalSeconds"), totalSeconds);
    report.insert(QStringLiteral("totalMinutes"), totalSeconds / 60);

    QVariantList rows;
    const QJsonArray arr = obj.value(QStringLiteral("items")).toArray();
    rows.reserve(arr.size());

    for (const QJsonValue &value : arr) {
        const QJsonObject row = value.toObject();

        QVariantMap item;
        item.insert(QStringLiteral("songId"), row.value(QStringLiteral("songId")).toInt());
        item.insert(QStringLiteral("title"), row.value(QStringLiteral("title")).toString());
        item.insert(QStringLiteral("genre"), row.value(QStringLiteral("genre")).toString());
        item.insert(QStringLiteral("playTimes"), row.value(QStringLiteral("playTimes")).toInt());
        item.insert(QStringLiteral("totalSeconds"), row.value(QStringLiteral("totalSeconds")).toInt());
        item.insert(QStringLiteral("isFavorite"), row.value(QStringLiteral("isFavorite")).toBool());

        rows.append(item);
    }
    report.insert(QStringLiteral("items"), rows);

    emit reportReady(report);
}