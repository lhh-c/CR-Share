#include "mainwindow.h"
#include "searchresultwindow.h"
#include "resourcedetailwindow.h"
#include <QUrl>
#include <QNetworkRequest>
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
    m_userRole("student")
{
    setupUI();
    setupNetworkManager();
    loadResources();
    loadTags();
}

void MainWindow::setUserId(int userId)
{
    m_userId = userId;
}

void MainWindow::setUserRole(const QString &role)
{
    m_userRole = role;
}

void MainWindow::setupUI()
{
    setWindowTitle("享阅 - 高校学习资源共享平台");
    setMinimumSize(1000, 700);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

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
    mainLayout->addLayout(searchLayout);

    QLabel *recommendedLabel = new QLabel("推荐资源");
    mainLayout->addWidget(recommendedLabel);
    m_recommendedResourcesList = new QListWidget();
    mainLayout->addWidget(m_recommendedResourcesList);

    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_recommendedResourcesList, &QListWidget::itemClicked, this, &MainWindow::onResourceSelected);
}

void MainWindow::setupNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &MainWindow::onNetworkReply);
}

QJsonObject MainWindow::makeRequest(const QString &urlStr, const QString &method,
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
    } else if (method == "DELETE") {
        reply = m_networkManager->deleteResource(request);
    }

    if (reply) {
        reply->setProperty("url", urlStr);
        reply->setProperty("method", method);
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
        QJsonDocument errDoc = QJsonDocument::fromJson(errData);
        if (!errDoc.isNull() && errDoc.isObject()) {
            QString errorText = errDoc.object()["error"].toString().trimmed();
            showError("操作失败：" + errorText);
        } else {
            showError("操作失败：服务器拒绝了请求");
        }
        reply->deleteLater();
        return;
    }

    if (statusCode < 200 || statusCode >= 300) {
        QByteArray errData = reply->readAll();
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

        if (m_tagFilter) {
            m_tagFilter->clear();
            m_tagFilter->addItem("全部标签", "");

            for (const QJsonValue &val : tagsArray) {
                if (!val.isObject()) {
                    continue;
                }
                QJsonObject tag = val.toObject();
                QString name = tag.value("name").toString();
                int id = tag.value("id").toInt();
                if (name.isEmpty()) {
                    continue;
                }
                m_tagFilter->addItem(name, id);
            }
        }
    }
    else if (urlStr.contains("/api/resources") && !urlStr.contains("/api/resources/")) {
        QJsonArray resources = json["resources"].toArray();

        m_recommendedResourcesList->clear();
        for (const QJsonValue &val : resources) {
            QJsonObject res = val.toObject();
            QString title = res["title"].toString();
            int id = res["id"].toInt();
            QListWidgetItem *item = new QListWidgetItem(title);
            item->setData(Qt::UserRole, id);
            m_recommendedResourcesList->addItem(item);
        }

        m_recommendedResourcesList->update();
        m_recommendedResourcesList->repaint();
        m_recommendedResourcesList->scrollToTop();
        QApplication::processEvents();
    }

    reply->deleteLater();
}

void MainWindow::onSearchClicked()
{
    QString keyword = m_searchEdit->text();
    QString tag = m_tagFilter->currentData().toString();

    SearchResultWindow *searchWindow = new SearchResultWindow(keyword, tag, m_userId, this);
    searchWindow->show();
}

void MainWindow::onRefreshClicked()
{
    loadResources();
}

void MainWindow::onResourceSelected(QListWidgetItem *item)
{
    if (!item) return;
    int resourceId = item->data(Qt::UserRole).toInt();
    if (resourceId <= 0) return;

    ResourceDetailWindow *detailWindow = new ResourceDetailWindow(resourceId, m_userId, this);
    detailWindow->show();
}

void MainWindow::loadResources()
{
    QString url = "http://localhost:5000/api/resources";
    makeRequest(url, "GET");
}

void MainWindow::loadTags()
{
    makeRequest("http://localhost:5000/api/tags", "GET");
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::critical(this, "错误", message);
}

void MainWindow::showSuccess(const QString &message)
{
    QMessageBox::information(this, "成功", message);
}
