#include "MainWindow.hpp"

MainWindow::MainWindow(QWidget *parent) 
    : QMainWindow(parent) {
    setWindowTitle("WizzMania Client");
    resize(800, 600);
}

MainWindow::~MainWindow() {
}