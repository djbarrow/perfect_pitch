#include "utils.h"
#include "ui_perfect_pitch.h"
#include "kde_ui.h"

int main(int argc,char *argv[])
{
	QApplication app(argc,argv);
        MainWindow *mainWindow = new MainWindow;
	mainWindow->show();
	return app.exec();
}
