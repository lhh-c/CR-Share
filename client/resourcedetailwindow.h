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
    explicit ResourceDetailWindow(int resourceId, int userId, const QString &userRole, const QString &viewMode = "user", QWidget *parent = nullptr);

signals:
    void resourceDeleted(int resourceId);

private slots:
    void onDownloadClicked();
    void onPreviewPdfClicked();
    void onReportClicked();
    void onCommentSubmit();
    void onCancelReply();
    void onAiAsk();
    void onNetworkReply(QNetworkReply *reply);
    void onDeleteResourceClicked();

private:
    void setupUI();
    void setupNetworkManager();
    void loadResourceDetails();
    void loadComments();
    QJsonObject makeRequest(const QString &url, const QString &method = "GET",
                           const QJsonObject &data = QJsonObject());
    void updateUIForRoleAndOwnership();
    
    int m_resourceId;
    int m_userId;
    QString m_userRole;
    QString m_viewMode; // "user" 或 "moderator"
    int m_uploaderId = 0;
    
    QTextEdit *m_resourceDetails;
    QPushButton *m_downloadBtn;
    QPushButton *m_previewPdfBtn;
    QPushButton *m_reportBtn;
    QPushButton *m_deleteResourceBtn;

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
