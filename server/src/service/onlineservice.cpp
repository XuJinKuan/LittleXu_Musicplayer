#include "onlineservice.h"

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include "util/http.h"

namespace {

constexpr int kRequestTimeoutMs = 8000;
constexpr char kUserAgent[] = "Mozilla/5.0 (Windows NT 10.0; Win64; x64)";

ServiceResult fail(int status, const QString &msg)
{
    ServiceResult r;
    r.httpStatus = status;
    r.body = http::result(status, msg);
    return r;
}

ServiceResult ok(const QJsonObject &data)
{
    ServiceResult r;
    r.httpStatus = 200;
    r.body = http::result(0, QStringLiteral("ok"), data);
    return r;
}

// 酷我搜索返回的是单引号包裹的伪 JSON（如 {'HIT':'1','abslist':[{'NAME':'...'}]}），
// QJsonDocument 无法解析，因此这里只取 "abslist" 之后的内容，按花括号配对切出每个歌曲对象。
QStringList splitAbslist(const QString &text)
{
    QStringList blocks;

    const int keyPos = text.indexOf(QLatin1String("abslist"));
    if (keyPos < 0)
        return blocks;

    const int arrayStart = text.indexOf(QLatin1Char('['), keyPos);
    if (arrayStart < 0)
        return blocks;

    int depth = 0;
    int blockStart = -1;

    for (int i = arrayStart + 1; i < text.size(); ++i) {
        const QChar ch = text.at(i);

        if (ch == QLatin1Char('{')) {
            if (depth == 0)
                blockStart = i;
            ++depth;
        } else if (ch == QLatin1Char('}')) {
            --depth;
            if (depth == 0 && blockStart >= 0) {
                blocks.append(text.mid(blockStart, i - blockStart + 1));
                blockStart = -1;
            }
        } else if (ch == QLatin1Char(']') && depth == 0) {
            break;
        }
    }

    return blocks;
}

// 从伪 JSON 片段中取 '字段名':'值'
QString field(const QString &block, const QString &name)
{
    const QRegularExpression re(
        QStringLiteral("'%1':'([^']*)'").arg(QRegularExpression::escape(name)));

    const QRegularExpressionMatch match = re.match(block);
    return match.hasMatch() ? match.captured(1) : QString();
}

} // namespace

QByteArray OnlineService::syncGet(const QString &url, QString *error)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setRawHeader("Accept-Encoding", "identity");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam.get(request);

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(kRequestTimeoutMs);
    loop.exec();

    QByteArray body;
    if (!reply->isFinished()) {
        if (error)
            *error = QStringLiteral("请求超时");
        reply->abort();
    } else if (reply->error() != QNetworkReply::NoError) {
        if (error)
            *error = reply->errorString();
    } else {
        body = reply->readAll();
    }

    reply->deleteLater();
    return body;
}

ServiceResult OnlineService::search(const QMap<QString, QString> &query)
{
    const QString keyword = query.value(QStringLiteral("keyword")).trimmed();
    if (keyword.isEmpty())
        return fail(400, QStringLiteral("请提供搜索关键词 keyword"));

    QUrl url(QStringLiteral("http://search.kuwo.cn/r.s"));

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("all"), keyword);
    q.addQueryItem(QStringLiteral("ft"), QStringLiteral("music"));
    q.addQueryItem(QStringLiteral("itemset"), QStringLiteral("web_2013"));
    q.addQueryItem(QStringLiteral("client"), QStringLiteral("kt"));
    q.addQueryItem(QStringLiteral("pn"), QStringLiteral("0"));
    q.addQueryItem(QStringLiteral("rn"), QStringLiteral("20"));
    q.addQueryItem(QStringLiteral("rformat"), QStringLiteral("json"));
    q.addQueryItem(QStringLiteral("encoding"), QStringLiteral("utf8"));
    url.setQuery(q);

    QString error;
    const QByteArray raw = syncGet(url.toString(), &error);
    if (raw.isEmpty()) {
        return fail(502, QStringLiteral("在线音源搜索失败：%1")
                              .arg(error.isEmpty() ? QStringLiteral("无响应") : error));
    }

    const QString text = QString::fromUtf8(raw);
    const QStringList blocks = splitAbslist(text);

    if (blocks.isEmpty())
        return fail(502, QStringLiteral("在线音源返回内容无法解析"));

    QJsonArray items;
    for (const QString &block : blocks) {
        const QString rid = field(block, QStringLiteral("MUSICRID"));
        const QString name = field(block, QStringLiteral("NAME"));
        if (rid.isEmpty() || name.isEmpty())
            continue;

        QJsonObject item;
        item.insert(QStringLiteral("rid"), rid);
        item.insert(QStringLiteral("name"), name);
        item.insert(QStringLiteral("artist"), field(block, QStringLiteral("ARTIST")));
        item.insert(QStringLiteral("album"), field(block, QStringLiteral("ALBUM")));
        item.insert(QStringLiteral("duration"), field(block, QStringLiteral("DURATION")).toInt());
        items.append(item);
    }

    const int hit = field(text, QStringLiteral("HIT")).toInt();

    QJsonObject data;
    data.insert(QStringLiteral("total"), hit > 0 ? hit : items.size());
    data.insert(QStringLiteral("items"), items);
    return ok(data);
}

ServiceResult OnlineService::url(const QMap<QString, QString> &query)
{
    const QString rid = query.value(QStringLiteral("rid")).trimmed();
    if (rid.isEmpty())
        return fail(400, QStringLiteral("请提供歌曲 rid"));

    // 只接受 MUSIC_ + 数字，避免外部输入被拼进外部 URL
    static const QRegularExpression ridPattern(QStringLiteral("^MUSIC_[0-9]{1,20}$"));
    if (!ridPattern.match(rid).hasMatch())
        return fail(400, QStringLiteral("rid 格式非法"));

    QUrl url(QStringLiteral("http://antiserver.kuwo.cn/anti.s"));

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("type"), QStringLiteral("convert_url"));
    q.addQueryItem(QStringLiteral("rid"), rid);
    q.addQueryItem(QStringLiteral("format"), QStringLiteral("mp3"));
    q.addQueryItem(QStringLiteral("response"), QStringLiteral("url"));
    url.setQuery(q);

    QString error;
    const QString body = QString::fromUtf8(syncGet(url.toString(), &error));

    // 正常返回是一行纯文本直链，但异常时可能是提示文案，逐行找 http 开头的行
    QString direct;
    const QStringList lines = body.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1String("http"))) {
            direct = trimmed;
            break;
        }
    }

    if (direct.isEmpty()) {
        const QString reason = error.isEmpty() ? body.trimmed().left(80) : error;
        return fail(502, QStringLiteral("音源未提供可播放地址：%1")
                              .arg(reason.isEmpty() ? QStringLiteral("无响应") : reason));
    }

    QJsonObject data;
    data.insert(QStringLiteral("url"), direct);
    return ok(data);
}

ServiceResult OnlineService::lrc(const QMap<QString, QString> &query)
{
    const QString rid = query.value(QStringLiteral("rid")).trimmed();
    if (rid.isEmpty())
        return fail(400, QStringLiteral("请提供歌曲 rid"));

    // 与 /api/online/url 同样校验，避免外部输入被拼进外部 URL
    static const QRegularExpression ridPattern(QStringLiteral("^MUSIC_[0-9]{1,20}$"));
    if (!ridPattern.match(rid).hasMatch())
        return fail(400, QStringLiteral("rid 格式非法"));

    // 酷我歌词接口：m.kuwo.cn 的 newh5 页面接口
    QUrl url(QStringLiteral("http://m.kuwo.cn/newh5/singles/songinfoandlrc"));

    QUrlQuery q;
    q.addQueryItem(QStringLiteral("musicId"), rid);
    q.addQueryItem(QStringLiteral("httpsStatus"), QStringLiteral("1"));
    url.setQuery(q);

    QString error;
    const QByteArray raw = syncGet(url.toString(), &error);
    if (raw.isEmpty()) {
        return fail(502, QStringLiteral("歌词获取失败：%1")
                              .arg(error.isEmpty() ? QStringLiteral("无响应") : error));
    }

    // 返回的是 JSON，内含 lrclist 数组（每项含 time 和 lineLyric）
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isNull() || !doc.isObject()) {
        return fail(502, QStringLiteral("歌词返回内容无法解析"));
    }

    const QJsonObject root = doc.object();
    const QJsonObject dataObj = root.value(QStringLiteral("data")).toObject();
    const QJsonArray lrclist = dataObj.value(QStringLiteral("lrclist")).toArray();

    // 把 lrclist 拼成标准 LRC 文本
    QStringList lines;
    for (const QJsonValue &value : lrclist) {
        const QJsonObject item = value.toObject();
        const QString time = item.value(QStringLiteral("time")).toString();
        const QString lyric = item.value(QStringLiteral("lineLyric")).toString();
        if (!lyric.isEmpty()) {
            // time 是秒数（如 "23.5"），转为 [mm:ss.xx]
            bool ok = false;
            const double seconds = time.toDouble(&ok);
            if (ok) {
                const int m = static_cast<int>(seconds) / 60;
                const int s = static_cast<int>(seconds) % 60;
                const int cs = static_cast<int>((seconds - static_cast<int>(seconds)) * 100);
                lines.append(QStringLiteral("[%1:%2.%3]%4")
                    .arg(m, 2, 10, QLatin1Char('0'))
                    .arg(s, 2, 10, QLatin1Char('0'))
                    .arg(cs, 2, 10, QLatin1Char('0'))
                    .arg(lyric));
            } else {
                lines.append(lyric);
            }
        }
    }

    QJsonObject data;
    data.insert(QStringLiteral("rid"), rid);
    data.insert(QStringLiteral("lrc"), lines.join(QStringLiteral("\n")));
    return ok(data);
}