#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

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

private:
    void setupUI();
    void showError(const QString &message);

    QLineEdit *m_searchEdit;
    QPushButton *m_searchBtn;
    QPushButton *m_refreshBtn;
    QListWidget *m_recommendedResourcesList;

    int m_userId;
    QString m_userRole;
};

#endif // MAINWINDOW_H
