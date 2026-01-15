# 工具函数
import requests
from server.config import AI_SERVICE_URL, AI_SERVICE_API_KEY, AI_SERVICE_TIMEOUT


def call_ai_service(question, context=""):
    # 调用AI接口
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
        return None
