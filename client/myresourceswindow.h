#ifndef MYRESOURCESWINDOW_H
#define MYRESOURCESWINDOW_H

#include <QDialog>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

class MyResourcesWindow : public QDialog
{
    Q_OBJECT

public:
    explicit MyResourcesWindow(int userId, const QString &userRole, QWidget *parent = nullptr);

signals:
    void openResourceRequested(int resourceId);

private slots:
    void loadResources();
    void onNetworkReply(QNetworkReply *reply);
    void onItemDoubleClicked(QListWidgetItem *item);

private:
    int m_userId;
    QString m_userRole;

    QNetworkAccessManager *m_networkManager;

    QListWidget *m_list;
    QPushButton *m_refreshBtn;

    int m_page = 1;
    int m_pageSize = 20;
    int m_total = 0;
    QLabel *m_pageLabel;
    QPushButton *m_prevBtn;
    QPushButton *m_nextBtn;
};

#endif // MYRESOURCESWINDOW_H
