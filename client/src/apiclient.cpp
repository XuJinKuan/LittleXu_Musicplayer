#include "apiclient.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_baseUrl(QStringLiteral("http://127.0.0.1:8080"))
{
}

QString ApiClient::baseUrl() const
{
    return m_baseUrl;
}

void ApiClient::setBaseUrl(const QString &baseUrl)
{
    const QString trimmed = baseUrl.trimmed();
    if (trimmed.isEmpty() || trimmed == m_baseUrl)
        return;

    // 允许用户只填 IP:端口
    if (!trimmed.startsWith(QLatin1String("http://"))
        && !trimmed.startsWith(QLatin1String("https://"))) {
        m_baseUrl = QStringLiteral("http://") + trimmed;
        return;
    }
    m_baseUrl = trimmed;
}

void ApiClient::get(const QString &tag, const QString &path, const QUrlQuery &query)
{
    QUrl url(m_baseUrl + path);
    if (!query.isEmpty())
        url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    watch(m_manager->get(request), tag);
}

void ApiClient::post(const QString &tag, const QString &path, const QJsonObject &body)
{
    QNetworkRequest request{QUrl(m_baseUrl + path)};
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));

    watch(m_manager->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact)), tag);
}

void ApiClient::watch(QNetworkReply *reply, const QString &tag)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, tag]() {
        reply->deleteLater();

        const QByteArray payload = reply->readAll();
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // 连不上服务器时没有 HTTP 状态码，此时才按网络故障处理
        if (httpStatus == 0) {
            emit finished(tag, false, 0, QJsonValue(), reply->errorString());
            return;
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            emit finished(tag, false, httpStatus, QJsonValue(),
                          QStringLiteral("服务端响应不是合法 JSON（HTTP %1）").arg(httpStatus));
            return;
        }

        const QJsonObject obj = doc.object();
        // 服务端复用 HTTP 状态码作为业务 code，0 表示成功
        const int code = obj.value(QStringLiteral("code")).toInt(httpStatus);
        const QString msg = obj.value(QStringLiteral("msg")).toString();
        const QJsonValue data = obj.value(QStringLiteral("data"));

        emit finished(tag, code == 0, code, data, msg);
    });
}