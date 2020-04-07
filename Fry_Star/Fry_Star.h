#pragma once

#include <QtWidgets/QWidget>
#include "ui_Fry_Star.h"

class Fry_Star : public QWidget
{
	Q_OBJECT

public:
	Fry_Star(QWidget *parent = Q_NULLPTR);

private:
	Ui::Fry_StarClass ui;
};
