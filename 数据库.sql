mysql -u root -p
123456

-- 创建数据库（正确指定字符集）
CREATE DATABASE IF NOT EXISTS mydb 
CHARACTER SET utf8mb4 
COLLATE utf8mb4_unicode_ci;

USE mydb;

-- 删除已存在的表（如果存在）
DROP TABLE IF EXISTS files;
DROP TABLE IF EXISTS users;

-- 创建用户表（优化注释和存储引擎）
CREATE TABLE users (
    id INT AUTO_INCREMENT PRIMARY KEY,
    username VARCHAR(50) UNIQUE NOT NULL,
    password VARCHAR(50) NOT NULL COMMENT '测试阶段使用明文',
    total_quota BIGINT UNSIGNED DEFAULT 10737418240 COMMENT '10GB空间',
    used_quota BIGINT UNSIGNED DEFAULT 0
) ENGINE=InnoDB;

-- 创建文件表（新增生成列实现软删除唯一性约束）
CREATE TABLE files (
    id INT AUTO_INCREMENT PRIMARY KEY,
    user_id INT NOT NULL,
    file_size BIGINT UNSIGNED NOT NULL,
    file_path VARCHAR(512) NOT NULL,
    filename VARCHAR(255) NOT NULL,
    is_deleted BOOLEAN DEFAULT FALSE,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    
    -- 修改为VIRTUAL生成列
    active_file_key VARCHAR(768) GENERATED ALWAYS AS (
        CASE 
            WHEN is_deleted = 0 THEN CONCAT(user_id, '::', file_path)
            ELSE NULL 
        END
    ) VIRTUAL COMMENT '激活文件唯一性标识',
    
    -- 外键约束（级联删除）
    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE,
    
    -- 唯一索引（取消注释以启用）
    UNIQUE KEY uniq_active_file (active_file_key),
    
    -- 辅助索引
    INDEX idx_user_deleted (user_id, is_deleted),
    INDEX idx_file_path (file_path(255))
) ENGINE=InnoDB ROW_FORMAT=DYNAMIC;
-- 添加注释说明
ALTER TABLE users COMMENT '系统用户信息表';
ALTER TABLE files COMMENT '用户文件元数据表';