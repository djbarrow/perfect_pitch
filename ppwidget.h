#include <QtWidgets/QWidget>
#include "utils.h"
class PPWidget : public QWidget
{
	Q_OBJECT
	public:
	PPWidget(QWidget *parent=0,Qt::WindowFlags f=Qt::WindowFlags());
	void draw_window();
	void paintEvent(QPaintEvent *event);
};

extern u32 true_buff_width;
extern u8 *rgb_buff;
extern s32 scroll_buff_offset;
extern int note_vert_line_offset;
extern int num_notes;
extern int num_notes_div2;
