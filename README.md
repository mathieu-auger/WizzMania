# WizzMania - MSN Messenger Style Chat Application

## 📖 About the Project

WizzMania is a modern reinterpretation of the iconic MSN Messenger from the 2000s, featuring the legendary "Wizz" function. This project was developed as part of a C++ networking course, implementing a complete client-server architecture with real-time communication capabilities.

The application allows users to:
- Register and authenticate securely
- Send private messages to friends
- Manage friend requests and contacts
- Send "Wizz" (vibration effect) to online friends
- Update profile information (display name, email, birthdate)
- Delete account with all associated data

## 🏗️ Architecture & Technologies

### Server Side (C++17)
- **Crow** - HTTP and WebSocket server framework
- **SQLite3** - Persistent database storage
- **OpenSSL** - Password hashing (SHA-256 with salt)
- **Thread-safe** design with mutex protection

### Client Side
- **Qt6** - Cross-platform GUI framework (Core, Widgets, Network modules)
- **QNetworkAccessManager** - HTTP requests handling
- **Custom HTTP client** - Lightweight console client for testing

### Development Tools
- **CMake** - Build system
- **Postman** - API testing
- **Robot Framework** - Automated API tests

## 📁 Project Structure

WizzMania/

├── server/ # Backend server

│ ├── main.cpp # Server entry point

│ ├── routes.cpp # All API endpoints

│ ├── database.cpp # SQLite operations

│ └── CMakeLists.txt

├── client/ # Qt GUI client

│ ├── MainWindow.cpp # Main window logic

│ ├── MainWindow.ui # UI layout

│ └── CMakeLists.txt

├── console_client/ # Command-line client

│ ├── HttpClient.cpp # HTTP client implementation

│ ├── main_console.cpp # CLI interface

│ └── CMakeLists.txt

├── src/common/ # Shared components

│ ├── Message.hpp/cpp # Message data structure

│ ├── Protocol.hpp/cpp # Communication protocol

│ └── http_server/ # Legacy storage module

├── tests/ # Automated tests

│ └── api_tests.robot # Robot Framework tests

└── CMakeLists.txt # Main build configuration

## Run : 
### Start server
cd server
./msn_server.exe

### Run GUI client
cd ../client
./wizzmania_client.exe

### Run console client
cd ../console_client
./wizzmania_console.exe



---

# 📄 **README.md - French Version**

# WizzMania - Application de Chat Style MSN Messenger

## 📖 À Propos du Projet

WizzMania est une réinterprétation moderne du mythique MSN Messenger des années 2000, avec la légendaire fonction "Wizz". Ce projet a été développé dans le cadre d'un cours de programmation réseau en C++, implémentant une architecture client-serveur complète avec des capacités de communication en temps réel.

L'application permet aux utilisateurs de :
- S'inscrire et s'authentifier de manière sécurisée
- Envoyer des messages privés à leurs amis
- Gérer les demandes d'amis et les contacts
- Envoyer des "Wizz" (effet de vibration) aux amis en ligne
- Modifier les informations de profil (pseudo, email, date de naissance)
- Supprimer son compte avec toutes les données associées

## 🏗️ Architecture & Technologies

### Côté Serveur (C++17)
- **Crow** - Framework HTTP et WebSocket
- **SQLite3** - Stockage persistant
- **OpenSSL** - Hachage des mots de passe (SHA-256 avec sel)
- **Thread-safe** - Protection par mutex

### Côté Client
- **Qt6** - Framework GUI multiplateforme (Core, Widgets, Network)
- **QNetworkAccessManager** - Gestion des requêtes HTTP
- **Client HTTP personnalisé** - Version console légère pour les tests

### Outils de Développement
- **CMake** - Système de build
- **Postman** - Tests API
- **Robot Framework** - Tests API automatisés

## 📁 Structure du Projet

WizzMania/

├── server/ # Serveur backend

│ ├── main.cpp # Point d'entrée du serveur

│ ├── routes.cpp # Tous les endpoints API

│ ├── database.cpp # Opérations SQLite

│ └── CMakeLists.txt

├── client/ # Client Qt (GUI)

│ ├── MainWindow.cpp # Logique de la fenêtre principale

│ ├── MainWindow.ui # Interface utilisateur

│ └── CMakeLists.txt

├── console_client/ # Client en ligne de commande

│ ├── HttpClient.cpp # Implémentation du client HTTP

│ ├── main_console.cpp # Interface CLI

│ └── CMakeLists.txt

├── src/common/ # Composants partagés

│ ├── Message.hpp/cpp # Structure des messages

│ ├── Protocol.hpp/cpp # Protocole de communication

│ └── http_server/ # Module de stockage legacy

├── tests/ # Tests automatisés

│ └── api_tests.robot # Tests Robot Framework

└── CMakeLists.txt # Configuration principale

## Exécution : 
### Démarrer le serveur
cd server
./msn_server.exe

### Lancer le client GUI
cd ../client
./wizzmania_client.exe

### Lancer le client console
cd ../console_client
./wizzmania_console.exe
