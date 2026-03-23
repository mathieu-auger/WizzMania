#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QDebug>
#include <QScrollBar>
#include <QWidget>
#include <QTextEdit>
#include <QMenu>

#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>

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

    connect(ui->wyzzButton, &QPushButton::clicked,
        this, &MainWindow::onWyzzButtonClicked);
    connect(ui->sendButton, &QPushButton::clicked, this, &MainWindow::onSendButtonClicked);
    connect(ui->messageInput, &QLineEdit::returnPressed, this, &MainWindow::onSendButtonClicked);
    connect(ui->settingsButton, &QPushButton::clicked,this, &MainWindow::onSettingsButtonClicked);
    connect(ui->pingButton, &QPushButton::clicked, this, &MainWindow::onPingButtonClicked);
    connect(ui->chatButton, &QPushButton::clicked, this, &MainWindow::onChatButtonClicked);
    connect(ui->loginButton, &QPushButton::clicked, this, &MainWindow::onLoginButtonClicked);
    connect(ui->registerButton, &QPushButton::clicked, this, &MainWindow::onRegisterButtonClicked);

    // Quand on clique sur un utilisateur, on recharge la conversation
    connect(ui->userList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item)
            return;

        QString other = item->text().trimmed();

        if (other.isEmpty() || other == currentUser_)
            return;

        currentChatUser_ = other;

        qDebug() << "[CHAT] opening conversation with" << currentChatUser_;
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

QString MainWindow::formatMessageHtml(const QString& from, const QString& text, bool isMine) const
{
    const QString safeFrom = from.toHtmlEscaped();
    const QString safeText = text.toHtmlEscaped().replace("\n", "<br>");

    if (isMine) {
        return QString(
            "<div style='width:100%; margin:6px 0; text-align:right;'>"
            "  <div style='display:inline-block; max-width:70%;"
            "              background-color:#cce5ff;"
            "              border-radius:12px;"
            "              padding:8px 12px;"
            "              text-align:left;'>"
            "    <div style='font-size:11px; color:#555; margin-bottom:2px;'><b>%1</b></div>"
            "    <div>%2</div>"
            "  </div>"
            "</div>"
        ).arg(safeFrom, safeText);
    }

    return QString(
        "<div style='width:100%; margin:6px 0; text-align:left;'>"
        "  <div style='display:inline-block; max-width:70%;"
        "              background-color:#e5e5e5;"
        "              border-radius:12px;"
        "              padding:8px 12px;"
        "              text-align:left;'>"
        "    <div style='font-size:11px; color:#555; margin-bottom:2px;'><b>%1</b></div>"
        "    <div>%2</div>"
        "  </div>"
        "</div>"
    ).arg(safeFrom, safeText);
}

void MainWindow::onSettingsButtonClicked()
{
    QMenu menu(this);

    QAction* renameAction = menu.addAction("Changer le pseudo");
    QAction* emailAction = menu.addAction("Changer l'email");
    QAction* birthdateAction = menu.addAction("Changer la date de naissance");
    menu.addSeparator();
    QAction* deleteAction = menu.addAction("Supprimer le compte");

    QAction* selected = menu.exec(QCursor::pos());
    if (!selected)
        return;

    if (selected == renameAction)
    {
        bool ok = false;

        QString newPseudo = QInputDialog::getText(
            this,
            "Changer le pseudo",
            "Nouveau pseudo :",
            QLineEdit::Normal,
            "",
            &ok
        );

        if (ok && !newPseudo.trimmed().isEmpty())
            changeDisplayName(newPseudo.trimmed());
    }
    else if (selected == emailAction)
    {
        bool ok = false;

        QString newEmail = QInputDialog::getText(
            this,
            "Changer l'email",
            "Nouvel email :",
            QLineEdit::Normal,
            "",
            &ok
        );

        if (ok && !newEmail.trimmed().isEmpty())
            changeEmail(newEmail.trimmed());
    }
    else if (selected == birthdateAction)
    {
        bool ok = false;

        QString newBirthdate = QInputDialog::getText(
            this,
            "Changer la date de naissance",
            "Nouvelle date de naissance (YYYY-MM-DD) :",
            QLineEdit::Normal,
            "",
            &ok
        );

        if (ok && !newBirthdate.trimmed().isEmpty())
            changeBirthdate(newBirthdate.trimmed());
    }
    else if (selected == deleteAction)
    {
        QMessageBox::StandardButton confirm = QMessageBox::warning(
            this,
            "Supprimer le compte",
            "Voulez-vous vraiment supprimer votre compte ?\nCette action est définitive.",
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );

        if (confirm == QMessageBox::Yes)
            deleteAccount();
    }
}

void MainWindow::deleteAccount()
{
    QNetworkRequest req(QUrl(baseUrl_ + "/account/delete"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->post(req, QByteArray("{}"));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        qDebug() << "[SETTINGS][DELETE] status =" << status;
        qDebug() << "[SETTINGS][DELETE] body =" << raw;

        if (reply->error() == QNetworkReply::NoError)
        {
            QMessageBox::information(
                this,
                "Compte supprimé",
                "Votre compte a été supprimé."
            );

            // Nettoyage local
            token_.clear();
            currentUser_.clear();
            currentChatUser_.clear();

            QApplication::quit();
        }
        else
        {
            showStatus(
                "❌ Suppression impossible: " +
                reply->errorString() + " | " + QString::fromUtf8(raw),
                true
            );
        }

        reply->deleteLater();
    });
}

void MainWindow::changeBirthdate(const QString& newBirthdate)
{
    QJsonObject obj;
    obj["birthdate"] = newBirthdate;

    QNetworkRequest req(QUrl(baseUrl_ + "/account/birthdate"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->post(
        req,
        QJsonDocument(obj).toJson(QJsonDocument::Compact)
    );

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        qDebug() << "[SETTINGS][BIRTHDATE] status =" << status;
        qDebug() << "[SETTINGS][BIRTHDATE] body =" << raw;

        if (reply->error() == QNetworkReply::NoError)
        {
            showStatus("✅ Date de naissance mise à jour");
        }
        else
        {
            showStatus("❌ Erreur date de naissance: " + reply->errorString() + " | " + QString::fromUtf8(raw), true);
        }

        reply->deleteLater();
    });
}

void MainWindow::changeEmail(const QString& newEmail)
{
    QJsonObject obj;
    obj["email"] = newEmail;

    QNetworkRequest req(QUrl(baseUrl_ + "/account/email"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->post(
        req,
        QJsonDocument(obj).toJson(QJsonDocument::Compact)
    );

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        qDebug() << "[SETTINGS][EMAIL] status =" << status;
        qDebug() << "[SETTINGS][EMAIL] body =" << raw;

        if (reply->error() == QNetworkReply::NoError)
        {
            showStatus("✅ Email mis à jour");
        }
        else
        {
            showStatus("❌ Erreur email: " + reply->errorString() + " | " + QString::fromUtf8(raw), true);
        }

        reply->deleteLater();
    });
}

void MainWindow::changeDisplayName(const QString& newPseudo)
{
    QJsonObject obj;
    obj["display_name"] = newPseudo;

    QNetworkRequest req(QUrl(baseUrl_ + "/account/display-name"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->post(
        req,
        QJsonDocument(obj).toJson(QJsonDocument::Compact)
    );

    connect(reply, &QNetworkReply::finished, this, [this, reply, newPseudo]()
    {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        qDebug() << "[SETTINGS][DISPLAY_NAME] status =" << status;
        qDebug() << "[SETTINGS][DISPLAY_NAME] body =" << raw;

        if (reply->error() == QNetworkReply::NoError)
        {
            showStatus("✅ Pseudo mis à jour");

            // Optionnel : mettre à jour localement si tu affiches ton pseudo
            currentUserDisplayName_ = newPseudo;
        }
        else
        {
            showStatus("❌ Erreur pseudo: " + reply->errorString() +
                       " | " + QString::fromUtf8(raw), true);
        }

        reply->deleteLater();
    });
}

void MainWindow::onWyzzButtonClicked()
{
    if (currentChatUser_.isEmpty())
    {
        ui->chatDisplay->append("Aucune discussion ouverte.");
        return;
    }

    QJsonObject obj;
    obj["to"] = currentChatUser_;

    QJsonDocument doc(obj);

    QNetworkRequest req(QUrl(baseUrl_ + "/wyzz"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->post(req, doc.toJson(QJsonDocument::Compact));

    connect(reply, &QNetworkReply::finished, this, [this, reply]()
    {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray raw = reply->readAll();

        qDebug() << "[WYZZ SEND] HTTP status =" << status;
        qDebug() << "[WYZZ SEND] body =" << raw;

        if (reply->error() != QNetworkReply::NoError)
        {
            showStatus("❌ Wyzz failed: " + reply->errorString() + " | " + QString::fromUtf8(raw), true);
        }
        else
        {
            showStatus("⚡ Wyzz envoyé");
        }

        reply->deleteLater();
    });
}

void MainWindow::shakeWindow()
{
    const QPoint originalPos = this->pos();
    const int amplitude = 10;      // intensité du tremblement
    const int durationMs = 450;    // durée totale
    const int stepMs = 20;         // intervalle entre déplacements

    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < durationMs)
    {
        int dx = QRandomGenerator::global()->bounded(-amplitude, amplitude + 1);
        int dy = QRandomGenerator::global()->bounded(-amplitude, amplitude + 1);

        move(originalPos + QPoint(dx, dy));

        // Laisse Qt traiter les événements pour que l'UI reste fluide
        QCoreApplication::processEvents(QEventLoop::AllEvents, stepMs);
    }

    move(originalPos); // revenir à la position initiale
}

void MainWindow::onSendButtonClicked()
{
    const QString text = ui->messageInput->text().trimmed();
    if (text.isEmpty())
        return;

    if (currentUser_.isEmpty() || token_.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please login first!");
        return;
    }

    QString to;

    // si un utilisateur est sélectionné dans la liste
    if (auto* item = ui->userList->currentItem()) {
        to = item->text().trimmed(); // ← pseudo affiché
    }

    // sinon on demande le pseudo
    if (to.isEmpty()) {

        bool ok = false;
        to = QInputDialog::getText(
            this,
            "Send DM",
            "Pseudo:",
            QLineEdit::Normal,
            "",
            &ok
        ).trimmed();

        if (!ok || to.isEmpty())
            return;
    }

    QJsonObject obj;
    obj["to"] = to;        // ← pseudo maintenant
    obj["text"] = text;

    QNetworkRequest req(make_url(baseUrl_, "/dm/send"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    req.setRawHeader("Authorization", bearer_header_value(token_));

    QNetworkReply* reply = net_->post(
        req,
        QJsonDocument(obj).toJson(QJsonDocument::Compact)
    );

    connect(reply, &QNetworkReply::finished, this, [this, reply, to]() {

        if (reply->error() == QNetworkReply::NoError) {

            ui->messageInput->clear();
            showStatus("✅ Message sent");

            // ajoute le pseudo dans la liste si absent
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

            const QString err =
                reply->errorString() + " / " +
                QString::fromUtf8(reply->readAll());

            showStatus("❌ Send failed: " + err, true);
        }

        reply->deleteLater();
    });
}

void MainWindow::displayMessages(const QJsonArray& messages)
{
    QString html;
    bool hasWyzz = false;

    for (const QJsonValue& value : messages)
    {
        if (!value.isObject())
            continue;

        const QJsonObject obj = value.toObject();

        const QString type = obj.value("type").toString();
        qDebug() << "[DISPLAY] type =" << type;

        if (type == "wyzz")
        {
            qDebug() << "[DISPLAY] WYZZ détecté";
            hasWyzz = true;
            continue;
        }

        const QString fromId = obj.value("from").toString();
        const QString text = obj.value("body").toString();

        QString displayName = obj.value("from_name").toString();

        // fallback si aucun pseudo n'est trouvé
        if (displayName.isEmpty())
            displayName = fromId;

        const bool isMine = (fromId == currentUser_);

        html += formatMessageHtml(displayName, text, isMine);
    }

    ui->chatDisplay->setHtml(html);

    QScrollBar* bar = ui->chatDisplay->verticalScrollBar();
    bar->setValue(bar->maximum());

    if (hasWyzz)
    {
        qDebug() << "[DISPLAY] shakeWindow() va être appelée";
        shakeWindow();
    }
}

void MainWindow::checkForMessages()
{
    if (currentUser_.isEmpty() || token_.isEmpty())
        return;

    if (currentChatUser_.isEmpty()) {
        qDebug() << "[POLL] no valid conversation selected";
        return;
    }

    const QString other = currentChatUser_;

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

        displayMessages(arr);

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
        "Pseudo:",
        QLineEdit::Normal,
        "",
        &ok
    ).trimmed();

    if (!ok || other.isEmpty())
        return;

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

    currentChatUser_ = other;
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
    if (!ok || username.isEmpty())
        return;

    const QString password = QInputDialog::getText(
        this,
        "Login",
        "Password:",
        QLineEdit::Password,
        "",
        &ok
    );
    if (!ok || password.isEmpty())
        return;

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
        currentChatUser_.clear();

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

    const QString displayName = QInputDialog::getText(
    this,
    "Register",
    "Pseudo:",
    QLineEdit::Normal,
    "",
    &ok
).trimmed();
if (!ok || displayName.isEmpty()) return;

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
obj["display_name"] = displayName;
obj["email"] = email;
obj["password"] = password;}

void MainWindow::showStatus(const QString& msg, bool isError)
{
    statusBar()->showMessage(msg, 3000);
    if (isError) {
        qDebug() << "Error:" << msg;
    }
}