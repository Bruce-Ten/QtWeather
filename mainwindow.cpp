#include "mainwindow.h"
#include <QApplication>
#include <QDesktopWidget>
#include <QDateTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QJsonParseError>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include <QScrollArea>
#include <QDir>
#include <QDesktopServices>
#include <QTextBrowser>
#include <QPushButton>
#include <QGridLayout>
#include <QLineEdit>

static const QMap<QString, QString> weatherMap {
    {"NA", "55"},
    {"晴", "0"},
    {"多云", "1"},
    {"阴", "2"},
    {"阵雨", "3"},
    {"雷阵雨", "4"},
    {"小雨", "7"},
    {"中雨", "8"},
    {"大雨", "9"},
    {"中到大雨", "9"},
    {"雪", "13"},
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    ,settings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
    setStyleSheet("QLabel { color:white; }"
                  "QScrollArea { border:none; }"
                  "QScrollBar:horizontal { background:transparent; }");
    setWindowTitle("中国天气预报");
    setFixedSize(700,300);
    move((QApplication::desktop()->width() - QApplication::desktop()->width())/2, (QApplication::desktop()->height() - QApplication::desktop()->height())/2);
    QWidget *widget = new QWidget;
    setAttribute(Qt::WA_TranslucentBackground, true);
    widget->setAttribute(Qt::WA_TranslucentBackground, true);

    QVBoxLayout *vbox = new QVBoxLayout;
    QHBoxLayout *hbox = new QHBoxLayout;
    labelCity = new QLabel("城市");
    labelCity->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelCity);
    labelTemp = new QLabel("温度");
    labelTemp->setStyleSheet("font-size:30px;");
    labelTemp->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelTemp);
    labelSD = new QLabel("湿度");
    labelSD->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelSD);
    //labelWind = new QLabel("风向?\n风力?");
    //labelWind->setAlignment(Qt::AlignCenter);
    //hbox->addWidget(labelWind);
    labelPM = new QLabel("PM2.5");
    labelPM->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelPM);
    labelAQI = new QLabel("空气质量");
    labelAQI->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelAQI);
    labelUT = new QLabel("更新");
    labelUT->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelUT);
    searchEdit = new QLineEdit(city);
    searchEdit->setAlignment(Qt::AlignCenter);
    hbox->addWidget(searchEdit);
    searchButton = new QPushButton("更换城市");
    hbox->addWidget(searchButton);
    vbox->addLayout(hbox);

    QWidget *widget1 = new QWidget;
    widget1->setFixedSize(1200,180);
    widget1->setAttribute(Qt::WA_TranslucentBackground, true);
    QGridLayout *gridLayout = new QGridLayout;
    for(int i=0; i<15; i++){
        labelDate[i] = new QLabel("1月1日\n星期一");
        labelDate[i]->setAlignment(Qt::AlignCenter);
        gridLayout->addWidget(labelDate[i],1,i);
        labelWImg[i] = new QLabel("");
        labelWImg[i]->setPixmap(QPixmap(QApplication::applicationDirPath() + "/images/55.png").scaled(50,50));
        labelWImg[i]->setAlignment(Qt::AlignCenter);
        gridLayout->addWidget(labelWImg[i],2,i);
        labelWeather[i] = new QLabel("晴\n15°C ~ 20°C\n北风1级");
        labelWeather[i]->setAlignment(Qt::AlignCenter);
        gridLayout->addWidget(labelWeather[i],3,i);
    }
    widget1->setLayout(gridLayout);
    QScrollArea *scrollArea = new QScrollArea;
    scrollArea->setWidget(widget1);
    vbox->addWidget(scrollArea);

    labelComment = new QLabel;
    vbox->addWidget(labelComment);
    widget->setLayout(vbox);
    setCentralWidget(widget);


}

void MainWindow::closeEvent(QCloseEvent *event)//此函数在QWidget关闭时执行
{
    hide();
    //不退出App
    event->ignore();
}

void MainWindow::getWeather()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString log = currentDateTime.toString("yyyy/MM/dd HH:mm:ss") + "\n";

    QUrl url;
    QString surl;
    QNetworkAccessManager manager;
    QNetworkReply *reply;
    QEventLoop loop;
    QByteArray BA;
    QJsonDocument JD;
    QJsonParseError JPE;

    city = settings.value("City", "").toString();
    if (city == "") {
        surl = "http://ip-api.com/json/?lang=zh-CN";
        url.setUrl(surl);
        reply = manager.get(QNetworkRequest(url));
        //请求结束并下载完成后，退出子事件循环
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        //开启子事件循环
        loop.exec();
        BA = reply->readAll();
        qDebug() << surl ;
        qDebug() << BA;
        log += surl + "\n";
        log += BA + "\n";
        JD = QJsonDocument::fromJson(BA, &JPE);
        if(JPE.error == QJsonParseError::NoError) {
            if(JD.isObject()) {
                QJsonObject obj = JD.object();
                if(obj.contains("city")) {
                    QJsonValue JV_city = obj.take("city");
                    if(JV_city.isString()) {
                        city = JV_city.toString();
                    }
                }
            }
        }
    }

    //城市名转ID
    //    /*
        surl = "http://hao.weidunewtab.com/tianqi/city.php?city=" + city;
        url.setUrl(surl);
        reply = manager.get(QNetworkRequest(url));
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();
        cityID = reply->readAll();
        qDebug() << surl;
        qDebug() << cityID;
        log += surl + "\n";
        log += cityID + "\n";
        if (cityID == "") {
            labelComment->setText("错误：城市名返回城市ID为空");
            return;
        } else if(cityID == "ERROR") {
            labelComment->setText(city + " ：城市名称错误");
            return;
        } else {
            bool ok;
            int dec = cityID.toInt(&ok, 10);
            Q_UNUSED(dec);
            if(!ok){
                labelComment->setText(reply->readAll());
            }
        }

        //获取天气信息
        surl = "http://t.weather.itboy.net/api/weather/city/"+ cityID;
            url.setUrl(surl);
            reply = manager.get(QNetworkRequest(url));
            QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
            loop.exec();
            BA = reply->readAll();
            qDebug() << surl;
            //qDebug() << BA;
            log += surl + "\n";
            log += BA + "\n";
            JD = QJsonDocument::fromJson(BA, &JPE);
            //qDebug() << "QJsonParseError:" << JPE.errorString();
            if (JPE.error == QJsonParseError::NoError) {
                if (JD.isObject()) {
                    QJsonObject obj = JD.object();
                    city = obj.value("cityInfo").toObject().value("city").toString().replace("市","");
                    labelCity->setText(city);
                    QString SUT = obj.value("cityInfo").toObject().value("updateTime").toString();
                    labelUT->setText("更新\n" + SUT);
                    //if (obj.contains("data")) {
                    QJsonObject JO_data = JD.object().value("data").toObject();
                    QString wendu = JO_data.value("wendu").toString() + "°C";
                    labelTemp->setText(wendu);
                    QString shidu = JO_data.value("shidu").toString();
                    labelSD->setText("湿度\n" + shidu);
                    //labelWind->setText(JO_data.value("wendu").toString() + "\n" + JO_data.value("WS").toString());
                    QString pm25 = QString::number(JO_data.value("pm25").toInt());
                    labelPM->setText("PM2.5\n" + pm25);
                    QString quality = JO_data.value("quality").toString();
                    QString ganmao = JO_data.value("ganmao").toString();
                    labelAQI->setText("空气质量 " + quality + "\n" + ganmao);

                    //if(JO_data.contains("forecast")){
                    QJsonArray JA_forecast = JO_data.value("forecast").toArray();
                    for (int i=0; i<15; i++) {
                        labelDate[i]->setText(JA_forecast[i].toObject().value("date").toString());
                        labelDate[i]->setAlignment(Qt::AlignCenter);
                        QString wtype = JA_forecast[i].toObject().value("type").toString();
                        qDebug() << wtype;
                        QString icon_path = ":/images/" + weatherMap[wtype] + ".png";
                        if(i == 0){
                            sw0 = JA_forecast[i].toObject().value("type").toString();
                            icon_path0 = icon_path;
                        }
                        //qDebug() << icon_path;
                        QPixmap pixmap(icon_path);
                        labelWImg[i]->setToolTip(wtype);
                        labelWImg[i]->setPixmap(pixmap.scaled(50,50));
                        labelWImg[i]->setAlignment(Qt::AlignCenter);
                        labelWeather[i]->setText(wtype + "\n" + JA_forecast[i].toObject().value("high").toString() + "\n" + JA_forecast[i].toObject().value("low").toString() + "\n" + JA_forecast[i].toObject().value("fx").toString() + JA_forecast[i].toObject().value("fl").toString());
                        labelWeather[i]->setAlignment(Qt::AlignCenter);
                    }
                    //}
                    //}
                    swn = city + "\n" + sw0 + "\n" + wendu + "\n湿度：" + shidu + "\nPM2.5：" + pm25 + "\n空气质量：" + quality +"\n" + ganmao + "\n更新：" + SUT;
                    //qDebug() << swn;
                    systray->setToolTip(swn);
                    systray->setIcon(QIcon(icon_path0));
                }
            }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    qDebug() << reason;
    switch(reason) {
    case QSystemTrayIcon::Trigger:
        //systray->showMessage("实时天气", swn, QSystemTrayIcon::MessageIcon::Information, 9000);//图标被修改
        break;
    case QSystemTrayIcon::DoubleClick://无效
        //showForecast();
        break;
    case QSystemTrayIcon::MiddleClick:
        showForecast();
        break;
    default:
        break;
    }
}

void MainWindow::showForecast()
{
    move((QApplication::desktop()->width() - width())/2, (QApplication::desktop()->height() - height())/2);
    show();
    raise();
    activateWindow();
}
