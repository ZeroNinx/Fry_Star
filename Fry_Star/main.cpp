#include "Fry_Star.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Fry_Star w;
	w.show();
	return a.exec();
}
