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
    void loadResources();
    void onNetworkReply(QNetworkReply *reply);

private:
    void setupUI();
    void setupNetworkManager();
    QJsonObject makeRequest(const QString &url, const QString &method = "GET",
                            const QJsonObject &data = QJsonObject());
    void showError(const QString &message);
    void showSuccess(const QString &message);
    
    QLineEdit *m_searchEdit;
    QPushButton *m_searchBtn;
    QPushButton *m_refreshBtn;
    QListWidget *m_recommendedResourcesList;
    
    QNetworkAccessManager *m_networkManager;
    
    int m_userId;
    QString m_userRole;
};

#endif // MAINWINDOW_H
