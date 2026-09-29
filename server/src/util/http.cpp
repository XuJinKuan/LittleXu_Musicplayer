#include "http.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QList>
#include <QUrl>
#include <QUrlQuery>

namespace http {

namespace {

constexpr int kMaxRequestLineBytes = 8192;

int findHeaderEnd(const QByteArray &buffer)
{
    return buffer.indexOf("\r\n\r\n");
}

} // namespace

QByteArray statusText(int status)
{
    switch (status) {
    case 200: return QByteArrayLiteral("OK");
    case 204: return QByteArrayLiteral("No Content");
    case 400: return QByteArrayLiteral("Bad Request");
    case 401: return QByteArrayLiteral("Unauthorized");
    case 403: return QByteArrayLiteral("Forbidden");
    case 404: return QByteArrayLiteral("Not Found");
    case 405: return QByteArrayLiteral("Method Not Allowed");
    case 409: return QByteArrayLiteral("Conflict");
    case 413: return QByteArrayLiteral("Payload Too Large");
    case 500: return QByteArrayLiteral("Internal Server Error");
    case 503: return QByteArrayLiteral("Service Unavailable");
    default:  return QByteArrayLiteral("Unknown");
    }
}

bool parseRequest(const QByteArray &buffer, HttpRequest *out)
{
    if (!out) {
        return false;
    }
    *out = HttpRequest();

    const int headerEnd = findHeaderEnd(buffer);
    if (headerEnd < 0) {
        if (buffer.size() > kMaxRequestLineBytes * 8) {
            out->complete = true;
            out->valid = false;
            out->error = QStringLiteral("请求头过大");
        }
        return false;
    }

    const QByteArray head = buffer.left(headerEnd);
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty()) {
        out->complete = true;
        out->valid = false;
        out->error = QStringLiteral("空请求");
        return false;
    }

    // ---- 请求行 ----
    QByteArray requestLine = lines.first().trimmed();
    if (requestLine.isEmpty() || requestLine.size() > kMaxRequestLineBytes) {
        out->complete = true;
        out->valid = false;
        out->error = QStringLiteral("请求行非法");
        return false;
    }

    const QList<QByteArray> parts = requestLine.split(' ');
    if (parts.size() < 2) {
        out->complete = true;
        out->valid = false;
        out->error = QStringLiteral("请求行非法");
        return false;
    }

    out->method = QString::fromUtf8(parts.at(0)).toUpper();
    const QString rawTarget = QString::fromUtf8(parts.at(1));

    const int question = rawTarget.indexOf(QLatin1Char('?'));
    if (question >= 0) {
        out->path = rawTarget.left(question);
        const QUrlQuery uq(rawTarget.mid(question + 1));
        const auto items = uq.queryItems();
        for (const auto &item : items) {
            out->query.insert(item.first, item.second);
        }
    } else {
        out->path = rawTarget;
    }
    out->path = QUrl::fromPercentEncoding(out->path.toUtf8());

    if (out->path.isEmpty()) {
        out->path = QStringLiteral("/");
    }
    if (out->path.size() > 1 && out->path.endsWith(QLatin1Char('/'))) {
        out->path.chop(1);
    }

    // ---- 请求头 ----
    qint64 contentLength = 0;
    bool chunked = false;
    for (int i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            continue;
        }
        const int colon = line.indexOf(':');
        if (colon <= 0) {
            continue;
        }
        const QByteArray key = line.left(colon).trimmed().toLower();
        const QByteArray value = line.mid(colon + 1).trimmed();

        if (key == QByteArrayLiteral("content-length")) {
            bool ok = false;
            contentLength = value.toLongLong(&ok);
            if (!ok || contentLength < 0) {
                out->complete = true;
                out->valid = false;
                out->error = QStringLiteral("Content-Length 非法");
                return false;
            }
        } else if (key == QByteArrayLiteral("transfer-encoding")
                   && value.toLower().contains(QByteArrayLiteral("chunked"))) {
            chunked = true;
        }
    }

    if (chunked) {
        out->complete = true;
        out->valid = false;
        out->error = QStringLiteral("不支持 chunked 请求体");
        return false;
    }

    const int bodyStart = headerEnd + 4;
    if (buffer.size() - bodyStart < contentLength) {
        return false; // 数据还没收全，继续等
    }

    out->body = buffer.mid(bodyStart, static_cast<int>(contentLength));
    out->complete = true;

    if (!out->body.isEmpty()) {
        QJsonParseError perr{};
        const QJsonDocument doc = QJsonDocument::fromJson(out->body, &perr);
        if (perr.error != QJsonParseError::NoError || !doc.isObject()) {
            out->valid = false;
            out->error = QStringLiteral("请求体不是合法 JSON 对象");
            return false;
        }
        out->json = doc.object();
    }

    out->valid = true;
    return true;
}

QByteArray makeResponse(int status, const QByteArray &body, const QByteArray &contentType)
{
    QByteArray resp;
    resp.reserve(body.size() + 256);
    resp += "HTTP/1.1 " + QByteArray::number(status) + ' ' + statusText(status) + "\r\n";
    resp += "Content-Type: " + contentType + "\r\n";
    resp += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Access-Control-Allow-Headers: Content-Type\r\n";
    resp += "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    resp += body;
    return resp;
}

QJsonObject result(int code, const QString &msg, const QJsonValue &data)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("code"), code);
    obj.insert(QStringLiteral("msg"), msg);
    if (!data.isUndefined()) {
        obj.insert(QStringLiteral("data"), data);
    }
    return obj;
}

QByteArray jsonResponse(const QJsonObject &body, int status)
{
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    return makeResponse(status, payload);
}

} // namespace http