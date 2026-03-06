#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

static QUrl make_url(const QString& base, const QString& path)
{
    QUrl u(base + path);
    return u;
}

static QByteArray bearer_header_value(const QString& token)
{
    return QByteArray("Bearer ") + token.toUtf8();
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , net_(new QNetworkAccessManager(this))
    , baseUrl_("http://127.0.0.1:18080")
    , messageTimer_(new QTimer(this))
{
    ui->setupUi(this);

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

    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(ui->messageInput, &QLineEdit::returnPressed, this, &MainWindow::onSendButtonClicked);
    connect(ui->pingButton, &QPushButton::clicked, this, &MainWindow::onPingButtonClicked);
    connect(ui->chatButton, &QPushButton::clicked, this, &MainWindow::onChatButtonClicked);
    connect(ui->loginButton, &QPushButton::clicked, this, &MainWindow::onLoginButtonClicked);
    connect(ui->registerButton, &QPushButton::clicked, this, &MainWindow::onRegisterButtonClicked);

    // Quand on clique sur un utilisateur, on recharge la conversation
    connect(ui->userList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;

        QString other = item->text().trimmed();
        if (other.isEmpty() || other == currentUser_)
            return;

        qDebug() << "[CHAT] opening conversation with" << other;
        checkForMessages();
    });

    connect(messageTimer_, &QTimer::timeout, this, &MainWindow::checkForMessages);
    messageTimer_->start(1000);

    // Check serveur via /ping
    {
        QNetworkRequest req(make_url(baseUrl_, "/ping"));
        QNetworkReply* reply = net_->get(req);

        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            if (reply->error() == QNetworkReply::NoError) {
                const QString body = QString::fromUtf8(reply->readAll());
                showStatus("✅ Server is available (" + body.trimmed() + ")");
            } else {
                showStatus("❌ Server is not responding: " + reply->errorString(), true);
            }
            reply->deleteLater();
        });
    }

    setMinimumSize(800, 600);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onSendButtonClicked()
{
    const QString text = ui->messageInput->text().trimmed();
    if (text.isEmpty()) return;

    if (currentUser_.isEmpty() || token_.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please login first!");
        return;
    }

    QString to;
    if (auto* item = ui->userList->currentItem()) {
        to = item->text().trimmed();
    }

    if (to.isEmpty() || to == currentUser_) {
        bool ok = false;
        to = QInputDialog::getText(
            this,
            "Send DM",
            "To (username):",
            QLineEdit::Normal,
            "",
            &ok
        ).trimmed();

        if (!ok || to.isEmpty()) return;

        if (to == currentUser_) {
            QMessageBox::warning(this, "Error", "You cannot send a message to yourself.");
            return;
        }
    }

    QJsonObject obj;
    obj["to"] = to;
    obj["text"] = text;

    QNetworkRequest req(make_url(baseUrl_, "/dm/send"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, text, to]() {
        if (reply->error() == QNetworkReply::NoError) {
            ui->messageInput->clear();
            showStatus("✅ Message sent");

            // Si le user n'existe pas encore dans la liste, on l'ajoute
            bool exists = false;
            for (int i = 0; i < ui->userList->count(); ++i) {
                if (ui->userList->item(i)->text() == to) {
                    exists = true;
                    ui->userList->setCurrentRow(i);
                    break;
                }
            }

            if (!exists) {
                ui->userList->addItem(to);
                ui->userList->setCurrentRow(ui->userList->count() - 1);
            }

            checkForMessages();
        } else {
            const QString err = reply->errorString() + " / " + QString::fromUtf8(reply->readAll());
            showStatus("❌ Send failed: " + err, true);
        }
        reply->deleteLater();
    });
}

void MainWindow::checkForMessages()
{
    if (currentUser_.isEmpty() || token_.isEmpty())
        return;

    QString other;
    if (auto* item = ui->userList->currentItem())
        other = item->text().trimmed();

    qDebug() << "[POLL] currentUser_ =" << currentUser_;
    qDebug() << "[POLL] selected other =" << other;

    if (other.isEmpty() || other == currentUser_) {
        qDebug() << "[POLL] no valid conversation selected";
        return;
    }

    QUrl url = make_url(baseUrl_, "/dm/history");
    QUrlQuery q;
    q.addQueryItem("with", other);
    url.setQuery(q);

    qDebug() << "[POLL] url =" << url.toString();

    QNetworkRequest req(url);
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, other]() {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        qDebug() << "[POLL] HTTP status =" << status;
        qDebug() << "[POLL] body =" << raw;

        if (reply->error() != QNetworkReply::NoError) {
            ui->chatDisplay->append(
                "ERROR: " + reply->errorString() + " | " + QString::fromUtf8(raw)
            );
            reply->deleteLater();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isArray()) {
            ui->chatDisplay->append("ERROR: invalid history JSON");
            reply->deleteLater();
            return;
        }

        const QJsonArray arr = doc.array();

        ui->chatDisplay->clear();
        ui->chatDisplay->append("— History with " + other + " —");

        // Le backend renvoie ORDER BY id DESC, donc le plus récent en premier.
        // Pour afficher dans l'ordre naturel, on parcourt à l'envers.
        for (int i = arr.size() - 1; i >= 0; --i) {
            if (!arr[i].isObject()) continue;

            QJsonObject obj = arr[i].toObject();
            QString from = obj.value("from").toString();
            QString body = obj.value("body").toString();

            if (from == currentUser_)
                ui->chatDisplay->append("You: " + body);
            else
                ui->chatDisplay->append(from + ": " + body);
        }

        reply->deleteLater();
    });
}

void MainWindow::onPingButtonClicked()
{
    QNetworkRequest req(make_url(baseUrl_, "/ping"));
    QNetworkReply* reply = net_->get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            const QString body = QString::fromUtf8(reply->readAll());
            ui->chatDisplay->append("→ Ping: " + body.trimmed());
            showStatus("Ping received");
        } else {
            showStatus("Ping failed: " + reply->errorString(), true);
        }
        reply->deleteLater();
    });
}

void MainWindow::onChatButtonClicked()
{
    if (currentUser_.isEmpty() || token_.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please login first!");
        return;
    }

    bool ok = false;
    QString other = QInputDialog::getText(
        this,
        "Open chat",
        "Username:",
        QLineEdit::Normal,
        "",
        &ok
    ).trimmed();

    if (!ok || other.isEmpty())
        return;

    if (other == currentUser_) {
        QMessageBox::warning(this, "Error", "You cannot chat with yourself.");
        return;
    }

    bool exists = false;
    for (int i = 0; i < ui->userList->count(); ++i) {
        if (ui->userList->item(i)->text() == other) {
            exists = true;
            ui->userList->setCurrentRow(i);
            break;
        }
    }

    if (!exists) {
        ui->userList->addItem(other);
        ui->userList->setCurrentRow(ui->userList->count() - 1);
    }

    checkForMessages();
}

void MainWindow::onLoginButtonClicked()
{
    bool ok = false;

    const QString username = QInputDialog::getText(
        this,
        "Login",
        "Username or email:",
        QLineEdit::Normal,
        "",
        &ok
    ).trimmed();
    if (!ok || username.isEmpty()) return;

    const QString password = QInputDialog::getText(
        this,
        "Login",
        "Password:",
        QLineEdit::Password,
        "",
        &ok
    );
    if (!ok || password.isEmpty()) return;

    QJsonObject obj;
    obj["username_or_email"] = username;
    obj["password"] = password;

    QNetworkRequest req(make_url(baseUrl_, "/login"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = net_->post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply, username]() {
        if (reply->error() != QNetworkReply::NoError) {
            const QString err = reply->errorString() + " / " + QString::fromUtf8(reply->readAll());
            showStatus("❌ Login failed: " + err, true);
            reply->deleteLater();
            return;
        }

        const QByteArray raw = reply->readAll();
        const QJsonDocument doc = QJsonDocument::fromJson(raw);
        if (!doc.isObject()) {
            showStatus("❌ Login response is not JSON object", true);
            reply->deleteLater();
            return;
        }

        const QJsonObject o = doc.object();
        const bool okFlag = o.value("ok").toBool(false);
        const QString token = o.value("token").toString();
        const QString uname = o.value("username").toString(username);

        if (!okFlag || token.isEmpty()) {
            showStatus("❌ Login invalid response", true);
            reply->deleteLater();
            return;
        }

        token_ = token;
        currentUser_ = uname;

        showStatus("✅ Successful login as " + uname);
        ui->chatDisplay->append("*** You logged in as " + uname + " ***");

        ui->userList->clear();
        ui->userList->addItem(uname);

        reply->deleteLater();
    });
}

void MainWindow::onRegisterButtonClicked()
{
    bool ok = false;

    const QString username = QInputDialog::getText(
        this,
        "Register",
        "Username:",
        QLineEdit::Normal,
        "",
        &ok
    ).trimmed();
    if (!ok || username.isEmpty()) return;

    const QString email = QInputDialog::getText(
        this,
        "Register",
        "Email:",
        QLineEdit::Normal,
        "",
        &ok
    ).trimmed();
    if (!ok || email.isEmpty()) return;

    const QString password = QInputDialog::getText(
        this,
        "Register",
        "Password:",
        QLineEdit::Password,
        "",
        &ok
    );
    if (!ok || password.isEmpty()) return;

    QJsonObject obj;
    obj["username"] = username;
    obj["email"] = email;
    obj["password"] = password;

    QNetworkRequest req(make_url(baseUrl_, "/register"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = net_->post(req, QJsonDocument(obj).toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            showStatus("✅ Registered");
            ui->chatDisplay->append("*** Registered successfully ***");
        } else {
            const QString err = reply->errorString() + " / " + QString::fromUtf8(reply->readAll());
            showStatus("❌ Register failed: " + err, true);
        }
        reply->deleteLater();
    });
}

void MainWindow::showStatus(const QString& msg, bool isError)
{
    statusBar()->showMessage(msg, 3000);
    if (isError) {
        qDebug() << "Error:" << msg;
    }
}