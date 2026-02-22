#include <QtWidgets/QMainWindow>
class MainWindow : public QMainWindow
{
	Q_OBJECT
	public:
	MainWindow();
	private slots:
	void quit();
	void start_recording();
	void start_playback();
 	void agc_sample();
	void generate_test_sample();
	void stop_current_work();	
        void start_resonance_analysis();
        void playback_resonance_analysis();
};
extern int  win_width,win_height;
