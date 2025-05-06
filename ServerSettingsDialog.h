#pragma once

#include <QDialog>
#include <QSettings>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>

class ServerSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    ServerSettingsDialog(QWidget* parent = nullptr);
    ServerSettingsDialog(const QString& serverAddress, int serverPort, QWidget* parent = nullptr);
    ~ServerSettingsDialog();

    QString getServerAddress() const;
    int getServerPort() const;

private slots:
    void onOkClicked();
    void onCancelClicked();
    void onTestConnectionClicked();

private:
    void loadSettings();
    void saveSettings();

    QLineEdit* m_serverAddressEdit;
    QSpinBox* m_serverPortSpinBox;
    QPushButton* m_okButton;
    QPushButton* m_cancelButton;
    QPushButton* m_testConnectionButton;
    QLabel* m_statusLabel;
}; 