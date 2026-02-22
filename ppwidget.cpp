#include <QPainter>
#include <QPaintEvent>
#include <QtWidgets/QWidget>
#include <stdio.h>
#include "utils.h"
#include "ui_perfect_pitch.h"
#include "kde_ui.h"

int font_height=12;
int font_width=20;
int num_notes;
int num_notes_div2;
int note_vert_line_offset;
s32 scroll_buff_offset=0;
u8  *rgb_buff=NULL;
u32 true_buff_width;

PPWidget::PPWidget(QWidget *parent,Qt::WindowFlags f)
	:QWidget::QWidget(parent,f)
{
	QFont *font=new QFont("Helvetica");
	font->setPixelSize(font_height);
	num_notes=win_height/font_height;
	num_notes_div2=num_notes>>1;
	setFont(*font);
}


const char *notes[]=
{
	"C",
	"C#",
	"D",
	"D#",
	"E",
	"F",
	"F#",
	"G",
	"G#",
	"A",
	"A#",
	"B"
};

QRgb note_colours[]=
{
	0xffffff,
	0xff0000,
	0x00ff00,
	0x0000ff,
	0xff7f00,
	0x3fff3f,
	0x007fff,
	0xffff00,
	0xff00ff,
	0x00ffff,
	0x7f7f00,
	0X7f007f
};

void draw_rgb_image(QPainter &painter,
	int x,int y,
	int width,int height,
	uchar *rgb_buff,int rowstride)
{
#if 0
	int i;
	for(i=0;i<(width*height*4);i++)
		rgb_buff[i]=255;
#endif
	QImage image(rgb_buff,
		     width,height,rowstride,
		     QImage::Format_RGB32);	
	painter.drawImage(x,y,image,0,0,
			  width,height);
}




void PPWidget::draw_window()
{
	int i,note,octave;
	u32 curr_height;
	char notebuf[20];
	s32 curr_offset,curr_offset2;
	QPainter painter(this);

	curr_offset2=scroll_buff_offset%true_buff_width;
	curr_offset=scroll_buff_offset%win_width;

	if(curr_offset2<win_width)
	{

		draw_rgb_image (painter,
				0,0,win_width-curr_offset,win_height,
				&rgb_buff[curr_offset*4],true_buff_width*4);
		draw_rgb_image (painter,
				win_width-curr_offset, 0,curr_offset,win_height,
				&rgb_buff[win_width*4],true_buff_width*4);
		}
		else
		{
			draw_rgb_image(painter,
					    0, 0,win_width-curr_offset,win_height,
					    &rgb_buff[curr_offset2*4],true_buff_width*4);
			draw_rgb_image(painter,
					win_width-curr_offset, 0,curr_offset,win_height,
					&rgb_buff[0],true_buff_width*4);

		}
	curr_height=win_height-font_height;
	for(i=-num_notes_div2;i<num_notes_div2;i++)
	{
		note=i%12;
		if(note<0)
			note+=12;
		if(i<0)
			octave=((i-11)/12)+5;
		else
			octave=(i/12)+5;
		sprintf(notebuf,"%d %s",octave,notes[note]);
		painter.setPen(note_colours[note]);
		painter.drawText(note_vert_line_offset-font_width,curr_height,notebuf);
		painter.drawLine((int)0,(int)curr_height,(int)win_width,(int)curr_height);
			curr_height-=font_height;
	}
}


void PPWidget::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	QRect dirtyRect=event->rect();
	painter.fillRect(dirtyRect,Qt::black);
	draw_window();
}
