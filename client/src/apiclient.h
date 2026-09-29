#ifndef APICLIENT_H
#define APICLIENT_H

#include <QJsonObject>
#include <QJsonValue>
#include <QObject>
#include <QString>
#include <QUrlQuery>

class QNetworkAccessManager;
class QNetworkReply;

// 极简 REST 客户端：只负责发请求、把响应解成 {code, msg, data}，
// 通过 finished 信号把结果连同调用方给的 tag 一起抛回去。
class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    QString baseUrl() const;
    void setBaseUrl(const QString &baseUrl);

    void get(const QString &tag, const QString &path, const QUrlQuery &query = QUrlQuery());
    void post(const QString &tag, const QString &path, const QJsonObject &body = QJsonObject());
    void del(const QString &tag, const QString &path);

signals:
    // ok 等价于服务端 code == 0；网络层失败时 code 为 0 且 msg 为错误描述。
    void finished(const QString &tag, bool ok, int code,
                  const QJsonValue &data, const QString &msg);

private:
    void watch(QNetworkReply *reply, const QString &tag);

    QNetworkAccessManager *m_manager = nullptr;
    QString m_baseUrl;
};

#endif // APICLIENT_H