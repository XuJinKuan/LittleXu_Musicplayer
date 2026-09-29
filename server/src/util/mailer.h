#pragma once

#include "config.h"

#include <QString>

// 极简 SMTP 客户端：只实现「认证 + 发一封纯文本（base64 正文）邮件」这一条路径。
// QQ 邮箱要求隐式 SSL（465 端口）且用授权码代替登录密码。
//
// 采用同步阻塞实现：服务端是单线程事件循环，发送期间不处理其他请求。
// 这样做的代价是发信时短暂卡顿，换来的是「发信失败能立刻如实返回给前端」，
// 避免出现前端提示已发送、实际邮件根本没出去的情况。
class Mailer
{
public:
    static void init(const cfg::MailConfig &config);
    static bool isReady();

    // 成功返回 true；失败时通过 error 回传可读原因
    static bool send(const QString &to, const QString &subject,
                     const QString &body, QString *error);

private:
    Mailer() = delete;
};