#include "mainwindow.h"
#include "searchresultwindow.h"
#include "resourcedetailwindow.h"
#include "reportmanagementwindow.h"
#include "notificationwindow.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QFileInfo>
#include <QTimer>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGroupBox>
#include <QApplication>
#include <QScrollBar>
#include <QDialog>
#include <QDialogButtonBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_userId(0),
    m_userRole("student"),
    m_uploadDialog(nullptr),
    m_subscriptionsDialog(nullptr)
{
    setupUI();
    setupNetworkManager();
    loadResources();
    loadTags();
}

void MainWindow::setUserId(int userId)
{
    m_userId = userId;

    // 加载个人信息
    QNetworkRequest request{QUrl("http://localhost:5000/api/users/me")};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());
    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "my_profile");
}

void MainWindow::setUserRole(const QString &role)
{
    m_userRole = role;

    // 显示/隐藏举报管理入口（仅审核员）
    auto reportManageBtn = findChild<QPushButton*>("reportManageBtn");
    if (reportManageBtn) {
        reportManageBtn->setVisible(m_userRole == "moderator");
    }

    if (m_userRole == "moderator") {
        m_isModeratorMode = true;
        m_modeToggleBtn->setVisible(true);
        m_modeToggleBtn->setText("切换到用户");

        m_reviewGroup->setVisible(true);
        m_recommendationGroup->setVisible(false);

        // 默认待审核
        m_reviewStatusFilter->setCurrentIndex(0);
        loadPendingResources();
    } else {
        m_isModeratorMode = false;
        m_modeToggleBtn->setVisible(false);

        m_reviewGroup->setVisible(false);
        m_recommendationGroup->setVisible(true);
    }
}

void MainWindow::setupUI()
{
    setWindowTitle("享阅 - 高校学习资源共享平台");
    setMinimumSize(1000, 700);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 推荐资源列表
    // 用户态视图（推荐资源）
    m_recommendationGroup = new QGroupBox();
    QVBoxLayout *recLayout = new QVBoxLayout(m_recommendationGroup);

    // 搜索栏（只给用户态用，审核态不需要）
    QHBoxLayout *searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索资源...");
    m_tagFilter = new QComboBox();
    m_tagFilter->addItem("全部标签", "");
    m_searchBtn = new QPushButton("搜索");
    m_refreshBtn = new QPushButton("刷新");
    searchLayout->addWidget(m_searchEdit);
    searchLayout->addWidget(m_tagFilter);
    searchLayout->addWidget(m_searchBtn);
    searchLayout->addWidget(m_refreshBtn);
    recLayout->addLayout(searchLayout);

    QLabel *recommendedLabel = new QLabel("推荐资源");
    recLayout->addWidget(recommendedLabel);

    m_recommendedResourcesList = new QListWidget();
    recLayout->addWidget(m_recommendedResourcesList);

    QHBoxLayout *mainPagerLayout = new QHBoxLayout();
    m_mainPrevBtn = new QPushButton("上一页");
    m_mainNextBtn = new QPushButton("下一页");
    m_mainPageLabel = new QLabel();
    m_mainPageLabel->setAlignment(Qt::AlignCenter);
    mainPagerLayout->addWidget(m_mainPrevBtn);
    mainPagerLayout->addWidget(m_mainPageLabel, 1);
    mainPagerLayout->addWidget(m_mainNextBtn);
    recLayout->addLayout(mainPagerLayout);

    mainLayout->addWidget(m_recommendationGroup);

    // 审核态视图（资源审核）
    m_reviewGroup = new QGroupBox("资源审核");
    QVBoxLayout *reviewLayout = new QVBoxLayout(m_reviewGroup);

    m_reviewStatusFilter = new QComboBox();
    m_reviewStatusFilter->addItem("待审核", "pending");
    m_reviewStatusFilter->addItem("已通过", "approved");
    m_reviewStatusFilter->addItem("已拒绝", "rejected");
    reviewLayout->addWidget(m_reviewStatusFilter);

    m_pendingResourcesList = new QListWidget();
    reviewLayout->addWidget(m_pendingResourcesList);

    QHBoxLayout *reviewPagerLayout = new QHBoxLayout();
    m_reviewPrevBtn = new QPushButton("上一页");
    m_reviewNextBtn = new QPushButton("下一页");
    m_reviewPageLabel = new QLabel();
    m_reviewPageLabel->setAlignment(Qt::AlignCenter);
    reviewPagerLayout->addWidget(m_reviewPrevBtn);
    reviewPagerLayout->addWidget(m_reviewPageLabel, 1);
    reviewPagerLayout->addWidget(m_reviewNextBtn);
    reviewLayout->addLayout(reviewPagerLayout);

    QHBoxLayout *reviewBtnLayout = new QHBoxLayout();
    m_reviewRefreshBtn = new QPushButton("刷新");
    m_approveBtn = new QPushButton("通过");
    m_rejectBtn = new QPushButton("拒绝");

    QPushButton *reportManageBtn = new QPushButton("举报管理");
    reportManageBtn->setObjectName("reportManageBtn");
    reportManageBtn->setVisible(false);

    reviewBtnLayout->addWidget(reportManageBtn);
    reviewBtnLayout->addStretch();
    reviewBtnLayout->addWidget(m_reviewRefreshBtn);
    reviewBtnLayout->addWidget(m_approveBtn);
    reviewBtnLayout->addWidget(m_rejectBtn);
    reviewLayout->addLayout(reviewBtnLayout);

    connect(reportManageBtn, &QPushButton::clicked, this, [this]() {
        ReportManagementWindow *w = new ReportManagementWindow(m_userId, this);
        w->setAttribute(Qt::WA_DeleteOnClose);
        w->show();
    });

    connect(m_reviewPrevBtn, &QPushButton::clicked, this, [this]() {
        if (m_reviewPage > 1) {
            m_reviewPage -= 1;
            loadPendingResources();
        }
    });

    connect(m_reviewNextBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = (m_reviewTotal + m_reviewPageSize - 1) / m_reviewPageSize;
        if (totalPages <= 0) totalPages = 1;
        if (m_reviewPage < totalPages) {
            m_reviewPage += 1;
            loadPendingResources();
        }
    });

    connect(m_reviewRefreshBtn, &QPushButton::clicked, this, [this]() {
        m_reviewPage = 1;
        loadPendingResources();
    });

    mainLayout->addWidget(m_reviewGroup);

    connect(m_mainPrevBtn, &QPushButton::clicked, this, [this]() {
        if (m_mainPage > 1) {
            m_mainPage -= 1;
            loadResources();
        }
    });

    connect(m_mainNextBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = (m_mainTotal + m_mainPageSize - 1) / m_mainPageSize;
        if (totalPages <= 0) totalPages = 1;
        if (m_mainPage < totalPages) {
            m_mainPage += 1;
            loadResources();
        }
    });

    // 我的信息（所有角色都显示）
    QGroupBox *profileGroup = new QGroupBox("我的信息");
    QVBoxLayout *profileLayout = new QVBoxLayout(profileGroup);

    m_profileUsernameLabel = new QLabel("用户名：-");
    m_profileEmailLabel = new QLabel("邮箱：-");
    m_profileRoleLabel = new QLabel("角色：-");

    profileLayout->addWidget(m_profileUsernameLabel);
    profileLayout->addWidget(m_profileEmailLabel);
    profileLayout->addWidget(m_profileRoleLabel);

    QHBoxLayout *accountBtnLayout = new QHBoxLayout();
    m_logoutBtn = new QPushButton("退出登录");
    m_deleteAccountBtn = new QPushButton("注销账号");
    m_notificationsBtn = new QPushButton("通知");
    accountBtnLayout->addWidget(m_notificationsBtn);
    accountBtnLayout->addWidget(m_logoutBtn);
    accountBtnLayout->addWidget(m_deleteAccountBtn);
    accountBtnLayout->addStretch();
    profileLayout->addLayout(accountBtnLayout);

    mainLayout->addWidget(profileGroup);

    // 功能按钮区域（用户态）
    QHBoxLayout *functionLayout = new QHBoxLayout();
    m_uploadBtn = new QPushButton("上传资源");
    m_subscriptionsBtn = new QPushButton("我的订阅");
    functionLayout->addWidget(m_uploadBtn);
    functionLayout->addWidget(m_subscriptionsBtn);
    functionLayout->addStretch();
    recLayout->addLayout(functionLayout);

    connect(m_notificationsBtn, &QPushButton::clicked, this, &MainWindow::onNotificationsClicked);
    connect(m_logoutBtn, &QPushButton::clicked, this, &MainWindow::onLogoutBtnClicked);
    connect(m_deleteAccountBtn, &QPushButton::clicked, this, &MainWindow::onDeleteAccountBtnClicked);

    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_recommendedResourcesList, &QListWidget::itemClicked, this, &MainWindow::onResourceSelected);
    connect(m_uploadBtn, &QPushButton::clicked, this, &MainWindow::onUploadClicked);
    connect(m_subscriptionsBtn, &QPushButton::clicked, this, &MainWindow::onSubscriptionsClicked);
    connect(m_reviewStatusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_reviewPage = 1;
        loadPendingResources();
    });

    connect(m_pendingResourcesList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        int resId = item->data(Qt::UserRole).toInt();
        if (resId <= 0) return;
        ResourceDetailWindow *detailWindow = new ResourceDetailWindow(resId, m_userId, m_userRole, "moderator", this);
        connect(detailWindow, &ResourceDetailWindow::resourceDeleted, this, [this](int) {
            loadPendingResources();
        });
        detailWindow->show();
    });

    connect(m_approveBtn, &QPushButton::clicked, this, &MainWindow::onReviewResource);
    connect(m_rejectBtn, &QPushButton::clicked, this, &MainWindow::onReviewResource);

    // 左下角切换按钮
    m_modeToggleBtn = new QPushButton("切换到审核");
    m_modeToggleBtn->setVisible(false);
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(m_modeToggleBtn);
    bottomLayout->addStretch();
    mainLayout->addLayout(bottomLayout);

    connect(m_modeToggleBtn, &QPushButton::clicked, this, [this]() {
        if (m_userRole != "moderator") return;

        m_isModeratorMode = !m_isModeratorMode;

        if (m_isModeratorMode) {
            m_modeToggleBtn->setText("切换到用户");
            m_reviewGroup->setVisible(true);
            m_recommendationGroup->setVisible(false);
            m_reviewStatusFilter->setCurrentIndex(0);
            loadPendingResources();
        } else {
            m_modeToggleBtn->setText("切换到审核");
            m_reviewGroup->setVisible(false);
            m_recommendationGroup->setVisible(true);
        }
    });

    // 默认先显示用户态
    m_reviewGroup->setVisible(false);
    m_recommendationGroup->setVisible(true);

    m_uploadDialog = nullptr;
    m_subscriptionsDialog = nullptr;
}

void MainWindow::setupNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onNetworkReply);
}

QJsonObject MainWindow::makeRequest(const QString &urlStr, const QString &method,
                                    const QJsonObject &jsonData, const QString &filePath)
{
    QUrl url(urlStr);
    QNetworkRequest request(url);
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = nullptr;

    if (method == "GET") {
        reply = m_networkManager->get(request);
    } else if (method == "POST") {
        if (!filePath.isEmpty()) {
            QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

            QHttpPart titlePart;
            titlePart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"title\""));
            titlePart.setBody(jsonData["title"].toString().toUtf8());
            multiPart->append(titlePart);

            QHttpPart descPart;
            descPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"description\""));
            descPart.setBody(jsonData["description"].toString().toUtf8());
            multiPart->append(descPart);

            QHttpPart tagsPart;
            tagsPart.setHeader(QNetworkRequest::ContentDispositionHeader, QVariant("form-data; name=\"tags\""));
            QString tagsStr;
            QJsonArray tagsArr = jsonData["tags"].toArray();
            for (int i = 0; i < tagsArr.size(); ++i) {
                if (i > 0) tagsStr += ",";
                tagsStr += tagsArr[i].toString().trimmed();
            }
            tagsPart.setBody(tagsStr.toUtf8());
            multiPart->append(tagsPart);

            QFile *file = new QFile(filePath);
            if (!file->open(QIODevice::ReadOnly)) {
                showError("无法打开文件: " + file->errorString());
                delete file;
                delete multiPart;
                return QJsonObject();
            }
            QFileInfo fi(filePath);
            QHttpPart filePart;
            filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                               QVariant(QString("form-data; name=\"file\"; filename=\"%1\"").arg(fi.fileName())));
            filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("application/octet-stream"));
            filePart.setBodyDevice(file);
            file->setParent(multiPart);
            multiPart->append(filePart);

            reply = m_networkManager->post(request, multiPart);
            multiPart->setParent(reply);
        } else {
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
            QJsonDocument doc(jsonData);
            reply = m_networkManager->post(request, doc.toJson());
        }
    } else if (method == "DELETE") {
        reply = m_networkManager->deleteResource(request);
    }

    if (reply) {
        reply->setProperty("url", urlStr);
        reply->setProperty("method", method);
        reply->setProperty("isUpload", !filePath.isEmpty());
    }
    return QJsonObject();
}

void MainWindow::onNetworkReply(QNetworkReply *reply)
{

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (statusCode == 401) {
        reply->deleteLater();
        return;
    }

    if (statusCode == 400) {
        QByteArray errData = reply->readAll();
        QString friendlyMsg = "操作失败（400）";
        QJsonDocument errDoc = QJsonDocument::fromJson(errData);
        if (!errDoc.isNull() && errDoc.isObject()) {
            QString errorText = errDoc.object()["error"].toString().trimmed();
            if (errorText.contains("已订阅") ||
                errorText.contains("已经订阅") ||
                errorText.contains("重复") ||
                errorText.contains("duplicate") ||
                errorText.contains("exists") ||
                errorText.contains("订阅过")) {
                friendlyMsg = "您已经订阅过这个标签了";
            } else {
                friendlyMsg = "订阅失败：" + errorText;
            }
        } else {
            friendlyMsg = "订阅失败：服务器拒绝了请求";
        }
        showError(friendlyMsg);
        reply->deleteLater();
        return;
    }

    if (statusCode < 200 || statusCode >= 300) {
        showError(QString("服务器错误 %1").arg(statusCode));
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString errMsg = QString("网络错误: %1").arg(reply->errorString());
        showError(errMsg);
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QString urlStr = reply->url().toString();
    QString requestType = reply->property("requestType").toString();

    QJsonDocument doc = QJsonDocument::fromJson(responseData);
    if (doc.isNull() || !doc.isObject()) {
        showError("服务器响应格式错误（非有效 JSON）");
        reply->deleteLater();
        return;
    }
    QJsonObject json = doc.object();

    if (urlStr.contains("/api/tags")) {
        QJsonArray tagsArray = json["tags"].toArray();
        qDebug() << "获取 tagsArray 成功，大小:" << tagsArray.size();

        if (m_tagFilter) {
            qDebug() << "m_tagFilter 不为空，开始 clear";
            m_tagFilter->clear();
            qDebug() << "m_tagFilter clear 完成";
            m_tagFilter->addItem("全部标签", "");
            qDebug() << "添加 '全部标签' 项完成";

            int index = 0;
            for (const QJsonValue &val : tagsArray) {
                qDebug() << "处理第" << index << "个 tag 值:" << val.toVariant();
                if (!val.isObject()) {
                    qDebug() << "警告: 第" << index << "个 val 不是 JSON 对象，跳过";
                    continue;
                }
                QJsonObject tag = val.toObject();
                qDebug() << "转换 tag 为对象完成";
                QString name = tag.value("name").toString();
                int id = tag.value("id").toInt();
                qDebug() << "提取 name:" << name << " id:" << id;
                if (name.isEmpty()) {
                    qDebug() << "警告: name 为空，跳过添加";
                    continue;
                }
                m_tagFilter->addItem(name, id);
                qDebug() << "成功添加第" << index << "个 tag 到 m_tagFilter";
                index++;
            }
        } else {
            qDebug() << "警告: m_tagFilter 是 nullptr，未处理下拉框";
        }

        qDebug() << "标签加载完成，总数:" << tagsArray.size();
    }
    else if (reply->property("requestType").toString() == "subscription_resources") {
        m_subPage = json["page"].toInt(1);
        m_subPageSize = json["page_size"].toInt(10);
        m_subTotal = json["total"].toInt(0);

        QJsonArray resources = json["resources"].toArray();
        qDebug() << "订阅资源加载完成，数量:" << resources.size() << "总数:" << m_subTotal;
        
        m_subscribedResourcesList->clear();
        for (const QJsonValue &val : resources) {
            QJsonObject res = val.toObject();
            QString title = res["title"].toString();
            int resId = res["id"].toInt();
            QListWidgetItem *item = new QListWidgetItem(title);
            item->setData(Qt::UserRole, resId);
            m_subscribedResourcesList->addItem(item);
        }
        if (resources.isEmpty()) {
            QListWidgetItem *emptyItem = new QListWidgetItem("暂无订阅资源");
            emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
            m_subscribedResourcesList->addItem(emptyItem);
        }

        // 更新分页UI
        int totalPages = (m_subTotal + m_subPageSize - 1) / m_subPageSize;
        if (totalPages <= 0) totalPages = 1;

        m_subPageLabel->setText(QString("第 %1/%2 页").arg(m_subPage).arg(totalPages));
        m_subPrevBtn->setEnabled(m_subPage > 1);
        m_subNextBtn->setEnabled(m_subPage < totalPages);
    }
    else if (reply->property("requestType").toString() == "review_resources") {
        m_reviewPage = json["page"].toInt(1);
        m_reviewPageSize = json["page_size"].toInt(20);
        m_reviewTotal = json["total"].toInt(0);

        QJsonArray resources = json["resources"].toArray();
        qDebug() << "=== 处理审核资源列表 ===";
        qDebug() << "数量:" << resources.size() << "总数:" << m_reviewTotal;

        if (m_pendingResourcesList) {
            m_pendingResourcesList->clear();

            if (resources.isEmpty()) {
                QString statusText = m_reviewStatusFilter ? m_reviewStatusFilter->currentText() : "待审核";
                QListWidgetItem *emptyItem = new QListWidgetItem(QString("暂无%1资源").arg(statusText));
                emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
                m_pendingResourcesList->addItem(emptyItem);
            } else {
                for (const QJsonValue &val : resources) {
                    QJsonObject res = val.toObject();
                    QString title = res["title"].toString();
                    int id = res["id"].toInt();
                    QListWidgetItem *item = new QListWidgetItem(title);
                    item->setData(Qt::UserRole, id);
                    m_pendingResourcesList->addItem(item);
                }
            }

            // 更新分页UI
            int totalPages = (m_reviewTotal + m_reviewPageSize - 1) / m_reviewPageSize;
            if (totalPages <= 0) totalPages = 1;
            m_reviewPageLabel->setText(QString("第 %1/%2 页").arg(m_reviewPage).arg(totalPages));
            m_reviewPrevBtn->setEnabled(m_reviewPage > 1);
            m_reviewNextBtn->setEnabled(m_reviewPage < totalPages);

            m_pendingResourcesList->update();
            m_pendingResourcesList->repaint();
        } else {
            qDebug() << "m_pendingResourcesList nullptr";
        }
    }
    else if (reply->property("requestType").toString() == "my_profile") {
        QJsonObject u = json["user"].toObject();
        QString username = u["username"].toString();
        QString email = u["email"].toString();
        QString role = u["role"].toString();
        if (m_profileUsernameLabel) m_profileUsernameLabel->setText(QString("用户名：%1").arg(username.isEmpty() ? "-" : username));
        if (m_profileEmailLabel) m_profileEmailLabel->setText(QString("邮箱：%1").arg(email.isEmpty() ? "-" : email));
        if (m_profileRoleLabel) m_profileRoleLabel->setText(QString("角色：%1").arg(role.isEmpty() ? "-" : role));
    }
    else if (reply->property("requestType").toString() == "main_resources") {
        m_mainPage = json["page"].toInt(1);
        m_mainPageSize = json["page_size"].toInt(20);
        m_mainTotal = json["total"].toInt(0);
        QJsonArray resources = json["resources"].toArray();
        
        qDebug() << "主界面推荐资源加载完成，数量:" << resources.size() << "总数:" << m_mainTotal;

        m_recommendedResourcesList->clear();
        for (const QJsonValue &val : resources) {
            QJsonObject res = val.toObject();
            QString title = res["title"].toString();
            int id = res["id"].toInt();
            QListWidgetItem *item = new QListWidgetItem(title);
            item->setData(Qt::UserRole, id);
            m_recommendedResourcesList->addItem(item);
        }
        
        if (resources.isEmpty()) {
            QListWidgetItem *emptyItem = new QListWidgetItem("暂无推荐资源");
            emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
            m_recommendedResourcesList->addItem(emptyItem);
        }

        // 更新分页UI
        int totalPages = (m_mainTotal + m_mainPageSize - 1) / m_mainPageSize;
        if (totalPages <= 0) totalPages = 1;

        m_mainPageLabel->setText(QString("第 %1/%2 页").arg(m_mainPage).arg(totalPages));
        m_mainPrevBtn->setEnabled(m_mainPage > 1);
        m_mainNextBtn->setEnabled(m_mainPage < totalPages);
    }
    else if (reply->property("requestType").toString() == "logout") {
        showSuccess("已退出登录");
        emit logoutRequested();
        close();
    }
    else if (reply->property("requestType").toString() == "delete_account") {
        showSuccess("账号已注销");
        emit logoutRequested();
        close();
    }
    else if (statusCode == 201 && urlStr.contains("/api/resources") && !urlStr.contains("/api/resources/")) {
        showSuccess("资源上传成功！");
        loadResources();
    }

    reply->deleteLater();
}

void MainWindow::onNotificationsClicked()
{
    NotificationWindow *w = new NotificationWindow(m_userId, this);
    w->setAttribute(Qt::WA_DeleteOnClose);
    connect(w, &NotificationWindow::openResourceRequested, this, [this](int resourceId) {
        if (resourceId <= 0) return;
        ResourceDetailWindow *detailWindow = new ResourceDetailWindow(resourceId, m_userId, m_userRole, "user", this);
        connect(detailWindow, &ResourceDetailWindow::resourceDeleted, this, [this](int) {
            loadResources();
        });
        detailWindow->show();
    });
    w->show();
}

void MainWindow::onLogoutBtnClicked()
{
    auto ret = QMessageBox::question(this, "确认", "确定要退出登录吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QNetworkRequest request{QUrl("http://localhost:5000/api/auth/logout")};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());
    QNetworkReply *reply = m_networkManager->post(request, QByteArray());
    reply->setProperty("requestType", "logout");
}

void MainWindow::onDeleteAccountBtnClicked()
{
    auto ret = QMessageBox::warning(this, "危险操作", "确定要注销账号吗？此操作不可恢复。", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QNetworkRequest request{QUrl("http://localhost:5000/api/users/me")};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());
    QNetworkReply *reply = m_networkManager->deleteResource(request);
    reply->setProperty("requestType", "delete_account");
}

void MainWindow::onSearchClicked()
{
    QString keyword = m_searchEdit->text();
    QString tag = m_tagFilter->currentData().toString();

    SearchResultWindow *searchWindow = new SearchResultWindow(keyword, tag, m_userId, m_userRole, this);
    searchWindow->show();
}

void MainWindow::onRefreshClicked()
{
    m_mainPage = 1;
    loadResources();
}

void MainWindow::onResourceSelected(QListWidgetItem *item)
{
    if (!item) return;
    int resourceId = item->data(Qt::UserRole).toInt();
    if (resourceId <= 0) return;

    ResourceDetailWindow *detailWindow = new ResourceDetailWindow(resourceId, m_userId, m_userRole, "user", this);
    connect(detailWindow, &ResourceDetailWindow::resourceDeleted, this, [this](int) {
        loadResources();
    });
    detailWindow->show();
}

void MainWindow::onUploadClicked()
{
    if (!m_uploadDialog) {
        setupUploadDialog();
    }
    m_uploadDialog->exec();
}

void MainWindow::setupUploadDialog()
{
    m_uploadDialog = new QDialog(this);
    m_uploadDialog->setWindowTitle("上传资源");
    m_uploadDialog->setMinimumSize(500, 400);

    QVBoxLayout *layout = new QVBoxLayout(m_uploadDialog);

    m_uploadTitleEdit = new QLineEdit();
    m_uploadTitleEdit->setPlaceholderText("资源标题 *");
    layout->addWidget(m_uploadTitleEdit);

    QLabel *descLabel = new QLabel("资源描述:");
    layout->addWidget(descLabel);
    m_uploadDescriptionEdit = new QTextEdit();
    m_uploadDescriptionEdit->setMaximumHeight(100);
    layout->addWidget(m_uploadDescriptionEdit);

    m_uploadTagsEdit = new QLineEdit();
    m_uploadTagsEdit->setPlaceholderText("标签（用逗号分隔）");
    layout->addWidget(m_uploadTagsEdit);

    m_selectFileBtn = new QPushButton("选择文件");
    m_selectedFileLabel = new QLabel("未选择文件");
    layout->addWidget(m_selectFileBtn);
    layout->addWidget(m_selectedFileLabel);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    m_uploadSubmitBtn = buttonBox->button(QDialogButtonBox::Ok);
    m_uploadSubmitBtn->setText("上传");
    layout->addWidget(buttonBox);

    connect(m_selectFileBtn, &QPushButton::clicked, this, [this]() {
        QString filePath = QFileDialog::getOpenFileName(m_uploadDialog, "选择文件", "",
                                                        "所有文件 (*.*);;PDF文件 (*.pdf);;Word文档 (*.doc *.docx);;PPT (*.ppt *.pptx)");
        if (!filePath.isEmpty()) {
            m_selectedFilePath = filePath;
            m_selectedFileLabel->setText(QFileInfo(filePath).fileName());
        }
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, &MainWindow::onUploadSubmit);
    connect(buttonBox, &QDialogButtonBox::rejected, m_uploadDialog, &QDialog::reject);
}

void MainWindow::onUploadSubmit()
{
    if (m_selectedFilePath.isEmpty()) {
        showError("请先选择文件");
        return;
    }
    if (m_uploadTitleEdit->text().isEmpty()) {
        showError("请输入资源标题");
        return;
    }

    QJsonObject data;
    data["title"] = m_uploadTitleEdit->text();
    data["description"] = m_uploadDescriptionEdit->toPlainText();
    QStringList tags = m_uploadTagsEdit->text().split(',', Qt::SkipEmptyParts);
    QJsonArray tagsArray;
    for (QString tag : tags) tagsArray.append(tag.trimmed());
    data["tags"] = tagsArray;

    makeRequest("http://localhost:5000/api/resources", "POST", data, m_selectedFilePath);

    m_uploadTitleEdit->clear();
    m_uploadDescriptionEdit->clear();
    m_uploadTagsEdit->clear();
    m_selectedFilePath.clear();
    m_selectedFileLabel->setText("未选择文件");

    m_uploadDialog->accept();
}

void MainWindow::onSubscriptionsClicked()
{
    if (!m_subscriptionsDialog) {
        setupSubscriptionsDialog();
        qDebug() << "订阅对话框已创建";
    }

    if (m_tagsList) {
        m_tagsList->clear();
        qDebug() << "刷新 m_tagsList 开始";

        for (int i = 1; i < m_tagFilter->count(); ++i) {
            QString name = m_tagFilter->itemText(i);
            int id = m_tagFilter->itemData(i).toInt();
            if (!name.isEmpty()) {
                QListWidgetItem *item = new QListWidgetItem(name);
                item->setData(Qt::UserRole, id);
                m_tagsList->addItem(item);
                qDebug() << "添加标签到 m_tagsList: " << name << " (ID: " << id << ")";
            }
        }

        if (m_tagsList->count() == 0) {
            m_tagsList->addItem("暂无可用标签");
            qDebug() << "m_tagsList 为空，已添加占位项";
        }
    } else {
        qDebug() << "错误: m_tagsList 是 nullptr，无法刷新（检查 setupSubscriptionsDialog()）";
    }

    m_subscriptionsDialog->show();

    m_subPage = 1;
    loadSubscriptions();
}

void MainWindow::setupSubscriptionsDialog()
{
    m_subscriptionsDialog = new QDialog(this);
    m_subscriptionsDialog->setWindowTitle("我的订阅");
    m_subscriptionsDialog->setMinimumSize(800, 500);

    QHBoxLayout *layout = new QHBoxLayout(m_subscriptionsDialog);

    QVBoxLayout *tagsLayout = new QVBoxLayout();
    tagsLayout->addWidget(new QLabel("可用标签:"));
    m_tagsList = new QListWidget();
    m_subscribeBtn = new QPushButton("订阅选中标签");
    m_unsubscribeBtn = new QPushButton("取消订阅选中标签");
    tagsLayout->addWidget(m_tagsList);
    tagsLayout->addWidget(m_subscribeBtn);
    tagsLayout->addWidget(m_unsubscribeBtn);

    QVBoxLayout *subResourcesLayout = new QVBoxLayout();
    subResourcesLayout->addWidget(new QLabel("订阅的资源:"));
    m_subscribedResourcesList = new QListWidget();
    subResourcesLayout->addWidget(m_subscribedResourcesList);

    QHBoxLayout *pagerLayout = new QHBoxLayout();
    m_subPrevBtn = new QPushButton("上一页");
    m_subNextBtn = new QPushButton("下一页");
    m_subPageLabel = new QLabel();
    m_subPageLabel->setAlignment(Qt::AlignCenter);
    pagerLayout->addWidget(m_subPrevBtn);
    pagerLayout->addWidget(m_subPageLabel, 1);
    pagerLayout->addWidget(m_subNextBtn);
    subResourcesLayout->addLayout(pagerLayout);

    connect(m_subPrevBtn, &QPushButton::clicked, this, [this]() {
        if (m_subPage > 1) {
            m_subPage -= 1;
            loadSubscriptions();
        }
    });

    connect(m_subNextBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = (m_subTotal + m_subPageSize - 1) / m_subPageSize;
        if (totalPages <= 0) totalPages = 1;
        if (m_subPage < totalPages) {
            m_subPage += 1;
            loadSubscriptions();
        }
    });

    layout->addLayout(tagsLayout);
    layout->addLayout(subResourcesLayout);

    connect(m_subscribeBtn, &QPushButton::clicked, this, &MainWindow::onSubscribeClicked);
    connect(m_unsubscribeBtn, &QPushButton::clicked, this, &MainWindow::onUnsubscribeClicked);
    connect(m_subscribedResourcesList, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!item) return;
        int resId = item->data(Qt::UserRole).toInt();
        if (resId <= 0) return;
        ResourceDetailWindow *detailWindow = new ResourceDetailWindow(resId, m_userId, m_userRole, "user", this);
        connect(detailWindow, &ResourceDetailWindow::resourceDeleted, this, [this](int) {
            loadSubscriptions();
        });
        detailWindow->show();
    });
}


void MainWindow::onSubscribeClicked()
{
    QListWidgetItem *item = m_tagsList->currentItem();
    if (!item) {
        showError("请选择要订阅的标签");
        return;
    }

    int tagId = item->data(Qt::UserRole).toInt();
    qDebug() << "准备订阅 tag_id =" << tagId;

    QJsonObject data;
    data["tag_id"] = tagId;

    QString url = "http://localhost:5000/api/subscriptions";
    makeRequest(url, "POST", data);

    QTimer::singleShot(600, this, &MainWindow::loadSubscriptions);
}

void MainWindow::onReviewResource()
{
    QListWidgetItem *item = m_pendingResourcesList->currentItem();
    if (!item) {
        showError("请选择要审核的资源");
        return;
    }

    int resourceId = item->data(Qt::UserRole).toInt();
    QString status = sender() == m_approveBtn ? "approved" : "rejected";

    QJsonObject data;
    data["status"] = status;

    QString url = QString("http://localhost:5000/api/resources/%1/review").arg(resourceId);
    makeRequest(url, "POST", data);

    QTimer::singleShot(500, this, &MainWindow::loadPendingResources);
}

void MainWindow::loadResources()
{
    QString url = QString("http://localhost:5000/api/resources?status=approved&page=%1&page_size=%2&sort=new")
                  .arg(m_mainPage).arg(m_mainPageSize);

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "main_resources");
}

void MainWindow::loadTags()
{
    makeRequest("http://localhost:5000/api/tags", "GET");
}

void MainWindow::loadSubscriptions()
{
    qDebug() << "loadSubscriptions 被调用，当前 m_userId =" << m_userId;

    if (m_userId <= 0) {
        qDebug() << "用户 ID 无效，不加载订阅资源";
        m_subscribedResourcesList->clear();
        m_subscribedResourcesList->addItem("请先登录查看我的订阅");
        return;
    }

    QString url = QString("http://localhost:5000/api/subscriptions/resources?page=%1&page_size=%2")
                  .arg(m_subPage).arg(m_subPageSize);
    
    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "subscription_resources");
}

void MainWindow::loadPendingResources()
{
    QString status = "pending";
    if (m_reviewStatusFilter) {
        status = m_reviewStatusFilter->currentData().toString();
        if (status.isEmpty()) status = "pending";
    }

    QString url = QString("http://localhost:5000/api/resources?status=%1&page=%2&page_size=%3&sort=new")
                  .arg(status)
                  .arg(m_reviewPage)
                  .arg(m_reviewPageSize);

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "review_resources");

    qDebug() << "加载审核资源:" << status << "page=" << m_reviewPage;
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::critical(this, "错误", message);
}

void MainWindow::showSuccess(const QString &message)
{
    QMessageBox::information(this, "成功", message);
}

void MainWindow::onUnsubscribeClicked()
{
    QListWidgetItem *item = m_tagsList->currentItem();
    if (!item) {
        showError("请选择要取消订阅的标签");
        return;
    }

    int tagId = item->data(Qt::UserRole).toInt();
    QString url = QString("http://localhost:5000/api/subscriptions/%1").arg(tagId);
    makeRequest(url, "DELETE");

    QTimer::singleShot(500, this, &MainWindow::loadSubscriptions);
}
