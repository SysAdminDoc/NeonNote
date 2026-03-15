#include <QApplication>
#include "MainWindow.h"
#include "Theme.h"

int main(int argc, char* argv[])
{
    QApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    app.setApplicationName("NeonNote");
    app.setApplicationVersion("2.0.0");
    app.setStyleSheet(Theme::globalStylesheet());

    QFont defaultFont("Segoe UI", 9);
    app.setFont(defaultFont);

    MainWindow window;
    window.show();

    return app.exec();
}
