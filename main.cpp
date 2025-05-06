#include "FaceAuthClient.h"
#include <QtWidgets/QApplication>
#include <QDir>
#include <QDebug>
#include <QFile>
#include <QStandardPaths>
#include <QNetworkProxyFactory>

int main(int argc, char *argv[])
{
    // 禁用网络代理，避免连接问题
    QNetworkProxyFactory::setUseSystemConfiguration(false);
    
    QApplication a(argc, argv);
    
    // 设置应用信息
    QApplication::setApplicationName("FaceAuthAccess 人脸识别系统");
    QApplication::setOrganizationName("FaceAuthTeam");
    QApplication::setApplicationVersion("1.0.0");
    
    // 添加OpenCV DLL路径到搜索路径
    QDir::addSearchPath("opencv_dll", "E:/qt_project/opencv/build/x64/vc16/bin");
    qDebug("OpenCV DLL search path added: %s", qPrintable("E:/qt_project/opencv/build/x64/vc16/bin"));
    
    // 创建主窗口并显示
    FaceAuthClient w;
    w.show();
    
    return a.exec();
}
