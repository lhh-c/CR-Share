# 配置文件
import os
from pathlib import Path

# 基础路径
BASE_DIR = Path(__file__).parent.parent
UPLOAD_FOLDER = BASE_DIR / 'uploads'
UPLOAD_FOLDER.mkdir(exist_ok=True)

# Flask配置
SECRET_KEY = os.environ.get('SECRET_KEY', 'dev-secret-key-change-in-production')
MAX_CONTENT_LENGTH = 100 * 1024 * 1024  # 100MB

# 数据库
DATABASE_CONFIG = {
    'host': os.environ.get('DB_HOST', 'localhost'),
    'port': int(os.environ.get('DB_PORT', 5432)),
    'database': os.environ.get('DB_NAME', 'xiangyue'),
    'user': os.environ.get('DB_USER', 'postgres'),
    'password': os.environ.get('DB_PASSWORD', 'postgres')
}

SQLALCHEMY_DATABASE_URI = (
    f"postgresql://{DATABASE_CONFIG['user']}:{DATABASE_CONFIG['password']}"
    f"@{DATABASE_CONFIG['host']}:{DATABASE_CONFIG['port']}"
    f"/{DATABASE_CONFIG['database']}"
)

SQLALCHEMY_TRACK_MODIFICATIONS = False

# AI服务
AI_SERVICE_URL = os.environ.get('AI_SERVICE_URL', '')
AI_SERVICE_API_KEY = os.environ.get('AI_SERVICE_API_KEY', '')
AI_SERVICE_TIMEOUT = 5

# 允许的文件类型
ALLOWED_EXTENSIONS = {'pdf', 'doc', 'docx', 'ppt', 'pptx', 'txt', 'jpg', 'jpeg', 'png'}

# 角色
ROLE_STUDENT = 'student'
ROLE_TEACHER = 'teacher'
ROLE_MODERATOR = 'moderator'
