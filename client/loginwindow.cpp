#include "loginwindow.h"
#include <QMessageBox>
#include <QUrl>
#include <QNetworkRequest>

LoginWindow::LoginWindow(QWidget *parent)
    : QDialog(parent)
    , m_userId(0)
{
    setWindowTitle("享阅 - 登录");
    setFixedSize(350, 200);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    
    QLabel *titleLabel = new QLabel("享阅");
    titleLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText("用户名");
    layout->addWidget(m_usernameEdit);
    
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setPlaceholderText("密码");
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(m_passwordEdit);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_loginBtn = new QPushButton("登录");
    m_registerBtn = new QPushButton("注册");
    btnLayout->addWidget(m_loginBtn);
    btnLayout->addWidget(m_registerBtn);
    layout->addLayout(btnLayout);
    
    m_networkManager = new QNetworkAccessManager(this);

    connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
    connect(m_registerBtn, &QPushButton::clicked, this, &LoginWindow::onRegisterClicked);
}

void LoginWindow::onLoginClicked()
{
    QString username = m_usernameEdit->text();
    QString password = m_passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入用户名和密码");
        return;
    }

    m_userId = 1;
    m_userRole = "student";

    QMessageBox::information(this, "成功", "登录成功");
    accept();
}

void LoginWindow::onRegisterClicked()
{
    QString username = m_usernameEdit->text();
    QString password = m_passwordEdit->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "警告", "请输入用户名和密码");
        return;
    }

    QMessageBox::information(this, "成功", "注册成功，请登录");
}

void LoginWindow::onNetworkReply(QNetworkReply *reply)
{
    reply->deleteLater();
}
