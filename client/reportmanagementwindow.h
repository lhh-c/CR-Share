#ifndef REPORTMANAGEMENTWINDOW_H
#define REPORTMANAGEMENTWINDOW_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QLabel>

class ReportManagementWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ReportManagementWindow(int userId, QWidget *parent = nullptr);

private slots:
    void loadReports();
    void onNetworkReply(QNetworkReply *reply);
    void onResolveDeleteClicked();
    void onResolveRejectClicked();
    void onSelectionChanged();

private:
    struct ReportRow {
        int id = 0;
        int resourceId = 0;
        QString resourceTitle;
        QString reporterName;
        QString reason;
        QString status;
        QString createdAt;
    };

    void sendResolveRequest(int reportId, const QString &action);

    int m_userId;

    QNetworkAccessManager *m_networkManager;
    QComboBox *m_statusFilter;
    QPushButton *m_refreshBtn;

    QTableWidget *m_table;
    QPushButton *m_deleteResourceBtn;
    QPushButton *m_rejectReportBtn;

    int m_page = 1;
    int m_pageSize = 20;
    int m_total = 0;
    QLabel *m_pageLabel;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;

    QList<ReportRow> m_rows;
};

#endif // REPORTMANAGEMENTWINDOW_H
