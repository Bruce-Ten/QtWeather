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
    {"小雪", "14"},
    {"中雪", "15"},
    {"大雪", "13"},
    {"霾", "53"},
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    ,settings(QCoreApplication::organizationName(), QCoreApplication::applicationName())
{
//    setStyleSheet("QLabel { color:white; }"
//                  "QScrollArea { border:none; }"
//                  "QScrollBar:horizontal { background:transparent; }");
    setWindowTitle("中国天气预报");
    move((QApplication::desktop()->width() - QApplication::desktop()->width())/2, (QApplication::desktop()->height() - QApplication::desktop()->height())/2);
    QWidget *widget = new QWidget;
//    setAttribute(Qt::WA_TranslucentBackground, true);

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
    labelPM = new QLabel("PM2.5");
    labelPM->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelPM);
    labelAQI = new QLabel("空气质量：");
    labelAQI->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelAQI);
    labelUT = new QLabel("更新时间");
    labelUT->setAlignment(Qt::AlignCenter);
    hbox->addWidget(labelUT);
    //用于修改城市
    searchEdit = new QLineEdit(city);
    searchEdit->setAlignment(Qt::AlignCenter);
    hbox->addWidget(searchEdit);
    searchButton = new QPushButton("更换城市");
    hbox->addWidget(searchButton);
    vbox->addLayout(hbox);
    connect(searchButton, SIGNAL(clicked(bool)), this, SLOT(changeCity()));
    searchButton->setShortcut(Qt::Key_Enter);
    QWidget *widget1 = new QWidget;
    widget1->setFixedSize(1200,180);
    QGridLayout *gridLayout = new QGridLayout;
    //从1月1日开始显示默认天气
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

    //托盘菜单
    systray = new QSystemTrayIcon(this);
    systray->setToolTip("托盘天气");
    systray->setIcon(QIcon(":/images/55.png"));
    systray->setVisible(true);
    QMenu *traymenu = new QMenu(this);
    QAction *action_forecast = new QAction("预报", traymenu);
    action_forecast->setIcon(QIcon::fromTheme("audio-volume-high"));
    QAction *action_refresh = new QAction("刷新", traymenu);
    action_refresh->setIcon(QIcon::fromTheme("view-refresh"));
    QAction *action_about = new QAction("关于", traymenu);
    action_about->setIcon(QIcon::fromTheme("help-about"));
    QAction *action_changelog = new QAction("更新日志", traymenu);
    action_changelog->setIcon(QIcon::fromTheme("document-new"));
    QAction *action_quit = new QAction("退出", traymenu);
    action_quit->setIcon(QIcon::fromTheme("application-exit"));
    traymenu->addAction(action_forecast);
    traymenu->addAction(action_refresh);
    traymenu->addAction(action_about);
    traymenu->addAction(action_changelog);
    traymenu->addAction(action_quit);
    systray->setContextMenu(traymenu);
    systray->show();

    connect(systray, SIGNAL(activated(QSystemTrayIcon::ActivationReason)), this, SLOT(trayActivated(QSystemTrayIcon::ActivationReason)));
    connect(action_forecast, SIGNAL(triggered(bool)), this, SLOT(showForecast()));
    connect(action_refresh, SIGNAL(triggered(bool)), this, SLOT(getWeather()));
    connect(action_about, SIGNAL(triggered(bool)), this, SLOT(about()));
    connect(action_changelog, SIGNAL(triggered(bool)), this, SLOT(changelog()));
    connect(action_quit, SIGNAL(triggered()), qApp, SLOT(quit()));
    QTimer *timer = new QTimer();
    timer->setInterval(1800000);
    timer->start();
    connect(timer, SIGNAL(timeout()), this, SLOT(getWeather()));
    getWeather();
}

void MainWindow::closeEvent(QCloseEvent *event)//此函数在QWidget关闭时执行
{
    //将主界面关闭  最小化到任务栏
    hide();

    event->ignore();
}

void MainWindow::getWeather()
{
    //规定时间格式
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
        cityID = "101181801";
        return;
    } else {
        bool ok;
        int dec = cityID.toInt(&ok, 10);
        Q_UNUSED(dec);
        if(!ok){
            labelComment->setText(reply->readAll());
        } else {
            labelComment->setText("");
        }
    }

    //读取本地文件代替网络API，更快更可靠。
    QFile file(":/cityID.txt");
    bool ok = file.open(QIODevice::ReadOnly);
    if (ok) {
        QTextStream TS(&file);
        QString s = TS.readAll();
        file.close();
        QStringList SL = s.split("\n");
            for(int i=0; i<SL.length(); i++){
                QString line = SL.at(i);
                if (line.contains(city)) {
                    cityID = line.left(line.indexOf("="));
                    break;
                }
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
    log += surl + "\n";
    log += BA + "\n";
    JD = QJsonDocument::fromJson(BA, &JPE);
    if (JPE.error == QJsonParseError::NoError) {
        //如果正常，没有发生错误 则执行以下操作
        if (JD.isObject()) {
            QJsonObject obj = JD.object();
            city = obj.value("cityInfo").toObject().value("city").toString().replace("市","");
            labelCity->setText(city);
            QString SUT = obj.value("cityInfo").toObject().value("updateTime").toString();
            labelUT->setText("更新时间：\n" + SUT);
            QJsonObject JO_data = JD.object().value("data").toObject();
            QString wendu = JO_data.value("wendu").toString() + "°C";
            labelTemp->setText(wendu);
            QString shidu = JO_data.value("shidu").toString();
            labelSD->setText("湿度\n" + shidu);
            QString pm25 = QString::number(JO_data.value("pm25").toInt());
            labelPM->setText("PM2.5\n" + pm25);
            QString quality = JO_data.value("quality").toString();
            QString ganmao = JO_data.value("ganmao").toString();
            labelAQI->setText("空气质量：" + quality + "\n" + ganmao);

            QJsonArray JA_forecast = JO_data.value("forecast").toArray();
            for (int i=0; i<15; i++) {
                labelDate[i]->setText(JA_forecast[i].toObject().value("date").toString() + "日");
                labelDate[i]->setAlignment(Qt::AlignCenter);
                QString wtype = JA_forecast[i].toObject().value("type").toString();
                qDebug() << wtype;
                QString icon_path = ":/images/" + weatherMap[wtype] + ".png";
                if(i == 0){
                    sw0 = JA_forecast[i].toObject().value("type").toString();
                    icon_path0 = icon_path;
                }
                QPixmap pixmap(icon_path);
                labelWImg[i]->setToolTip(wtype);
                labelWImg[i]->setPixmap(pixmap.scaled(50,50));
                labelWImg[i]->setAlignment(Qt::AlignCenter);
                labelWeather[i]->setText(wtype + "\n" + JA_forecast[i].toObject().value("high").toString() + "\n" + JA_forecast[i].toObject().value("low").toString() + "\n" + JA_forecast[i].toObject().value("fx").toString() + JA_forecast[i].toObject().value("fl").toString());
                labelWeather[i]->setAlignment(Qt::AlignCenter);
            }
            swn = city + "\n" + sw0 + "\n" + wendu + "\n湿度：" + shidu + "\nPM2.5：" + pm25 + "\n空气质量：" + quality +"\n" + ganmao + "\n更新时间：" + SUT;
            systray->setToolTip(swn);
            systray->setIcon(QIcon(icon_path0));
        }
    }
}

void MainWindow::trayActivated(QSystemTrayIcon::ActivationReason reason)
{
    qDebug() << reason;
    switch(reason) {
//    case QSystemTrayIcon::DoubleClick:    //双击与单击会冲突，暂时取消该功能
//        showForecast();
//        break;
    case QSystemTrayIcon::MiddleClick:    //鼠标中键
        showForecast();
        break;
    case QSystemTrayIcon::Trigger:        //单击
        systray->showMessage("实时天气", swn, QSystemTrayIcon::MessageIcon::Information, 9000);
        break;
    default:
        break;
    }
}

void MainWindow::about()
{
    QMessageBox MB(QMessageBox::NoIcon, "关于", "中国天气预报 2.6\n一款基于Qt的天气预报程序。\n作者：牛童\nE-mail: 1248449622@qq.com\n学号：201941412119\n"
                                              "天气API：https://www.sojson.com/blog/305.html\n\n");
    MB.setIconPixmap(QPixmap(":/icon.png"));
    MB.exec();
}

void MainWindow::changelog()
{
    QString s = "v.2.6 \n天气预报增加到15天。\n滚动布局。\n增加更换城市功能。\n图标加入资源，方便单文件运行。\n城市名转ID，读取本地文件代替网络API，更快更可靠。\n\n"
                "v.2.5 \n1.更换失效的天气预报API，优化结构，解决窗口顶置问题。\n\n"
                "v.2.4 \n1.增加写log文件，不需要调试也可以查看API的问题了。\n\n"
                "v.2.3 \n1.修复：日期数据不含月转换为日期引起的日期错误。\n\n"
                "v.2.2 \n1.使用本地图标代替边缘有白色的网络图标，可用于暗色背景了！\n\n"
                "v.2.1 \n1.窗体始终居中。\n\n"
                "v.2.0 \n1.使用QScript库代替QJsonDocument解析JSON，开发出兼容Qt4的版本。\n"
                "2.单击图标弹出实时天气消息，弥补某些系统不支持鼠标悬浮信息的不足。\n"
                "3.由于QScriptValueIterator.value().property解析不了某个JSON，使用QScriptValue.property.property代替。\n"
                "4.托盘右键增加一个刷新菜单。\n\n"
                "v.1.0 \n1.动态修改天气栏托盘图标，鼠标悬浮显示实时天气，点击菜单弹出窗口显示7天天气预报。\n2.每30分钟自动刷新一次。\n3.窗体透明。";
    QDialog *dialog = new QDialog;
    dialog->setWindowTitle("更新历史");
    dialog->setFixedSize(400, 300);
    QVBoxLayout *vbox = new QVBoxLayout;
    QTextBrowser *textBrowser = new QTextBrowser;
    textBrowser->setText(s);
    textBrowser->zoomIn();
    vbox->addWidget(textBrowser);
    QHBoxLayout *hbox = new QHBoxLayout;
    QPushButton *pushButton_confirm = new QPushButton("确定");
    hbox->addStretch();
    hbox->addWidget(pushButton_confirm);
    hbox->addStretch();
    vbox->addLayout(hbox);
    dialog->setLayout(vbox);
    dialog->show();
    connect(pushButton_confirm, SIGNAL(clicked()), dialog, SLOT(accept()));
    if(dialog->exec() == QDialog::Accepted){
        dialog->close();
    }
}

void MainWindow::showForecast()
{
    //显示主界面
    move((QApplication::desktop()->width() - width())/2, (QApplication::desktop()->height() - height())/2);
    show();
    raise();
    activateWindow();
}

void MainWindow::changeCity()
{
    //获取到文本框中的城市
    settings.setValue("City", searchEdit->text());
    //调用函数 改变城市
    getWeather();
    searchEdit->setText("");
}
