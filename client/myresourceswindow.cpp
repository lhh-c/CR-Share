#include "myresourceswindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>

MyResourcesWindow::MyResourcesWindow(int userId, const QString &userRole, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
    , m_userRole(userRole)
{
    setWindowTitle("我上传的资源");
    setMinimumSize(700, 500);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *topLayout = new QHBoxLayout();
    m_refreshBtn = new QPushButton("刷新");
    topLayout->addWidget(m_refreshBtn);
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
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &MyResourcesWindow::onNetworkReply);

    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        m_page = 1;
        loadResources();
    });

    connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
        if (m_page > 1) {
            m_page -= 1;
            loadResources();
        }
    });

    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
        if (totalPages <= 0) totalPages = 1;
        if (m_page < totalPages) {
            m_page += 1;
            loadResources();
        }
    });

    connect(m_list, &QListWidget::itemDoubleClicked, this, &MyResourcesWindow::onItemDoubleClicked);

    loadResources();
}

void MyResourcesWindow::loadResources()
{
    QString url = QString("http://localhost:5000/api/resources/my?page=%1&page_size=%2")
                      .arg(m_page)
                      .arg(m_pageSize);

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "my_resources");
}

void MyResourcesWindow::onItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return;
    int resId = item->data(Qt::UserRole).toInt();
    if (resId <= 0) return;
    emit openResourceRequested(resId);
}

void MyResourcesWindow::onNetworkReply(QNetworkReply *reply)
{
    if (!reply) return;

    if (reply->property("requestType").toString() != "my_resources") {
        reply->deleteLater();
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "网络错误", reply->errorString());
        reply->deleteLater();
        return;
    }

    if (statusCode < 200 || statusCode >= 300) {
        QMessageBox::warning(this, "错误", QString("服务器错误 %1").arg(statusCode));
        reply->deleteLater();
        return;
    }

    QByteArray resp = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(resp);
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::warning(this, "错误", "服务器响应格式错误");
        reply->deleteLater();
        return;
    }

    QJsonObject json = doc.object();
    m_page = json.value("page").toInt(1);
    m_pageSize = json.value("page_size").toInt(m_pageSize);
    m_total = json.value("total").toInt(0);

    m_list->clear();

    QJsonArray arr = json.value("resources").toArray();
    for (const QJsonValue &v : arr) {
        QJsonObject r = v.toObject();
        int id = r.value("id").toInt();
        QString title = r.value("title").toString();
        QString status = r.value("status").toString();

        QListWidgetItem *item = new QListWidgetItem(QString("%1  [%2]").arg(title).arg(status));
        item->setData(Qt::UserRole, id);
        m_list->addItem(item);
    }

    if (arr.isEmpty()) {
        QListWidgetItem *emptyItem = new QListWidgetItem("暂无上传资源");
        emptyItem->setFlags(emptyItem->flags() & ~Qt::ItemIsSelectable);
        m_list->addItem(emptyItem);
    }

    int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
    if (totalPages <= 0) totalPages = 1;
    m_pageLabel->setText(QString("第 %1/%2 页（共 %3 条）").arg(m_page).arg(totalPages).arg(m_total));
    m_prevBtn->setEnabled(m_page > 1);
    m_nextBtn->setEnabled(m_page < totalPages);

    reply->deleteLater();
}
