#ifndef RESOURCEDETAILWINDOW_H
#define RESOURCEDETAILWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QPushButton>
#include <QLineEdit>
#include <QGroupBox>
#include <QTreeWidget>
#include <QLabel>
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
    void onDownloadClicked();
    void onPreviewPdfClicked();
    void onReportClicked();
    void onCommentSubmit();
    void onCancelReply();
    void onAiAsk();
    void onNetworkReply(QNetworkReply *reply);

private:
    void setupUI();
    void setupNetworkManager();
    void loadResourceDetails();
    void loadComments();
    QJsonObject makeRequest(const QString &url, const QString &method = "GET",
                           const QJsonObject &data = QJsonObject());
    
    int m_resourceId;
    int m_userId;
    
    QTextEdit *m_resourceDetails;
    QPushButton *m_downloadBtn;
    QPushButton *m_previewPdfBtn;
    QPushButton *m_reportBtn;

    QString m_fileType;
    QString m_lastPdfPath;
    QGroupBox *m_commentGroup;
    QTreeWidget *m_commentsTree;
    QLabel *m_replyToLabel;
    QPushButton *m_cancelReplyBtn;
    int m_replyToCommentId = 0;
    QTextEdit *m_commentEdit;
    QPushButton *m_commentSubmitBtn;
    QGroupBox *m_aiGroup;
    QLineEdit *m_aiQuestionEdit;
    QPushButton *m_aiAskBtn;
    QTextEdit *m_aiAnswerDisplay;
    
    QNetworkAccessManager *m_networkManager;
};

#endif
