/********************************************************************************
** Form generated from reading UI file 'MainWindow.ui'
**
** Created by: Qt User Interface Compiler version 6.10.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout;
    QTextEdit *chatDisplay;
    QHBoxLayout *horizontalLayout_2;
    QLineEdit *messageInput;
    QPushButton *sendButton;
    QVBoxLayout *verticalLayout_2;
    QLabel *label;
    QListWidget *userList;
    QPushButton *pingButton;
    QPushButton *loginButton;
    QPushButton *registerButton;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(800, 600);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName("horizontalLayout");
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        chatDisplay = new QTextEdit(centralwidget);
        chatDisplay->setObjectName("chatDisplay");
        chatDisplay->setReadOnly(true);

        verticalLayout->addWidget(chatDisplay);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        messageInput = new QLineEdit(centralwidget);
        messageInput->setObjectName("messageInput");

        horizontalLayout_2->addWidget(messageInput);

        sendButton = new QPushButton(centralwidget);
        sendButton->setObjectName("sendButton");

        horizontalLayout_2->addWidget(sendButton);


        verticalLayout->addLayout(horizontalLayout_2);


        horizontalLayout->addLayout(verticalLayout);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName("verticalLayout_2");
        label = new QLabel(centralwidget);
        label->setObjectName("label");
        label->setAlignment(Qt::AlignCenter);

        verticalLayout_2->addWidget(label);

        userList = new QListWidget(centralwidget);
        userList->setObjectName("userList");

        verticalLayout_2->addWidget(userList);

        pingButton = new QPushButton(centralwidget);
        pingButton->setObjectName("pingButton");

        verticalLayout_2->addWidget(pingButton);

        loginButton = new QPushButton(centralwidget);
        loginButton->setObjectName("loginButton");

        verticalLayout_2->addWidget(loginButton);

        registerButton = new QPushButton(centralwidget);
        registerButton->setObjectName("registerButton");

        verticalLayout_2->addWidget(registerButton);


        horizontalLayout->addLayout(verticalLayout_2);

        MainWindow->setCentralWidget(centralwidget);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "WizzMania Client", nullptr));
        chatDisplay->setPlaceholderText(QCoreApplication::translate("MainWindow", "Chat messages will appear here...", nullptr));
        messageInput->setPlaceholderText(QCoreApplication::translate("MainWindow", "Type your message...", nullptr));
        sendButton->setText(QCoreApplication::translate("MainWindow", "Send", nullptr));
        label->setText(QCoreApplication::translate("MainWindow", "Users Online", nullptr));
        pingButton->setText(QCoreApplication::translate("MainWindow", "Ping", nullptr));
        loginButton->setText(QCoreApplication::translate("MainWindow", "Login", nullptr));
        registerButton->setText(QCoreApplication::translate("MainWindow", "Register", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
