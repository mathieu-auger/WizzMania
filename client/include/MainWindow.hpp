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
#include <vector> 
#include "common/Message.hpp"
#include "HttpClient.hpp"  

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
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
    HttpClient* client;             
    QString currentUser;
    QTimer* messageTimer;
    
    void setupUI();
    void appendMessage(const QString& msg, bool isOwn = false);
    void updateUserList(const std::vector<Message>& messages);
    void showStatus(const QString& msg, bool isError = false);
};

#endif