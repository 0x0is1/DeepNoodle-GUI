#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <iostream>
#include <fstream>
#include "base64.cpp"
#include <QTimer>
#include <QThread>
#include <QImage>

using namespace std;

static std::string BASE_PREFIX = base64_decode("aHR0cHM6Ly9kZWVwbnVkZS50bw==");
static int TOKLEN_SESSIONID = 32;
static int TOKLEN_IMGID = 15;
static std::string session_id = "Not Available";
static std::string image_id = "Not Available";
static int status_code = 000;
static std::string delimiter = "://";
static std::string host_name = BASE_PREFIX.substr(BASE_PREFIX.find(delimiter)+3, BASE_PREFIX.length());
static bool token_validation = false;

class url_prefixes{
public:
    std::string status = "/api/status/";
    std::string upload_request = "/api/request-upload/";
    std::string image_upload = "/upload/";
    std::string get_im_prefix = "/img/";
    std::string get_im_suffix = "/watermark.jpg";
};

static url_prefixes urls;

MainWindow::MainWindow(QWidget *parent):QMainWindow(parent),ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    url_prefixes urls;
    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);

    const QUrl url(QString::fromStdString(BASE_PREFIX));
    QNetworkRequest request(url);

    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:75.0) Gecko/20100101 Firefox/75.0");
    request.setRawHeader("Accept-Encoding", "gzip, deflate, br");
    request.setRawHeader("Accept","text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.5");
    request.setRawHeader("Connection", "Keep-Alive");

    QNetworkReply *reply = mgr->get(request);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            ui->statusCode->setNum(status_code);
            ui->sessidLabel->setText(QString::fromStdString(session_id));
            ui->imgidLabel->setText(QString::fromStdString(image_id));

        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
        }
        reply->deleteLater();
    });
    ui->getimgButton->setDisabled(false);
}


std::string ran_string(int n)
{
    char alphabet[36] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g',
                          'h', 'i', 'j', 'k', 'l', 'm', 'n',
                          'o', 'p', 'q', 'r', 's', 't', 'u',
                          'v', 'w', 'x', 'y', 'z', '0', '1',
                          '2', '3', '4', '5', '6', '7', '8',
                          '9'
                        };

    string res = "";
    for (int i = 0; i < n; i++)
        res = res + alphabet[rand() % 36];

    return res;
}


MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_selectButton_clicked()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Choose file"), "", tr("*.jpg"));
    if (QString::compare(filename, QString()) != 0){
        QImage image;
        bool valid_img = image.load(filename);
        if (valid_img){
            image = image.scaledToWidth(ui->imgHolder->width(), Qt::SmoothTransformation);
            ui->imgHolder->setPixmap(QPixmap::fromImage(image));
            ui->imgName->setText(filename);
            ui->getimgButton->setDisabled(true);
            validatetoken();
        }
    }
}

void MainWindow::validatetoken()
{
    token_validation = false;
    srand(time(NULL));
    // Getting upload permissions
    image_id = ran_string(TOKLEN_IMGID);
    session_id = ran_string(TOKLEN_SESSIONID);
    ui->sessidLabel->setText(QString::fromStdString(session_id));
    ui->imgidLabel->setText(QString::fromStdString(image_id));

    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    const QUrl url(QString::fromStdString(BASE_PREFIX + urls.upload_request + image_id));
    QNetworkRequest request(url);
    request.setRawHeader("Host", QByteArray::fromStdString(host_name));
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:75.0) Gecko/20100101 Firefox/75.0");
    request.setRawHeader("Accept","*/*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.5");
    request.setRawHeader("Accept-Encoding", "gzip, deflate, br");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setRawHeader("Origin", QByteArray::fromStdString(BASE_PREFIX));
    request.setRawHeader("DNT", "1");
    request.setRawHeader("Connection", "Keep-Alive");
    request.setRawHeader("Cookie", "userid="+QByteArray::fromStdString(session_id) +
                         ";identifier=" + QByteArray::fromStdString(session_id));
    request.setRawHeader("TE", "Trailers");

    QByteArray data = "";
    QNetworkReply *reply = mgr->post(request, data);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            ui->statusCode->setNum(status_code);
            ui->sessidLabel->setText(QString::fromStdString(session_id));
            ui->imgidLabel->setText(QString::fromStdString(image_id));
            QByteArray contents = QByteArray(reply->readAll());
            if (status_code==200){
                QJsonDocument doc(QJsonDocument::fromJson(contents));
                QJsonObject json = doc.object();
                if (json["status"].toString()!="ok"){
                    return;
                }
                token_validation = true;
                uploadimg();
            }
        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << contents;
        }
        reply->deleteLater();
    });
    ui->tokenvLabel->setNum(token_validation);
}

void MainWindow::uploadimg()
{
    QString filename = ui->imgName->text();
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) return;
    QByteArray blob = file.readAll();
    file.close();

    // Uploading image
    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    const QUrl url(QString::fromStdString(BASE_PREFIX + urls.image_upload + image_id));
    QNetworkRequest request(url);

    request.setRawHeader("Host", QByteArray::fromStdString(host_name));
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:75.0) Gecko/20100101 Firefox/75.0");
    request.setRawHeader("Accept","*/*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.5");
    request.setRawHeader("Accept-Encoding", "gzip, deflate, br");
    request.setRawHeader("Content-Type", "image/jpeg");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setRawHeader("Content-Length", QByteArray::fromStdString(to_string(blob.length()-3)));
    request.setRawHeader("Origin", QByteArray::fromStdString(BASE_PREFIX));
    request.setRawHeader("DNT", "1");
    request.setRawHeader("Connection", "Keep-Alive");
    request.setRawHeader("Referer", QByteArray::fromStdString(BASE_PREFIX));
    request.setRawHeader("Cookie", "userid="+QByteArray::fromStdString(session_id) +
                         ";identifier=" + QByteArray::fromStdString(session_id));
    request.setRawHeader("TE", "Trailers");

    QNetworkReply *reply = mgr->put(request, blob);
    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if(status_code==200 and token_validation){
                ui->getimgButton->setDisabled(false);
            }
            else {
                return;
            }
        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << contents;
        }
        reply->deleteLater();
    });

}

void MainWindow::on_getimgButton_clicked()
{
    qDebug() << "Sleep 20";
    QThread::sleep(20);

    QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
    const QUrl url(QString::fromStdString(BASE_PREFIX + urls.get_im_prefix + image_id + urls.get_im_suffix));
    QNetworkRequest request(url);
    request.setRawHeader("Host", QByteArray::fromStdString(host_name));
    request.setRawHeader("User-Agent", "Mozilla/5.0 (X11; Linux x86_64; rv:75.0) Gecko/20100101 Firefox/75.0");
    request.setRawHeader("Accept","image/webp,*/*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.5");
    request.setRawHeader("Accept-Encoding", "gzip, deflate, br");
    request.setRawHeader("DNT", "1");
    request.setRawHeader("Connection", "Keep-Alive");
    request.setRawHeader("Referer", QByteArray::fromStdString(BASE_PREFIX));
    request.setRawHeader("Cookie", "userid="+QByteArray::fromStdString(session_id) +
                         ";identifier=" + QByteArray::fromStdString(session_id));
    request.setRawHeader("TE", "Trailers");

    QNetworkReply *reply = mgr->get(request);
    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QImage image;
            bool valid_img = image.loadFromData(reply->readAll());
            if (valid_img){
                image = image.scaledToWidth(ui->imgHolder->width(), Qt::SmoothTransformation);
                ui->imgHolder->setPixmap(QPixmap::fromImage(image));
            }
            qDebug() << status_code;
        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << contents;
        }
        reply->deleteLater();
    });

}

void MainWindow::on_saveimgButton_clicked()
{
    QString filename = QFileDialog::getSaveFileName(this, tr("Save as"), "", tr("Images (*.png, *.jpg, *.jpeg)"));
    const QPixmap* pixmap = ui->imgHolder->pixmap();
    if (pixmap){
        QImage image = pixmap->toImage();
        image.save(filename);
    }
}
