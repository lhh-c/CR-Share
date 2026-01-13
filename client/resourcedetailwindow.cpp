#include "resourcedetailwindow.h"
#include <QVBoxLayout>
#include <QWidget>

ResourceDetailWindow::ResourceDetailWindow(int resourceId, int userId, QWidget *parent)
    : QMainWindow(parent)
    , m_resourceId(resourceId)
    , m_userId(userId)
    , m_tipLabel(nullptr)
{
    setWindowTitle("资源详情 - 享阅");
    setMinimumSize(700, 450);

    setupUI();
}

void ResourceDetailWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *layout = new QVBoxLayout(centralWidget);

    m_tipLabel = new QLabel("详情页开发中", this);
    m_tipLabel->setMinimumHeight(200);
    m_tipLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_tipLabel);
}
