-- =============================================================
-- 06_email_verify.sql
-- 邮箱验证码注册 —— 增量迁移，在 01~05 之后执行
--
-- 变更内容：
--   1. user 表新增 email 列（可空 + 唯一），已有账号不受影响
--      （MySQL 唯一索引允许多个 NULL，老用户的 email 为 NULL 不冲突）
--   2. 新建 email_verify 表，保存注册时下发的 4 位验证码
-- =============================================================

USE music_app;

-- -------------------------------------------------------------
-- 1) user 表补充邮箱列
-- -------------------------------------------------------------
ALTER TABLE `user`
    ADD COLUMN email VARCHAR(100) NULL COMMENT '邮箱' AFTER nickname,
    ADD UNIQUE KEY uk_user_email (email);

-- -------------------------------------------------------------
-- 2) 邮箱验证码表
--    同一邮箱可能有多条历史记录，取最新未使用且未过期的一条校验
-- -------------------------------------------------------------
DROP TABLE IF EXISTS email_verify;
CREATE TABLE email_verify (
    verify_id   INT          NOT NULL AUTO_INCREMENT COMMENT '验证码ID',
    email       VARCHAR(100) NOT NULL                COMMENT '目标邮箱',
    code        CHAR(4)      NOT NULL                COMMENT '4位数字验证码',
    expire_time DATETIME     NOT NULL                COMMENT '过期时间',
    used        TINYINT      NOT NULL DEFAULT 0      COMMENT '0未使用 1已使用',
    send_time   DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP COMMENT '发送时间',
    PRIMARY KEY (verify_id),
    KEY idx_ev_email (email),
    KEY idx_ev_expire (expire_time)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COMMENT='邮箱验证码表';