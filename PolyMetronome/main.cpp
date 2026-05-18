#include "poly_metronome_dialog.h"
#include "wake_dpi_fixer.h"

#include <QApplication>
#include <QStyleHints>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    WakeDpiFixer dpi_fixer;

    app.setStyle("fusion");
    QStyleHints* hints = QGuiApplication::styleHints();
    hints->setColorScheme(Qt::ColorScheme::Dark);

    PolyMetronomeDialog dialog;
    dialog.show();

    return app.exec();
}
