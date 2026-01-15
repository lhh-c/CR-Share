# 服务器主文件
from flask import Flask, request, jsonify
from flask_cors import CORS
from sqlalchemy import or_
import os

from server.config import (
    SQLALCHEMY_DATABASE_URI, UPLOAD_FOLDER, 
    MAX_CONTENT_LENGTH
)
from server.models import db, User, Resource
from server.utils import call_ai_service

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
    keyword = request.args.get('keyword') or request.args.get('search')

    resources_query = Resource.query

    if keyword and keyword.strip():
        keyword = keyword.strip()
        resources_query = resources_query.filter(
            or_(
                Resource.title.ilike(f'%{keyword}%'),
                Resource.description.ilike(f'%{keyword}%')
            )
        )

    resources = resources_query.order_by(Resource.created_at.desc()).all()

    return jsonify({'resources': [r.to_dict() for r in resources]}), 200


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
        return jsonify({'answer': ai_answer}), 200
    else:
        return jsonify({'error': 'AI服务暂时不可用'}), 503

if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(host='0.0.0.0', port=5000, debug=True)
