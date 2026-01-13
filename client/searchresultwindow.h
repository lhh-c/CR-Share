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

class SearchResultWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit SearchResultWindow(const QString &searchKeyword,
                                int userId,
                                QWidget *parent = nullptr);

private slots:
    void onResourceSelected(QListWidgetItem *item);
    void onNetworkReply(QNetworkReply *reply);

private:
    void setupUI();
    void setupNetworkManager();
    void loadSearchResults();
    void makeRequest(const QString &url, const QString &method = "GET");

    QListWidget *m_resultList;
    QLabel *m_statusLabel;
    QNetworkAccessManager *m_networkManager;

    QString m_searchKeyword;
    int m_userId;
};

#endif // SEARCHRESULTWINDOW_H
