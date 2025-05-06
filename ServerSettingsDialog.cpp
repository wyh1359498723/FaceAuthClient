#include "ServerSettingsDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTcpSocket>
#include <QTimer>
#include <QMessageBox>

ServerSettingsDialog::ServerSettingsDialog(QWidget* parent)
    : ServerSettingsDialog("", 8101, parent)
{
}

ServerSettingsDialog::ServerSettingsDialog(const QString& serverAddress, int serverPort, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("服务器设置");   
    setMinimumWidth(400);
    
    // 创建控件
    m_serverAddressEdit = new QLineEdit(this);
    m_serverPortSpinBox = new QSpinBox(this);
    m_serverPortSpinBox->setRange(1, 65535);
    m_serverPortSpinBox->setValue(serverPort);
    
    if (!serverAddress.isEmpty()) {
        m_serverAddressEdit->setText(serverAddress);
    }
    
    m_okButton = new QPushButton("确定", this);
    m_cancelButton = new QPushButton("取消", this);
    m_testConnectionButton = new QPushButton("测试Socket连接", this);
    
    m_statusLabel = new QLabel("", this);
    m_statusLabel->setWordWrap(true);
    
    // 创建表单布局
    QFormLayout* formLayout = new QFormLayout;
    formLayout->addRow("Socket服务器地址:", m_serverAddressEdit);
    formLayout->addRow("Socket服务器端口:", m_serverPortSpinBox);
    
    // 创建按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(m_testConnectionButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    
    // 创建主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->addLayout(formLayout);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(buttonLayout);
    
    // 连接信号槽
    connect(m_okButton, &QPushButton::clicked, this, &ServerSettingsDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &ServerSettingsDialog::onCancelClicked);
    connect(m_testConnectionButton, &QPushButton::clicked, this, &ServerSettingsDialog::onTestConnectionClicked);
    
    // 加载设置
    loadSettings();
}

ServerSettingsDialog::~ServerSettingsDialog()
{
}

QString ServerSettingsDialog::getServerAddress() const
{
    return m_serverAddressEdit->text().trimmed();
}

int ServerSettingsDialog::getServerPort() const
{
    return m_serverPortSpinBox->value();
}

void ServerSettingsDialog::onOkClicked()
{
    // 保存设置
    saveSettings();
    accept();
}

void ServerSettingsDialog::onCancelClicked()
{
    reject();
}

void ServerSettingsDialog::onTestConnectionClicked()
{
    QString address = m_serverAddressEdit->text().trimmed();
    int port = m_serverPortSpinBox->value();
    
    if (address.isEmpty()) {
        m_statusLabel->setText("请输入服务器地址。");
        return;
    }
    
    m_statusLabel->setText("测试Socket连接...");
    m_testConnectionButton->setEnabled(false);
    
    // 创建套接字并测试连接
    QTcpSocket* socket = new QTcpSocket(this);
    
    // 连接超时定时器
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    
    // 连接信号槽
    connect(socket, &QTcpSocket::connected, [this, socket, timer]() {
        m_statusLabel->setText("Socket连接成功!");
        m_testConnectionButton->setEnabled(true);
        timer->stop();
        socket->disconnectFromHost();
        socket->deleteLater();
        timer->deleteLater();
    });
    
    connect(socket, &QTcpSocket::errorOccurred, 
            [this, socket, timer](QAbstractSocket::SocketError) {
        m_statusLabel->setText("Socket连接失败: " + socket->errorString());
        m_testConnectionButton->setEnabled(true);
        timer->stop();
        socket->deleteLater();
        timer->deleteLater();
    });
    
    connect(timer, &QTimer::timeout, [this, socket, timer]() {
        m_statusLabel->setText("Socket连接超时。");
        m_testConnectionButton->setEnabled(true);
        socket->abort();
        socket->deleteLater();
        timer->deleteLater();
    });
    
    // 开始连接
    socket->connectToHost(address, port);
    
    // 启动超时定时器
    timer->start(5000); // 5秒超时
}

void ServerSettingsDialog::loadSettings()
{
    QSettings settings("FaceAuthTeam", "FaceAuthAccess");
    
    m_serverAddressEdit->setText(settings.value("服务器地址", "142.171.34.18").toString());
    m_serverPortSpinBox->setValue(settings.value("服务器端口", 8101).toInt());
}

void ServerSettingsDialog::saveSettings()
{
    QSettings settings("FaceAuthTeam", "FaceAuthAccess");
    
    settings.setValue("服务器地址", m_serverAddressEdit->text().trimmed());
    settings.setValue("服务器端口", m_serverPortSpinBox->value());
} 