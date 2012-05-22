#include "config.h"
#include <QtCore/QSettings>

/*
 * ini file location: %APPDATA%\TS3Client
 */

#define CONF_FILE				"test_plugin" // ini file
#define CONF_APP				"TS3Client"   // application folder
#define CONF_OBJ(x)			QSettings x(QSettings::IniFormat, QSettings::UserScope, CONF_APP, CONF_FILE);

Config::Config()
{
	setupUi(this);
	CONF_OBJ(cfg);

	lineEdit->setText(cfg.value("global/test").toString());
}

Config::~Config()
{

}

void Config::accept()
{
	CONF_OBJ(cfg);
	cfg.setValue("global/test", lineEdit->text());
	
	QDialog::accept();
}

void Config::reject()
{
	QDialog::reject();
}