#include "poly_metronome_dialog.h"

#include <QApplication>
#include <QStyleHints>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setStyle("fusion");
    QStyleHints* hints = QGuiApplication::styleHints();
    hints->setColorScheme(Qt::ColorScheme::Dark);

    PolyMetronomeDialog dialog;
    dialog.show();

    return app.exec();
}
