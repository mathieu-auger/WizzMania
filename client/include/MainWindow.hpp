#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP

#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>

#include <vector>
#include "common/Message.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onSendButtonClicked();
    void onLoginButtonClicked();
    void onRegisterButtonClicked();
    void checkForMessages();
    void onPingButtonClicked();
    void onChatButtonClicked();
    void onWyzzButtonClicked();
    void onSettingsButtonClicked();
    
private:
    Ui::MainWindow *ui;

    QNetworkAccessManager* net_;
    QString baseUrl_;
    QString token_;
    QString currentUser_;
    QString currentChatUser_;
    QTimer* messageTimer_;
    QString currentUserDisplayName_;

    void shakeWindow();
    void changeDisplayName(const QString& newPseudo);
    void changeEmail(const QString& newEmail);
    void changeBirthdate(const QString& newBirthdate);
    void deleteAccount();
    void displayMessages(const QJsonArray& messages);
    QString formatMessageHtml(const QString& from, const QString& text, bool isMine) const;

    void httpGet(const QString& path,
                 const std::function<void(int, const QByteArray&)>& cb);

    void httpPostJson(const QString& path,
                      const QJsonObject& obj,
                      const std::function<void(int, const QByteArray&)>& cb);

    void showStatus(const QString& msg, bool isError = false);
};

#endif