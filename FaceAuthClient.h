#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_FaceAuthClient.h"
#include <QTcpSocket>
#include <QBuffer>
#include <QImage>
#include <opencv2/opencv.hpp>
#include <QMediaDevices>
#include <QMediaCaptureSession>
#include <QCamera>
#include <QCameraDevice>
#include <QCameraFormat>
#include <QVideoSink>
#include <QVideoFrame>
#include <QImageCapture>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVector>

class ServerSettingsDialog;

class FaceAuthClient : public QMainWindow
{
    Q_OBJECT

public:
    FaceAuthClient(QWidget *parent = nullptr);
    ~FaceAuthClient();

private slots:
    void onLoginButtonClicked();
    void onCaptureButtonClicked();
    void onRegisterButtonClicked();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
    void onServerSettingsTriggered();
    void onFrameAvailable(const QVideoFrame &frame);

private:
    Ui::FaceAuthClientClass ui;
    bool initOpenCV();
    bool startCamera();
    void stopCamera();
    QImage matToQImage(const cv::Mat& mat);
    void sendLoginRequest(const QString& username, const QString& password, const QByteArray& faceData = QByteArray());
    void sendRegisterRequest(const QString& username, const QString& password, const QByteArray& faceData = QByteArray());
    void processServerResponse(const QByteArray& data);
    
    QTcpSocket* m_socket;
    QCamera* m_camera;
    QMediaCaptureSession* m_captureSession;
    QVideoSink* m_videoSink;
    QImageCapture* m_imageCapture;
    
    bool m_isCameraActive;
    QString m_serverAddress;
    quint16 m_serverPort;
    QByteArray m_capturedFaceData;
    QByteArray m_receiveBuffer;
};
