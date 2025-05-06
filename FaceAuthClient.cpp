#include "FaceAuthClient.h"
#include "ServerSettingsDialog.h"
#include <opencv2/opencv.hpp>
#include <QDebug>
#include <QDir>
#include <QString>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QBuffer>
#include <QFile>
#include <QFileDialog>
#include <QSettings>
#include <QCamera>
#include <QBoxLayout>
#include <QVideoFrame>
//构造时初始化
FaceAuthClient::FaceAuthClient(QWidget* parent)
    : QMainWindow(parent),
    m_socket(nullptr),
    m_camera(nullptr),
    m_captureSession(nullptr),
    m_videoSink(nullptr),
    m_imageCapture(nullptr),
    m_isCameraActive(false),
    m_serverAddress("142.171.34.18"),
    m_serverPort(8101)
{
    ui.setupUi(this);

    // 初始化OpenCV
    if (!initOpenCV()) {
        QMessageBox::critical(this, "Error", "Failed to initialize OpenCV!");
        return;
    }

    // 初始化网络连接
    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &FaceAuthClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &FaceAuthClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &FaceAuthClient::onSocketReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred, this, &FaceAuthClient::onSocketError);

    // 初始化摄像头
    m_captureSession = new QMediaCaptureSession(this);
    m_videoSink = new QVideoSink(this);
    m_imageCapture = new QImageCapture(this);
    
    // 将视频输出连接到VideoSink
    m_captureSession->setVideoSink(m_videoSink);
    m_captureSession->setImageCapture(m_imageCapture);
    
    // 连接视频帧信号
    connect(m_videoSink, &QVideoSink::videoFrameChanged, this, &FaceAuthClient::onFrameAvailable);

    // 绑定按钮事件
    connect(ui.loginButton, &QPushButton::clicked, this, &FaceAuthClient::onLoginButtonClicked);
    connect(ui.captureButton, &QPushButton::clicked, this, &FaceAuthClient::onCaptureButtonClicked);
    connect(ui.registerButton, &QPushButton::clicked, this, &FaceAuthClient::onRegisterButtonClicked);
    
    // 绑定菜单事件
    connect(ui.actionServer_Settings, &QAction::triggered, this, &FaceAuthClient::onServerSettingsTriggered);
    connect(ui.actionExit, &QAction::triggered, this, &FaceAuthClient::close);
    
    // 自动启动摄像头
    startCamera();
    
    ui.statusLabel->setText("Ready to login");
}
//析构时关闭socket连接
FaceAuthClient::~FaceAuthClient()
{
    stopCamera();
    
    if (m_socket && m_socket->isOpen()) {
        m_socket->close();
    }
}

void FaceAuthClient::onServerSettingsTriggered()
{
    ServerSettingsDialog dialog(m_serverAddress, m_serverPort, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_serverAddress = dialog.getServerAddress();
        m_serverPort = dialog.getServerPort();
        
        // 如果已经连接，则需要断开重连
        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->disconnectFromHost();
        }
        
        ui.statusLabel->setText("Server settings updated");
        
        // 保存设置到配置文件
        QSettings settings("FaceAuthTeam", "FaceAuthAccess");
        settings.setValue("ServerAddress", m_serverAddress);
        settings.setValue("ServerPort", m_serverPort);
    }
}

bool FaceAuthClient::initOpenCV()
{
    try {
        // 输出OpenCV版本信息
        qDebug() << "OpenCV版本:" << CV_VERSION;
        
        // 执行一个简单的OpenCV操作来测试库是否正确加载
        cv::Mat testMat(10, 10, CV_8UC1, cv::Scalar(0));
        
        // 如果能创建并操作矩阵，返回成功
        bool success = !testMat.empty();
        
        if (success) {
            // 测试内存分配和深拷贝操作
            cv::Mat testClone = testMat.clone();
            if (testClone.empty()) {
                qDebug() << "OpenCV克隆操作失败";
                return false;
            }
            
            // 如果没有崩溃，说明基本操作正常
            qDebug() << "OpenCV初始化成功";
        } else {
            qDebug() << "OpenCV测试矩阵创建失败";
        }
        
        return success;
    }
    catch (const cv::Exception& e) {
        qDebug() << "OpenCV异常:" << e.what();
        return false;
    }
    catch (const std::exception& e) {
        qDebug() << "标准异常:" << e.what();
        return false;
    }
    catch (...) {
        qDebug() << "OpenCV初始化过程中发生未知异常";
        return false;
    }
}

bool FaceAuthClient::startCamera()
{
    if (m_isCameraActive) {
        return true; // 摄像头已经启动
    }
    
    try {
        // 获取可用的相机设备
        const QList<QCameraDevice> cameras = QMediaDevices::videoInputs();
        if (cameras.isEmpty()) {
            QMessageBox::warning(this, "Camera Error", "没有找到可用的相机设备");
            return false;
        }
        
        // 使用第一个找到的相机
        if (m_camera) {
            delete m_camera;
        }
        m_camera = new QCamera(cameras.first(), this);
        
        // 设置相机质量和帧率以减少CPU负担
        QCameraFormat bestFormat;
        int bestResolution = 0;
        
        // 查找最适合的相机格式（分辨率适中）
        for (const QCameraFormat &format : cameras.first().videoFormats()) {
            //videoFormats()：这是QCameraDevice类的一个方法
            //返回该摄像头设备支持的视频格式列表。返回值是一个QList<QCameraFormat>
            // 640x480
            int resolution = format.resolution().width() * format.resolution().height();
            if (bestFormat.isNull() || 
                (resolution >= 307200 && resolution < bestResolution) || // 640x480=307200
                (bestResolution < 307200 && resolution > bestResolution)) {
                bestFormat = format;
                bestResolution = resolution;
            }
        }
        
        // 如果找到了合适格式，设置相机格式
        if (!bestFormat.isNull()) {
            m_camera->setCameraFormat(bestFormat);
            qDebug() << "设置相机分辨率:" << bestFormat.resolution() 
                     << "最大帧率:" << bestFormat.maxFrameRate();
        }
        
        // 设置捕获会话的相机
        m_captureSession->setCamera(m_camera);
        
        // 启动相机
        m_camera->start();
        m_isCameraActive = true;
        
        qDebug() << "相机已启动";
        return true;
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, "Camera Error", 
                            QString("Exception: %1").arg(e.what()));
        return false;
    }
    catch (...) {
        QMessageBox::critical(this, "Camera Error", "初始化摄像头失败");
        return false;
    }
}

void FaceAuthClient::stopCamera()
{
    if (!m_isCameraActive) {
        return;
    }
    
    if (m_camera) {
        m_camera->stop();
    }
    
    m_isCameraActive = false;
    ui.cameraView->setText("Camera stopped");
}

void FaceAuthClient::onFrameAvailable(const QVideoFrame &frame)
{
    if (!m_isCameraActive) {
        return;
    }
    
    try {
        // 仅将QVideoFrame转换为QImage用于UI显示
        QImage image = frame.toImage();
        
        if (image.isNull()) {
            return;
        }
        
        // 显示图像到UI 
        QPixmap pixmap = QPixmap::fromImage(image);
        ui.cameraView->setPixmap(pixmap.scaled(
            ui.cameraView->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    catch (const std::exception& e) {
        qDebug() << "处理帧时发生异常:" << e.what();
    }
    catch (...) {
        qDebug() << "处理帧时发生未知异常";
    }
}

QImage FaceAuthClient::matToQImage(const cv::Mat& mat)
{
    // 检查图像是否为空
    if (mat.empty()) {
        qDebug() << "Empty matrix in matToQImage";
        return QImage();
    }
    
    // 检查数据连续性
    if (!mat.isContinuous()) {
        qDebug() << "Matrix is not continuous, creating a continuous copy";
        return matToQImage(mat.clone());
    }
    
    // 检查数据是否有效
    if (mat.data == nullptr) {
        qDebug() << "Matrix data is null";
        return QImage();
    }
    
    try {
        // 根据图像类型转换为QImage
        if (mat.type() == CV_8UC3) {
            // BGR → RGB转换
            cv::Mat rgbMat;
            cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
            
            QImage image(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                    static_cast<int>(rgbMat.step), QImage::Format_RGB888);
            
            // 创建深拷贝以避免OpenCV图像被释放后出现问题
            return image.copy();
        }
        else if (mat.type() == CV_8UC1) {
            // 灰度图像
            QImage image(mat.data, mat.cols, mat.rows, 
                    static_cast<int>(mat.step), QImage::Format_Grayscale8);
                    
            // 创建深拷贝
            return image.copy();
    }
    else {
            qDebug() << "Unsupported matrix format: " << mat.type();
            
            // 尝试转换为RGB
            cv::Mat rgbMat;
            cv::cvtColor(mat, rgbMat, cv::COLOR_BGR2RGB);
            
            QImage image(rgbMat.data, rgbMat.cols, rgbMat.rows, 
                    static_cast<int>(rgbMat.step), QImage::Format_RGB888);
                    
            return image.copy();
        }
    }
    catch (const cv::Exception& e) {
        qDebug() << "OpenCV exception in matToQImage:" << e.what();
    }
    catch (const std::exception& e) {
        qDebug() << "Standard exception in matToQImage:" << e.what();
    }
    catch (...) {
        qDebug() << "Unknown exception in matToQImage";
    }
    
    return QImage(); // 出错时返回空图像
}

void FaceAuthClient::onCaptureButtonClicked()
{
    if (!m_isCameraActive) {
        QMessageBox::warning(this, "错误", "相机未激活");
        return;
    }
    
    try {
        // 从相机视图获取当前显示的图像
        QPixmap currentPixmap = ui.cameraView->pixmap(Qt::ReturnByValue);
        if (currentPixmap.isNull()) {
            QMessageBox::warning(this, "错误", "无法获取当前图像");
            return;
        }
        
        // 转换为QImage，并确保是RGB格式
        QImage capturedImage = currentPixmap.toImage().convertToFormat(QImage::Format_RGB888);
        
        // 将QImage直接转换为cv::Mat
        cv::Mat capturedFrame(
            capturedImage.height(),
            capturedImage.width(),
            CV_8UC3,
            const_cast<uchar*>(capturedImage.constBits()),
            capturedImage.bytesPerLine()
        );
        
        // 由于QImage是RGB格式，OpenCV是BGR格式，需要转换颜色空间
        cv::Mat bgrFrame;
        cv::cvtColor(capturedFrame, bgrFrame, cv::COLOR_RGB2BGR);
        
        // 将图像编码为JPEG格式（使用内存而非文件）
        std::vector<uchar> buf;
        cv::imencode(".jpg", bgrFrame, buf);
        m_capturedFaceData = QByteArray(reinterpret_cast<const char*>(buf.data()), buf.size());
        
        if (m_capturedFaceData.isEmpty()) {
            QMessageBox::warning(this, "错误", "图像编码失败");
            return;
        }
        
        // 在UI上显示"已捕获"消息
        ui.statusLabel->setText("成功捕获人脸图像");
    }
    catch (const cv::Exception& e) {
        QMessageBox::critical(this, "错误", 
                            QString("OpenCV异常: %1").arg(e.what()));
    }
    catch (const std::exception& e) {
        QMessageBox::critical(this, "错误", 
                            QString("标准异常: %1").arg(e.what()));
    }
    catch (...) {
        QMessageBox::critical(this, "错误", "捕获过程中发生未知错误");
    }
}

void FaceAuthClient::onLoginButtonClicked()
{
    QString username = ui.usernameEdit->text().trimmed();
    QString password = ui.passwordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "用户名和密码不能为空");
        return;
    }
    
    if (m_capturedFaceData.isEmpty()) {
        QMessageBox::warning(this, "需要人脸图像", "请在登录前拍照");
        return;
    }
    
    // 更新UI状态
    ui.statusLabel->setText("发送登录请求...");
    ui.loginButton->setEnabled(false);
    
    // 发送登录请求
    sendLoginRequest(username, password, m_capturedFaceData);
    
    // 登录按钮将在收到服务器响应后重新启用
}

void FaceAuthClient::onRegisterButtonClicked()
{
    QString username = ui.usernameEdit->text().trimmed();
    QString password = ui.passwordEdit->text();
    
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "用户名和密码不能为空");
        return;
    }
    
    if (m_capturedFaceData.isEmpty()) {
        QMessageBox::warning(this, "需要人脸图像", "请在注册前拍照");
        return;
    }
    
    // 更新UI状态
    ui.statusLabel->setText("发送注册请求..."); 
    ui.registerButton->setEnabled(false);
    
    // 发送注册请求
    sendRegisterRequest(username, password, m_capturedFaceData);
    
    // 注册按钮将在收到服务器响应后重新启用
}

void FaceAuthClient::onSocketConnected()
{
    ui.statusLabel->setText("已连接到服务器");
}

void FaceAuthClient::onSocketDisconnected()
{
    // 只在状态栏显示断开信息，不显示弹窗
    ui.statusLabel->setText("与服务器连接断开");
    qDebug() << "与服务器连接断开";
    
    // 重新启用按钮
    ui.loginButton->setEnabled(true);
    ui.registerButton->setEnabled(true);
}

void FaceAuthClient::onSocketError(QAbstractSocket::SocketError error)
{
    // 忽略远程主机关闭连接的错误
    if (error == QAbstractSocket::RemoteHostClosedError) {
        qDebug() << "远程主机关闭连接";
        return;
    }
    
    // 只在状态栏显示错误信息，不显示弹窗
    QString errorMessage = "网络错误: " + m_socket->errorString();
    ui.statusLabel->setText(errorMessage);
    qDebug() << "网络错误:" << m_socket->errorString();
    
    // 重新启用UI按钮
    ui.loginButton->setEnabled(true);
    ui.registerButton->setEnabled(true);
}

void FaceAuthClient::onSocketReadyRead()
{
    // 读取所有可用数据
    QByteArray data = m_socket->readAll();
    
    // 将新数据添加到缓冲区
    m_receiveBuffer.append(data);
    
    // 处理接收到的数据
    processServerResponse(m_receiveBuffer);
}

void FaceAuthClient::processServerResponse(const QByteArray& data)
{
    if (data.size() < 8) {
        // 数据不完整，继续等待更多数据
        qDebug() << "接收到的数据不完整，只有" << data.size() << "字节，至少需要8字节";
        return;
    }
    
    // 检查响应头部
    if (data.left(4) != "RESP") {
        qDebug() << "无效的响应头部:" << data.left(4);
        // 清空接收缓冲区
        m_receiveBuffer.clear();
        
        // 重新启用UI按钮
        ui.loginButton->setEnabled(true);
        ui.registerButton->setEnabled(true);
        return;
    }
    
    // 读取JSON数据长度 (使用BigEndian)
    QDataStream stream(data.mid(4, 4));
    stream.setByteOrder(QDataStream::BigEndian);
    qint32 jsonLength;
    stream >> jsonLength;
    
    qDebug() << "服务器响应: 头部=" << data.left(4) << ", JSON长度=" << jsonLength;
    
    // 检查是否有足够的数据
    if (data.size() < 8 + jsonLength) {
        qDebug() << "数据不完整: 接收了" << data.size() << "字节，需要" << (8 + jsonLength) << "字节";
        // 数据不完整，继续等待
        return;
    }
    
    // 提取JSON数据
    QByteArray jsonData = data.mid(8, jsonLength);
    
    // 输出原始JSON数据以便调试
    qDebug() << "原始JSON数据:" << QString(jsonData);
    
    // 解析JSON
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull() || !doc.isObject()) {
        qDebug() << "无效的JSON数据";
        m_receiveBuffer.clear();
        
        // 重新启用UI按钮
        ui.loginButton->setEnabled(true);
        ui.registerButton->setEnabled(true);
        return;
    }
    
    QJsonObject response = doc.object();
    
    // 处理不同类型的响应
    QString type = response["type"].toString();
    
    // 解析success字段，支持多种格式(布尔值、字符串、数字)
    QJsonValue successValue = response.value("success");
    bool success = false;
    
    if (!successValue.isNull()) {
        if (successValue.isBool()) {
            success = successValue.toBool();
        } else if (successValue.isString()) {
            // 尝试将字符串转换为布尔值
            QString successStr = successValue.toString().toLower();
            success = (successStr == "true" || successStr == "1" || successStr == "yes");
        } else if (successValue.isDouble()) {
            // 尝试将数字转换为布尔值
            success = (successValue.toInt() != 0);
        }
    }
    
    // 如果消息表示成功但success标志为false，则可能是格式问题，尝试通过消息内容推断
    QString message = response["message"].toString();
    if (!success && message.contains("successful", Qt::CaseInsensitive)) {
        qDebug() << "消息内容表明成功，但success标志为false，自动修正为true";
        success = true;
    }
    
    qDebug() << "收到服务器响应: 类型=" << type << ", 成功=" << success << ", 消息=" << message;
    
    if (type == "login") {
        ui.loginButton->setEnabled(true);
        
        if (success) {
            ui.statusLabel->setText("登录成功: " + message);
            QMessageBox::information(this, "登录成功", "您已通过人脸认证登录成功啦.\n" + message);
        } else {
            ui.statusLabel->setText("登录失败: " + message);
            QMessageBox::warning(this, "登录失败", "登录失败\n" + message);
        }
    } else if (type == "register") {
        ui.registerButton->setEnabled(true);
        
        if (success) {
            ui.statusLabel->setText("注册成功: " + message);
            QMessageBox::information(this, "注册成功", 
                                   "您已成功注册\n" + message);
        } else {
            ui.statusLabel->setText("注册失败: " + message);
            QMessageBox::warning(this, "注册失败啦", 
                               "注册失败啦\n" + message);
        }
    } else {
        ui.statusLabel->setText("未知的响应类型: " + type);
        qDebug() << "未知的响应类型:" << type;
        
        // 重新启用所有UI按钮
        ui.loginButton->setEnabled(true);
        ui.registerButton->setEnabled(true);
    }
    
    // 清空接收缓冲区，准备接收下一个响应
    m_receiveBuffer.clear();
}

void FaceAuthClient::sendLoginRequest(const QString& username, const QString& password, const QByteArray& faceData)
{
    if (!m_socket) {
        qDebug() << "Socket未初始化";
        ui.statusLabel->setText("错误: 未初始化Socket");
        return;
    }

    // 禁用登录按钮防止重复点击
    ui.loginButton->setEnabled(false);

    // 调试输出
    qDebug() << "准备发送登录请求到" << m_serverAddress << ":" << m_serverPort;

    // 检查是否已连接到服务器，如果没有连接则尝试连接
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        ui.statusLabel->setText("连接到服务器...");
        qDebug() << "尝试连接到服务器...";
        
        // 连接到服务器
        m_socket->connectToHost(m_serverAddress, m_serverPort);
        
        // 等待连接完成
        if (!m_socket->waitForConnected(5000)) {
            QString errorMsg = "连接失败:" + m_socket->errorString();
            ui.statusLabel->setText(errorMsg);
            QMessageBox::critical(this, "连接错误", errorMsg);
            qDebug() << "连接失败:" << m_socket->errorString();
            ui.loginButton->setEnabled(true);
            return;
        }
        
        qDebug() << "已连接到服务器";
    }
    
    // 创建JSON对象保存登录信息
    QJsonObject loginData;
    loginData["type"] = "login";
    loginData["username"] = username;
    loginData["password"] = password;
    loginData["face_data_size"] = faceData.size();
    
    // 转换为JSON文档
    QJsonDocument doc(loginData);
    QByteArray jsonData = doc.toJson();
    
    // 重新构建请求头部 - 使用正确的方式
    // 创建完整的8字节头部
    QByteArray packet;
    
    // 1. 添加"FACE"作为前4个字节 (直接字符串，不使用QDataStream)
    packet.append("FACE", 4);
    
    // 2. 添加JSON长度作为后4个字节 (使用BigEndian字节序)
    QByteArray lengthBytes;
    QDataStream lengthStream(&lengthBytes, QIODevice::WriteOnly);
    lengthStream.setByteOrder(QDataStream::BigEndian);
    lengthStream << (qint32)jsonData.size();
    packet.append(lengthBytes);
    
    // 3. 添加JSON数据
    packet.append(jsonData);
    
    // 4. 添加人脸图像数据
    if (!faceData.isEmpty()) {
        packet.append(faceData);
    }
    
    // 验证头部是否正确
    QByteArray header = packet.left(8);
    qDebug() << "头部内容:" << header.toHex();
    if (header.left(4) != "FACE") {
        qDebug() << "错误：头部格式错误，应为'FACE'，实际为:" << header.left(4).toHex();
        ui.statusLabel->setText("Error: Invalid header format");
        ui.loginButton->setEnabled(true);
        return;
    }
    
    // 添加调试信息
    qDebug() << "头部十六进制:" << header.toHex();
    qDebug() << "头部大小:" << header.size() << "字节";
    qDebug() << "JSON大小:" << jsonData.size() << "字节";
    if (!faceData.isEmpty()) {
        qDebug() << "人脸数据大小:" << faceData.size() << "字节";
    } else {
        qDebug() << "警告：没有人脸数据添加到登录请求";
    }
    qDebug() << "总数据包大小:" << packet.size() << "字节";
    
    // 发送数据包
    qint64 bytesSent = m_socket->write(packet);
    
    if (bytesSent == -1) {
        ui.statusLabel->setText("Failed to send data: " + m_socket->errorString());
        qDebug() << "发送数据失败:" << m_socket->errorString();
        ui.loginButton->setEnabled(true);
    } else {
        ui.statusLabel->setText(QString("Sent %1 bytes to server, awaiting response...").arg(bytesSent));
        qDebug() << "已发送" << bytesSent << "字节到服务器，等待响应...";
        
        // 确保数据发送出去
        m_socket->flush();
        
        // 添加：发送后等待服务器处理
        if (!m_socket->waitForBytesWritten(3000)) {
            qDebug() << "发送数据超时，服务器可能没有收到完整数据";
        }
    }
}

void FaceAuthClient::sendRegisterRequest(const QString& username, const QString& password, const QByteArray& faceData)
{
    if (!m_socket) {
        qDebug() << "Socket未初始化";
        ui.statusLabel->setText("错误: 未初始化Socket");
        return;
    }

    // 禁用注册按钮防止重复点击
    ui.registerButton->setEnabled(false);

    // 调试输出
    qDebug() << "准备发送注册请求到" << m_serverAddress << ":" << m_serverPort;

    // 检查是否已连接到服务器，如果没有连接则尝试连接
    if (m_socket->state() != QAbstractSocket::ConnectedState) {
        ui.statusLabel->setText("Connecting to server...");
        qDebug() << "尝试连接到服务器...";
        
        // 连接到服务器
        m_socket->connectToHost(m_serverAddress, m_serverPort);
        
        // 等待连接完成
        if (!m_socket->waitForConnected(5000)) {
            QString errorMsg = "Failed to connect to server: " + m_socket->errorString();
            ui.statusLabel->setText(errorMsg);
            QMessageBox::critical(this, "Connection Error", errorMsg);
            qDebug() << "连接失败:" << m_socket->errorString();
            ui.registerButton->setEnabled(true);
            return;
        }
        
        qDebug() << "已连接到服务器";
    }
    
    // 创建JSON对象保存注册信息
    QJsonObject registerData;
    registerData["type"] = "register";
    registerData["username"] = username;
    registerData["password"] = password;
    registerData["face_data_size"] = faceData.size();
    
    // 转换为JSON文档
    QJsonDocument doc(registerData);
    QByteArray jsonData = doc.toJson();
    
    // 重新构建请求头部 - 使用正确的方式
    // 创建完整的8字节头部
    QByteArray packet;
    
    // 1. 添加"FACE"作为前4个字节 (直接字符串，不使用QDataStream)
    packet.append("FACE", 4);
    
    // 2. 添加JSON长度作为后4个字节 (使用BigEndian字节序)
    QByteArray lengthBytes;
    QDataStream lengthStream(&lengthBytes, QIODevice::WriteOnly);
    lengthStream.setByteOrder(QDataStream::BigEndian);
    lengthStream << (qint32)jsonData.size();
    packet.append(lengthBytes);
    
    // 3. 添加JSON数据
    packet.append(jsonData);
    
    // 4. 添加人脸图像数据
    if (!faceData.isEmpty()) {
        packet.append(faceData);
    }
    
    // 验证头部是否正确
    QByteArray header = packet.left(8);
    qDebug() << "头部内容:" << header.toHex();
    if (header.left(4) != "FACE") {
        qDebug() << "错误：头部格式错误，应为'FACE'，实际为:" << header.left(4).toHex();
        ui.statusLabel->setText("错误：头部格式错误");
        ui.registerButton->setEnabled(true);
        return;
    }
    
    // 添加调试信息
    qDebug() << "头部十六进制:" << header.toHex();
    qDebug() << "头部大小:" << header.size() << "字节";
    qDebug() << "JSON大小:" << jsonData.size() << "字节";
    if (!faceData.isEmpty()) {
        qDebug() << "人脸数据大小:" << faceData.size() << "字节";
    } else {
        qDebug() << "警告：没有人脸数据添加到注册请求";
    }
    qDebug() << "总数据包大小:" << packet.size() << "字节";
    
    // 发送数据包
    qint64 bytesSent = m_socket->write(packet);
    
    if (bytesSent == -1) {
        ui.statusLabel->setText("Failed to send data: " + m_socket->errorString());
        qDebug() << "发送数据失败:" << m_socket->errorString();
        ui.registerButton->setEnabled(true);
    } else {
        ui.statusLabel->setText(QString("已发送 %1 字节到服务器，等待响应...").arg(bytesSent));
        qDebug() << "已发送" << bytesSent << "字节到服务器，等待响应...";
        
        // 确保数据发送出去
        m_socket->flush();
        
        // 添加：发送后等待服务器处理
        if (!m_socket->waitForBytesWritten(3000)) {
            qDebug() << "发送数据超时，服务器可能没有收到完整数据";
        }
    }
}
