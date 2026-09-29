#pragma once

#include <QJsonObject>

// Service 层返回值：HTTP 状态码 + 统一响应体
struct ServiceResult {
    int httpStatus = 200;
    QJsonObject body;
};