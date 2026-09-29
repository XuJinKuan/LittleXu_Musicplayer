#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QJsonValue>
#include <QObject>
#include <QString>
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

    Q_INVOKABLE void login(const QString &username, const QString &password);
    Q_INVOKABLE void registerUser(const QString &username, const QString &password,
                                  const QString &nickname, const QString &email,
                                  const QString &code);
    Q_INVOKABLE void sendEmailCode(const QString &email);
    Q_INVOKABLE void logout();

    Q_INVOKABLE void loadSongs(const QString &keyword, int page);
    Q_INVOKABLE void loadSongDetail(int songId);
    Q_INVOKABLE void recordPlay(int songId, int playedSeconds);

    Q_INVOKABLE void loadMonthlyReport(int year, int month);

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

private slots:
    void onApiFinished(const QString &tag, bool ok, int code,
                       const QJsonValue &data, const QString &msg);

private:
    void setBusy(bool busy);
    void fail(const QString &message, bool notify = true);

    void handleSongs(const QJsonValue &data);
    void handleSongDetail(const QJsonValue &data);
    void handleReport(const QJsonValue &data);

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
};

#endif // APPCONTROLLER_H