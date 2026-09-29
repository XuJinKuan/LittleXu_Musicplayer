#include "songlistmodel.h"

namespace {

QString formatDuration(int seconds)
{
    if (seconds <= 0)
        return QStringLiteral("--:--");

    return QStringLiteral("%1:%2")
        .arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

SongListModel::SongListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int SongListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;

    return m_songs.size();
}

QVariant SongListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_songs.size())
        return QVariant();

    const SongItem &item = m_songs.at(index.row());
    switch (role) {
    case SongIdRole:        return item.songId;
    case TitleRole:         return item.title;
    case ArtistRole:        return item.artist;
    case AlbumRole:         return item.album;
    case GenreRole:         return item.genre;
    case DurationRole:      return item.duration;
    case PlayCountRole:     return item.playCount;
    case DurationTextRole:  return formatDuration(item.duration);
    default:                return QVariant();
    }
}

QHash<int, QByteArray> SongListModel::roleNames() const
{
    return {
        { SongIdRole,       "songId" },
        { TitleRole,        "title" },
        { ArtistRole,       "artist" },
        { AlbumRole,        "album" },
        { GenreRole,        "genre" },
        { DurationRole,     "duration" },
        { PlayCountRole,    "playCount" },
        { DurationTextRole, "durationText" }
    };
}

void SongListModel::setSongs(const QVector<SongItem> &songs)
{
    beginResetModel();
    m_songs = songs;
    endResetModel();
}

void SongListModel::clear()
{
    beginResetModel();
    m_songs.clear();
    endResetModel();
}