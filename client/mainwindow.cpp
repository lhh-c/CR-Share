#include "mainwindow.h"
#include <QMessageBox>
#include <QApplication>

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


void MainWindow::onSearchClicked()
{
    showError("搜索功能还没做");
}

void MainWindow::onRefreshClicked()
{
    showError("刷新功能还没做");
}

void MainWindow::onResourceSelected(QListWidgetItem *item)
{
    if (!item) return;
    QMessageBox::information(this, "提示", "资源详情暂未开放");
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::critical(this, "错误", message);
}
