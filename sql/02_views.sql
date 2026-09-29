-- =============================================================
-- 小徐爱听歌 · 视图脚本
-- 对应 PLAN.md 第 5.3 节：v_song_detail / v_user_play_summary / v_hot_song
-- 依赖：先执行 01_schema.sql
-- 说明：CREATE OR REPLACE，可重复执行，不影响基础表数据
-- =============================================================

USE music_app;

-- -------------------------------------------------------------
-- 1. v_song_detail 歌曲明细视图
-- 用途：列表/播放页一次取齐 歌曲 + 专辑 + 歌手(含角色)，避免客户端手写多表 JOIN
-- 注意：一首歌有多位歌手时会产出多行（与 song_artist 行数一致）
-- -------------------------------------------------------------
CREATE OR REPLACE VIEW v_song_detail AS
SELECT
    s.song_id       AS song_id,
    s.title         AS title,
    s.duration      AS duration,
    s.bitrate       AS bitrate,
    s.year          AS year,
    s.genre         AS genre,
    s.play_count    AS play_count,
    s.file_path     AS file_path,
    s.album_id      AS album_id,
    al.name         AS album_name,
    al.release_date AS album_release_date,
    ar.artist_id    AS artist_id,
    ar.name         AS artist_name,
    sa.role         AS artist_role
FROM song s
LEFT JOIN album al       ON s.album_id = al.album_id
LEFT JOIN song_artist sa ON s.song_id = sa.song_id
LEFT JOIN artist ar      ON sa.artist_id = ar.artist_id;

-- -------------------------------------------------------------
-- 2. v_user_play_summary 用户听歌汇总视图
-- 用途：个人主页总体指标（播放次数、总时长、去重歌曲数、完整播放次数、最近收听时间）
-- -------------------------------------------------------------
CREATE OR REPLACE VIEW v_user_play_summary AS
SELECT
    u.user_id                          AS user_id,
    u.username                         AS username,
    u.nickname                         AS nickname,
    COUNT(pr.record_id)                AS play_times,
    COALESCE(SUM(pr.played_seconds), 0) AS total_seconds,
    COUNT(DISTINCT pr.song_id)         AS distinct_songs,
    COALESCE(SUM(pr.is_completed), 0)  AS completed_times,
    MAX(pr.played_at)                  AS last_played_at
FROM `user` u
LEFT JOIN play_record pr ON u.user_id = pr.user_id
GROUP BY u.user_id, u.username, u.nickname;

-- -------------------------------------------------------------
-- 3. v_hot_song 热度榜视图
-- 用途：全站热度榜；play_count 由触发器 trg_play_insert 维护，
--       listener_count 为去重听众数，artist_names 为 '主唱/feat' 拼接串
-- -------------------------------------------------------------
CREATE OR REPLACE VIEW v_hot_song AS
SELECT
    s.song_id    AS song_id,
    s.title      AS title,
    s.genre      AS genre,
    s.play_count AS play_count,
    al.name      AS album_name,
    (SELECT GROUP_CONCAT(ar.name ORDER BY sa.role SEPARATOR '/')
       FROM song_artist sa
       JOIN artist ar ON sa.artist_id = ar.artist_id
      WHERE sa.song_id = s.song_id) AS artist_names,
    COUNT(DISTINCT pr.user_id)      AS listener_count
FROM song s
LEFT JOIN album al       ON s.album_id = al.album_id
LEFT JOIN play_record pr ON s.song_id = pr.song_id
GROUP BY s.song_id, s.title, s.genre, s.play_count, al.name;