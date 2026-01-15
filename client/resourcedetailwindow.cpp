#include "resourcedetailwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QUrl>
#include <QNetworkRequest>
#include <QApplication>

ResourceDetailWindow::ResourceDetailWindow(int resourceId, int userId, QWidget *parent)
    : QMainWindow(parent)
    , m_resourceId(resourceId)
    , m_userId(userId)
{
    setWindowTitle("资源详情 - 享阅");
    setMinimumSize(900, 700);
    
    setupUI();
    setupNetworkManager();
    activateWindow();
    raise();
}

void ResourceDetailWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    m_aiGroup = new QGroupBox("AI提问");
    QVBoxLayout *aiLayout = new QVBoxLayout();
    m_aiQuestionEdit = new QLineEdit();
    m_aiQuestionEdit->setPlaceholderText("输入问题...");
    m_aiAskBtn = new QPushButton("提问");
    m_aiAnswerDisplay = new QTextEdit();
    m_aiAnswerDisplay->setReadOnly(true);
    aiLayout->addWidget(m_aiQuestionEdit);
    aiLayout->addWidget(m_aiAskBtn);
    aiLayout->addWidget(m_aiAnswerDisplay);
    m_aiGroup->setLayout(aiLayout);
    layout->addWidget(m_aiGroup);
    connect(m_aiAskBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onAiAsk);
}

void ResourceDetailWindow::setupNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &ResourceDetailWindow::onNetworkReply);
}

void ResourceDetailWindow::onAiAsk()
{
    QString question = m_aiQuestionEdit->text();
    if (question.isEmpty()) {
        QMessageBox::warning(this, "错误", "请输入问题");
        return;
    }
    
    QJsonObject data;
    data["question"] = question;
    
    QString url = QString("http://localhost:5000/api/resources/%1/ai-ask").arg(m_resourceId);
    makeRequest(url, "POST", data);
}

QJsonObject ResourceDetailWindow::makeRequest(const QString &urlStr, const QString &method,
                                             const QJsonObject &jsonData)
{
    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());
    
    QNetworkReply *reply = nullptr;
    if (method == "GET") {
        reply = m_networkManager->get(request);
    } else if (method == "POST") {
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        QJsonDocument doc(jsonData);
        reply = m_networkManager->post(request, doc.toJson());
    }
    
    if (reply) {
        reply->setProperty("url", urlStr);
        reply->setProperty("method", method);
    }
    
    return QJsonObject();
}

void ResourceDetailWindow::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "错误", "网络请求失败: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString urlStr = reply->property("url").toString();
    QString method = reply->property("method").toString();
    
    if (statusCode < 200 || statusCode >= 300) {
        QMessageBox::warning(this, "错误", QString("服务器错误 %1").arg(statusCode));
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::warning(this, "错误", "响应格式错误");
        reply->deleteLater();
        return;
    }
    
    QJsonObject json = doc.object();
    
    if (urlStr.contains("/ai-ask") && json.contains("answer")) {
        m_aiAnswerDisplay->setPlainText(json["answer"].toString());
        m_aiQuestionEdit->clear();
    }
    
    reply->deleteLater();
}
