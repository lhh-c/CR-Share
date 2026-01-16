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

// 前向声明
class SearchResultWindow;
class ResourceDetailWindow;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void setUserId(int userId);
    void setUserRole(const QString &role);

signals:
    void logoutRequested();

private slots:
    void onSearchClicked();
    void onResourceSelected(QListWidgetItem *item);
    void onRefreshClicked();
    void onUploadClicked();
    void onSubscriptionsClicked();
    void loadResources();
    void loadTags();
    void onNetworkReply(QNetworkReply *reply);
    void onReviewResource();
    void onLogoutBtnClicked();
    void onDeleteAccountBtnClicked();
    void onNotificationsClicked();

private:
    void setupUI();
    void setupNetworkManager();
    QJsonObject makeRequest(const QString &url, const QString &method = "GET",
                            const QJsonObject &data = QJsonObject(),
                            const QString &filePath = "");
    void showError(const QString &message);
    void showSuccess(const QString &message);
    
    // 主界面UI组件
    QLineEdit *m_searchEdit;
    QComboBox *m_tagFilter;
    QPushButton *m_searchBtn;
    QPushButton *m_refreshBtn;
    QListWidget *m_recommendedResourcesList;  // 推荐资源列表
    
    // 功能按钮
    QPushButton *m_modeToggleBtn;  // 审核员模式切换
    QPushButton *m_uploadBtn;
    QPushButton *m_subscriptionsBtn;
    QPushButton *m_logoutBtn;
    QPushButton *m_deleteAccountBtn;
    QPushButton *m_notificationsBtn;

    // 视图容器
    QGroupBox *m_recommendationGroup;
    QGroupBox *m_reviewGroup;

    // 审核员当前模式
    bool m_isModeratorMode = false;

    QLabel *m_recommendedLabel;

    // 我的信息展示
    QLabel *m_profileUsernameLabel;
    QLabel *m_profileEmailLabel;
    QLabel *m_profileRoleLabel;
    
    // 上传对话框相关（保留用于上传功能）
    QDialog *m_uploadDialog;
    QLineEdit *m_uploadTitleEdit;
    QTextEdit *m_uploadDescriptionEdit;
    QLineEdit *m_uploadTagsEdit;
    QPushButton *m_selectFileBtn;
    QPushButton *m_uploadSubmitBtn;
    QLabel *m_selectedFileLabel;
    QString m_selectedFilePath;
    
    // 订阅对话框相关（保留用于订阅功能）
    QDialog *m_subscriptionsDialog;
    QListWidget *m_tagsList;
    QPushButton *m_subscribeBtn;
    QListWidget *m_subscribedResourcesList;
    QPushButton *m_unsubscribeBtn;

    // 订阅资源分页（简单做法）
    QPushButton *m_subPrevBtn;
    QPushButton *m_subNextBtn;
    QLabel *m_subPageLabel;
    int m_subPage = 1;
    int m_subPageSize = 10;
    int m_subTotal = 0;

    // 主界面推荐资源分页
    QPushButton *m_mainPrevBtn;
    QPushButton *m_mainNextBtn;
    QLabel *m_mainPageLabel;
    int m_mainPage = 1;
    int m_mainPageSize = 20;
    int m_mainTotal = 0;
    
    // 审核界面相关（主界面内嵌）
    QComboBox *m_reviewStatusFilter;
    QListWidget *m_pendingResourcesList;
    QPushButton *m_approveBtn;
    QPushButton *m_rejectBtn;
    QPushButton *m_reviewRefreshBtn;

    // 审核列表分页
    QPushButton *m_reviewPrevBtn;
    QPushButton *m_reviewNextBtn;
    QLabel *m_reviewPageLabel;
    int m_reviewPage = 1;
    int m_reviewPageSize = 20;
    int m_reviewTotal = 0;
    
    // 网络
    QNetworkAccessManager *m_networkManager;
    
    // 用户信息
    int m_userId;
    QString m_userRole;
    
    // 辅助函数
    void loadPendingResources();
    //void loadPendingResources(const QString &status = "pending");
    void setupUploadDialog();
    void setupSubscriptionsDialog();
    void onUploadSubmit();
    void onSubscribeClicked();
    void onUnsubscribeClicked();
    void loadSubscriptions();
};

#endif // MAINWINDOW_H
