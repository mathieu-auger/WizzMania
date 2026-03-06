// #ifndef MAINWINDOW_HPP
// #define MAINWINDOW_HPP

// #include <QMainWindow>
// #include <QWidget>

// class MainWindow : public QMainWindow {
//     Q_OBJECT  

// public:
//     MainWindow(QWidget *parent = nullptr);
//     ~MainWindow();
// };

// #endif // MAINWINDOW_HPP

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

private:
    Ui::MainWindow *ui;

    // ✅ Qt Network
    QNetworkAccessManager* net_;
    QString baseUrl_;     // ex: "http://127.0.0.1:18080"
    QString token_;       // Bearer token après login
    QString currentUser_;
    QTimer* messageTimer_;

    // helpers HTTP
    void httpGet(const QString& path,
                 const std::function<void(int, const QByteArray&)>& cb);

    void httpPostJson(const QString& path,
                      const QJsonObject& obj,
                      const std::function<void(int, const QByteArray&)>& cb);

    void showStatus(const QString& msg, bool isError = false);
};

#endif