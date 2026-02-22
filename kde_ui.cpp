#include "ui_perfect_pitch.h"
#include "kde_ui.h"
#include "worker.h"
#include <QGuiApplication>
#include <QScreen>


int  win_width,win_height;

Ui::MainWindow ui;

MainWindow::MainWindow()
	:QMainWindow::QMainWindow()
{
	QScreen *screen = QGuiApplication::primaryScreen();
	const QRect rect=screen->geometry();
	win_width=(rect.width()/4)*3;
	win_height=(rect.height()/4)*3;
	ui.setupUi(this);
	resize(win_width,win_height);
	setCentralWidget(ui.centralwidget);
	connect(ui.actionQuit,SIGNAL(triggered()),
		this,SLOT(quit()));
	connect(ui.actionStart_Recording,
		SIGNAL(triggered()),this,SLOT(start_recording()));
	connect(ui.actionStart_Playback,
		SIGNAL(triggered()),this,SLOT(start_playback()));
        connect(ui.actionMaximise_Sample_Volume,
		SIGNAL(triggered()),this,SLOT(agc_sample()));
        connect(ui.actionGenerate_Test_Sample,
		SIGNAL(triggered()),this,SLOT(generate_test_sample()));
        connect(ui.actionStop_Current_Work,
		SIGNAL(triggered()),this,SLOT(stop_current_work()));
        connect(ui.actionStart_Resonance_Analysis,
		SIGNAL(triggered()),this,SLOT(start_resonance_analysis()));
        connect(ui.actionPlayback_Resonance_Analysis,
		SIGNAL(triggered()),this,SLOT(playback_resonance_analysis()));
	init_worker();
}

void MainWindow::quit()
{
	exit(0);
}
void MainWindow::start_recording()
{
	set_new_work_state(recording);
}
void MainWindow::start_playback()
{
	set_new_work_state(playback);
}
void MainWindow::agc_sample()
{
	set_new_work_state(maximising_sample_volume);
}
void MainWindow::generate_test_sample()
{
	set_new_work_state(generating_test_sample);
}
void MainWindow::stop_current_work()
{
	if(worker_state!=idle)
		set_worker_state(stopping_current_work);
}	
void MainWindow::start_resonance_analysis()
{
	set_new_work_state(doing_resonance_analysis);
}
void MainWindow::playback_resonance_analysis()
{
	set_new_work_state(playingback_resonance_analysis);
}


