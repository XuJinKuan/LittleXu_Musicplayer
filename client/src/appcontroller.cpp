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

int AppController::currentSongId() const
{
    return m_currentSongId;
}

QString AppController::currentTitle() const
{
    return m_currentTitle;
}

QString AppController::currentArtist() const
{
    return m_currentArtist;
}

QString AppController::currentStreamUrl() const
{
    return m_currentStreamUrl;
}

QString AppController::currentRid() const
{
    return m_currentRid;
}

QString AppController::currentLrc() const
{
    return m_currentLrc;
}

QVariantList AppController::onlineSongs() const
{
    return m_onlineSongs;
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

void AppController::playSong(int songId, const QString &title, const QString &artist,
                             int songIndex)
{
    if (songId <= 0) {
        fail(QStringLiteral("歌曲 ID 非法"));
        return;
    }

    // 记录当前索引，用于 prev/next
    if (songIndex >= 0) {
        m_currentSongIndex = songIndex;
    }

    // 本地无 mp3，歌曲库播放也走在线音源：先调 /api/songs/{id}/online 取直链
    m_pendingSongId = songId;
    m_pendingName = title;
    m_pendingArtist = artist;

    m_lastError.clear();
    emit lastErrorChanged();
    setBusy(true);

    m_api->get(QStringLiteral("songOnline"),
               QStringLiteral("/api/songs/%1/online").arg(songId),
               QUrlQuery());
}

void AppController::recordPlay(int songId, int playedSeconds, bool completed)
{
    if (!loggedIn()) {
        fail(QStringLiteral("请先登录后再记录播放"));
        return;
    }

    // songId > 0：本地歌曲；songId == 0：在线歌曲（需携带 online 信息）
    if (songId <= 0 && m_currentRid.isEmpty()) {
        fail(QStringLiteral("无法识别在线歌曲信息"));
        return;
    }

    setBusy(true);

    QJsonObject body;
    body.insert(QStringLiteral("userId"), m_userId);
    body.insert(QStringLiteral("songId"), songId);
    body.insert(QStringLiteral("playedSeconds"), playedSeconds);
    body.insert(QStringLiteral("completed"), completed);

    if (songId <= 0) {
        // 在线歌曲：传入 online 信息
        body.insert(QStringLiteral("onlineTitle"), m_currentTitle);
        body.insert(QStringLiteral("onlineArtist"), m_currentArtist);
        body.insert(QStringLiteral("onlineRid"), m_currentRid);
    }

    m_api->post(QStringLiteral("play"), QStringLiteral("/api/play"), body);
}

void AppController::showToast(const QString &message)
{
    emit toast(message);
}

void AppController::addSong(const QString &rid, const QString &title,
                            const QString &artist, const QString &album,
                            const QString &durationText)
{
    if (!loggedIn()) {
        fail(QStringLiteral("请先登录"));
        return;
    }

    // 酷我搜索返回的 durationText 是 "MM:SS" 格式，转为秒
    int duration = 0;
    const QStringList parts = durationText.split(QLatin1Char(':'));
    if (parts.size() == 2) {
        duration = parts[0].toInt() * 60 + parts[1].toInt();
    }
    if (duration <= 0) {
        duration = 180; // 默认 3 分钟
    }

    m_pendingAddRid = rid;
    m_pendingAddTitle = title;
    m_pendingAddArtist = artist;
    m_pendingAddAlbum = album;
    m_pendingAddDuration = duration;

    setBusy(true);

    QJsonObject body;
    body.insert(QStringLiteral("title"), title);
    body.insert(QStringLiteral("duration"), duration);
    body.insert(QStringLiteral("artist"), artist);
    body.insert(QStringLiteral("album"), album);
    body.insert(QStringLiteral("genre"), QStringLiteral("其他"));

    m_api->post(QStringLiteral("addSong"), QStringLiteral("/api/songs"), body);
}

void AppController::deleteSong(int songId)
{
    if (!loggedIn()) {
        fail(QStringLiteral("请先登录"));
        return;
    }

    if (songId <= 0) {
        fail(QStringLiteral("歌曲 ID 非法"));
        return;
    }

    setBusy(true);

    m_api->del(QStringLiteral("deleteSong"), QStringLiteral("/api/songs/%1").arg(songId));
}

void AppController::playNext()
{
    if (m_songsList.isEmpty() || m_currentSongIndex < 0) {
        fail(QStringLiteral("歌曲库为空或未在播放"));
        return;
    }

    // 内循环：到末尾回到开头
    const int nextIndex = (m_currentSongIndex + 1) % m_songsList.size();
    const QVariantMap song = m_songsList.at(nextIndex).toMap();

    m_currentSongIndex = nextIndex;
    playSong(song.value(QStringLiteral("songId")).toInt(),
             song.value(QStringLiteral("title")).toString(),
             song.value(QStringLiteral("artist")).toString());
}

void AppController::playPrev()
{
    if (m_songsList.isEmpty() || m_currentSongIndex < 0) {
        fail(QStringLiteral("歌曲库为空或未在播放"));
        return;
    }

    // 内循环：到开头回到末尾
    const int prevIndex = (m_currentSongIndex - 1 + m_songsList.size()) % m_songsList.size();
    const QVariantMap song = m_songsList.at(prevIndex).toMap();

    m_currentSongIndex = prevIndex;
    playSong(song.value(QStringLiteral("songId")).toInt(),
             song.value(QStringLiteral("title")).toString(),
             song.value(QStringLiteral("artist")).toString());
}

void AppController::searchOnline(const QString &keyword)
{
    const QString key = keyword.trimmed();
    if (key.isEmpty()) {
        fail(QStringLiteral("请输入要搜索的歌名或歌手"));
        return;
    }

    m_lastError.clear();
    emit lastErrorChanged();
    setBusy(true);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("keyword"), key);

    m_api->get(QStringLiteral("onlineSearch"), QStringLiteral("/api/online/search"), query);
}

void AppController::playOnlineSong(const QString &rid, const QString &name, const QString &artist)
{
    const QString key = rid.trimmed();
    if (key.isEmpty()) {
        fail(QStringLiteral("在线歌曲标识非法"));
        return;
    }

    // 直链每次都要现取（含 hash 且有时效），拿到之后再真正开始播放
    m_pendingRid = key;
    m_pendingName = name;
    m_pendingArtist = artist;
    m_pendingSongId = 0;    // 在线歌曲没有数据库 songId

    m_lastError.clear();
    emit lastErrorChanged();
    setBusy(true);

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("rid"), key);

    m_api->get(QStringLiteral("onlineUrl"), QStringLiteral("/api/online/url"), query);
}

void AppController::loadLrc()
{
    if (m_currentRid.isEmpty()) {
        m_currentLrc.clear();
        emit currentLrcChanged();
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("rid"), m_currentRid);

    m_api->get(QStringLiteral("onlineLrc"), QStringLiteral("/api/online/lrc"), query);
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

    if (tag == QLatin1String("onlineSearch")) {
        if (!ok) {
            m_onlineSongs.clear();
            emit onlineSongsChanged();
            fail(fallback);
            return;
        }

        handleOnlineSongs(data);
        m_lastError.clear();
        emit lastErrorChanged();
        emit onlineSongsChanged();
        return;
    }

    if (tag == QLatin1String("onlineUrl")) {
        if (!ok) {
            fail(fallback);
            return;
        }

        const QString direct = data.toObject().value(QStringLiteral("url")).toString();
        if (direct.isEmpty()) {
            fail(QStringLiteral("音源未返回可播放地址"));
            return;
        }

        // 在线歌曲：currentSongId 为 0，但保留 rid 用于记录和歌词
        m_currentSongId = 0;
        m_currentTitle = m_pendingName;
        m_currentArtist = m_pendingArtist;
        m_currentStreamUrl = direct;
        m_currentRid = m_pendingRid;

        emit currentSongChanged();
        emit playRequested();
        return;
    }

    if (tag == QLatin1String("songOnline")) {
        if (!ok) {
            fail(fallback);
            return;
        }

        const QJsonObject obj = data.toObject();
        const QString direct = obj.value(QStringLiteral("url")).toString();
        if (direct.isEmpty()) {
            fail(QStringLiteral("在线音源未返回可播放地址"));
            return;
        }

        // 歌曲库走在线音源：songId 保留用于记录，rid 用于歌词
        m_currentSongId = obj.value(QStringLiteral("songId")).toInt();
        m_currentTitle = obj.value(QStringLiteral("title")).toString();
        m_currentArtist = obj.value(QStringLiteral("artist")).toString();
        m_currentStreamUrl = direct;
        m_currentRid = obj.value(QStringLiteral("rid")).toString();

        emit currentSongChanged();
        emit playRequested();
        return;
    }

    if (tag == QLatin1String("onlineLrc")) {
        if (!ok) {
            m_currentLrc.clear();
            emit currentLrcChanged();
            return;
        }

        m_currentLrc = data.toObject().value(QStringLiteral("lrc")).toString();
        emit currentLrcChanged();
        return;
    }

    if (tag == QLatin1String("addSong")) {
        if (!ok) {
            fail(fallback);
            return;
        }

        emit toast(msg.isEmpty() ? QStringLiteral("已添加到歌曲库") : msg);
        // 刷新歌曲库列表
        loadSongs(QString(), 1);
        return;
    }

    if (tag == QLatin1String("deleteSong")) {
        if (!ok) {
            fail(fallback);
            return;
        }

        emit toast(msg.isEmpty() ? QStringLiteral("已从歌曲库删除") : msg);
        // 刷新歌曲库列表
        loadSongs(QString(), 1);
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

    m_songsList.clear();
    m_songsList.reserve(arr.size());

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

        // 缓存到 m_songsList，用于 prev/next
        QVariantMap map;
        map.insert(QStringLiteral("songId"), item.songId);
        map.insert(QStringLiteral("title"), item.title);
        map.insert(QStringLiteral("artist"), item.artist);
        m_songsList.append(map);
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

void AppController::handleOnlineSongs(const QJsonValue &data)
{
    const QJsonArray arr = data.toObject().value(QStringLiteral("items")).toArray();

    QVariantList rows;
    rows.reserve(arr.size());

    for (const QJsonValue &value : arr) {
        const QJsonObject row = value.toObject();
        const int duration = row.value(QStringLiteral("duration")).toInt();

        QVariantMap item;
        item.insert(QStringLiteral("rid"), row.value(QStringLiteral("rid")).toString());
        item.insert(QStringLiteral("name"), row.value(QStringLiteral("name")).toString());
        item.insert(QStringLiteral("artist"), row.value(QStringLiteral("artist")).toString());
        item.insert(QStringLiteral("album"), row.value(QStringLiteral("album")).toString());
        item.insert(QStringLiteral("duration"), duration);
        item.insert(QStringLiteral("durationText"), formatSeconds(duration));

        rows.append(item);
    }

    m_onlineSongs = rows;
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