#include "mainwindow.h"
#include "searchresultwindow.h"
#include "resourcedetailwindow.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGroupBox>
#include <QApplication>
#include <QScrollBar>
#include <QDialog>
#include <QDialogButtonBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    m_userId(0),
    m_userRole("student")
{
    setupUI();
}

void MainWindow::setUserId(int userId)
{
    m_userId = userId;
}

void MainWindow::setUserRole(const QString &role)
{
    m_userRole = role;
}

void MainWindow::setupUI()
{
    setWindowTitle("享阅 - 高校学习资源共享平台");
    setMinimumSize(1000, 700);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout *searchLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText("搜索资源...");
    m_searchBtn = new QPushButton("搜索");
    m_refreshBtn = new QPushButton("刷新");
    searchLayout->addWidget(m_searchEdit);
    searchLayout->addWidget(m_searchBtn);
    searchLayout->addWidget(m_refreshBtn);
    mainLayout->addLayout(searchLayout);

    QLabel *recommendedLabel = new QLabel("推荐资源");
    mainLayout->addWidget(recommendedLabel);
    m_recommendedResourcesList = new QListWidget();
    mainLayout->addWidget(m_recommendedResourcesList);

    connect(m_searchBtn, &QPushButton::clicked, this, &MainWindow::onSearchClicked);
    connect(m_refreshBtn, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_recommendedResourcesList, &QListWidget::itemClicked, this, &MainWindow::onResourceSelected);
}

void MainWindow::setupNetworkManager()
{
}

QJsonObject MainWindow::makeRequest(const QString &urlStr, const QString &method,
                                    const QJsonObject &jsonData)
{
    return QJsonObject();
}

void MainWindow::onNetworkReply(QNetworkReply *reply)
{
    reply->deleteLater();
}

void MainWindow::onSearchClicked()
{
    QString keyword = m_searchEdit->text();

    SearchResultWindow *searchWindow = new SearchResultWindow(keyword, m_userId, this);
    searchWindow->show();
}

void MainWindow::onRefreshClicked()
{
    loadResources();
}

void MainWindow::onResourceSelected(QListWidgetItem *item)
{
    if (!item) return;
    QMessageBox::information(this, "提示", "资源详情暂未开放");
}

void MainWindow::loadResources()
{
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::critical(this, "错误", message);
}

void MainWindow::showSuccess(const QString &message)
{
    QMessageBox::information(this, "成功", message);
}
