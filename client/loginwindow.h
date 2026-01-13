#ifndef LOGINWINDOW_H
#define LOGINWINDOW_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

class LoginWindow : public QDialog
{
    Q_OBJECT

public:
    explicit LoginWindow(QWidget *parent = nullptr);
    int getUserId() const { return m_userId; }
    QString getUserRole() const { return m_userRole; }

private slots:
    void onLoginClicked();
    void onRegisterClicked();
    void onNetworkReply(QNetworkReply *reply);

private:
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QPushButton *m_loginBtn;
    QPushButton *m_registerBtn;
    QNetworkAccessManager *m_networkManager;
    
    int m_userId;
    QString m_userRole;
};

#endif // LOGINWINDOW_H
