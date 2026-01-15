# 服务器主文件
from flask import Flask, request, jsonify, send_file
from flask_cors import CORS
from werkzeug.utils import secure_filename
from sqlalchemy import or_
import os
from datetime import datetime
from pathlib import Path

from server.config import (
    SQLALCHEMY_DATABASE_URI, UPLOAD_FOLDER, 
    MAX_CONTENT_LENGTH, ALLOWED_EXTENSIONS, 
    ROLE_MODERATOR
)
from server.models import db, User, Resource, Tag, ResourceTag, Comment, Subscription
from server.utils import allowed_file, call_ai_service

app = Flask(__name__)
app.config['SQLALCHEMY_DATABASE_URI'] = SQLALCHEMY_DATABASE_URI
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
app.config['MAX_CONTENT_LENGTH'] = MAX_CONTENT_LENGTH
app.config['UPLOAD_FOLDER'] = str(UPLOAD_FOLDER)

CORS(app)  # 跨域

db.init_app(app)

# 当前用户id，简单处理
current_user_id = None


def get_current_user():
    # 从请求头获取用户id
    user_id = request.headers.get('X-User-Id')
    if user_id:
        return User.query.get(int(user_id))
    return None


def require_auth(f):
    # 检查登录
    def wrapper(*args, **kwargs):
        user = get_current_user()
        if not user:
            return jsonify({'error': '未登录'}), 401
        return f(user, *args, **kwargs)
    wrapper.__name__ = f.__name__
    return wrapper


def require_role(role):
    # 检查权限
    def decorator(f):
        def wrapper(*args, **kwargs):
            user = get_current_user()
            if not user:
                return jsonify({'error': '未登录'}), 401
            if user.role != role:
                return jsonify({'error': '权限不足'}), 403
            return f(user, *args, **kwargs)
        wrapper.__name__ = f.__name__
        return wrapper
    return decorator


@app.route('/api/health', methods=['GET'])
def health_check():
    # 检查服务器是否正常
    return jsonify({'status': 'ok'})


@app.route('/api/auth/register', methods=['POST'])
def register():
    # 注册
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
    # 登录
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
        'user_id': user.id
    }), 200


@app.route('/api/resources', methods=['GET'])
def get_resources():
    # 获取资源列表
    status_filter = request.args.get('status')
    keyword = request.args.get('keyword') or request.args.get('search')
    tag_id_str = request.args.get('tag_id') or request.args.get('tag')

    resources_query = Resource.query

    # 根据用户角色决定能看到什么
    current_user = get_current_user()
    if current_user and current_user.role == ROLE_MODERATOR:
        if status_filter:
            resources_query = resources_query.filter(Resource.status == status_filter)
        else:
            resources_query = resources_query.filter(Resource.status == 'pending')
    else:
        resources_query = resources_query.filter(Resource.status == 'approved')

    # 搜索关键词
    if keyword and keyword.strip():
        keyword = keyword.strip()
        resources_query = resources_query.filter(
            or_(
                Resource.title.ilike(f'%{keyword}%'),
                Resource.description.ilike(f'%{keyword}%')
            )
        )

    # 按标签过滤
    if tag_id_str and tag_id_str.isdigit():
        tag_id = int(tag_id_str)
        resources_query = resources_query.join(ResourceTag).filter(ResourceTag.tag_id == tag_id)

    # 按时间倒序
    resources = resources_query.order_by(Resource.created_at.desc()).all()

    return jsonify({'resources': [r.to_dict() for r in resources]}), 200

@app.route('/api/resources', methods=['POST'])
@require_auth
def upload_resource(user):
    # 上传资源
    if 'file' not in request.files:
        return jsonify({'error': '没有上传文件'}), 400

    file = request.files['file']
    if file.filename == '':
        return jsonify({'error': '文件名为空'}), 400

    if not allowed_file(file.filename):
        return jsonify({'error': '不支持的文件类型'}), 400

    title = request.form.get('title')
    description = request.form.get('description', '')
    tags = request.form.getlist('tags')

    if not title:
        return jsonify({'error': '缺少标题'}), 400

    # 保存文件
    original_filename = file.filename
    secure_name = secure_filename(original_filename)
    timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
    unique_filename = f"{timestamp}_{secure_name}"

    file_path_for_db = unique_filename
    full_save_path = os.path.join(app.config['UPLOAD_FOLDER'], file_path_for_db)

    try:
        file.save(full_save_path)
    except Exception as e:
        return jsonify({'error': '文件保存失败'}), 500

    # 获取文件大小和类型
    file_size = os.path.getsize(full_save_path)
    file_type = ''
    if '.' in original_filename:
        file_type = original_filename.rsplit('.', 1)[1].lower()

    # 创建资源记录
    resource = Resource(
        title=title,
        description=description,
        file_path=file_path_for_db,
        file_name=original_filename,
        file_size=file_size,
        file_type=file_type,
        uploader_id=user.id,
        status='pending'
    )

    db.session.add(resource)

    # 处理标签
    for tag_name in tags:
        if not tag_name.strip():
            continue
        tag_name = tag_name.strip()
        tag = Tag.query.filter_by(name=tag_name).first()
        if not tag:
            tag = Tag(name=tag_name)
            db.session.add(tag)
            db.session.flush()

        resource_tag = ResourceTag(resource_id=resource.id, tag_id=tag.id)
        db.session.add(resource_tag)

    try:
        db.session.commit()
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': '保存资源信息失败'}), 500

    return jsonify({
        'message': '上传成功，等待审核',
        'resource': resource.to_dict()
    }), 201


@app.route('/api/resources/<int:resource_id>/download', methods=['GET'])
@require_auth
def download_resource(user, resource_id):
    # 下载资源
    resource = Resource.query.get_or_404(resource_id)

    if resource.status != 'approved':
        return jsonify({'error': '资源未审核通过'}), 403

    file_path = os.path.join(app.config['UPLOAD_FOLDER'], resource.file_path)

    if not os.path.exists(file_path):
        return jsonify({
            'error': '文件不存在于服务器',
            'checked_path': file_path
        }), 404

    # 下载次数+1
    resource.download_count += 1
    db.session.commit()

    try:
        response = send_file(
            file_path,
            as_attachment=True,
            download_name=resource.file_name,
            mimetype='application/octet-stream'
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
    # 审核资源
    resource = Resource.query.get_or_404(resource_id)
    data = request.json
    status = data.get('status')
    
    if status not in ['approved', 'rejected']:
        return jsonify({'error': '无效的状态'}), 400
    
    resource.status = status
    resource.reviewer_id = moderator.id
    resource.reviewed_at = datetime.utcnow()
    db.session.commit()
    
    return jsonify({'message': '审核完成', 'resource': resource.to_dict()}), 200


@app.route('/api/resources/<int:resource_id>/comments', methods=['GET'])
def get_comments(resource_id):
    # 获取评论
    resource = Resource.query.get_or_404(resource_id)
    comments = Comment.query.filter_by(resource_id=resource_id, parent_id=None).all()
    
    return jsonify({'comments': [c.to_dict() for c in comments]}), 200


@app.route('/api/resources/<int:resource_id>/comments', methods=['POST'])
@require_auth
def add_comment(user, resource_id):
    # 添加评论
    resource = Resource.query.get_or_404(resource_id)
    data = request.json
    content = data.get('content')
    parent_id = data.get('parent_id')
    
    if not content:
        return jsonify({'error': '评论内容不能为空'}), 400
    
    comment = Comment(
        resource_id=resource_id,
        author_id=user.id,
        content=content,
        parent_id=parent_id
    )
    
    db.session.add(comment)
    db.session.commit()
    
    return jsonify({'message': '评论成功', 'comment': comment.to_dict()}), 201


@app.route('/api/resources/<int:resource_id>/ai-ask', methods=['POST'])
@require_auth
def ai_ask(user, resource_id):
    # AI提问功能
    resource = Resource.query.get_or_404(resource_id)
    data = request.json
    question = data.get('question')
    
    if not question:
        return jsonify({'error': '问题不能为空'}), 400
    
    context = f"资源标题: {resource.title}\n资源描述: {resource.description or '无'}"
    
    ai_answer = call_ai_service(question, context)
    
    if ai_answer:
        ai_comment = Comment(
            resource_id=resource_id,
            author_id=user.id,
            content=f"AI回答: {ai_answer}",
            is_ai_response=True
        )
        db.session.add(ai_comment)
        db.session.commit()
        
        return jsonify({'answer': ai_answer, 'comment': ai_comment.to_dict()}), 200
    else:
        return jsonify({'error': 'AI服务暂时不可用'}), 503


@app.route('/api/tags', methods=['GET'])
def get_tags():
    # 获取所有标签
    tags = Tag.query.all()
    return jsonify({'tags': [{'id': t.id, 'name': t.name} for t in tags]}), 200


@app.route('/api/subscriptions', methods=['GET'])
@require_auth
def get_subscriptions(user):
    # 获取用户订阅
    subscriptions = Subscription.query.filter_by(user_id=user.id).all()
    return jsonify({
        'subscriptions': [{'tag_id': s.tag_id, 'tag_name': s.tag.name} for s in subscriptions]
    }), 200


@app.route('/api/subscriptions', methods=['POST'])
@require_auth
def subscribe_tag(user):
    # 订阅标签
    data = request.json
    tag_id = data.get('tag_id')
    
    if not tag_id:
        return jsonify({'error': '缺少标签ID'}), 400
    
    tag = Tag.query.get_or_404(tag_id)
    
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
    # 取消订阅
    subscription = Subscription.query.filter_by(user_id=user.id, tag_id=tag_id).first()
    if not subscription:
        return jsonify({'error': '未订阅该标签'}), 404
    
    db.session.delete(subscription)
    db.session.commit()
    
    return jsonify({'message': '取消订阅成功'}), 200


@app.route('/api/subscriptions/resources', methods=['GET'])
@require_auth
def get_subscribed_resources(user):
    # 获取订阅的资源
    subscriptions = Subscription.query.filter_by(user_id=user.id).all()
    tag_ids = [s.tag_id for s in subscriptions]
    
    if not tag_ids:
        return jsonify({'resources': []}), 200
    
    resources = Resource.query.join(ResourceTag).filter(
        ResourceTag.tag_id.in_(tag_ids),
        Resource.status == 'approved'
    ).distinct().order_by(Resource.created_at.desc()).all()
    
    return jsonify({'resources': [r.to_dict() for r in resources]}), 200

@app.route('/api/resources/<int:resource_id>', methods=['GET'])
def get_resource(resource_id):
    # 获取单个资源详情
    resource = Resource.query.get_or_404(resource_id)

    resource.view_count += 1
    db.session.commit()

    return jsonify(resource.to_dict()), 200


if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(host='0.0.0.0', port=5000, debug=True)
