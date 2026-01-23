#include <MainWindow.hpp>

MainWindow::MainWindow(QWidget *parent)
: QMainWindow(parent)
{
    resize(800,500);
}

//monter un serveur http (crow)
// le faire ecouter une route (requeter la route avec postman pour tester, ex :http://localhost:8080/ping)

// creer une fenetre QT
// faire un bouton sur la fenetre qui te permet d'envoyer http://localhost:8080/ping

// preparer la route websocket avec crow 
// tester la route websocket avec postman (ws )