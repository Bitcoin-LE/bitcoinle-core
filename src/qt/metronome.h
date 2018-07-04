// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_METRONOME_H
#define BITCOIN_QT_METRONOME_H

#include <QDialog>
#include <QMutex>
#include <QThread>

static const bool DEFAULT_CHOOSE_METRONOME_SERVER = false;

namespace Ui {
    class Metronome;
}

/** Introduction screen (pre-GUI startup).
  Allows the user to choose a metronome server.
 */
class Metronome : public QDialog
{
    Q_OBJECT

public:
    explicit Metronome(QWidget *parent = 0);
    ~Metronome();

	QString getMetronomeAddress();

	QString getMetronomePort();

	QString getMetronomeUsername();

	QString getMetronomePassword();

    /**
     * Determine data directory. Let the user choose if the current one doesn't exist.
     *
     * @returns true if a data directory was selected, false if the user cancelled the selection
     * dialog.
     *
     * @note do NOT call global GetDataDir() before calling this function, this
     * will cause the wrong path to be cached.
     */
    static bool pickMetronomeServer();

Q_SIGNALS:

public Q_SLOTS:

private Q_SLOTS:

    void on_buttonBox_clicked();

private:
    Ui::Metronome *ui;
    QThread *thread;
    QMutex mutex;
    bool signalled;
	bool doneForm;
    QString pathToCheck;
};

#endif // BITCOIN_QT_METRONOME_H
