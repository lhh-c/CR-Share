-- 享阅数据库初始化脚本
-- PostgreSQL数据库

-- 创建数据库（需要在PostgreSQL中手动执行）
-- CREATE DATABASE xiangyue;

-- 连接到数据库后执行以下SQL

-- 注意：Flask-SQLAlchemy会自动创建表结构
-- 此脚本仅用于参考和手动初始化

-- 创建扩展（如果需要）
-- CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- 表结构将由SQLAlchemy模型自动创建
-- 如需手动创建，可以参考以下结构：

/*
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username VARCHAR(80) UNIQUE NOT NULL,
    email VARCHAR(120) UNIQUE NOT NULL,
    password_hash VARCHAR(255) NOT NULL,
    role VARCHAR(20) NOT NULL DEFAULT 'student',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    is_active BOOLEAN DEFAULT TRUE
);

CREATE INDEX idx_users_username ON users(username);

CREATE TABLE resources (
    id SERIAL PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    description TEXT,
    file_path VARCHAR(500) NOT NULL,
    file_name VARCHAR(255) NOT NULL,
    file_size INTEGER,
    file_type VARCHAR(50),
    uploader_id INTEGER REFERENCES users(id),
    status VARCHAR(20) DEFAULT 'pending',
    reviewer_id INTEGER REFERENCES users(id),
    reviewed_at TIMESTAMP,
    view_count INTEGER DEFAULT 0,
    download_count INTEGER DEFAULT 0,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE tags (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE INDEX idx_tags_name ON tags(name);

CREATE TABLE resource_tags (
    id SERIAL PRIMARY KEY,
    resource_id INTEGER REFERENCES resources(id) ON DELETE CASCADE,
    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    UNIQUE(resource_id, tag_id)
);

CREATE TABLE comments (
    id SERIAL PRIMARY KEY,
    resource_id INTEGER REFERENCES resources(id) ON DELETE CASCADE,
    author_id INTEGER REFERENCES users(id),
    content TEXT NOT NULL,
    parent_id INTEGER REFERENCES comments(id),
    is_ai_response BOOLEAN DEFAULT FALSE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE subscriptions (
    id SERIAL PRIMARY KEY,
    user_id INTEGER REFERENCES users(id) ON DELETE CASCADE,
    tag_id INTEGER REFERENCES tags(id) ON DELETE CASCADE,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    UNIQUE(user_id, tag_id)
);
*/

-- 插入示例数据（可选）
-- INSERT INTO users (username, email, password_hash, role) VALUES
-- ('admin', 'admin@example.com', 'hashed_password', 'moderator'),
-- ('teacher1', 'teacher1@example.com', 'hashed_password', 'teacher'),
-- ('student1', 'student1@example.com', 'hashed_password', 'student');
