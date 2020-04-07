#include "stdafx.h"
#include "Fry_Star.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Fry_Star w;
	w.setWindowIcon(QIcon("Resources/icon.ico"));
	w.show();
	return a.exec();
}
