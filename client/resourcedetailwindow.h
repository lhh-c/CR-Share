#ifndef RESOURCEDETAILWINDOW_H
#define RESOURCEDETAILWINDOW_H

#include <QMainWindow>
#include <QLabel>

class ResourceDetailWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ResourceDetailWindow(int resourceId, int userId, QWidget *parent = nullptr);

private:
    void setupUI();

    int m_resourceId;
    int m_userId;

    QLabel *m_tipLabel;
};

#endif
