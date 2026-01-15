#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QGroupBox>

class SearchResultWindow;
class ResourceDetailWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setUserId(int userId);
    void setUserRole(const QString &role);

private slots:
    void onSearchClicked();
    void onResourceSelected(QListWidgetItem *item);
    void onRefreshClicked();
    void onUploadClicked();
    void onSubscriptionsClicked();
    void onReviewClicked();
    void loadResources();
    void loadTags();
    void onNetworkReply(QNetworkReply *reply);

private:
    void setupUI();
    void setupNetworkManager();
    QJsonObject makeRequest(const QString &url, const QString &method = "GET",
                            const QJsonObject &data = QJsonObject(),
                            const QString &filePath = "");
    void showError(const QString &message);
    void showSuccess(const QString &message);
    
    QLineEdit *m_searchEdit;
    QComboBox *m_tagFilter;
    QPushButton *m_searchBtn;
    QPushButton *m_refreshBtn;
    QListWidget *m_recommendedResourcesList;
    
    QPushButton *m_uploadBtn;
    QPushButton *m_subscriptionsBtn;
    QPushButton *m_reviewBtn;
    
    QDialog *m_uploadDialog;
    QLineEdit *m_uploadTitleEdit;
    QTextEdit *m_uploadDescriptionEdit;
    QLineEdit *m_uploadTagsEdit;
    QPushButton *m_selectFileBtn;
    QPushButton *m_uploadSubmitBtn;
    QLabel *m_selectedFileLabel;
    QString m_selectedFilePath;
    
    QDialog *m_subscriptionsDialog;
    QListWidget *m_tagsList;
    QPushButton *m_subscribeBtn;
    QListWidget *m_subscribedResourcesList;
    QPushButton *m_unsubscribeBtn;
    
    QDialog *m_reviewDialog;
    QComboBox *m_reviewStatusFilter;
    QListWidget *m_pendingResourcesList;
    QPushButton *m_approveBtn;
    QPushButton *m_rejectBtn;
    
    QNetworkAccessManager *m_networkManager;
    
    int m_userId;
    QString m_userRole;
    
    void loadPendingResources();
    void setupUploadDialog();
    void setupSubscriptionsDialog();
    void setupReviewDialog();
    void onUploadSubmit();
    void onSubscribeClicked();
    void onUnsubscribeClicked();
    void onReviewResource();
    void loadSubscriptions();
};

#endif // MAINWINDOW_H
