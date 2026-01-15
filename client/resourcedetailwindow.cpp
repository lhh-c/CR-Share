#include "resourcedetailwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QUrl>
#include <QNetworkRequest>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QScrollBar>
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
    loadResourceDetails();
    activateWindow();
    raise();
}

void ResourceDetailWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    m_resourceDetails = new QTextEdit();
    m_resourceDetails->setReadOnly(true);
    layout->addWidget(m_resourceDetails);
    
    m_downloadBtn = new QPushButton("下载资源");
    layout->addWidget(m_downloadBtn);
    connect(m_downloadBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onDownloadClicked);
    
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

void ResourceDetailWindow::loadResourceDetails()
{
    QString url = QString("http://localhost:5000/api/resources/%1").arg(m_resourceId);
    makeRequest(url, "GET");
}

void ResourceDetailWindow::onDownloadClicked()
{
    QString url = QString("http://localhost:5000/api/resources/%1/download").arg(m_resourceId);
    
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());
    
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "download");
    reply->setProperty("resourceId", m_resourceId);
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
    
    if (reply->property("requestType").toString() == "download") {
        QByteArray fileData = reply->readAll();
        QString suggestedFileName = QString("resource_%1.pdf").arg(m_resourceId);
        
        QString savePath = QFileDialog::getSaveFileName(
            this, "保存资源文件", QDir::homePath() + "/" + suggestedFileName,
            "PDF 文件 (*.pdf);;所有文件 (*.*)");
        
        if (!savePath.isEmpty()) {
            QFile file(savePath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(fileData);
                file.close();
                QMessageBox::information(this, "成功", "文件下载成功！");
            } else {
                QMessageBox::warning(this, "错误", "无法保存文件");
            }
        }
        reply->deleteLater();
        return;
    }
    
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
    
    if (urlStr.contains(QString("/api/resources/%1").arg(m_resourceId)) &&
        !urlStr.contains("/ai-ask") && !urlStr.contains("/download")) {
        QString details = QString("标题: %1\n\n描述: %2\n\n上传者: %3\n\n"
                                 "浏览量: %4\n下载量: %5")
                         .arg(json["title"].toString())
                         .arg(json["description"].toString())
                         .arg(json["uploader"].toString())
                         .arg(json["view_count"].toInt())
                         .arg(json["download_count"].toInt());
        m_resourceDetails->setPlainText(details);
    }
    
    if (urlStr.contains("/ai-ask") && json.contains("answer")) {
        m_aiAnswerDisplay->setPlainText(json["answer"].toString());
        m_aiQuestionEdit->clear();
    }
    
    reply->deleteLater();
}
