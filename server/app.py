"""
享阅服务器主应用
"""
from flask import Flask, request, jsonify, send_file
from functools import wraps
from flask_cors import CORS
from werkzeug.utils import secure_filename
from sqlalchemy import or_
from sqlalchemy.exc import SQLAlchemyError
import os
from datetime import datetime
from pathlib import Path

from server.config import (
    SQLALCHEMY_DATABASE_URI, UPLOAD_FOLDER, 
    MAX_CONTENT_LENGTH, ALLOWED_EXTENSIONS, 
    ROLE_MODERATOR
)
from server.models import db, User, Resource, Tag, ResourceTag, Comment, Subscription, Report, Notification
from server.utils import allowed_file, call_ai_service

app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = SQLALCHEMY_DATABASE_URI
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH
app.config['UPLOAD_FOLDER'] = str(UPLOAD_FOLDER)

CORS(app)  # 允许跨域请求

db.init_app(app)

# 会话管理
current_user_id = None


def get_current_user():
    """获取当前登录用户"""
    user_id = request.headers.get('X-User-Id')
    if user_id:
        return User.query.get(int(user_id))
    return None


def require_auth(f):
    """认证装饰器"""
    @wraps(f)
    def wrapper(*args, **kwargs):
        user = get_current_user()
        if not user:
            return jsonify({'error': '未登录'}), 401
        return f(user, *args, **kwargs)
    return wrapper


def require_role(role):
    """角色权限装饰器"""
    def decorator(f):
        @wraps(f)
        def wrapper(*args, **kwargs):
            user = get_current_user()
            if not user:
                return jsonify({'error': '未登录'}), 401
            if user.role != role:
                return jsonify({'error': '权限不足'}), 403
            return f(user, *args, **kwargs)
        return wrapper
    return decorator


@app.route('/api/health', methods=['GET'])
def health_check():
    """健康检查"""
    return jsonify({'status': 'ok'})


@app.route('/api/auth/register', methods=['POST'])
def register():
    """用户注册"""
    data = request.json
    username = data.get('username')
    email = data.get('email')
    password = data.get('password')
    role = data.get('role', 'student')
    
    if not username or not email or not password:
        return jsonify({'error': '缺少必要字段'}), 400
    
    if User.query.filter_by(username=username).first():
        return jsonify({'error': '用户名已存在'}), 400
    
    if User.query.filter_by(email=email).first():
        return jsonify({'error': '邮箱已存在'}), 400
    
    user = User(username=username, email=email, role=role)
    user.set_password(password)
    db.session.add(user)
    db.session.commit()
    
    return jsonify({'message': '注册成功', 'user': user.to_dict()}), 201


@app.route('/api/auth/login', methods=['POST'])
def login():
    """用户登录"""
    data = request.json
    username = data.get('username')
    password = data.get('password')
    
    if not username or not password:
        return jsonify({'error': '缺少用户名或密码'}), 400
    
    user = User.query.filter_by(username=username).first()
    
    if not user or not user.check_password(password):
        return jsonify({'error': '用户名或密码错误'}), 401
    
    if not user.is_active:
        return jsonify({'error': '账户已被禁用'}), 403
    
    return jsonify({
        'message': '登录成功',
        'user': user.to_dict(),
        'user_id': user.id  # 简化版，生产环境应返回JWT token
    }), 200


@app.route('/api/users/me', methods=['GET'])
@require_auth
def get_my_profile(user):
    """获取当前登录用户信息"""
    return jsonify({'user': user.to_dict()}), 200


@app.route('/api/auth/logout', methods=['POST'])
@require_auth
def logout(user):
    """退出登录（无状态鉴权：由客户端清除 user_id 实现）"""
    return jsonify({'message': '退出登录成功'}), 200


@app.route('/api/users/me', methods=['DELETE'])
@require_auth
def delete_my_account(user):
    """注销账号（删除当前用户）"""
    try:
        # 先删订阅，避免外键约束问题
        Subscription.query.filter_by(user_id=user.id).delete(synchronize_session=False)

        # 删除评论（评论 author_id 外键不可为空）
        Comment.query.filter_by(author_id=user.id).delete(synchronize_session=False)

        # 删除资源标签关联、评论，再删除资源（资源 uploader_id 外键不可为空）
        resources = Resource.query.filter_by(uploader_id=user.id).all()
        for r in resources:
            ResourceTag.query.filter_by(resource_id=r.id).delete(synchronize_session=False)
            Comment.query.filter_by(resource_id=r.id).delete(synchronize_session=False)
            db.session.delete(r)

        db.session.delete(user)
        db.session.commit()
        return jsonify({'message': '账号已注销'}), 200
    except SQLAlchemyError as e:
        db.session.rollback()
        return jsonify({'error': '注销失败', 'detail': str(e)}), 500


@app.route('/api/resources/my', methods=['GET'])
@require_auth
def get_my_resources(user):
    try:
        page = int(request.args.get('page', 1))
    except Exception:
        page = 1

    try:
        page_size = int(request.args.get('page_size', 20))
    except Exception:
        page_size = 20

    page = max(page, 1)
    page_size = max(1, min(page_size, 100))

    q = Resource.query.filter(Resource.uploader_id == user.id).order_by(Resource.created_at.desc())

    total = q.count()
    items = q.offset((page - 1) * page_size).limit(page_size).all()

    return jsonify({
        'resources': [r.to_dict() for r in items],
        'page': page,
        'page_size': page_size,
        'total': total
    }), 200


@app.route('/api/resources', methods=['GET'])
def get_resources():
    # 获取查询参数（统一用 keyword 和 tag_id）
    status_filter = request.args.get('status')              # approved / pending / rejected
    keyword = request.args.get('keyword') or request.args.get('search')  # 支持两种参数名
    tag_id_str = request.args.get('tag_id') or request.args.get('tag')

    # 分页参数（page 从 1 开始）
    try:
        page = int(request.args.get('page', 1))
    except Exception:
        page = 1
    try:
        page_size = int(request.args.get('page_size', 20))
    except Exception:
        page_size = 20

    page = max(page, 1)
    page_size = max(1, min(page_size, 100))

    sort = (request.args.get('sort') or 'new').strip().lower()

    resources_query = Resource.query

    # 根据角色 + status 参数决定可见范围
    current_user = get_current_user()
    if current_user and current_user.role == ROLE_MODERATOR:
        if status_filter:
            resources_query = resources_query.filter(Resource.status == status_filter)
        else:
            resources_query = resources_query.filter(Resource.status == 'pending')
    else:
        resources_query = resources_query.filter(Resource.status == 'approved')

    # 关键词搜索（模糊匹配标题或描述）
    keyword_clean = None
    if keyword and keyword.strip():
        keyword_clean = keyword.strip()
        resources_query = resources_query.filter(
            or_(
                Resource.title.ilike(f'%{keyword_clean}%'),
                Resource.description.ilike(f'%{keyword_clean}%')
            )
        )

    # 标签过滤（通过关联表 ResourceTag）
    if tag_id_str and tag_id_str.isdigit():
        tag_id = int(tag_id_str)
        resources_query = resources_query.join(ResourceTag).filter(ResourceTag.tag_id == tag_id)

    # 总数
    total = resources_query.distinct(Resource.id).count()

    # 排序
    if sort == 'relevance':
        # 相关性排序：标题/描述包含关键字的排前面（学生写法：查出来再排序）
        tmp = resources_query.order_by(Resource.created_at.desc()).all()

        def rel_score(r):
            s = 0
            if keyword_clean:
                try:
                    if r.title and keyword_clean.lower() in r.title.lower():
                        s += 100
                except Exception:
                    pass
                try:
                    if r.description and keyword_clean.lower() in r.description.lower():
                        s += 50
                except Exception:
                    pass
            return s

        tmp.sort(key=lambda x: rel_score(x), reverse=True)
        total = len(tmp)
        start = (page - 1) * page_size
        end = start + page_size
        resources = tmp[start:end]

        return jsonify({
            'resources': [r.to_dict() for r in resources],
            'page': page,
            'page_size': page_size,
            'total': total
        }), 200

    elif sort == 'views':
        resources_query = resources_query.order_by(Resource.view_count.desc(), Resource.created_at.desc())

    elif sort == 'smart':
        # 保留 smart：时间 + 浏览量 + 相关性（学生写法：先查出来再打分排序）
        tmp = resources_query.order_by(Resource.created_at.desc()).all()

        def calc_score(r):
            s = 0

            if keyword_clean:
                try:
                    if r.title and keyword_clean.lower() in r.title.lower():
                        s += 60
                except Exception:
                    pass
                try:
                    if r.description and keyword_clean.lower() in r.description.lower():
                        s += 30
                except Exception:
                    pass

            try:
                s += int(r.view_count or 0) * 1
            except Exception:
                pass

            try:
                s += int(r.created_at.timestamp()) // 100000
            except Exception:
                pass

            return s

        tmp.sort(key=lambda x: calc_score(x), reverse=True)

        total = len(tmp)
        start = (page - 1) * page_size
        end = start + page_size
        resources = tmp[start:end]

        return jsonify({
            'resources': [r.to_dict() for r in resources],
            'page': page,
            'page_size': page_size,
            'total': total
        }), 200

    elif sort == 'hot':
        resources_query = resources_query.order_by(Resource.view_count.desc(), Resource.created_at.desc())

    else:
        # new
        resources_query = resources_query.order_by(Resource.created_at.desc())

    resources = resources_query.offset((page - 1) * page_size).limit(page_size).all()

    return jsonify({
        'resources': [r.to_dict() for r in resources],
        'page': page,
        'page_size': page_size,
        'total': total
    }), 200


@app.route('/api/resources', methods=['POST'])
@require_auth
def upload_resource(user):
    """上传资源"""

    # 检查是否有文件
    if 'file' not in request.files:
        return jsonify({'error': '没有上传文件'}), 400

    file = request.files['file']
    if file.filename == '':
        return jsonify({'error': '文件名为空'}), 400

    if not allowed_file(file.filename):
        return jsonify({'error': '不支持的文件类型'}), 400

    title = request.form.get('title')
    description = request.form.get('description', '')
    tags = request.form.getlist('tags')  # 前端传 tags[] 数组

    if not title:
        return jsonify({'error': '缺少标题'}), 400

    # ──────────────── 文件保存部分 ────────────────
    # 1. 生成安全且唯一的文件名
    original_filename = file.filename
    secure_name = secure_filename(original_filename)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    unique_filename = f"{timestamp}_{secure_name}"

    # 目前使用平铺方式（最简单）
    file_path_for_db = unique_filename                  # ← 数据库只存这个！
    full_save_path = os.path.join(app.config['UPLOAD_FOLDER'], file_path_for_db)

    # 3. 保存文件
    try:
        file.save(full_save_path)
    except Exception:
        return jsonify({'error': '文件保存失败'}), 500

    # 4. 获取文件信息
    file_size = os.path.getsize(full_save_path)
    file_type = ''
    if '.' in original_filename:
        file_type = original_filename.rsplit('.', 1)[1].lower()

    # ──────────────── 创建资源记录 ────────────────
    resource = Resource(
        title=title,
        description=description,
        file_path=file_path_for_db,          # ← 关键修改：只存相对路径
        file_name=original_filename,         # 原始文件名，用于下载时显示
        file_size=file_size,
        file_type=file_type,
        uploader_id=user.id,
        status='pending'                     # 待审核
    )

    db.session.add(resource)

    # ──────────────── 处理标签 ────────────────
    for tag_name in tags:
        if not tag_name.strip():
            continue
        tag_name = tag_name.strip()
        tag = Tag.query.filter_by(name=tag_name).first()
        if not tag:
            tag = Tag(name=tag_name)
            db.session.add(tag)
            db.session.flush()  # 获取 tag.id

        # 关联资源和标签
        resource_tag = ResourceTag(resource_id=resource.id, tag_id=tag.id)
        db.session.add(resource_tag)

    try:
        db.session.commit()
    except Exception:
        db.session.rollback()
        return jsonify({'error': '保存资源信息失败'}), 500

    return jsonify({
        'message': '上传成功，等待审核',
        'resource': resource.to_dict()
    }), 201


@app.route('/api/resources/<int:resource_id>/download', methods=['GET'])
@require_auth
def download_resource(user, resource_id):
    resource = Resource.query.get_or_404(resource_id)

    if resource.status != 'approved':
        # 审核员允许预览/下载待审核资源
        if not (user and user.role == ROLE_MODERATOR and resource.status == 'pending'):
            return jsonify({'error': '资源未审核通过'}), 403

    # 使用相对路径拼接（现在数据库已经是相对路径了）
    file_path = os.path.join(app.config['UPLOAD_FOLDER'], resource.file_path)


    if not os.path.exists(file_path):
        return jsonify({
            'error': '文件不存在于服务器',
            'checked_path': file_path
        }), 404

    # 增加下载计数
    resource.download_count += 1
    db.session.commit()

    # 核心：发送文件
    try:
        response = send_file(
            file_path,
            as_attachment=True,
            download_name=resource.file_name,
            mimetype='application/octet-stream'  # 强制二进制下载，防止浏览器预览
        )
        return response
    except Exception as e:
        return jsonify({
            'error': '文件发送失败',
            'detail': str(e)
        }), 500


@app.route('/api/resources/<int:resource_id>/review', methods=['POST'])
@require_role(ROLE_MODERATOR)
def review_resource(moderator, resource_id):
    """审核资源"""
    resource = Resource.query.get_or_404(resource_id)
    data = request.json
    status = data.get('status')  # approved or rejected
    
    if status not in ['approved', 'rejected']:
        return jsonify({'error': '无效的状态'}), 400
    
    resource.status = status
    resource.reviewer_id = moderator.id
    resource.reviewed_at = datetime.utcnow()
    db.session.commit()
    
    return jsonify({'message': '审核完成', 'resource': resource.to_dict()}), 200


@app.route('/api/resources/<int:resource_id>/comments', methods=['GET'])
def get_comments(resource_id):
    """获取资源评论"""
    resource = Resource.query.get_or_404(resource_id)
    comments = Comment.query.filter_by(resource_id=resource_id, parent_id=None).all()
    
    return jsonify({'comments': [c.to_dict() for c in comments]}), 200


@app.route('/api/resources/<int:resource_id>/comments', methods=['POST'])
@require_auth
def add_comment(user, resource_id):
    """添加评论"""
    resource = Resource.query.get_or_404(resource_id)
    data = request.json or {}
    content = (data.get('content') or '').strip()
    parent_id = data.get('parent_id')
    
    if not content:
        return jsonify({'error': '评论内容不能为空'}), 400
    
    parent_comment = None
    if parent_id is not None:
        try:
            parent_id_int = int(parent_id)
        except Exception:
            return jsonify({'error': 'parent_id 无效'}), 400

        parent_comment = Comment.query.get_or_404(parent_id_int)
        if parent_comment.resource_id != resource_id:
            return jsonify({'error': 'parent_id 不属于该资源'}), 400
    
    comment = Comment(
        resource_id=resource_id,
        author_id=user.id,
        content=content,
        parent_id=parent_comment.id if parent_comment else None
    )
    
    db.session.add(comment)
    db.session.flush()  # 获取 comment.id

    def preview(text: str) -> str:
        t = (text or '').strip().replace('\n', ' ')
        return t[:50]

    # 生成通知
    notify_user_ids = set()

    if parent_comment is None:
        # 新评论：通知资源上传者
        if resource.uploader_id and resource.uploader_id != user.id:
            notify_user_ids.add(resource.uploader_id)
        n_type = 'comment'
    else:
        # 新回复：通知被回复的评论作者
        if parent_comment.author_id and parent_comment.author_id != user.id:
            notify_user_ids.add(parent_comment.author_id)
        n_type = 'reply'

    for uid in notify_user_ids:
        notif = Notification(
            user_id=uid,
            type=n_type,
            resource_id=resource_id,
            comment_id=comment.id,
            from_user_id=user.id,
            from_username=user.username,
            content_preview=preview(content),
            is_read=False
        )
        db.session.add(notif)

    db.session.commit()
    
    return jsonify({'message': '评论成功', 'comment': comment.to_dict()}), 201


@app.route('/api/notifications/unread_count', methods=['GET'])
@require_auth
def get_unread_notification_count(user):
    count = Notification.query.filter_by(user_id=user.id, is_read=False).count()
    return jsonify({'unread_count': count}), 200


@app.route('/api/notifications', methods=['GET'])
@require_auth
def get_notifications(user):
    unread_only = request.args.get('unread')

    try:
        page = int(request.args.get('page', 1))
    except Exception:
        page = 1

    try:
        page_size = int(request.args.get('page_size', 20))
    except Exception:
        page_size = 20

    page = max(page, 1)
    page_size = max(1, min(page_size, 100))

    q = Notification.query.filter(Notification.user_id == user.id)
    if unread_only in ['1', 'true', 'True']:
        q = q.filter(Notification.is_read.is_(False))

    total = q.count()
    items = q.order_by(Notification.created_at.desc()).offset((page - 1) * page_size).limit(page_size).all()

    return jsonify({
        'notifications': [n.to_dict() for n in items],
        'page': page,
        'page_size': page_size,
        'total': total
    }), 200


@app.route('/api/notifications/<int:notif_id>/read', methods=['POST'])
@require_auth
def mark_notification_read(user, notif_id):
    notif = Notification.query.get_or_404(notif_id)
    if notif.user_id != user.id:
        return jsonify({'error': '权限不足'}), 403

    notif.is_read = True
    db.session.commit()
    return jsonify({'message': 'ok'}), 200


@app.route('/api/notifications/read_all', methods=['POST'])
@require_auth
def mark_all_notifications_read(user):
    Notification.query.filter_by(user_id=user.id, is_read=False).update({'is_read': True})
    db.session.commit()
    return jsonify({'message': 'ok'}), 200


@app.route('/api/resources/<int:resource_id>/ai-ask', methods=['POST'])
@require_auth
def ai_ask(user, resource_id):
    """AI提问"""
    resource = Resource.query.get_or_404(resource_id)
    data = request.json
    question = data.get('question')
    
    if not question:
        return jsonify({'error': '问题不能为空'}), 400
    
    # 构建上下文
    context = f"资源标题: {resource.title}\n资源描述: {resource.description or '无'}"
    
    # 调用AI服务
    ai_answer = call_ai_service(question, context)
    
    if ai_answer:
        return jsonify({'answer': ai_answer}), 200
    else:
        return jsonify({'error': 'AI服务暂时不可用'}), 503


@app.route('/api/tags', methods=['GET'])
def get_tags():
    """获取所有标签"""
    tags = Tag.query.all()
    return jsonify({'tags': [{'id': t.id, 'name': t.name} for t in tags]}), 200


@app.route('/api/subscriptions', methods=['GET'])
@require_auth
def get_subscriptions(user):
    """获取用户的订阅"""
    subscriptions = Subscription.query.filter_by(user_id=user.id).all()
    return jsonify({
        'subscriptions': [{'tag_id': s.tag_id, 'tag_name': s.tag.name} for s in subscriptions]
    }), 200


@app.route('/api/subscriptions', methods=['POST'])
@require_auth
def subscribe_tag(user):
    """订阅标签"""
    data = request.json
    tag_id = data.get('tag_id')
    
    if not tag_id:
        return jsonify({'error': '缺少标签ID'}), 400
    
    tag = Tag.query.get_or_404(tag_id)
    
    # 检查是否已订阅
    existing = Subscription.query.filter_by(user_id=user.id, tag_id=tag_id).first()
    if existing:
        return jsonify({'error': '已订阅该标签'}), 400
    
    subscription = Subscription(user_id=user.id, tag_id=tag_id)
    db.session.add(subscription)
    db.session.commit()
    
    return jsonify({'message': '订阅成功'}), 201


@app.route('/api/subscriptions/<int:tag_id>', methods=['DELETE'])
@require_auth
def unsubscribe_tag(user, tag_id):
    """取消订阅"""
    subscription = Subscription.query.filter_by(user_id=user.id, tag_id=tag_id).first()
    if not subscription:
        return jsonify({'error': '未订阅该标签'}), 404
    
    db.session.delete(subscription)
    db.session.commit()
    
    return jsonify({'message': '取消订阅成功'}), 200


@app.route('/api/subscriptions/resources', methods=['GET'])
@require_auth
def get_subscribed_resources(user):
    """获取订阅标签的资源"""
    subscriptions = Subscription.query.filter_by(user_id=user.id).all()
    tag_ids = [s.tag_id for s in subscriptions]

    # 分页参数（page 从 1 开始）
    try:
        page = int(request.args.get('page', 1))
    except Exception:
        page = 1
    try:
        page_size = int(request.args.get('page_size', 20))
    except Exception:
        page_size = 20

    page = max(page, 1)
    page_size = max(1, min(page_size, 100))

    if not tag_ids:
        return jsonify({
            'resources': [],
            'page': page,
            'page_size': page_size,
            'total': 0
        }), 200

    resources_query = Resource.query.join(ResourceTag).filter(
        ResourceTag.tag_id.in_(tag_ids),
        Resource.status == 'approved'
    ).distinct()

    total = resources_query.count()

    resources = resources_query.order_by(Resource.created_at.desc()) \
        .offset((page - 1) * page_size) \
        .limit(page_size) \
        .all()

    return jsonify({
        'resources': [r.to_dict() for r in resources],
        'page': page,
        'page_size': page_size,
        'total': total
    }), 200


@app.route('/api/reports', methods=['POST'])
@require_auth
def create_report(user):
    data = request.json or {}
    resource_id = data.get('resource_id')
    reason = (data.get('reason') or '').strip()

    if not resource_id or not reason:
        return jsonify({'error': '缺少必要参数'}), 400

    resource = Resource.query.get_or_404(int(resource_id))

    existing = Report.query.filter_by(resource_id=resource.id, reporter_id=user.id, status='pending').first()
    if existing:
        return jsonify({'error': '您已经举报过该资源'}), 400

    report = Report(resource_id=resource.id, reporter_id=user.id, reason=reason, status='pending')
    db.session.add(report)
    db.session.commit()

    return jsonify({'message': '举报已提交', 'report': report.to_dict()}), 201


@app.route('/api/reports', methods=['GET'])
@require_role(ROLE_MODERATOR)
def get_reports(moderator):
    status = (request.args.get('status') or 'pending').strip().lower()

    try:
        page = int(request.args.get('page', 1))
    except Exception:
        page = 1

    try:
        page_size = int(request.args.get('page_size', 20))
    except Exception:
        page_size = 20

    page = max(page, 1)
    page_size = max(1, min(page_size, 100))

    q = Report.query
    if status != 'all':
        q = q.filter(Report.status == status)

    total = q.count()
    items = q.order_by(Report.created_at.desc()).offset((page - 1) * page_size).limit(page_size).all()

    return jsonify({
        'reports': [r.to_dict() for r in items],
        'page': page,
        'page_size': page_size,
        'total': total
    }), 200


@app.route('/api/reports/<int:report_id>/resolve', methods=['POST'])
@require_role(ROLE_MODERATOR)
def resolve_report(moderator, report_id):
    report = Report.query.get_or_404(report_id)

    if report.status != 'pending':
        return jsonify({'error': '该举报已处理'}), 400

    data = request.json or {}
    action = (data.get('action') or '').strip().lower()  # delete / reject

    if action not in ['delete', 'reject']:
        return jsonify({'error': '无效的操作'}), 400

    report.resolver_id = moderator.id
    report.resolved_at = datetime.utcnow()

    if action == 'delete':
        report.status = 'resolved'
        resource = Resource.query.get(report.resource_id)
        if resource:
            # PostgreSQL 最稳妥：先删子表再删父表（避免外键未设置 ON DELETE CASCADE 时失败）
            ResourceTag.query.filter_by(resource_id=resource.id).delete(synchronize_session=False)
            Comment.query.filter_by(resource_id=resource.id).delete(synchronize_session=False)
            Report.query.filter_by(resource_id=resource.id).delete(synchronize_session=False)

            # 删除文件（尽力而为）
            try:
                file_path = os.path.join(app.config['UPLOAD_FOLDER'], resource.file_path)
                if os.path.exists(file_path):
                    os.remove(file_path)
            except Exception as e:
                app.logger.error(f"删除资源文件失败: {e}")

            db.session.delete(resource)

        db.session.commit()
        return jsonify({'message': '操作成功，资源已删除'}), 200
    else:
        report.status = 'rejected'
        db.session.commit()
        return jsonify({'message': '操作成功', 'report': report.to_dict()}), 200


@app.route('/api/resources/<int:resource_id>', methods=['GET'])
def get_resource(resource_id):
    resource = Resource.query.get_or_404(resource_id)

    resource.view_count += 1
    db.session.commit()

    return jsonify(resource.to_dict()), 200


@app.route('/api/resources/<int:resource_id>', methods=['DELETE'])
@require_auth
def delete_resource(user, resource_id):
    resource = Resource.query.get_or_404(resource_id)

    if user.role != ROLE_MODERATOR and resource.uploader_id != user.id:
        return jsonify({'error': '权限不足'}), 403

    try:
        ResourceTag.query.filter_by(resource_id=resource.id).delete(synchronize_session=False)
        Comment.query.filter_by(resource_id=resource.id).delete(synchronize_session=False)
        Report.query.filter_by(resource_id=resource.id).delete(synchronize_session=False)

        try:
            file_path = os.path.join(app.config['UPLOAD_FOLDER'], resource.file_path)
            if os.path.exists(file_path):
                os.remove(file_path)
        except Exception:
            pass

        db.session.delete(resource)
        db.session.commit()
        return jsonify({'message': '资源已删除'}), 200
    except SQLAlchemyError as e:
        db.session.rollback()
        return jsonify({'error': '删除资源失败', 'detail': str(e)}), 500


if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(host='0.0.0.0', port=5000, debug=True)
