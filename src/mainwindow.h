#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QtNetwork/QNetworkAccessManager>
#include <QMainWindow>
#include <QFileDialog>
#include <QtNetwork/QNetworkReply>
namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_selectButton_clicked();

    void validatetoken();

    void uploadimg();

    void on_getimgButton_clicked();

    void on_saveimgButton_clicked();

private:
    Ui::MainWindow *ui;
    QNetworkAccessManager *mgr;
    QNetworkRequest request;
};

#endif // MAINWINDOW_H
