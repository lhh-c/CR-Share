"""
工具函数
"""
import os
import requests
from werkzeug.utils import secure_filename
from server.config import ALLOWED_EXTENSIONS, AI_SERVICE_URL, AI_SERVICE_API_KEY, AI_SERVICE_TIMEOUT


def allowed_file(filename):
    """检查文件扩展名是否允许"""
    return '.' in filename and \
           filename.rsplit('.', 1)[1].lower() in ALLOWED_EXTENSIONS


def get_file_extension(filename):
    """获取文件扩展名"""
    return filename.rsplit('.', 1)[1].lower() if '.' in filename else ''


def call_ai_service(question, context=""):
    """
    调用AI服务获取回答
    
    Args:
        question: 用户问题
        context: 上下文信息（可选）
    
    Returns:
        str: AI回答，如果失败返回None
    """
    if not AI_SERVICE_URL or not AI_SERVICE_API_KEY:
        return None
    
    try:
        payload = {
            'question': question,
            'context': context
        }
        headers = {
            'Authorization': f'Bearer {AI_SERVICE_API_KEY}',
            'Content-Type': 'application/json'
        }
        
        response = requests.post(
            AI_SERVICE_URL,
            json=payload,
            headers=headers,
            timeout=AI_SERVICE_TIMEOUT
        )
        
        if response.status_code == 200:
            data = response.json()
            return data.get('answer', '抱歉，无法获取AI回答。')
        else:
            return None
    except Exception as e:
        print(f"AI服务调用失败: {e}")
        return None
