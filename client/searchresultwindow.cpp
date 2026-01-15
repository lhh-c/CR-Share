#include "searchresultwindow.h"
#include "resourcedetailwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QUrl>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDebug>

SearchResultWindow::SearchResultWindow(const QString &searchKeyword,
                                       const QString &tagFilter,
                                       int userId,
                                       QWidget *parent)
    : QMainWindow(parent)
    , m_searchKeyword(searchKeyword.trimmed())
    , m_tagFilter(tagFilter)
    , m_userId(userId)
{
    setWindowTitle("搜索结果 - 享阅");
    setMinimumSize(800, 600);

    setupUI();
    setupNetworkManager();
    loadSearchResults();
}

void SearchResultWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    m_statusLabel = new QLabel("正在搜索...");
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
    layout->addWidget(m_statusLabel);

    m_resultList = new QListWidget();
    m_resultList->setStyleSheet("QListWidget { font-size: 14px; }");
    layout->addWidget(m_resultList);

    connect(m_resultList, &QListWidget::itemClicked,
            this, &SearchResultWindow::onResourceSelected);
}

void SearchResultWindow::setupNetworkManager()
{
    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &SearchResultWindow::onNetworkReply);
}

void SearchResultWindow::loadSearchResults()
{
    QUrl url("http://localhost:5000/api/resources");
    QUrlQuery query;

    if (!m_searchKeyword.isEmpty()) {
        query.addQueryItem("keyword", m_searchKeyword);
    }

    if (!m_tagFilter.isEmpty() && m_tagFilter != "") {
        query.addQueryItem("tag_id", m_tagFilter);
    }

    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "searchResults");

    m_statusLabel->setText(QString("搜索中... (关键词: %1, 标签: %2)")
                               .arg(m_searchKeyword.isEmpty() ? "无" : m_searchKeyword)
                               .arg(m_tagFilter.isEmpty() ? "无" : m_tagFilter));
}

void SearchResultWindow::onResourceSelected(QListWidgetItem *item)
{
    if (!item) return;

    int resourceId = item->data(Qt::UserRole).toInt();
    if (resourceId <= 0) return;

    ResourceDetailWindow *detailWindow = new ResourceDetailWindow(resourceId, m_userId, this);
    detailWindow->show();
}

void SearchResultWindow::onNetworkReply(QNetworkReply *reply)
{
    if (reply->property("requestType").toString() != "searchResults") {
        reply->deleteLater();
        return;
    }

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText("网络错误: " + reply->errorString());
        QMessageBox::warning(this, "错误", reply->errorString());
        reply->deleteLater();
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode >= 300) {
        m_statusLabel->setText("服务器错误 " + QString::number(statusCode));
        reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (doc.isNull() || !doc.isObject()) {
        m_statusLabel->setText("响应格式错误");
        reply->deleteLater();
        return;
    }

    QJsonObject json = doc.object();
    QJsonArray resources = json["resources"].toArray();

    m_resultList->clear();

    if (resources.isEmpty()) {
        m_statusLabel->setText(QString("未找到匹配资源\n关键词: %1 | 标签: %2")
                                   .arg(m_searchKeyword.isEmpty() ? "无" : m_searchKeyword)
                                   .arg(m_tagFilter.isEmpty() ? "无" : m_tagFilter));
        m_resultList->addItem("没有匹配的结果，试试其他关键词或标签吧~");
    } else {
        m_statusLabel->setText(QString("找到 %1 个匹配资源\n关键词: %2 | 标签: %3")
                                   .arg(resources.size())
                                   .arg(m_searchKeyword.isEmpty() ? "无" : m_searchKeyword)
                                   .arg(m_tagFilter.isEmpty() ? "无" : m_tagFilter));

        for (const QJsonValue &val : resources) {
            QJsonObject res = val.toObject();
            QString title = res["title"].toString();
            int id = res["id"].toInt();

            QListWidgetItem *item = new QListWidgetItem(title);
            item->setData(Qt::UserRole, id);
            item->setToolTip(res["description"].toString().left(100) + "...");
            m_resultList->addItem(item);
        }
    }

    reply->deleteLater();
}
