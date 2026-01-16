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
#include <QPdfDocument>
#include <QPdfView>

ResourceDetailWindow::ResourceDetailWindow(int resourceId, int userId, QWidget *parent)
    : QMainWindow(parent)
    , m_resourceId(resourceId)
    , m_userId(userId)
{
    m_fileType = "";
    m_lastPdfPath = "";
    setWindowTitle("资源详情 - 享阅");
    setMinimumSize(900, 700);
    
    setupUI();
    setupNetworkManager();
    loadResourceDetails();
    loadComments();
    activateWindow();
    raise();
}

void ResourceDetailWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout *layout = new QVBoxLayout(centralWidget);
    
    // 资源详情
    m_resourceDetails = new QTextEdit();
    m_resourceDetails->setReadOnly(true);
    layout->addWidget(m_resourceDetails);
    
    // 下载按钮
    m_downloadBtn = new QPushButton("下载资源");
    layout->addWidget(m_downloadBtn);
    connect(m_downloadBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onDownloadClicked);

    // 预览按钮（先默认禁用，拿到资源详情后再决定能不能用）
    m_previewPdfBtn = new QPushButton("预览PDF");
    m_previewPdfBtn->setEnabled(false);
    layout->addWidget(m_previewPdfBtn);
    connect(m_previewPdfBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onPreviewPdfClicked);
    
    // 评论区域
    m_commentGroup = new QGroupBox("评论");
    QVBoxLayout *commentLayout = new QVBoxLayout();
    m_commentsDisplay = new QTextEdit();
    m_commentsDisplay->setReadOnly(true);
    m_commentEdit = new QTextEdit();
    m_commentEdit->setMaximumHeight(80);
    m_commentEdit->setPlaceholderText("输入评论...");
    m_commentSubmitBtn = new QPushButton("发表评论");
    commentLayout->addWidget(m_commentsDisplay);
    commentLayout->addWidget(m_commentEdit);
    commentLayout->addWidget(m_commentSubmitBtn);
    m_commentGroup->setLayout(commentLayout);
    layout->addWidget(m_commentGroup);
    connect(m_commentSubmitBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onCommentSubmit);
    
    // AI提问区域
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

void ResourceDetailWindow::loadComments()
{
    QString url = QString("http://localhost:5000/api/resources/%1/comments").arg(m_resourceId);
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

void ResourceDetailWindow::onPreviewPdfClicked()
{
    // 只支持 PDF
    if (m_fileType.toLower() != "pdf") {
        QMessageBox::information(this, "提示", "这个资源不是PDF，暂时不支持预览");
        return;
    }

    // 如果之前下载过并保存了路径，直接打开
    if (!m_lastPdfPath.isEmpty() && QFile::exists(m_lastPdfPath)) {
        QPdfDocument *doc = new QPdfDocument(this);
        if (doc->load(m_lastPdfPath) != QPdfDocument::Error::None) {
            QMessageBox::warning(this, "错误", "PDF打开失败");
            return;
        }

        QPdfView *view = new QPdfView();
        view->setWindowTitle("PDF预览");
        view->resize(900, 700);
        view->setDocument(doc);
        view->show();
        return;
    }

    // 没有文件：直接请求下载到临时目录，然后预览（类似在线预览）
    QString url = QString("http://localhost:5000/api/resources/%1/download").arg(m_resourceId);

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "preview_download");
    reply->setProperty("resourceId", m_resourceId);

    m_previewPdfBtn->setEnabled(false);
    m_previewPdfBtn->setText("正在加载...");
}

void ResourceDetailWindow::onCommentSubmit()
{
    QString content = m_commentEdit->toPlainText().trimmed();
    if (content.isEmpty()) {
        QMessageBox::warning(this, "错误", "评论内容不能为空");
        return;
    }
    
    QJsonObject data;
    data["content"] = content;
    
    QString url = QString("http://localhost:5000/api/resources/%1/comments").arg(m_resourceId);
    makeRequest(url, "POST", data);
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
    
    // 处理文件下载
    if (reply->property("requestType").toString() == "download" || reply->property("requestType").toString() == "preview_download") {
        QByteArray fileData = reply->readAll();
        QString suggestedFileName = QString("resource_%1.pdf").arg(m_resourceId);
        
        if (reply->property("requestType").toString() == "preview_download") {
            QString tmpPath = QDir::tempPath() + QString("/xiangyue_preview_%1.pdf").arg(m_resourceId);
            QFile file(tmpPath);
            if (file.open(QIODevice::WriteOnly)) {
                file.write(fileData);
                file.close();

                m_lastPdfPath = tmpPath;
                m_previewPdfBtn->setEnabled(true);
                m_previewPdfBtn->setText("预览PDF");

                QPdfDocument *doc = new QPdfDocument(this);
                if (doc->load(m_lastPdfPath) != QPdfDocument::Error::None) {
                    QMessageBox::warning(this, "错误", "PDF打开失败");
                    reply->deleteLater();
                    return;
                }

                QPdfView *view = new QPdfView();
                view->setWindowTitle("PDF预览");
                view->resize(900, 700);
                view->setDocument(doc);
                view->show();
            } else {
                QMessageBox::warning(this, "错误", "无法缓存PDF用于预览");
                m_previewPdfBtn->setEnabled(true);
                m_previewPdfBtn->setText("预览PDF");
            }
        } else {
            QString savePath = QFileDialog::getSaveFileName(
                this, "保存资源文件", QDir::homePath() + "/" + suggestedFileName,
                "PDF 文件 (*.pdf);;所有文件 (*.*)");

            if (!savePath.isEmpty()) {
                QFile file(savePath);
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(fileData);
                    file.close();
                    QMessageBox::information(this, "成功", "文件下载成功！");

                    if (savePath.toLower().endsWith(".pdf")) {
                        m_lastPdfPath = savePath;
                        m_previewPdfBtn->setEnabled(true);
                    }
                } else {
                    QMessageBox::warning(this, "错误", "无法保存文件");
                }
            }
        }

        reply->deleteLater();
        return;
    }
    
    // 处理评论提交成功
    if (statusCode == 201 && urlStr.contains("/comments")) {
        QMessageBox::information(this, "成功", "评论发表成功！");
        m_commentEdit->clear();
        loadComments();
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
    
    // 处理资源详情
    if (urlStr.contains(QString("/api/resources/%1").arg(m_resourceId)) &&
        !urlStr.contains("/comments") && !urlStr.contains("/ai-ask") && !urlStr.contains("/download")) {
        QStringList tagList;
        QJsonArray tagsArray = json["tags"].toArray();
        for (const QJsonValue &v : tagsArray) {
            tagList << v.toString();
        }
        
        QString details = QString("标题: %1\n\n描述: %2\n\n上传者: %3\n\n"
                                 "浏览量: %4\n下载量: %5\n\n标签: %6")
                         .arg(json["title"].toString())
                         .arg(json["description"].toString())
                         .arg(json["uploader"].toString())
                         .arg(json["view_count"].toInt())
                         .arg(json["download_count"].toInt())
                         .arg(tagList.join(", "));
        m_resourceDetails->setPlainText(details);

        m_fileType = json["file_type"].toString();
        if (m_previewPdfBtn) {
            if (m_fileType.toLower() == "pdf") {
                m_previewPdfBtn->setEnabled(true);
            } else {
                m_previewPdfBtn->setEnabled(false);
            }
        }
    }
    
    // 处理评论列表
    if (urlStr.contains("/comments") && json.contains("comments")) {
        QJsonArray comments = json["comments"].toArray();
        QString commentsText;

        if (comments.isEmpty()) {
            commentsText = "暂无评论，快来抢沙发吧~";
        } else {
            for (const QJsonValue &val : comments) {
                QJsonObject c = val.toObject();
                QString author = c["author"].toString("匿名");
                QString rawTime = c["created_at"].toString("未知时间");

                // 格式化时间：从 ISO 格式提取年-月-日 时:分:秒
                QString formattedTime = rawTime;
                if (rawTime.contains("T")) {
                    QString datePart = rawTime.split("T").first();           // 2026-01-13
                    QString timePart = rawTime.split("T").last().left(8);    // 07:30:53
                    formattedTime = QString("（%1 %2）").arg(datePart).arg(timePart);
                }

                QString content = c["content"].toString().trimmed();

                // 修改后的拼接：第一行是 “用户名 （年-月-日 时:分:秒）”
                commentsText += QString("%1 %2\n%3\n────────────────────\n\n")
                                    .arg(author)
                                    .arg(formattedTime)
                                    .arg(content);
            }
        }

        m_commentsDisplay->setPlainText(commentsText);

        // 滚动到底部
        QTimer::singleShot(150, this, [this]() {
            QScrollBar *scroll = m_commentsDisplay->verticalScrollBar();
            if (scroll) {
                scroll->setValue(scroll->maximum());
            }
            m_commentsDisplay->ensureCursorVisible();
            m_commentsDisplay->repaint();
            QApplication::processEvents();
        });
    }
    
    // 处理AI回答
    if (urlStr.contains("/ai-ask") && json.contains("answer")) {
        m_aiAnswerDisplay->setPlainText(json["answer"].toString());
        m_aiQuestionEdit->clear();
    }
    
    reply->deleteLater();
}
