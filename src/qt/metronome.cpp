// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include "config/bitcoin-config.h"
#endif

#include "fs.h"
#include "metronome.h"
#include "ui_metronome.h"

#include "guiutil.h"

#include "util.h"

#include <QFileDialog>
#include <QSettings>
#include <QMessageBox>

#include <cmath>
#include <fstream>

static const uint64_t GB_BYTES = 1000000000LL;
/* Minimum free space (in GB) needed for data directory */
static const uint64_t BLOCK_CHAIN_SIZE = 150;
/* Minimum free space (in GB) needed for data directory when pruned; Does not include prune target */
static const uint64_t CHAIN_STATE_SIZE = 3;
/* Total required space (in GB) depending on user choice (prune, not prune) */
static uint64_t requiredSpace;

#include "metronome.moc"

void TrySaveBitcoinConf(std::string metronomeAddr, std::string metronomePort, std::string metronomeUsername, std::string metronomePassword) {
	std::stringstream ss;
	ss << gArgs.GetArg("-datadir", GetDefaultDataDir().string()) << "/" << gArgs.GetArg("-conf", BITCOIN_CONF_FILENAME);

	std::stringstream out;
	out << "metronomeAddr=" << metronomeAddr << std::endl;
	out << "metronomePort=" << metronomePort << std::endl;
	out << "metronomeUser=" << metronomeUsername << std::endl;
	out << "metronomePassword=" << metronomePassword << std::endl;
	
	FILE* fp = fopen(ss.str().c_str(), "a+");
	fwrite(out.str().c_str(), sizeof(char), out.str().length(), fp);
	fclose(fp);
}

Metronome::Metronome(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::Metronome),
    thread(0),
    signalled(false),
	doneForm(false)
{
    ui->setupUi(this);
    ui->welcomeLabel->setText(ui->welcomeLabel->text().arg(tr(PACKAGE_NAME)));
    ui->storageLabel->setText(ui->storageLabel->text().arg(tr(PACKAGE_NAME)));

    ui->lblExplanation1->setText(ui->lblExplanation1->text()
        .arg(tr(PACKAGE_NAME))
        .arg(BLOCK_CHAIN_SIZE)
        .arg(2009)
        .arg(tr("Bitcoin"))
    );
}

Metronome::~Metronome()
{
    delete ui;
}

QString Metronome::getMetronomeAddress()
{
    return ui->metronomeServer->text();
}

QString Metronome::getMetronomePort()
{
	return ui->metronomePort->text();
}

QString Metronome::getMetronomeUsername()
{
	return ui->metronomeUsername->text();
}

QString Metronome::getMetronomePassword()
{
	return ui->metronomePassword->text();
}



bool Metronome::pickMetronomeServer()
{
    QSettings settings;
 
	if (!gArgs.GetArg("-metronomeAddr", "").empty() && !gArgs.GetArg("-metronomePort", "").empty()) {
		return true;
	}

    Metronome metronome;
	metronome.setWindowIcon(QIcon(":icons/bitcoin"));

	if (!metronome.exec()) {
		return false;
	}

	QString metronomeAddr = metronome.getMetronomeAddress();
	QString metronomePort = metronome.getMetronomePort();
	QString metronomeUsername = metronome.getMetronomeUsername();
	QString metronomePassword = metronome.getMetronomePassword();

	try {
		TrySaveBitcoinConf (
			metronomeAddr.toStdString(), 
			metronomePort.toStdString(), 
			metronomeUsername.toStdString(), 
			metronomePassword.toStdString()
		);
	} 
	catch (...) {
		QMessageBox::critical(0, tr(PACKAGE_NAME),
			tr("Error: Metronome connection settings could not be saved."));
	}

    return true;
}


void Metronome::on_buttonBox_clicked()
{
	if (getMetronomeAddress().toStdString() == "" || getMetronomePort().toStdString() == "") {
		QMessageBox::critical(0, QObject::tr(PACKAGE_NAME), QObject::tr("Address and Port must not be empty."));
	}
	else {
		accept();
	}
}