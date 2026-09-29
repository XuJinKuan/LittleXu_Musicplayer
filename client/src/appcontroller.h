#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "songlistmodel.h"

class ApiClient;

// QML 的唯一入口对象（在 main.cpp 里以 context property "app" 注入）。
// 负责把 REST 响应翻译成 QML 友好的属性与信号。
class AppController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString serverBase READ serverBase WRITE setServerBase NOTIFY serverBaseChanged)
    Q_PROPERTY(bool loggedIn READ loggedIn NOTIFY sessionChanged)
    Q_PROPERTY(int userId READ userId NOTIFY sessionChanged)
    Q_PROPERTY(QString username READ username NOTIFY sessionChanged)
    Q_PROPERTY(QString nickname READ nickname NOTIFY sessionChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(SongListModel *songs READ songs CONSTANT)
    Q_PROPERTY(int songTotal READ songTotal NOTIFY songsChanged)
    Q_PROPERTY(int songLimit READ songLimit NOTIFY songsChanged)
    Q_PROPERTY(int currentPage READ currentPage NOTIFY songsChanged)

    Q_PROPERTY(int currentSongId READ currentSongId NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentArtist READ currentArtist NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentStreamUrl READ currentStreamUrl NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentRid READ currentRid NOTIFY currentSongChanged)
    Q_PROPERTY(QString currentLrc READ currentLrc NOTIFY currentLrcChanged)
    Q_PROPERTY(QVariantList onlineSongs READ onlineSongs NOTIFY onlineSongsChanged)

public:
    explicit AppController(QObject *parent = nullptr);

    QString serverBase() const;
    void setServerBase(const QString &base);

    bool loggedIn() const;
    int userId() const;
    QString username() const;
    QString nickname() const;

    bool busy() const;
    QString lastError() const;

    SongListModel *songs();
    int songTotal() const;
    int songLimit() const;
    int currentPage() const;

    int currentSongId() const;
    QString currentTitle() const;
    QString currentArtist() const;
    QString currentStreamUrl() const;
    QString currentRid() const;
    QString currentLrc() const;
    QVariantList onlineSongs() const;

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password,
                                  const QString &nickname, const QString &email,
                                  const QString &code);
    Q_INVOKABLE void sendEmailCode(const QString &email);
    Q_INVOKABLE void logout();

    Q_INVOKABLE void loadSongs(const QString &keyword, int page);
    Q_INVOKABLE void loadSongDetail(int songId);

    // 切换当前播放曲目；发出 playRequested() 后由 PlayerBar 真正拉起播放
    // 歌曲库播放也走在线音源：先调 /api/songs/{id}/online 取直链
    Q_INVOKABLE void playSong(int songId, const QString &title, const QString &artist,
                              int songIndex = -1);
    Q_INVOKABLE void recordPlay(int songId, int playedSeconds, bool completed);

    // 在线音源：搜索后由服务端实时解析直链，再交给 PlayerBar 播放。
    // 在线歌曲也记录到 play_history（song_id 为 NULL，记录 online 信息）。
    Q_INVOKABLE void searchOnline(const QString &keyword);
    Q_INVOKABLE void playOnlineSong(const QString &rid, const QString &name, const QString &artist);

    // 加载当前歌曲歌词（仅在播放时有效）
    Q_INVOKABLE void loadLrc();

    // 歌曲库管理：从在线搜索添加 / 删除
    Q_INVOKABLE void addSong(const QString &rid, const QString &title,
                             const QString &artist, const QString &album,
                             const QString &durationText);
    Q_INVOKABLE void deleteSong(int songId);

    // 上一首 / 下一首（基于歌曲库列表循环）
    Q_INVOKABLE void playNext();
    Q_INVOKABLE void playPrev();

    Q_INVOKABLE void loadMonthlyReport(int year, int month);

    Q_INVOKABLE void showToast(const QString &message);

signals:
    void serverBaseChanged();
    void sessionChanged();
    void busyChanged();
    void lastErrorChanged();
    void songsChanged();

    void songDetailReady(const QVariantMap &detail);
    void reportReady(const QVariantMap &report);
    void emailCodeSent();
    void toast(const QString &message);
    void currentSongChanged();
    void currentLrcChanged();
    void playRequested();
    void onlineSongsChanged();

private slots:
    void onApiFinished(const QString &tag, bool ok, int code,
                       const QJsonValue &data, const QString &msg);

private:
    void setBusy(bool busy);
    void fail(const QString &message, bool notify = true);

    void handleSongs(const QJsonValue &data);
    void handleSongDetail(const QJsonValue &data);
    void handleReport(const QJsonValue &data);
    void handleOnlineSongs(const QJsonValue &data);

    ApiClient *m_api = nullptr;
    SongListModel *m_songs = nullptr;

    int m_userId = 0;
    QString m_username;
    QString m_nickname;

    bool m_busy = false;
    QString m_lastError;

    int m_songTotal = 0;
    int m_songLimit = 20;
    int m_currentPage = 1;
    int m_currentSongIndex = -1;    // 当前播放的歌曲在 songs 列表中的索引
    QVariantList m_songsList;       // songs 列表缓存，用于 prev/next

    int m_currentSongId = 0;
    QString m_currentTitle;
    QString m_currentArtist;
    QString m_currentStreamUrl;
    QString m_currentRid;       // 在线歌曲 rid，用于歌词加载
    QString m_currentLrc;       // 当前歌词文本（LRC 格式）

    QVariantList m_onlineSongs;
    QString m_pendingRid;
    QString m_pendingName;
    QString m_pendingArtist;
    int m_pendingSongId = 0;    // playSong 走在线时暂存数据库 songId

    // 添加歌曲时暂存酷我 rid，等待 /api/songs/{id}/online 返回
    QString m_pendingAddRid;
    QString m_pendingAddTitle;
    QString m_pendingAddArtist;
    QString m_pendingAddAlbum;
    int m_pendingAddDuration = 0;
};

#endif // APPCONTROLLER_H