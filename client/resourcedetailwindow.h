#ifndef RESOURCEDETAILWINDOW_H
#define RESOURCEDETAILWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class ResourceDetailWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ResourceDetailWindow(int resourceId, int userId, QWidget *parent = nullptr);

private slots:
    void onAiAsk();
    void onNetworkReply(QNetworkReply *reply);

private:
    void setupUI();
    void setupNetworkManager();
    QJsonObject makeRequest(const QString &url, const QString &method = "GET",
                           const QJsonObject &data = QJsonObject());
    
    int m_resourceId;
    int m_userId;
    
    QGroupBox *m_aiGroup;
    QLineEdit *m_aiQuestionEdit;
    QPushButton *m_aiAskBtn;
    QTextEdit *m_aiAnswerDisplay;
    
    QNetworkAccessManager *m_networkManager;
};

#endif
