-- =============================================================
-- 小徐爱听歌 · 触发器脚本
-- 对应 PLAN.md 第 5.4 节：trg_play_insert / trg_playlist_song_bi
-- 依赖：先执行 01_schema.sql
-- 说明：含 DROP TRIGGER IF EXISTS，可重复执行
-- =============================================================

USE music_app;

DROP TRIGGER IF EXISTS trg_play_insert;
DROP TRIGGER IF EXISTS trg_playlist_song_bi;

DELIMITER $$

-- -------------------------------------------------------------
-- 1. trg_play_insert
-- 时机：插入播放记录后
-- 作用：维护 song.play_count 累计播放计数（避免每次榜单实时 COUNT）
-- 说明：更新的是 song 表，与触发语句写入的 play_record 表不同，无自表读写冲突
-- -------------------------------------------------------------
CREATE TRIGGER trg_play_insert
AFTER INSERT ON play_record
FOR EACH ROW
BEGIN
    UPDATE song
       SET play_count = play_count + 1
     WHERE song_id = NEW.song_id;
END$$

-- -------------------------------------------------------------
-- 2. trg_playlist_song_bi
-- 时机：向歌单添加歌曲之前
-- 作用：未显式给出 sort_no（NULL 或 <=0）时，自动分配为当前歌单最大序号 +1
-- 说明：BEFORE 触发器允许读取本表（playlist_song）以取 MAX(sort_no)
-- -------------------------------------------------------------
CREATE TRIGGER trg_playlist_song_bi
BEFORE INSERT ON playlist_song
FOR EACH ROW
BEGIN
    IF NEW.sort_no IS NULL OR NEW.sort_no <= 0 THEN
        SET NEW.sort_no = COALESCE(
            (SELECT MAX(sort_no) FROM playlist_song WHERE playlist_id = NEW.playlist_id), 0
        ) + 1;
    END IF;
END$$

DELIMITER ;