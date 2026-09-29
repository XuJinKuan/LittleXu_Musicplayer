#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QPair>

// Service 层返回值：HTTP 状态码 + 统一响应体。
// binary == true 时改用 binaryBody 作为响应体（音频流等），body 被忽略。
struct ServiceResult {
    int httpStatus = 200;
    QJsonObject body;

    bool binary = false;
    QByteArray binaryBody;
    QByteArray contentType = QByteArrayLiteral("application/octet-stream");
    QList<QPair<QByteArray, QByteArray>> headers;   // 附加响应头
};