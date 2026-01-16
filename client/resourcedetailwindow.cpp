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
#include <QPdfPageNavigator>
#include <QInputDialog>
#include <QTreeWidget>
#include <QHeaderView>
#include <QDateTime>

ResourceDetailWindow::ResourceDetailWindow(int resourceId, int userId, const QString &userRole, const QString &viewMode, QWidget *parent)
    : QMainWindow(parent)
    , m_resourceId(resourceId)
    , m_userId(userId)
    , m_userRole(userRole)
    , m_viewMode(viewMode)
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

    // 举报按钮
    m_reportBtn = new QPushButton("举报资源");
    layout->addWidget(m_reportBtn);
    connect(m_reportBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onReportClicked);

    // 删除资源按钮（权限判定后再显示）
    m_deleteResourceBtn = new QPushButton("删除资源");
    m_deleteResourceBtn->setVisible(false);
    layout->addWidget(m_deleteResourceBtn);
    connect(m_deleteResourceBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onDeleteResourceClicked);
    
    // 评论区域
    m_commentGroup = new QGroupBox("评论");

    // 审核态精简视图：隐藏举报/评论/AI，保留下载/预览/删除
    bool isModeratorView = (m_viewMode == "moderator");
    if (isModeratorView) {
        m_reportBtn->setVisible(false);
    }

    QVBoxLayout *commentLayout = new QVBoxLayout();

    m_replyToLabel = new QLabel("当前回复对象：无");
    m_cancelReplyBtn = new QPushButton("取消回复");
    m_cancelReplyBtn->setEnabled(false);

    QHBoxLayout *replyBar = new QHBoxLayout();
    replyBar->addWidget(m_replyToLabel);
    replyBar->addStretch();
    replyBar->addWidget(m_cancelReplyBtn);
    commentLayout->addLayout(replyBar);

    m_commentsTree = new QTreeWidget();
    m_commentsTree->setHeaderHidden(true);
    m_commentsTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_commentsTree->setSelectionBehavior(QAbstractItemView::SelectItems);
    commentLayout->addWidget(m_commentsTree);

    m_commentEdit = new QTextEdit();
    m_commentEdit->setMaximumHeight(80);
    m_commentEdit->setPlaceholderText("输入评论...");

    QHBoxLayout *commentBtnBar = new QHBoxLayout();
    m_commentSubmitBtn = new QPushButton("发表评论");
    QPushButton *replyBtn = new QPushButton("回复选中评论");
    commentBtnBar->addWidget(replyBtn);
    commentBtnBar->addStretch();
    commentBtnBar->addWidget(m_commentSubmitBtn);

    commentLayout->addWidget(m_commentEdit);
    commentLayout->addLayout(commentBtnBar);

    m_commentGroup->setLayout(commentLayout);
    layout->addWidget(m_commentGroup);

    connect(m_commentSubmitBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onCommentSubmit);
    connect(m_cancelReplyBtn, &QPushButton::clicked, this, &ResourceDetailWindow::onCancelReply);
    connect(replyBtn, &QPushButton::clicked, this, [this]() {
        QTreeWidgetItem *item = m_commentsTree ? m_commentsTree->currentItem() : nullptr;
        if (!item) {
            QMessageBox::information(this, "提示", "请先在评论列表中选中要回复的评论");
            return;
        }
        int commentId = item->data(0, Qt::UserRole).toInt();
        QString author = item->data(0, Qt::UserRole + 1).toString();
        if (commentId <= 0) {
            QMessageBox::information(this, "提示", "该条目不可回复");
            return;
        }
        m_replyToCommentId = commentId;
        m_replyToLabel->setText(QString("当前回复对象：%1（评论ID：%2）").arg(author.isEmpty() ? "-" : author).arg(commentId));
        m_cancelReplyBtn->setEnabled(true);
    });
    
    // AI提问区域
    m_aiGroup = new QGroupBox("AI提问");
    if (isModeratorView) {
        m_commentGroup->setVisible(false);
    }

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

    if (isModeratorView) {
        m_aiGroup->setVisible(false);
    }

    updateUIForRoleAndOwnership();
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

void ResourceDetailWindow::onReportClicked()
{
    bool ok = false;
    QString reason = QInputDialog::getMultiLineText(this, "举报资源", "请输入举报原因（必填）：", "", &ok);
    if (!ok) return;

    reason = reason.trimmed();
    if (reason.isEmpty()) {
        QMessageBox::warning(this, "错误", "举报原因不能为空");
        return;
    }

    QJsonObject data;
    data["resource_id"] = m_resourceId;
    data["reason"] = reason;

    QNetworkRequest request{QUrl("http://localhost:5000/api/reports")};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonDocument doc(data);
    QNetworkReply *reply = m_networkManager->post(request, doc.toJson());
    reply->setProperty("requestType", "report");
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
        view->resize(900, 700);
        view->setPageMode(QPdfView::PageMode::MultiPage);
        view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
        view->setDocument(doc);

        int totalPages = doc->pageCount();
        int currentPage = 1;
        if (view->pageNavigator()) {
            currentPage = view->pageNavigator()->currentPage() + 1;
        }
        view->setWindowTitle(QString("PDF预览 - 第%1页/共%2页").arg(currentPage).arg(totalPages));

        if (view->pageNavigator()) {
            connect(view->pageNavigator(), &QPdfPageNavigator::currentPageChanged, view, [view, doc](int page) {
                int total = doc->pageCount();
                view->setWindowTitle(QString("PDF预览 - 第%1页/共%2页").arg(page + 1).arg(total));
            });
        }

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

void ResourceDetailWindow::onCancelReply()
{
    m_replyToCommentId = 0;
    if (m_replyToLabel) m_replyToLabel->setText("当前回复对象：无");
    if (m_cancelReplyBtn) m_cancelReplyBtn->setEnabled(false);
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
    if (m_replyToCommentId > 0) {
        data["parent_id"] = m_replyToCommentId;
    }
    
    QString url = QString("http://localhost:5000/api/resources/%1/comments").arg(m_resourceId);
    makeRequest(url, "POST", data);
}

void ResourceDetailWindow::updateUIForRoleAndOwnership()
{
    bool isModeratorView = (m_viewMode == "moderator");
    bool isModerator = (m_userRole == "moderator");

    bool canDelete = false;
    if (isModerator) {
        canDelete = true;
    } else if (m_uploaderId > 0 && m_uploaderId == m_userId) {
        canDelete = true;
    }

    if (m_deleteResourceBtn) {
        m_deleteResourceBtn->setVisible(canDelete);
        if (isModeratorView) {
            m_deleteResourceBtn->setVisible(true);
        }
    }
}

void ResourceDetailWindow::onDeleteResourceClicked()
{
    auto ret = QMessageBox::warning(this, "确认", "确定要删除该资源吗？此操作不可恢复。", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QString url = QString("http://localhost:5000/api/resources/%1").arg(m_resourceId);
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->deleteResource(request);
    reply->setProperty("requestType", "delete_resource");
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
    // 预览下载失败时，恢复按钮状态
    if (reply->property("requestType").toString() == "preview_download" && reply->error() != QNetworkReply::NoError) {
        m_previewPdfBtn->setEnabled(true);
        m_previewPdfBtn->setText("预览PDF");
    }

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "网络错误", "请求失败了，请检查网络连接: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    QString urlStr = reply->property("url").toString();
    QString method = reply->property("method").toString();

    if (statusCode >= 400) {
        // 预览下载失败时，恢复按钮状态
        if (reply->property("requestType").toString() == "preview_download") {
            m_previewPdfBtn->setEnabled(true);
            m_previewPdfBtn->setText("预览PDF");
        }

        QString errorMsg;
        switch (statusCode) {
            case 401:
                errorMsg = "您需要登录才能执行此操作，请重新登录。";
                break;
            case 403:
                errorMsg = "抱歉，您没有权限执行此操作。";
                break;
            case 404:
                errorMsg = "找不到请求的资源，它可能已被删除。";
                break;
            default:
                if (statusCode >= 500) {
                    errorMsg = QString("服务器开小差了（错误码: %1），请稍后再试。").arg(statusCode);
                } else {
                    errorMsg = QString("请求出错了（错误码: %1）。").arg(statusCode);
                }
                break;
        }
        QMessageBox::warning(this, "出错了", errorMsg);
        reply->deleteLater();
        return;
    }

    
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
                view->resize(900, 700);
                view->setPageMode(QPdfView::PageMode::MultiPage);
                view->setZoomMode(QPdfView::ZoomMode::FitToWidth);
                view->setDocument(doc);

                int totalPages = doc->pageCount();
                int currentPage = 1;
                if (view->pageNavigator()) {
                    currentPage = view->pageNavigator()->currentPage() + 1;
                }
                view->setWindowTitle(QString("PDF预览 - 第%1页/共%2页").arg(currentPage).arg(totalPages));

                if (view->pageNavigator()) {
                    connect(view->pageNavigator(), &QPdfPageNavigator::currentPageChanged, view, [view, doc](int page) {
                        int total = doc->pageCount();
                        view->setWindowTitle(QString("PDF预览 - 第%1页/共%2页").arg(page + 1).arg(total));
                    });
                }

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
    
    // 处理删除资源
    if (reply->property("requestType").toString() == "delete_resource") {
        QMessageBox::information(this, "成功", "资源已删除");
        emit resourceDeleted(m_resourceId);
        close();
        reply->deleteLater();
        return;
    }

    // 处理举报提交成功
    if (reply->property("requestType").toString() == "report") {
        QMessageBox::information(this, "成功", "举报已提交，感谢您的反馈！");
        reply->deleteLater();
        return;
    }
    
    // 处理评论提交成功
    if (statusCode == 201 && urlStr.contains("/comments")) {
        QMessageBox::information(this, "成功", "评论发表成功！");
        m_commentEdit->clear();
        onCancelReply();
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

        m_uploaderId = json["uploader_id"].toInt(0);
        updateUIForRoleAndOwnership();

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
        m_commentsTree->clear();

        std::function<void(const QJsonArray&, QTreeWidgetItem*)> buildTree = [&](const QJsonArray &arr, QTreeWidgetItem *parentItem) {
            for (const QJsonValue &v : arr) {
                QJsonObject obj = v.toObject();
                int cid = obj["id"].toInt();
                QString author = obj["author"].toString("匿名");
                QString content = obj["content"].toString().trimmed();
                QString createIso = obj["created_at"].toString();

                QDateTime dt = QDateTime::fromString(createIso, Qt::ISODate);
                QString timeStr = dt.isValid() ? dt.toString("yyyy-MM-dd hh:mm:ss") : createIso;

                QString text = QString("%1 %2\n%3").arg(author).arg(timeStr).arg(content);

                QTreeWidgetItem *item;
                if (parentItem) {
                    item = new QTreeWidgetItem(parentItem);
        } else {
                    item = new QTreeWidgetItem(m_commentsTree);
                }
                item->setText(0, text);
                item->setData(0, Qt::UserRole, cid);
                item->setData(0, Qt::UserRole + 1, author);

                QJsonArray replies = obj["replies"].toArray();
                if (!replies.isEmpty()) {
                    buildTree(replies, item);
            }
        }
        };

        if (!comments.isEmpty()) {
            buildTree(comments, nullptr);
            m_commentsTree->expandAll();
        } else {
            QTreeWidgetItem *emptyItem = new QTreeWidgetItem(m_commentsTree);
            emptyItem->setText(0, "暂无评论，快来抢沙发吧~");
            emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        }
    }
    
    // 处理AI回答
    if (urlStr.contains("/ai-ask") && json.contains("answer")) {
        m_aiAnswerDisplay->setPlainText(json["answer"].toString());
        m_aiQuestionEdit->clear();
    }
    
    reply->deleteLater();
}
