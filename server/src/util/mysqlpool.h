#pragma once

// 注意：本头文件必须位于所有其他 #include 之前。
// MySQL C API 在 Windows 上会引入 winsock2.h / windows.h，若 Qt 头文件
// 先被包含，两者的顺序冲突会导致大量符号重定义错误。

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <winsock2.h>
#  include <windows.h>
#endif

extern "C" {
#include <mysql.h>
}

#include <QByteArray>
#include <QMutex>
#include <QSemaphore>
#include <QStack>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVector>

#include "config.h"

// ---------------------------------------------------------------------------
// 查询结果集：行以字符串保存，交给上层自行做类型转换。
// ---------------------------------------------------------------------------
struct SqlResult {
    QStringList columns;
    QVector<QStringList> rows;
    QString error;

    int columnIndex(const QString &name) const
    {
        return columns.indexOf(name);
    }

    int rowCount() const { return rows.size(); }
    bool isEmpty() const { return rows.isEmpty(); }

    QString at(int row, int col) const
    {
        if (row < 0 || row >= rows.size()) {
            return QString();
        }
        const QStringList &r = rows.at(row);
        return (col < 0 || col >= r.size()) ? QString() : r.at(col);
    }

    QString at(int row, const QString &column) const
    {
        return at(row, columnIndex(column));
    }
};

// ---------------------------------------------------------------------------
// 单条 MySQL 连接（RAII 持有 MYSQL*）。
// ---------------------------------------------------------------------------
class MySqlConn
{
public:
    MySqlConn() = default;
    ~MySqlConn();

    MySqlConn(const MySqlConn &) = delete;
    MySqlConn &operator=(const MySqlConn &) = delete;
    MySqlConn(MySqlConn &&other) noexcept;
    MySqlConn &operator=(MySqlConn &&other) noexcept;

    bool open(const cfg::DbConfig &c, QString *error);
    void close();

    bool isOpen() const { return m_conn != nullptr; }

    // 执行不关心结果集的语句（INSERT / UPDATE / DELETE / DDL）
    bool exec(const QString &sql, QString *error);

    // 执行查询，只取第一个「有结果集」的返回值（兼容 CALL 存储过程的多结果集）
    bool select(const QString &sql, SqlResult *out, QString *error);

    quint64 lastInsertId() const { return m_insertId; }
    qint64 affectedRows() const { return m_affectedRows; }
    QString lastError() const;

    MYSQL *raw() const { return m_conn; }

private:
    void resetResultInfo();
    void logError(const QString &sql, QString *error);

    MYSQL *m_conn = nullptr;
    quint64 m_insertId = 0;
    qint64 m_affectedRows = 0;
    QString m_lastError;
};

// ---------------------------------------------------------------------------
// 连接池（进程内单例）。线程安全，通过 Lease 借用连接。
// ---------------------------------------------------------------------------
class MySqlPool
{
public:
    static MySqlPool &instance();

    bool init(const cfg::DbConfig &c, QString *error);
    void shutdown();

    bool isReady() const { return m_inited; }

    // RAII 借用句柄：离开作用域自动归还
    class Lease
    {
    public:
        Lease() = default;
        Lease(MySqlPool *pool, MySqlConn *conn) : m_pool(pool), m_conn(conn) {}
        ~Lease() { release(); }

        Lease(const Lease &) = delete;
        Lease &operator=(const Lease &) = delete;
        Lease(Lease &&other) noexcept;
        Lease &operator=(Lease &&other) noexcept;

        MySqlConn *operator->() const { return m_conn; }
        MySqlConn *conn() const { return m_conn; }
        bool valid() const { return m_conn != nullptr; }
        void release();

    private:
        MySqlPool *m_pool = nullptr;
        MySqlConn *m_conn = nullptr;
    };

    Lease acquire(int timeoutMs = 5000);

    // 把字符串转成可安全嵌入 SQL 的带引号字面量
    static QString toSqlLiteral(const QString &s);

    // 把 "SELECT ... WHERE id = ?" 里的 ? 依次替换成 args 中的字面量。
    // 单引号内部的 ? 会被跳过。
    static QString buildSql(const QString &tpl, const QVariantList &args);

private:
    MySqlPool() = default;
    ~MySqlPool();
    MySqlPool(const MySqlPool &) = delete;
    MySqlPool &operator=(const MySqlPool &) = delete;

    MySqlConn *take(int timeoutMs);
    void give(MySqlConn *conn);

    cfg::DbConfig m_cfg;
    QStack<MySqlConn *> m_idle;
    QSemaphore m_available;
    mutable QMutex m_mutex;
    bool m_inited = false;
};