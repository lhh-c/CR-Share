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
    
    while (true) {
    LoginWindow loginWindow;
        if (loginWindow.exec() != QDialog::Accepted) {
            break;
        }

        MainWindow mainWindow;
        mainWindow.setUserId(loginWindow.getUserId());
        mainWindow.setUserRole(loginWindow.getUserRole());

        QObject::connect(&mainWindow, &MainWindow::logoutRequested, &app, [&app]() {
            app.quit();
        });

        mainWindow.show();
        app.exec();
    }
    
    return 0;
}
