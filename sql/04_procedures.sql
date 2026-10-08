-- =============================================================
-- 小徐爱听歌 · 存储过程脚本
-- 对应 PLAN.md 第 5.6 节：sp_user_monthly_report
-- 依赖：先执行 01_schema.sql
-- 说明：含 DROP PROCEDURE IF EXISTS，可重复执行
-- =============================================================

USE music_app;

DROP PROCEDURE IF EXISTS sp_user_monthly_report;

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

DELIMITER ;