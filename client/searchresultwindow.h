#ifndef SEARCHRESULTWINDOW_H
#define SEARCHRESULTWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPushButton>
#include <QComboBox>

class SearchResultWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SearchResultWindow(const QString &searchKeyword,
                                const QString &tagFilter,
                                int userId,
                                const QString &userRole,
                                QWidget *parent = nullptr);

private slots:
    void onResourceSelected(QListWidgetItem *item);
    void onNetworkReply(QNetworkReply *reply);

private:
    void setupUI();
    void setupNetworkManager();
    void loadSearchResults();
    void makeRequest(const QString &url, const QString &method = "GET");  // 改成 void

    QListWidget *m_resultList;
    QLabel *m_statusLabel;
    QComboBox *m_sortCombo;
    QLabel *m_pageLabel;
    QPushButton *m_prevButton;
    QPushButton *m_nextButton;
    QNetworkAccessManager *m_networkManager;

    QString m_sort = "relevance";

    int m_page = 1;
    int m_pageSize = 20;
    int m_total = 0;

    QString m_searchKeyword;
    QString m_tagFilter;
    int m_userId;
    QString m_userRole;
};

#endif // SEARCHRESULTWINDOW_H
