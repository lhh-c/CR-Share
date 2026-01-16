#include <QApplication>
#include "mainwindow.h"
#include "loginwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用信息
    app.setApplicationName("享阅");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("重庆师范大学");
    
    // 显示登录窗口
    LoginWindow loginWindow;
    if (loginWindow.exec() == QDialog::Accepted) {
        // 登录成功，显示主窗口
        MainWindow mainWindow;
        mainWindow.setUserId(loginWindow.getUserId());
        mainWindow.setUserRole(loginWindow.getUserRole());
        mainWindow.show();
        return app.exec();
    }
    
    return 0;
}
