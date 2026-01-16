"""
数据库模型定义
"""
from flask_sqlalchemy import SQLAlchemy
from datetime import datetime
from werkzeug.security import generate_password_hash, check_password_hash

db = SQLAlchemy()


class User(db.Model):
    """用户模型"""
    __tablename__ = 'users'

    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(80), unique=True, nullable=False, index=True)
    email = db.Column(db.String(120), unique=True, nullable=False)
    password_hash = db.Column(db.String(255), nullable=False)
    role = db.Column(db.String(20), nullable=False, default='student')  # student, teacher, moderator
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    is_active = db.Column(db.Boolean, default=True)

    # 关系
    resources = db.relationship(
        'Resource',
        backref='uploader',
        lazy=True,
        foreign_keys='Resource.uploader_id'
    )
    comments = db.relationship('Comment', backref='author', lazy=True)
    subscriptions = db.relationship('Subscription', backref='user', lazy=True, cascade='all, delete-orphan')

    def set_password(self, password):
        """设置密码"""
        self.password_hash = generate_password_hash(password)

    def check_password(self, password):
        """验证密码"""
        return check_password_hash(self.password_hash, password)

    def to_dict(self):
        """转换为字典"""
        return {
            'id': self.id,
            'username': self.username,
            'email': self.email,
            'role': self.role,
            'created_at': self.created_at.isoformat() if self.created_at else None
        }


class Resource(db.Model):
    """学习资源模型"""
    __tablename__ = 'resources'

    id = db.Column(db.Integer, primary_key=True)
    title = db.Column(db.String(200), nullable=False)
    description = db.Column(db.Text)
    file_path = db.Column(db.String(500), nullable=False)
    file_name = db.Column(db.String(255), nullable=False)
    file_size = db.Column(db.Integer)  # 字节
    file_type = db.Column(db.String(50))
    uploader_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    status = db.Column(db.String(20), default='pending')  # pending, approved, rejected
    reviewer_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=True)
    reviewed_at = db.Column(db.DateTime, nullable=True)
    view_count = db.Column(db.Integer, default=0)
    download_count = db.Column(db.Integer, default=0)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    updated_at = db.Column(db.DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)

    # 关系
    tags = db.relationship('ResourceTag', backref='resource', lazy=True, cascade='all, delete-orphan')
    comments = db.relationship('Comment', backref='resource', lazy=True, cascade='all, delete-orphan')

    def to_dict(self):
        """转换为字典"""
        return {
            'id': self.id,
            'title': self.title,
            'description': self.description,
            'file_name': self.file_name,
            'file_size': self.file_size,
            'file_type': self.file_type,
            'uploader': self.uploader.username if self.uploader else None,
            'uploader_id': self.uploader_id,
            'status': self.status,
            'view_count': self.view_count,
            'download_count': self.download_count,
            'created_at': self.created_at.isoformat() if self.created_at else None,
            'tags': [rt.tag.name for rt in self.tags] if self.tags else []
        }


class Tag(db.Model):
    """标签模型"""
    __tablename__ = 'tags'

    id = db.Column(db.Integer, primary_key=True)
    name = db.Column(db.String(50), unique=True, nullable=False, index=True)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # 关系
    resources = db.relationship('ResourceTag', backref='tag', lazy=True)
    subscriptions = db.relationship('Subscription', backref='tag', lazy=True)


class ResourceTag(db.Model):
    """资源标签关联表"""
    __tablename__ = 'resource_tags'

    id = db.Column(db.Integer, primary_key=True)
    resource_id = db.Column(db.Integer, db.ForeignKey('resources.id'), nullable=False)
    tag_id = db.Column(db.Integer, db.ForeignKey('tags.id'), nullable=False)

    __table_args__ = (db.UniqueConstraint('resource_id', 'tag_id'),)


class Comment(db.Model):
    """评论模型"""
    __tablename__ = 'comments'

    id = db.Column(db.Integer, primary_key=True)
    resource_id = db.Column(db.Integer, db.ForeignKey('resources.id'), nullable=False)
    author_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    content = db.Column(db.Text, nullable=False)
    parent_id = db.Column(db.Integer, db.ForeignKey('comments.id'), nullable=True)  # 回复的评论ID
    is_ai_response = db.Column(db.Boolean, default=False)  # 是否为AI回复
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    # 关系
    replies = db.relationship('Comment', backref=db.backref('parent', remote_side=[id]), lazy=True)

    def to_dict(self):
        """转换为字典"""
        return {
            'id': self.id,
            'resource_id': self.resource_id,
            'author': self.author.username if self.author else None,
            'author_id': self.author_id,
            'content': self.content,
            'parent_id': self.parent_id,
            'is_ai_response': self.is_ai_response,
            'created_at': self.created_at.isoformat() if self.created_at else None,
            'replies': [reply.to_dict() for reply in self.replies]
        }


class Notification(db.Model):
    """通知模型"""
    __tablename__ = 'notifications'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False, index=True)
    type = db.Column(db.String(20), nullable=False)  # comment / reply
    resource_id = db.Column(db.Integer, db.ForeignKey('resources.id'), nullable=False)
    comment_id = db.Column(db.Integer, db.ForeignKey('comments.id'), nullable=False)
    from_user_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    from_username = db.Column(db.String(80), nullable=False)
    content_preview = db.Column(db.String(60), nullable=False)
    is_read = db.Column(db.Boolean, default=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    user = db.relationship('User', foreign_keys=[user_id])
    resource = db.relationship('Resource', foreign_keys=[resource_id])
    comment = db.relationship('Comment', foreign_keys=[comment_id])
    from_user = db.relationship('User', foreign_keys=[from_user_id])

    def to_dict(self):
        return {
            'id': self.id,
            'type': self.type,
            'resource_id': self.resource_id,
            'resource_title': self.resource.title if self.resource else None,
            'comment_id': self.comment_id,
            'from_username': self.from_username,
            'content_preview': self.content_preview,
            'is_read': bool(self.is_read),
            'created_at': self.created_at.isoformat() if self.created_at else None
        }


class Report(db.Model):
    """举报模型"""
    __tablename__ = 'reports'

    id = db.Column(db.Integer, primary_key=True)
    resource_id = db.Column(db.Integer, db.ForeignKey('resources.id'), nullable=False)
    reporter_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    reason = db.Column(db.Text, nullable=False)
    status = db.Column(db.String(20), default='pending')  # pending, resolved, rejected
    created_at = db.Column(db.DateTime, default=datetime.utcnow)
    resolved_at = db.Column(db.DateTime, nullable=True)
    resolver_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=True)

    resource = db.relationship('Resource', backref=db.backref('reports', lazy=True, cascade='all, delete-orphan'))
    reporter = db.relationship('User', foreign_keys=[reporter_id], backref=db.backref('reports', lazy=True, cascade='all, delete-orphan'))
    resolver = db.relationship('User', foreign_keys=[resolver_id])

    def to_dict(self):
        return {
            'id': self.id,
            'resource_id': self.resource_id,
            'resource_title': self.resource.title if self.resource else None,
            'reporter_id': self.reporter_id,
            'reporter_name': self.reporter.username if self.reporter else None,
            'reason': self.reason,
            'status': self.status,
            'created_at': self.created_at.isoformat() if self.created_at else None,
            'resolved_at': self.resolved_at.isoformat() if self.resolved_at else None,
            'resolver_id': self.resolver_id
        }


class Subscription(db.Model):
    """订阅模型"""
    __tablename__ = 'subscriptions'

    id = db.Column(db.Integer, primary_key=True)
    user_id = db.Column(db.Integer, db.ForeignKey('users.id'), nullable=False)
    tag_id = db.Column(db.Integer, db.ForeignKey('tags.id'), nullable=False)
    created_at = db.Column(db.DateTime, default=datetime.utcnow)

    __table_args__ = (db.UniqueConstraint('user_id', 'tag_id'),)
