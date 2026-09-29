#include "mysqlpool.h"

#include <QDebug>
#include <QThread>
#include <utility>

namespace {

void setErr(QString *error, const QString &msg)
{
    if (error) {
        *error = msg;
    }
}

// 丢弃连接上残留的所有结果集，避免下一条语句报 "Commands out of sync"
void drainResults(MYSQL *conn)
{
    do {
        MYSQL_RES *res = mysql_store_result(conn);
        if (res) {
            mysql_free_result(res);
        }
    } while (mysql_next_result(conn) == 0);
}

} // namespace

// ===========================================================================
// MySqlConn
// ===========================================================================

MySqlConn::~MySqlConn()
{
    close();
}

MySqlConn::MySqlConn(MySqlConn &&other) noexcept
    : m_conn(other.m_conn),
      m_insertId(other.m_insertId),
      m_affectedRows(other.m_affectedRows),
      m_lastError(std::move(other.m_lastError))
{
    other.m_conn = nullptr;
    other.m_insertId = 0;
    other.m_affectedRows = 0;
}

MySqlConn &MySqlConn::operator=(MySqlConn &&other) noexcept
{
    if (this != &other) {
        close();
        m_conn = other.m_conn;
        m_insertId = other.m_insertId;
        m_affectedRows = other.m_affectedRows;
        m_lastError = std::move(other.m_lastError);
        other.m_conn = nullptr;
        other.m_insertId = 0;
        other.m_affectedRows = 0;
    }
    return *this;
}

bool MySqlConn::open(const cfg::DbConfig &c, QString *error)
{
    close();
    resetResultInfo();

    m_conn = mysql_init(nullptr);
    if (!m_conn) {
        setErr(error, QStringLiteral("mysql_init 失败：内存不足"));
        return false;
    }

    unsigned int timeout = static_cast<unsigned int>(c.connectTimeoutSec);
    mysql_options(m_conn, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);

    // 先按 UTF-8 建连接，再用 SET NAMES 兜底，避免服务端默认字符集不匹配
    const QByteArray charset = c.charset.toUtf8();
    mysql_options(m_conn, MYSQL_SET_CHARSET_NAME, charset.constData());

    const QByteArray host = c.host.toUtf8();
    const QByteArray user = c.user.toUtf8();
    const QByteArray pass = c.password.toUtf8();
    const QByteArray db = c.database.toUtf8();

    MYSQL *ok = mysql_real_connect(m_conn,
                                   host.constData(),
                                   user.constData(),
                                   pass.constData(),
                                   db.constData(),
                                   static_cast<unsigned int>(c.port),
                                   nullptr,
                                   CLIENT_MULTI_RESULTS);
    if (!ok) {
        const QString msg = QStringLiteral("连接 MySQL 失败（%1:%2/%3）：%4")
                                .arg(c.host)
                                .arg(c.port)
                                .arg(c.database)
                                .arg(QString::fromUtf8(mysql_error(m_conn)));
        mysql_close(m_conn);
        m_conn = nullptr;
        setErr(error, msg);
        return false;
    }

    return true;
}

void MySqlConn::close()
{
    if (m_conn) {
        mysql_close(m_conn);
        m_conn = nullptr;
    }
    resetResultInfo();
}

void MySqlConn::resetResultInfo()
{
    m_insertId = 0;
    m_affectedRows = 0;
    m_lastError.clear();
}

QString MySqlConn::lastError() const
{
    if (m_conn) {
        return QString::fromUtf8(mysql_error(m_conn));
    }
    return m_lastError;
}

void MySqlConn::logError(const QString &sql, QString *error)
{
    m_lastError = lastError();
    const QString msg = QStringLiteral("SQL 执行失败：%1\nSQL: %2").arg(m_lastError, sql);
    qWarning().noquote() << msg;
    setErr(error, m_lastError);
}

bool MySqlConn::exec(const QString &sql, QString *error)
{
    if (!m_conn) {
        setErr(error, QStringLiteral("连接未打开"));
        return false;
    }

    const QByteArray q = sql.toUtf8();
    if (mysql_real_query(m_conn, q.constData(), static_cast<unsigned long>(q.size())) != 0) {
        logError(sql, error);
        return false;
    }

    m_insertId = mysql_insert_id(m_conn);
    const my_ulonglong affected = mysql_affected_rows(m_conn);
    m_affectedRows = (affected == static_cast<my_ulonglong>(-1))
                         ? 0
                         : static_cast<qint64>(affected);

    drainResults(m_conn);
    return true;
}

bool MySqlConn::select(const QString &sql, SqlResult *out, QString *error)
{
    if (!out) {
        setErr(error, QStringLiteral("SqlResult 指针为空"));
        return false;
    }
    out->columns.clear();
    out->rows.clear();
    out->error.clear();

    if (!m_conn) {
        setErr(error, QStringLiteral("连接未打开"));
        return false;
    }

    const QByteArray q = sql.toUtf8();
    if (mysql_real_query(m_conn, q.constData(), static_cast<unsigned long>(q.size())) != 0) {
        logError(sql, error);
        return false;
    }

    bool gotRows = false;
    do {
        MYSQL_RES *res = mysql_store_result(m_conn);
        if (res) {
            if (!gotRows) {
                const unsigned int n = mysql_num_fields(res);
                MYSQL_FIELD *fields = mysql_fetch_fields(res);
                for (unsigned int i = 0; i < n; ++i) {
                    out->columns << QString::fromUtf8(fields[i].name);
                }

                MYSQL_ROW row = nullptr;
                while ((row = mysql_fetch_row(res)) != nullptr) {
                    unsigned long *lengths = mysql_fetch_lengths(res);
                    QStringList r;
                    r.reserve(static_cast<int>(n));
                    for (unsigned int i = 0; i < n; ++i) {
                        if (row[i] == nullptr) {
                            r << QString();
                        } else {
                            r << QString::fromUtf8(row[i], static_cast<int>(lengths[i]));
                        }
                    }
                    out->rows << r;
                }
                gotRows = true;
            }
            mysql_free_result(res);
        } else if (mysql_field_count(m_conn) != 0) {
            // 有字段却没有结果集，说明取结果时出错
            logError(sql, error);
            return false;
        }
    } while (mysql_next_result(m_conn) == 0);

    return true;
}

// ===========================================================================
// MySqlPool
// ===========================================================================

MySqlPool &MySqlPool::instance()
{
    static MySqlPool pool;
    return pool;
}

MySqlPool::~MySqlPool()
{
    shutdown();
}

bool MySqlPool::init(const cfg::DbConfig &c, QString *error)
{
    QMutexLocker locker(&m_mutex);
    if (m_inited) {
        return true;
    }

    m_cfg = c;
    mysql_library_init(0, nullptr, nullptr);

    const int count = qMax(1, c.poolSize);
    for (int i = 0; i < count; ++i) {
        MySqlConn *conn = new MySqlConn();
        QString err;
        if (!conn->open(c, &err)) {
            delete conn;
            for (MySqlConn *idle : m_idle) {
                delete idle;
            }
            m_idle.clear();
            setErr(error, err);
            return false;
        }
        m_idle.push(conn);
    }

    m_available.release(count);
    m_inited = true;
    qInfo().noquote() << QStringLiteral("MySQL 连接池就绪：%1 条连接 → %2:%3/%4")
                             .arg(count)
                             .arg(c.host)
                             .arg(c.port)
                             .arg(c.database);
    return true;
}

void MySqlPool::shutdown()
{
    QMutexLocker locker(&m_mutex);
    if (!m_inited) {
        return;
    }
    for (MySqlConn *conn : m_idle) {
        delete conn;
    }
    m_idle.clear();
    m_inited = false;
}

MySqlConn *MySqlPool::take(int timeoutMs)
{
    if (!m_inited) {
        return nullptr;
    }
    if (!m_available.tryAcquire(1, timeoutMs)) {
        qWarning() << "获取数据库连接超时";
        return nullptr;
    }

    QMutexLocker locker(&m_mutex);
    if (m_idle.isEmpty()) {
        m_available.release(1);
        return nullptr;
    }
    return m_idle.pop();
}

void MySqlPool::give(MySqlConn *conn)
{
    if (!conn) {
        return;
    }
    QMutexLocker locker(&m_mutex);
    if (!m_inited) {
        delete conn;
        return;
    }
    m_idle.push(conn);
    m_available.release(1);
}

MySqlPool::Lease MySqlPool::acquire(int timeoutMs)
{
    return Lease(this, take(timeoutMs));
}

MySqlPool::Lease::Lease(Lease &&other) noexcept
    : m_pool(other.m_pool), m_conn(other.m_conn)
{
    other.m_pool = nullptr;
    other.m_conn = nullptr;
}

MySqlPool::Lease &MySqlPool::Lease::operator=(Lease &&other) noexcept
{
    if (this != &other) {
        release();
        m_pool = other.m_pool;
        m_conn = other.m_conn;
        other.m_pool = nullptr;
        other.m_conn = nullptr;
    }
    return *this;
}

void MySqlPool::Lease::release()
{
    if (m_pool && m_conn) {
        m_pool->give(m_conn);
    }
    m_pool = nullptr;
    m_conn = nullptr;
}

QString MySqlPool::toSqlLiteral(const QString &s)
{
    QString out;
    out.reserve(s.size() + 2);
    out += QLatin1Char('\'');
    for (const QChar ch : s) {
        switch (ch.unicode()) {
        case u'\\':  out += QLatin1String("\\\\"); break;
        case u'\'':  out += QLatin1String("\\'");  break;
        case u'"':   out += QLatin1String("\\\""); break;
        case u'\0':  out += QLatin1String("\\0");  break;
        case u'\n':  out += QLatin1String("\\n");  break;
        case u'\r':  out += QLatin1String("\\r");  break;
        case 0x1a:   out += QLatin1String("\\Z");  break;
        default:     out += ch;                    break;
        }
    }
    out += QLatin1Char('\'');
    return out;
}

QString MySqlPool::buildSql(const QString &tpl, const QVariantList &args)
{
    QString out;
    out.reserve(tpl.size() + args.size() * 8);

    int argIndex = 0;
    bool inQuote = false;

    for (int i = 0; i < tpl.size(); ++i) {
        const QChar ch = tpl.at(i);

        if (inQuote) {
            out += ch;
            if (ch == QLatin1Char('\'')) {
                inQuote = false;
            }
            continue;
        }

        if (ch == QLatin1Char('\'')) {
            inQuote = true;
            out += ch;
            continue;
        }

        if (ch == QLatin1Char('?') && argIndex < args.size()) {
            const QVariant &v = args.at(argIndex++);
            if (v.isNull()) {
                out += QLatin1String("NULL");
            } else {
                switch (v.typeId()) {
                case QMetaType::Int:
                case QMetaType::UInt:
                case QMetaType::LongLong:
                case QMetaType::ULongLong:
                case QMetaType::Double:
                case QMetaType::Float:
                    out += v.toString();
                    break;
                case QMetaType::Bool:
                    out += v.toBool() ? QLatin1String("1") : QLatin1String("0");
                    break;
                default:
                    out += toSqlLiteral(v.toString());
                    break;
                }
            }
            continue;
        }

        out += ch;
    }

    return out;
}