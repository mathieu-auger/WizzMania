#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>  // для debug виводу

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , client(new HttpClient()) 
    , messageTimer(new QTimer(this))
{
    ui->setupUi(this);
    
    // DEBUG: перевірка чи створилися елементи
    qDebug() << "=== DEBUG UI ELEMENTS ===";
    qDebug() << "chatDisplay:" << ui->chatDisplay;
    qDebug() << "messageInput:" << ui->messageInput;
    qDebug() << "sendButton:" << ui->sendButton;
    qDebug() << "userList:" << ui->userList;
    qDebug() << "pingButton:" << ui->pingButton;
    qDebug() << "chatButton:" << ui->chatButton;
    qDebug() << "loginButton:" << ui->loginButton;
    qDebug() << "registerButton:" << ui->registerButton;
    qDebug() << "=========================";
    
    // Підключення сигналів кнопок
    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(ui->messageInput, &QLineEdit::returnPressed, this, &MainWindow::onSendButtonClicked);
    connect(ui->pingButton, &QPushButton::clicked, this, &MainWindow::onPingButtonClicked);
    connect(ui->chatButton, &QPushButton::clicked, this, &MainWindow::onChatButtonClicked);
    connect(ui->loginButton, &QPushButton::clicked, this, &MainWindow::onLoginButtonClicked);
    connect(ui->registerButton, &QPushButton::clicked, this, &MainWindow::onRegisterButtonClicked);
    
    // Таймер для перевірки повідомлень
    connect(messageTimer, &QTimer::timeout, this, &MainWindow::checkForMessages);
    messageTimer->start(1000); 
    
    // Перевірка сервера
    if (client->isServerAvailable()) {
        showStatus("✅ Server is available");
    } else {
        showStatus("❌ Server is not responding", true);
    }
    
    // Встановити мінімальний розмір
    setMinimumSize(800, 600);
}

MainWindow::~MainWindow() {
    delete ui;
    delete client;
}

void MainWindow::onSendButtonClicked() {
    QString text = ui->messageInput->text().trimmed();
    if (text.isEmpty()) {
        return;
    }
    
    if (currentUser.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please login first!");
        return;
    }
    
    std::string response = client->sendMessage(text.toStdString(), currentUser.toStdString());
    ui->chatDisplay->append("You: " + text);
    ui->messageInput->clear();
    showStatus(QString::fromStdString(response));
}

void MainWindow::checkForMessages() {
    if (currentUser.isEmpty()) return;
    
    std::string response = client->getMessages();
    if (!response.empty() && response != "404 Not Found") {
        ui->chatDisplay->append(QString::fromStdString(response));
    }
}

void MainWindow::onPingButtonClicked() {
    std::string response = client->ping();
    ui->chatDisplay->append("→ Ping: " + QString::fromStdString(response));
    showStatus("Ping received");
}

void MainWindow::onChatButtonClicked() {
    std::string response = client->chat();
    ui->chatDisplay->append("→ Chat: " + QString::fromStdString(response));
}

void MainWindow::onLoginButtonClicked() {
    bool ok;
    QString username = QInputDialog::getText(this, "Login", "Username:", QLineEdit::Normal, "", &ok);
    if (!ok || username.isEmpty()) return;
    
    QString password = QInputDialog::getText(this, "Login", "Password:", QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;
    
    std::string response = client->login(username.toStdString(), password.toStdString());
    if (response.find("successful") != std::string::npos) {
        currentUser = username;
        showStatus("✅ Successful login as " + username);
        ui->chatDisplay->append("*** You logged in as " + username + " ***");
        
        // Додати користувача в список
        ui->userList->clear();
        ui->userList->addItem(username);
    } else {
        showStatus("❌ Error: " + QString::fromStdString(response), true);
    }
}

void MainWindow::onRegisterButtonClicked() {
    bool ok;
    QString username = QInputDialog::getText(this, "Register", "Username:", QLineEdit::Normal, "", &ok);
    if (!ok || username.isEmpty()) return;
    
    QString password = QInputDialog::getText(this, "Register", "Password:", QLineEdit::Password, "", &ok);
    if (!ok || password.isEmpty()) return;
    
    std::string response = client->registerUser(username.toStdString(), password.toStdString());
    showStatus(QString::fromStdString(response));
}

void MainWindow::showStatus(const QString& msg, bool isError) {
    statusBar()->showMessage(msg, 3000);
    if (isError) {
        qDebug() << "Error:" << msg;
    }
}