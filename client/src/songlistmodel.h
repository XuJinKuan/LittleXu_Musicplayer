#ifndef SONGLISTMODEL_H
#define SONGLISTMODEL_H

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

struct SongItem
{
    int songId = 0;
    QString title;
    QString artist;
    QString album;
    QString genre;
    int duration = 0;   // 秒
    int playCount = 0;
};

// 列表接口 /api/songs 的内存模型，一次性整体替换。
class SongListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        SongIdRole = Qt::UserRole + 1,
        TitleRole,
        ArtistRole,
        AlbumRole,
        GenreRole,
        DurationRole,
        PlayCountRole,
        DurationTextRole
    };

    explicit SongListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setSongs(const QVector<SongItem> &songs);
    void clear();

private:
    QVector<SongItem> m_songs;
};

#endif // SONGLISTMODEL_H