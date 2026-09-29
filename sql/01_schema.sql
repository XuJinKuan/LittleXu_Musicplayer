-- =============================================================
-- 小徐爱听歌 · 数据库建表脚本
-- 对应 PLAN.md 第 5.2 节（11 张表）、5.5 节（三类完整性）、5.8 节（索引）
-- 环境：MySQL 8.0+ / InnoDB / utf8mb4
-- 注意：本脚本含 DROP TABLE IF EXISTS，重复执行会清空 music_app 库内数据
-- =============================================================

CREATE DATABASE IF NOT EXISTS music_app
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE music_app;

-- 按依赖倒序清理，便于反复执行
SET FOREIGN_KEY_CHECKS = 0;
DROP TABLE IF EXISTS song_tag;
DROP TABLE IF EXISTS tag;
DROP TABLE IF EXISTS favorite;
DROP TABLE IF EXISTS play_record;
DROP TABLE IF EXISTS playlist_song;
DROP TABLE IF EXISTS playlist;
DROP TABLE IF EXISTS song_artist;
DROP TABLE IF EXISTS song;
DROP TABLE IF EXISTS album;
DROP TABLE IF EXISTS artist;
DROP TABLE IF EXISTS `user`;
SET FOREIGN_KEY_CHECKS = 1;

-- -------------------------------------------------------------
-- 1. user 用户表
-- -------------------------------------------------------------
CREATE TABLE `user` (
    user_id       INT          NOT NULL AUTO_INCREMENT COMMENT '用户ID',
    username      VARCHAR(50)  NOT NULL                COMMENT '登录名',
    password_hash VARCHAR(64)  NOT NULL                COMMENT '密码哈希(SHA-256 十六进制)',
    salt          VARCHAR(32)  NOT NULL                COMMENT '盐值',
    nickname      VARCHAR(50)  NOT NULL                COMMENT '昵称',
    avatar_path   VARCHAR(255) NULL                    COMMENT '头像文件路径',
    create_time   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '注册时间',
    PRIMARY KEY (user_id),
    UNIQUE KEY uk_user_username (username)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='用户表';

-- -------------------------------------------------------------
-- 2. artist 歌手表
-- -------------------------------------------------------------
CREATE TABLE artist (
    artist_id INT          NOT NULL AUTO_INCREMENT COMMENT '歌手ID',
    name      VARCHAR(100) NOT NULL                COMMENT '歌手名',
    gender    ENUM('男','女','组合','未知') NOT NULL DEFAULT '未知' COMMENT '性别/组合',
    region    VARCHAR(50)  NULL                    COMMENT '地区',
    intro     TEXT         NULL                    COMMENT '简介',
    PRIMARY KEY (artist_id),
    UNIQUE KEY uk_artist_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌手表';

-- -------------------------------------------------------------
-- 3. album 专辑表
-- -------------------------------------------------------------
CREATE TABLE album (
    album_id     INT          NOT NULL AUTO_INCREMENT COMMENT '专辑ID',
    name         VARCHAR(150) NOT NULL                COMMENT '专辑名',
    release_date DATE         NULL                    COMMENT '发行日期',
    cover_path   VARCHAR(255) NULL                    COMMENT '封面文件路径',
    artist_id    INT          NULL                    COMMENT '演唱歌手',
    PRIMARY KEY (album_id),
    KEY idx_album_artist (artist_id),
    CONSTRAINT fk_album_artist FOREIGN KEY (artist_id) REFERENCES artist (artist_id)
        ON DELETE SET NULL ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='专辑表';

-- -------------------------------------------------------------
-- 4. song 歌曲表
-- -------------------------------------------------------------
CREATE TABLE song (
    song_id    INT          NOT NULL AUTO_INCREMENT COMMENT '歌曲ID',
    title      VARCHAR(150) NOT NULL                COMMENT '标题',
    duration   INT          NOT NULL                COMMENT '时长(秒)',
    file_path  VARCHAR(255) NULL                    COMMENT '音频文件路径（在线音源为 NULL）',
    bitrate    INT          NULL                    COMMENT '码率(kbps)',
    year       SMALLINT     NULL                    COMMENT '年份',
    genre      ENUM('流行','摇滚','民谣','电子','嘻哈','古典','爵士','其他')
                            NOT NULL DEFAULT '其他'  COMMENT '曲风',
    album_id   INT          NULL                    COMMENT '所属专辑',
    play_count INT          NOT NULL DEFAULT 0      COMMENT '累计播放次数(由触发器维护)',
    PRIMARY KEY (song_id),
    KEY idx_song_album (album_id),
    KEY idx_song_genre (genre),
    CONSTRAINT fk_song_album FOREIGN KEY (album_id) REFERENCES album (album_id)
        ON DELETE SET NULL ON UPDATE CASCADE,
    CONSTRAINT ck_song_duration CHECK (duration > 0),
    CONSTRAINT ck_song_play_count CHECK (play_count >= 0)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌曲表';

-- -------------------------------------------------------------
-- 5. song_artist 歌曲-歌手关联（支持合唱）
-- -------------------------------------------------------------
CREATE TABLE song_artist (
    song_id   INT NOT NULL COMMENT '歌曲ID',
    artist_id INT NOT NULL COMMENT '歌手ID',
    role      ENUM('主唱','feat') NOT NULL DEFAULT '主唱' COMMENT '角色',
    PRIMARY KEY (song_id, artist_id),
    KEY idx_sa_artist (artist_id),
    CONSTRAINT fk_sa_song FOREIGN KEY (song_id) REFERENCES song (song_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_sa_artist FOREIGN KEY (artist_id) REFERENCES artist (artist_id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌曲-歌手关联表';

-- -------------------------------------------------------------
-- 6. playlist 歌单表
-- -------------------------------------------------------------
CREATE TABLE playlist (
    playlist_id INT          NOT NULL AUTO_INCREMENT COMMENT '歌单ID',
    user_id     INT          NOT NULL                COMMENT '创建者',
    name        VARCHAR(100) NOT NULL                COMMENT '歌单名',
    description VARCHAR(255) NULL                    COMMENT '描述',
    is_public   TINYINT(1)   NOT NULL DEFAULT 0      COMMENT '是否公开',
    create_time DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    PRIMARY KEY (playlist_id),
    KEY idx_playlist_user (user_id),
    CONSTRAINT fk_playlist_user FOREIGN KEY (user_id) REFERENCES `user` (user_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT ck_playlist_public CHECK (is_public IN (0, 1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌单表';

-- -------------------------------------------------------------
-- 7. playlist_song 歌单-歌曲关联
-- -------------------------------------------------------------
CREATE TABLE playlist_song (
    playlist_id INT      NOT NULL COMMENT '歌单ID',
    song_id     INT      NOT NULL COMMENT '歌曲ID',
    sort_no     INT      NOT NULL DEFAULT 0 COMMENT '排序号(由触发器分配)',
    added_at    DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '添加时间',
    PRIMARY KEY (playlist_id, song_id),
    KEY idx_ps_song (song_id),
    KEY idx_ps_sort (playlist_id, sort_no),
    CONSTRAINT fk_ps_playlist FOREIGN KEY (playlist_id) REFERENCES playlist (playlist_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_ps_song FOREIGN KEY (song_id) REFERENCES song (song_id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌单-歌曲关联表';

-- -------------------------------------------------------------
-- 8. play_record 播放记录表
-- -------------------------------------------------------------
CREATE TABLE play_record (
    record_id      BIGINT     NOT NULL AUTO_INCREMENT COMMENT '记录ID',
    user_id        INT        NOT NULL                COMMENT '用户ID',
    song_id        INT        NULL                    COMMENT '歌曲ID（在线歌曲为 NULL）',
    online_title   VARCHAR(150) NULL                  COMMENT '在线歌曲标题',
    online_artist  VARCHAR(150) NULL                  COMMENT '在线歌曲歌手',
    online_rid     VARCHAR(50)  NULL                  COMMENT '在线歌曲标识',
    played_at      DATETIME   NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '播放时间',
    played_seconds INT        NOT NULL DEFAULT 0      COMMENT '播放时长(秒)',
    is_completed   TINYINT(1) NOT NULL DEFAULT 0      COMMENT '是否完整播放',
    PRIMARY KEY (record_id),
    KEY idx_pr_user_time (user_id, played_at),
    KEY idx_pr_song (song_id),
    CONSTRAINT fk_pr_user FOREIGN KEY (user_id) REFERENCES `user` (user_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_pr_song FOREIGN KEY (song_id) REFERENCES song (song_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT ck_pr_seconds CHECK (played_seconds >= 0),
    CONSTRAINT ck_pr_completed CHECK (is_completed IN (0, 1))
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='播放记录表';

-- -------------------------------------------------------------
-- 9. favorite 收藏表
-- -------------------------------------------------------------
CREATE TABLE favorite (
    user_id    INT      NOT NULL COMMENT '用户ID',
    song_id    INT      NOT NULL COMMENT '歌曲ID',
    rating     TINYINT  NOT NULL DEFAULT 5 COMMENT '喜爱度评分 1-5',
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '收藏时间',
    PRIMARY KEY (user_id, song_id),
    KEY idx_fav_song (song_id),
    CONSTRAINT fk_fav_user FOREIGN KEY (user_id) REFERENCES `user` (user_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_fav_song FOREIGN KEY (song_id) REFERENCES song (song_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT ck_fav_rating CHECK (rating BETWEEN 1 AND 5)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='收藏表';

-- -------------------------------------------------------------
-- 10. tag 标签表
-- -------------------------------------------------------------
CREATE TABLE tag (
    tag_id INT         NOT NULL AUTO_INCREMENT COMMENT '标签ID',
    name   VARCHAR(50) NOT NULL                COMMENT '标签名',
    PRIMARY KEY (tag_id),
    UNIQUE KEY uk_tag_name (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='标签表';

-- -------------------------------------------------------------
-- 11. song_tag 歌曲-标签关联
-- -------------------------------------------------------------
CREATE TABLE song_tag (
    song_id INT NOT NULL COMMENT '歌曲ID',
    tag_id  INT NOT NULL COMMENT '标签ID',
    PRIMARY KEY (song_id, tag_id),
    KEY idx_st_tag (tag_id),
    CONSTRAINT fk_st_song FOREIGN KEY (song_id) REFERENCES song (song_id)
        ON DELETE CASCADE ON UPDATE CASCADE,
    CONSTRAINT fk_st_tag FOREIGN KEY (tag_id) REFERENCES tag (tag_id)
        ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='歌曲-标签关联表';