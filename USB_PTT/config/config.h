#ifndef CONFIG_H
#define CONFIG_H

#include <QtGui/QMainWindow>
#include "ui_config.h"

class Config : public QDialog, public Ui::Config
{
	Q_OBJECT

public:
	Config();
	~Config();
	
protected slots:
	void accept();
	void reject();
};

#endif // CONFIG_H
