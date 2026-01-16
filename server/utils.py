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
    """调用AI服务获取回答"""
    if not AI_SERVICE_URL or not AI_SERVICE_API_KEY:
        return "AI服务未配置"

    try:
        # 构造请求体
        payload = {
            "model": "deepseek-chat",
            "messages": [
                {"role": "system", "content": f"你是享阅学习资源平台的AI助手。请根据以下学习资源的背景信息，简洁地回答用户的问题。\n背景信息：\n{context}"},
                {"role": "user", "content": question}
            ]
        }
        headers = {
            'Authorization': f'Bearer {AI_SERVICE_API_KEY}',
            'Content-Type': 'application/json'
        }

        # 发送请求
        response = requests.post(
            AI_SERVICE_URL,
            json=payload,
            headers=headers,
            timeout=AI_SERVICE_TIMEOUT
        )

        # 解析响应
        if response.status_code == 200:
            data = response.json()
            answer = data['choices'][0]['message']['content']
            return answer
        else:
            return f"AI服务出错（代码: {response.status_code}）"

    except Exception as e:
        print(f"AI服务调用异常: {e}")
        return "抱歉，AI服务暂时不可用。"
