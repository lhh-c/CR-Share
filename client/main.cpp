#include <QApplication>
#include "mainwindow.h"
#include "loginwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    app.setApplicationName("享阅");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("重庆师范大学");
    
    LoginWindow loginWindow;
    loginWindow.exec();

    MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}
