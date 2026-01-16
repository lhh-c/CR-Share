#include "reportmanagementwindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QDateTime>

ReportManagementWindow::ReportManagementWindow(int userId, QWidget *parent)
    : QDialog(parent)
    , m_userId(userId)
{
    setWindowTitle("举报管理");
    setMinimumSize(900, 600);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QHBoxLayout *topLayout = new QHBoxLayout();

    m_refreshBtn = new QPushButton("刷新");
    topLayout->addWidget(m_refreshBtn);

    topLayout->addStretch();
    layout->addLayout(topLayout);

    m_table = new QTableWidget();
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"资源标题", "举报人", "原因", "举报时间"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_table);

    QHBoxLayout *actionLayout = new QHBoxLayout();
    m_deleteResourceBtn = new QPushButton("删除资源");
    m_rejectReportBtn = new QPushButton("驳回举报");
    m_deleteResourceBtn->setEnabled(false);
    m_rejectReportBtn->setEnabled(false);

    actionLayout->addStretch();
    actionLayout->addWidget(m_deleteResourceBtn);
    actionLayout->addWidget(m_rejectReportBtn);
    layout->addLayout(actionLayout);

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

    connect(m_networkManager, &QNetworkAccessManager::finished, this, &ReportManagementWindow::onNetworkReply);
    connect(m_refreshBtn, &QPushButton::clicked, this, [this]() {
        m_page = 1;
        loadReports();
    });
    connect(m_prevBtn, &QPushButton::clicked, this, [this]() {
        if (m_page > 1) {
            m_page -= 1;
            loadReports();
        }
    });
    connect(m_nextBtn, &QPushButton::clicked, this, [this]() {
        int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
        if (totalPages <= 0) totalPages = 1;
        if (m_page < totalPages) {
            m_page += 1;
            loadReports();
        }
    });

    connect(m_table->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ReportManagementWindow::onSelectionChanged);
    connect(m_deleteResourceBtn, &QPushButton::clicked, this, &ReportManagementWindow::onResolveDeleteClicked);
    connect(m_rejectReportBtn, &QPushButton::clicked, this, &ReportManagementWindow::onResolveRejectClicked);

    loadReports();
}

void ReportManagementWindow::loadReports()
{
    QString url = QString("http://localhost:5000/api/reports?status=pending&page=%1&page_size=%2")
                      .arg(m_page)
                      .arg(m_pageSize);

    QNetworkRequest request{QUrl(url)};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());

    QNetworkReply *reply = m_networkManager->get(request);
    reply->setProperty("requestType", "list_reports");
}

void ReportManagementWindow::onSelectionChanged()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_rows.size()) {
        m_deleteResourceBtn->setEnabled(false);
        m_rejectReportBtn->setEnabled(false);
        return;
    }

    const auto &r = m_rows[row];
    Q_UNUSED(r);
    m_deleteResourceBtn->setEnabled(true);
    m_rejectReportBtn->setEnabled(true);
}

void ReportManagementWindow::sendResolveRequest(int reportId, const QString &action)
{
    QNetworkRequest request{QUrl(QString("http://localhost:5000/api/reports/%1/resolve").arg(reportId))};
    request.setRawHeader("X-User-Id", QString::number(m_userId).toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["action"] = action;

    QNetworkReply *reply = m_networkManager->post(request, QJsonDocument(body).toJson());
    reply->setProperty("requestType", "resolve_report");
}

void ReportManagementWindow::onResolveDeleteClicked()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_rows.size()) return;

    const auto &r = m_rows[row];
    auto ret = QMessageBox::warning(this, "确认", "确定要删除该举报对应的资源吗？此操作会删除资源文件及相关数据。", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    sendResolveRequest(r.id, "delete");
}

void ReportManagementWindow::onResolveRejectClicked()
{
    int row = m_table->currentRow();
    if (row < 0 || row >= m_rows.size()) return;

    const auto &r = m_rows[row];
    auto ret = QMessageBox::question(this, "确认", "确定要驳回该举报吗？", QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes) return;

    sendResolveRequest(r.id, "reject");
}

void ReportManagementWindow::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        QMessageBox::warning(this, "网络错误", reply->errorString());
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
    QString type = reply->property("requestType").toString();

    if (type == "list_reports") {
        m_rows.clear();
        m_table->setRowCount(0);

        m_page = json["page"].toInt(1);
        m_pageSize = json["page_size"].toInt(m_pageSize);
        m_total = json["total"].toInt(0);

        QJsonArray reports = json["reports"].toArray();
        for (const QJsonValue &v : reports) {
            QJsonObject r = v.toObject();
            ReportRow row;
            row.id = r["id"].toInt();
            row.resourceId = r["resource_id"].toInt();
            row.resourceTitle = r["resource_title"].toString();
            row.reporterName = r["reporter_name"].toString();
            row.reason = r["reason"].toString();
            row.status = r["status"].toString();
            row.createdAt = r["created_at"].toString();
            m_rows.append(row);

            QString createdAtFmt = row.createdAt;
            if (!row.createdAt.isEmpty()) {
                QDateTime dt = QDateTime::fromString(row.createdAt, Qt::ISODate);
                if (dt.isValid()) {
                    createdAtFmt = dt.toString("yyyy-MM-dd hh:mm:ss");
                }
            }

            int tr = m_table->rowCount();
            m_table->insertRow(tr);
            m_table->setItem(tr, 0, new QTableWidgetItem(row.resourceTitle));
            m_table->setItem(tr, 1, new QTableWidgetItem(row.reporterName));
            m_table->setItem(tr, 2, new QTableWidgetItem(row.reason));
            m_table->setItem(tr, 3, new QTableWidgetItem(createdAtFmt));
        }

        int totalPages = (m_total + m_pageSize - 1) / m_pageSize;
        if (totalPages <= 0) totalPages = 1;
        m_pageLabel->setText(QString("第 %1/%2 页（共 %3 条）").arg(m_page).arg(totalPages).arg(m_total));
        m_prevBtn->setEnabled(m_page > 1);
        m_nextBtn->setEnabled(m_page < totalPages);

        onSelectionChanged();
    } else if (type == "resolve_report") {
        QMessageBox::information(this, "成功", "处理完成");
        loadReports();
    }

    reply->deleteLater();
}
