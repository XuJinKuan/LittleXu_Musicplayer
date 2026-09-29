#include "mailer.h"

#include <QByteArray>
#include <QDateTime>
#include <QSslSocket>
#include <QUuid>

namespace {

constexpr int kReadTimeoutMs = 20000; // 等待服务器响应的上限
constexpr int kBase64LineWidth = 76;  // 邮件正文每行 base64 长度，RFC 2045 要求 <= 76

cfg::MailConfig g_config;

struct SmtpReply {
    int code = 0;
    QString text;
};

// 读取一条（可能多行的）SMTP 回复。多行回复形如：
//   250-smtp.qq.com
//   250-AUTH LOGIN
//   250 OK
// 第 4 个字符为 '-' 表示后面还有行，为 ' '（或该行只有状态码）表示结束。
bool readReply(QSslSocket *sock, SmtpReply *reply, QString *error)
{
    reply->code = 0;
    reply->text.clear();

    for (;;) {
        if (!sock->canReadLine() && !sock->waitForReadyRead(kReadTimeoutMs)) {
            *error = QStringLiteral("等待 SMTP 服务器响应超时（%1 ms）").arg(kReadTimeoutMs);
            return false;
        }

        while (sock->canReadLine()) {
            const QString line = QString::fromUtf8(sock->readLine()).trimmed();

            if (reply->code == 0 && line.size() >= 3) {
                bool ok = false;
                const int code = line.left(3).toInt(&ok);
                if (ok) {
                    reply->code = code;
                }
            }

            if (!reply->text.isEmpty()) {
                reply->text += QLatin1Char('\n');
            }
            reply->text += line;

            const QChar separator = line.size() > 3 ? line.at(3) : QLatin1Char(' ');
            if (separator != QLatin1Char('-')) {
                return true;
            }
        }
    }
}

bool command(QSslSocket *sock, const QByteArray &cmd, SmtpReply *reply, QString *error)
{
    sock->write(cmd);
    if (sock->bytesToWrite() > 0 && !sock->waitForBytesWritten(kReadTimeoutMs)) {
        *error = QStringLiteral("发送 SMTP 命令超时");
        return false;
    }
    return readReply(sock, reply, error);
}

bool expect(const SmtpReply &reply, int expected, const QString &step, QString *error)
{
    if (reply.code == expected) {
        return true;
    }
    *error = QStringLiteral("%1 被拒绝：%2 %3").arg(step).arg(reply.code).arg(reply.text);
    return false;
}

// 发一条命令并校验状态码
bool step(QSslSocket *sock, const QByteArray &cmd, int expected,
          const QString &name, QString *error)
{
    SmtpReply reply;
    if (!command(sock, cmd, &reply, error)) {
        return false;
    }
    return expect(reply, expected, name, error);
}

// base64 正文需要按行折行，否则部分邮件网关会截断超长行
QByteArray wrapBase64(const QByteArray &raw)
{
    const QByteArray encoded = raw.toBase64();
    QByteArray out;
    out.reserve(encoded.size() + encoded.size() / kBase64LineWidth * 2 + 2);

    for (int i = 0; i < encoded.size(); i += kBase64LineWidth) {
        out += encoded.mid(i, kBase64LineWidth);
        out += QByteArrayLiteral("\r\n");
    }
    return out;
}

// 主题/发件人昵称含中文时不能直接写入头部，需按 RFC 2047 编码
QString encodeHeader(const QString &text)
{
    return QStringLiteral("=?UTF-8?B?%1?=")
        .arg(QString::fromLatin1(text.toUtf8().toBase64()));
}

QByteArray buildMessage(const QString &to, const QString &subject, const QString &body)
{
    QByteArray msg;
    msg += "From: " + encodeHeader(g_config.fromName).toUtf8()
        + " <" + g_config.from.toUtf8() + ">\r\n";
    msg += "To: <" + to.toUtf8() + ">\r\n";
    msg += "Subject: " + encodeHeader(subject).toUtf8() + "\r\n";
    msg += "Date: " + QDateTime::currentDateTime().toString(Qt::RFC2822Date).toUtf8() + "\r\n";
    msg += "Message-ID: <" + QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8()
        + "@qq.com>\r\n";
    msg += "MIME-Version: 1.0\r\n";
    msg += "Content-Type: text/plain; charset=UTF-8\r\n";
    msg += "Content-Transfer-Encoding: base64\r\n";
    msg += "\r\n";
    msg += wrapBase64(body.toUtf8());
    return msg;
}

} // namespace

void Mailer::init(const cfg::MailConfig &config)
{
    g_config = config;
}

bool Mailer::isReady()
{
    return g_config.configured();
}

bool Mailer::send(const QString &to, const QString &subject,
                  const QString &body, QString *error)
{
    QString localError;
    QString *err = error ? error : &localError;
    err->clear();

    if (!g_config.configured()) {
        *err = QStringLiteral(
            "未配置 SMTP 授权码：请设置环境变量 MUSIC_SMTP_AUTH_CODE，"
            "或在可执行文件同目录创建 mail.local.ini");
        return false;
    }
    if (to.isEmpty()) {
        *err = QStringLiteral("收件人为空");
        return false;
    }

    QSslSocket sock;
    sock.connectToHostEncrypted(g_config.host, g_config.port);
    if (!sock.waitForEncrypted(kReadTimeoutMs)) {
        *err = QStringLiteral("无法连接 %1:%2（%3）")
                   .arg(g_config.host)
                   .arg(g_config.port)
                   .arg(sock.errorString());
        return false;
    }

    // 连接成功后的问候语
    SmtpReply greeting;
    if (!readReply(&sock, &greeting, err)) {
        sock.close();
        return false;
    }
    if (!expect(greeting, 220, QStringLiteral("连接问候"), err)) {
        sock.close();
        return false;
    }

    bool ok = true;
    ok = ok && step(&sock, QByteArrayLiteral("EHLO localhost\r\n"), 250,
                    QStringLiteral("EHLO"), err);
    ok = ok && step(&sock, QByteArrayLiteral("AUTH LOGIN\r\n"), 334,
                    QStringLiteral("AUTH LOGIN"), err);
    ok = ok && step(&sock, g_config.user.toUtf8().toBase64() + QByteArrayLiteral("\r\n"), 334,
                    QStringLiteral("用户名认证"), err);
    ok = ok && step(&sock, g_config.authCode.toUtf8().toBase64() + QByteArrayLiteral("\r\n"), 235,
                    QStringLiteral("授权码认证（请确认用的是授权码而非登录密码）"), err);
    ok = ok && step(&sock, "MAIL FROM:<" + g_config.from.toUtf8() + ">\r\n", 250,
                    QStringLiteral("MAIL FROM"), err);
    ok = ok && step(&sock, "RCPT TO:<" + to.toUtf8() + ">\r\n", 250,
                    QStringLiteral("RCPT TO"), err);
    ok = ok && step(&sock, QByteArrayLiteral("DATA\r\n"), 354,
                    QStringLiteral("DATA"), err);

    if (ok) {
        QByteArray payload = buildMessage(to, subject, body);
        payload += QByteArrayLiteral(".\r\n"); // 单独一行 . 表示正文结束
        ok = step(&sock, payload, 250, QStringLiteral("邮件正文"), err);
    }

    if (ok) {
        // QUIT 的回复不重要，失败也不影响「邮件已投递」这一结论
        SmtpReply bye;
        command(&sock, QByteArrayLiteral("QUIT\r\n"), &bye, err);
        err->clear();
    }

    sock.close();
    return ok;
}