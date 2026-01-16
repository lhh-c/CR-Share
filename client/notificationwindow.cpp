#include "notificationwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDateTime>

static QString formatIsoTime(const QString &iso)
{
    if (iso.isEmpty()) return "-";
    QDateTime dt = QDateTime::fromString(iso, Qt::ISODate);
    if (!dt.isValid()) return iso;
    return dt.toString("yyyy-MM-dd hh:mm:ss");
}

NotificationWindow::NotificationWindow(int userId, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
{
    setWindowTitle("通知");
    setMinimumSize(700, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *topLayout = new QHBoxLayout();
    m_unreadCountLabel = new QLabel("未读：0");
    topLayout->addWidget(m_unreadCountLabel);

    topLayout->addWidget(new QLabel("筛选："));
    m_filterCombo = new QComboBox();
    m_filterCombo->addItem("未读", "unread");
    m_filterCombo->addItem("全部", "all");
    topLayout->addWidget(m_filterCombo);

    m_refreshBtn = new QPushButton("刷新");
    topLayout->addWidget(m_refreshBtn);

    m_markAllReadBtn = new QPushButton("全部标记已读");
    topLayout->addWidget(m_markAllReadBtn);

    topLayout->addStretch();
    layout->addLayout(topLayout);

    m_list = new QListWidget();
    layout->addWidget(m_list);

    QHBoxLayout *pagerLayout = new QHBoxLayout();
    m_prevBtn = new QPushButton("上一页");
    m_nextBtn = new QPushButton("下一页");
    m_pageLabel = new QLabel();
    m_pageLabel->setAlignment(Qt::AlignCenter);
    pagerLayout->addWidget(m_prevBtn);
    pagerLayout->addWidget(m_pageLabel, 1);
    pagerLayout->addWidget(m_nextBtn);
    layout->addLayout(pagerLayout);

    m_networkManager = new QNetworkAccessManager(this);
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &NotificationWindow::onNetworkReply);

    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        m_page = 1;
        loadUnreadCount();
        loadNotifications();
    });

    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_page = 1;
        loadNotifications();
    });

    connect(m_markAllReadBtn, &QPushButton::clicked, this, &NotificationWindow::onMarkAllReadClicked);
    connect(m_list, &QListWidget::itemClicked, this, &NotificationWindow::onItemClicked);

    connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
        if (m_page > 1) {
            m_page -= 1;
            loadNotifications();
        }
    });

    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
        if (totalPages <= 0) totalPages = 1;
        if (m_page < totalPages) {
            m_page += 1;
            loadNotifications();
        }
    });

    loadUnreadCount();
    loadNotifications();
}

void NotificationWindow::loadUnreadCount()
{
    QNetworkRequest request{QUrl("http://localhost:5000/api/notifications/unread_count")};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "unread_count");
}

void NotificationWindow::loadNotifications()
{
    QString filter = m_filterCombo->currentData().toString();
    QString url;
    if (filter == "unread") {
        url = QString("http://localhost:5000/api/notifications?unread=1&page=%1&page_size=%2").arg(m_page).arg(m_pageSize);
    } else {
        url = QString("http://localhost:5000/api/notifications?page=%1&page_size=%2").arg(m_page).arg(m_pageSize);
    }

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "list");
}

void NotificationWindow::onMarkAllReadClicked()
{
    auto ret = QMessageBox::question(this, "确认", "确定要将所有未读通知标记为已读吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    QNetworkRequest request{QUrl("http://localhost:5000/api/notifications/read_all")};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->post(request, QByteArray());
    reply->setProperty("requestType", "read_all");
}

void NotificationWindow::onItemClicked(QListWidgetItem *item)
{
    if (!item) return;

    int notifId = item->data(Qt::UserRole).toInt();
    int resourceId = item->data(Qt::UserRole + 1).toInt();

    if (notifId > 0) {
        QNetworkRequest request{QUrl(QString("http://localhost:5000/api/notifications/%1/read").arg(notifId))};
        request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

        QNetworkReply *reply = m_networkManager->post(request, QByteArray());
        reply->setProperty("requestType", "read_one");
    }

    if (resourceId > 0) {
        emit openResourceRequested(resourceId);
    }
}

void NotificationWindow::onNetworkReply(QNetworkReply *reply)
{
    if (!reply) return;

    QString type = reply->property("requestType").toString();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "网络错误", reply->errorString());
        reply->deleteLater();
        return;
    }

    if (statusCode >= 400) {
        QMessageBox::warning(this, "错误", QString("服务器错误 %1").arg(statusCode));
        reply->deleteLater();
        return;
    }

    QByteArray resp = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    QJsonObject json;
    if (!doc.isNull() && doc.isObject()) {
        json = doc.object();
    }

    if (type == "unread_count") {
        int c = json.value("unread_count").toInt(0);
        m_unreadCountLabel->setText(QString("未读：%1").arg(c));
    } else if (type == "list") {
        m_page = json.value("page").toInt(1);
        m_pageSize = json.value("page_size").toInt(m_pageSize);
        m_total = json.value("total").toInt(0);

        m_list->clear();

        QJsonArray arr = json.value("notifications").toArray();
        for (const QJsonValue &v : arr) {
            QJsonObject n = v.toObject();

            int id = n.value("id").toInt();
            int resourceId = n.value("resource_id").toInt();
            QString fromUsername = n.value("from_username").toString();
            QString preview = n.value("content_preview").toString();
            QString createdAt = formatIsoTime(n.value("created_at").toString());
            bool isRead = n.value("is_read").toBool(false);

            QString text = QString("[%1] %2：%3\n%4")
                               .arg(isRead ? "已读" : "未读")
                               .arg(fromUsername)
                               .arg(preview)
                               .arg(createdAt);

            QListWidgetItem *item = new QListWidgetItem(text);
            item->setData(Qt::UserRole, id);
            item->setData(Qt::UserRole + 1, resourceId);
            m_list->addItem(item);
        }

        if (arr.isEmpty()) {
            QListWidgetItem *emptyItem = new QListWidgetItem("暂无通知");
            emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
            m_list->addItem(emptyItem);
        }

        int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
        if (totalPages <= 0) totalPages = 1;
        m_pageLabel->setText(QString("第 %1/%2 页（共 %3 条）").arg(m_page).arg(totalPages).arg(m_total));
        m_prevBtn->setEnabled(m_page > 1);
        m_nextBtn->setEnabled(m_page < totalPages);
    } else if (type == "read_all") {
        loadUnreadCount();
        loadNotifications();
    } else if (type == "read_one") {
        loadUnreadCount();
        loadNotifications();
    }

    reply->deleteLater();
}
