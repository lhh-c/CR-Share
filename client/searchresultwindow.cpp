#include "searchresultwindow.h"
#include "resourcedetailwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
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
                                       const QString &userRole,
                                       QWidget *parent)
    : QMainWindow(parent)
    , m_searchKeyword(searchKeyword.trimmed())
    , m_tagFilter(tagFilter)
    , m_userId(userId)
    , m_userRole(userRole)
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

    // 状态显示（关键词 + 结果数）
    m_statusLabel = new QLabel("正在搜索...");
    m_statusLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #333;");
    layout->addWidget(m_statusLabel);

    QHBoxLayout *sortLayout = new QHBoxLayout();
    sortLayout->addWidget(new QLabel("排序:"));
    m_sortCombo = new QComboBox();
    m_sortCombo->addItem("相关性", "relevance");
    m_sortCombo->addItem("时间", "new");
    m_sortCombo->addItem("浏览量", "views");
    sortLayout->addWidget(m_sortCombo);
    sortLayout->addStretch();
    layout->addLayout(sortLayout);

    // 结果列表
    m_resultList = new QListWidget();
    m_resultList->setStyleSheet("QListWidget { font-size: 14px; }");
    layout->addWidget(m_resultList);

    QHBoxLayout *pagerLayout = new QHBoxLayout();
    m_prevButton = new QPushButton("上一页");
    m_nextButton = new QPushButton("下一页");
    m_pageLabel = new QLabel();
    m_pageLabel->setAlignment(Qt::AlignCenter);

    pagerLayout->addWidget(m_prevButton);
    pagerLayout->addWidget(m_pageLabel, 1);
    pagerLayout->addWidget(m_nextButton);

    layout->addLayout(pagerLayout);

    connect(m_resultList, &QListWidget::itemClicked,
            this, &SearchResultWindow::onResourceSelected);

    connect(m_sortCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        m_sort = m_sortCombo->currentData().toString();
        m_page = 1;
        loadSearchResults();
    });
    connect(m_prevButton, &QPushButton::clicked, this, [this]() {
        if (m_page > 1) {
            m_page -= 1;
            loadSearchResults();
        }
    });
    connect(m_nextButton, &QPushButton::clicked, this, [this]() {
        int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
        if (totalPages <= 0) totalPages = 1;
        if (m_page < totalPages) {
            m_page += 1;
            loadSearchResults();
        }
    });

    int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
    if (totalPages <= 0) totalPages = 1;

    m_pageLabel->setText(QString("第 %1/%2 页").arg(m_page).arg(totalPages));
    m_prevButton->setEnabled(m_page > 1);
    m_nextButton->setEnabled(m_page < totalPages);

    m_statusLabel->setText(QString("搜索中... (关键词: %1, 标签: %2)\n页码: %3/%4")
                               .arg(m_searchKeyword.isEmpty() ? "无" : m_searchKeyword)
                               .arg(m_tagFilter.isEmpty() ? "无" : m_tagFilter)
                               .arg(m_page)
                               .arg(totalPages));
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
    query.addQueryItem("status", "approved");  // 默认只搜已通过

    if (!m_searchKeyword.isEmpty()) {
        query.addQueryItem("keyword", m_searchKeyword);  // 建议服务端用 keyword
    }

    if (!m_tagFilter.isEmpty() && m_tagFilter != "") {
        query.addQueryItem("tag_id", m_tagFilter);  // 服务端用 tag_id
    }

    query.addQueryItem("page", QString::number(m_page));
    query.addQueryItem("page_size", QString::number(m_pageSize));
    query.addQueryItem("sort", m_sort);

    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "searchResults");  // 关键标记，便于区分


    int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
    if (totalPages <= 0) totalPages = 1;
    m_pageLabel->setText(QString("第 %1/%2 页").arg(m_page).arg(totalPages));
    m_prevButton->setEnabled(m_page > 1);
    m_nextButton->setEnabled(m_page < totalPages);
    m_statusLabel->setText(QString("搜索中... (关键词: %1, 标签: %2)\n页码: %3/%4")
                               .arg(m_searchKeyword.isEmpty() ? "无" : m_searchKeyword)
                               .arg(m_tagFilter.isEmpty() ? "无" : m_tagFilter)
                               .arg(m_page)
                               .arg(totalPages));
}


void SearchResultWindow::onResourceSelected(QListWidgetItem *item)
{
    if (!item) return;

    int resourceId = item->data(Qt::UserRole).toInt();
    if (resourceId <= 0) return;

    ResourceDetailWindow *detailWindow = new ResourceDetailWindow(resourceId, m_userId, m_userRole, "user", this);
    connect(detailWindow, &ResourceDetailWindow::resourceDeleted, this, [this](int) {
        loadSearchResults();
    });
    detailWindow->show();
}

void SearchResultWindow::onNetworkReply(QNetworkReply *reply)
{
    if (reply->property("requestType").toString() != "searchResults") {
        reply->deleteLater();
        return;  // 只处理搜索请求
    }

    if (reply->error() != QNetworkReply::NoError) {
        m_statusLabel->setText("网络错误: " + reply->errorString());
        m_prevButton->setEnabled(false);
        m_nextButton->setEnabled(false);
        QMessageBox::warning(this, "网络错误", "请求失败了，请检查网络连接：" + reply->errorString());
        reply->deleteLater();
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode >= 300) {
        QString friendly;
        if (statusCode == 401) {
            friendly = "您需要登录才能查看搜索结果。";
        } else if (statusCode == 403) {
            friendly = "抱歉，您没有权限查看这些资源。";
        } else if (statusCode == 404) {
            friendly = "找不到请求的内容。";
        } else if (statusCode >= 500) {
            friendly = "服务器开小差了，请稍后再试。";
        } else {
            friendly = QString("请求失败（%1）").arg(statusCode);
        }

        m_statusLabel->setText(friendly);
        m_prevButton->setEnabled(false);
        m_nextButton->setEnabled(false);
        QMessageBox::warning(this, "出错了", friendly);
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
    m_page = json["page"].toInt(1);
    m_pageSize = json["page_size"].toInt(20);
    m_total = json["total"].toInt(0);

    QJsonArray resources = json["resources"].toArray();

    m_resultList->clear();

    if (resources.isEmpty()) {
        m_statusLabel->setText(QString("未找到匹配资源\n关键词: %1 | 标签: %2")
                                   .arg(m_searchKeyword.isEmpty() ? "无" : m_searchKeyword)
                                   .arg(m_tagFilter.isEmpty() ? "无" : m_tagFilter));
        QListWidgetItem* emptyItem = new QListWidgetItem("没有匹配的结果，试试其他关键词或标签吧~");
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        m_resultList->addItem(emptyItem);
    } else {
        m_statusLabel->setText(QString("共找到 %1 个匹配资源\n关键词: %2 | 标签: %3")
                                   .arg(m_total)
                                   .arg(m_searchKeyword.isEmpty() ? "无" : m_searchKeyword)
                                   .arg(m_tagFilter.isEmpty() ? "无" : m_tagFilter));

        for (const QJsonValue &val : resources) {
            QJsonObject res = val.toObject();
            QString title = res["title"].toString();
            int id = res["id"].toInt();
            int viewCount = res["view_count"].toInt(0);

            QString displayText = QString("%1 (浏览: %2)").arg(title).arg(viewCount);
            QListWidgetItem *item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, id);
            // 可选：加提示
            item->setToolTip(res["description"].toString().left(100) + "...");
            m_resultList->addItem(item);
        }
    }

    int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
    if (totalPages <= 0) totalPages = 1;
    m_pageLabel->setText(QString("第 %1/%2 页").arg(m_page).arg(totalPages));
    m_prevButton->setEnabled(m_page > 1);
    m_nextButton->setEnabled(m_page < totalPages);

    reply->deleteLater();
}
