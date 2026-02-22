#include "ppstatusbar.h"
#include <QtWidgets/QApplication>
#include "utils.h"
#include "unix_sound.h"

const char *state_str[]=
{
	"Idle",
	"Stopping_Current_Work",
	"Reading Wav",
	"Writing Wav",
	"Recording",
	"Playback",
	"Maximising Sample Volume",
	"Generating Test Sample",
	"Doing Resonance Analysis",
	"Playing Back Resonance Analysis"
};



PPStatusBar::PPStatusBar(QWidget *parent)
	:QStatusBar::QStatusBar(parent)
{
	label=NULL;
	connect(this,SIGNAL(SendUpdateWidgetSignal()),this,SLOT(UpdateWidget()));
}

void PPStatusBar::SendUpdateWidget()
{
	SendUpdateWidgetSignal();
}


void PPStatusBar::UpdateWidget()
{
	QString newString;

	newString.append(QApplication::translate("MainWindow","Status: "));
	newString.append(QApplication::translate("MainWindow",state_str[worker_state]));
	if(label)
	{
		removeWidget(label);
		delete label;
	}
	label=new QLabel(newString,this);
	addWidget(label);
	label->show();
}

