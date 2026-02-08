#include "MainWindow.h"

#include <QApplication>

int main(int argc, char **argv) {
  QApplication app(argc, argv);

  MainWindow window;
  window.resize(900, 600);
  window.show();

  return app.exec();
}
