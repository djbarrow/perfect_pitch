#include <QtCore/QObject>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QLabel>
class PPStatusBar : public QStatusBar
{
	Q_OBJECT
        public:
	PPStatusBar(QWidget *parent=0);
	void SendUpdateWidget();
	QLabel *label;
public slots:
	void UpdateWidget();
signals:
	void SendUpdateWidgetSignal();
};
