#pragma once

// 极简 HTTP/1.1 请求解析与响应组装。
// Qt 6.5.3 官方包不含 Qt6HttpServer，因此这里只实现本项目需要的最小子集：
// 短连接（Connection: close）、JSON 请求体、URL query 参数。

#include <QByteArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QMap>
#include <QPair>
#include <QString>

struct HttpRequest {
    QString method;                  // GET / POST
    QString path;                    // 已去掉 query，如 /api/songs/5
    QMap<QString, QString> query;    // ?a=1&b=2
    QMap<QString, QString> headers;  // 请求头，键统一小写，如 range / content-length
    QByteArray body;                 // 原始请求体
    QJsonObject json;                // body 解析出的 JSON（非 JSON 时为空对象）
    bool complete = false;           // 请求是否已收全
    bool valid = false;              // 解析是否成功
    QString error;                   // valid == false 时的原因
};

namespace http {

// 从当前已接收的字节流中尝试解析一个请求。
// 返回 false 表示「数据还不够」或「解析失败」，由 out 里的字段区分。
bool parseRequest(const QByteArray &buffer, HttpRequest *out);

// 组装完整响应报文字节。extraHeaders 用于附加响应头（如 Accept-Ranges / Content-Range）
QByteArray makeResponse(int status, const QByteArray &body,
                        const QByteArray &contentType = QByteArrayLiteral("application/json; charset=utf-8"),
                        const QList<QPair<QByteArray, QByteArray>> &extraHeaders
                            = QList<QPair<QByteArray, QByteArray>>());

// 统一响应体：{ "code": 0, "msg": "ok", "data": ... }
// data 默认 Undefined，此时响应体不含 data 字段
QJsonObject result(int code, const QString &msg,
                   const QJsonValue &data = QJsonValue(QJsonValue::Undefined));

// HTTP 状态码 + 统一响应体
QByteArray jsonResponse(const QJsonObject &body, int status = 200);

// 常用状态码文本
QByteArray statusText(int status);

} // namespace http