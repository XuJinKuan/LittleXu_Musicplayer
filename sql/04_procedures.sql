-- =============================================================
-- 小徐爱听歌 · 存储过程脚本
-- 对应 PLAN.md 第 5.6 节：sp_user_monthly_report / sp_similar_song
-- 依赖：先执行 01_schema.sql
-- 说明：含 DROP PROCEDURE IF EXISTS，可重复执行
-- =============================================================

USE music_app;

DROP PROCEDURE IF EXISTS sp_user_monthly_report;
DROP PROCEDURE IF EXISTS sp_similar_song;

DELIMITER $$

-- -------------------------------------------------------------
-- 1. sp_user_monthly_report 个人月度听歌报告
-- 入参：p_user_id 用户ID；p_year / p_month 统计年月
-- 出参：该月内每首歌的播放次数、累计时长、最近收听时间、是否已收藏
-- 调用：CALL sp_user_monthly_report(1, 2026, 9);
-- -------------------------------------------------------------
CREATE PROCEDURE sp_user_monthly_report(
    IN p_user_id INT,
    IN p_year    INT,
    IN p_month   INT
)
BEGIN
    SELECT
        s.song_id,
        s.title,
        s.genre,
        COUNT(pr.record_id)      AS play_times,
        SUM(pr.played_seconds)   AS total_seconds,
        MAX(pr.played_at)        AS last_played_at,
        CASE WHEN EXISTS (SELECT 1 FROM favorite f
                           WHERE f.user_id = p_user_id AND f.song_id = s.song_id)
             THEN 1 ELSE 0 END   AS is_favorite
    FROM play_record pr
    JOIN song s ON pr.song_id = s.song_id
    WHERE pr.user_id = p_user_id
      AND YEAR(pr.played_at)  = p_year
      AND MONTH(pr.played_at) = p_month
    GROUP BY s.song_id, s.title, s.genre
    ORDER BY play_times DESC, total_seconds DESC;
END$$

-- -------------------------------------------------------------
-- 2. sp_similar_song 相似歌曲推荐
-- 入参：p_song_id 目标歌曲；p_k 返回条数（<=0 或 NULL 时默认 10）
-- 相似度规则（加权求和）：
--   +3 * 共同标签数
--   +2 * 共同歌手数
--   +2   同曲风
--   +1   同专辑
-- 出参：按相似度降序，同分再按 play_count 降序
-- 调用：CALL sp_similar_song(1, 5);
-- -------------------------------------------------------------
CREATE PROCEDURE sp_similar_song(
    IN p_song_id INT,
    IN p_k       INT
)
BEGIN
    DECLARE v_genre VARCHAR(10);
    DECLARE v_album INT;

    IF p_k IS NULL OR p_k <= 0 THEN
        SET p_k = 10;
    END IF;

    SELECT s.genre, s.album_id INTO v_genre, v_album
      FROM song s WHERE s.song_id = p_song_id;

    SELECT
        t.song_id,
        t.title,
        t.genre,
        (SELECT COUNT(*)
           FROM song_tag st1
           JOIN song_tag st2 ON st1.tag_id = st2.tag_id
          WHERE st1.song_id = p_song_id AND st2.song_id = t.song_id) * 3
      + (SELECT COUNT(*)
           FROM song_artist a1
           JOIN song_artist a2 ON a1.artist_id = a2.artist_id
          WHERE a1.song_id = p_song_id AND a2.song_id = t.song_id) * 2
      + CASE WHEN t.genre = v_genre THEN 2 ELSE 0 END
      + CASE WHEN v_album IS NOT NULL AND t.album_id = v_album THEN 1 ELSE 0 END
        AS similarity
    FROM song t
    WHERE t.song_id <> p_song_id
    HAVING similarity > 0
    ORDER BY similarity DESC, t.play_count DESC
    LIMIT p_k;
END$$

DELIMITER ;