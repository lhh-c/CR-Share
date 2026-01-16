#ifndef NOTIFICATIONWINDOW_H
#define NOTIFICATIONWINDOW_H

#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QListWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>

class NotificationWindow : public QDialog
{
    Q_OBJECT

public:
    explicit NotificationWindow(int userId, QWidget *parent = nullptr);

signals:
    void openResourceRequested(int resourceId);

private slots:
    void loadUnreadCount();
    void loadNotifications();
    void onNetworkReply(QNetworkReply *reply);
    void onItemClicked(QListWidgetItem *item);
    void onMarkAllReadClicked();

private:
    int m_userId;
    QNetworkAccessManager *m_networkManager;

    QLabel *m_unreadCountLabel;
    QComboBox *m_filterCombo;
    QListWidget *m_list;

    QPushButton *m_refreshBtn;
    QPushButton *m_markAllReadBtn;

    int m_page = 1;
    int m_pageSize = 20;
    int m_total = 0;
    QLabel *m_pageLabel;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
};

#endif // NOTIFICATIONWINDOW_H
