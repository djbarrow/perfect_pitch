//#include <gnome.h>
//#include <gtk/gtk.h>

#include "utils.h"
#define __USE_UNIX98
#include <pthread.h>
#include <sys/time.h>
#include <signal.h>
#include <math.h>
#include <string.h>
#include "unix_sound.h"
#include "perfect_pitch.h"
#include <sched.h>

char *notes[]=
{
	"c",
	"c#",
	"d",
	"d#",
	"e",
	"f",
	"f#",
	"g",
	"g#",
	"a",
	"a#",
	"b"
};

typedef int QRgb;
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

#if 0
gboolean on_darea_expose (GtkWidget *widget,
			  GdkEventExpose *event,
			  gpointer user_data);
#endif
u32 width;
u32 height;
u32 note_vert_line_offset;
int num_notes;
int num_notes_div2;
u32 screen_width;
u32 screen_height;
u32 buff_width;
u32 true_buff_width;
u32 buff_size;
u32 additional_buff_width;
double pixels_per_portamento_inc,notes_per_portamento_inc;
double x_per_sample;
u8     *rgb_buff=NULL;


#if 0
GdkGC *gc;
GtkWidget *gwidget;
GdkFont *gfont;
gint     font_height,font_width;
#endif


pthread_t worker_thread;
pthread_mutex_t worker_mutex;
struct itimerval scroll_interval_itimer;
int first_resonance_call=TRUE;
u32 scroll_interval;
#if 0
pthread_mutex_t scroll_buffer_running;
pthread_mutexattr_t scroll_attr;
#endif
double low_note,high_note;
u16    portamento_scaler;

#ifdef PPALSA
u32 scroll_buff_offset=0;
pthread_t scroll_buff_thread;
pthread_mutex_t scroll_buff_mutex;
int scroll_buff_thread_exited;
s16 *zero_sample_buff;
#endif

void draw_window(int gthread_lock)
{
	int i,note,octave;
	u32 curr_height;
	char notebuf[20];
	GdkRectangle   rect;
	rect.x=rect.y=0;
	rect.width=width;
	rect.height=height;
	u32 curr_offset,curr_offset2;

	curr_offset2=scroll_buff_offset%true_buff_width;
	curr_offset=scroll_buff_offset%width;

	if(gwidget&&gc)
	{
		if(gthread_lock)
			gdk_threads_enter ();
		gdk_rgb_gc_set_foreground(gc,0xffff);
		gdk_rgb_gc_set_background(gc,0x000000);
		gdk_window_begin_paint_rect(gwidget->window,&rect);
		if(curr_offset2<width)
		{
			gdk_draw_rgb_image (gwidget->window,gc,
					    0, 0,width-curr_offset,height,
					    GDK_RGB_DITHER_MAX,&rgb_buff[curr_offset*3],true_buff_width*3);
			gdk_draw_rgb_image (gwidget->window,gc,
					    width-curr_offset, 0,curr_offset,height,
					    GDK_RGB_DITHER_MAX,&rgb_buff[width*3],true_buff_width*3);
		}
		else
		{
			gdk_draw_rgb_image (gwidget->window,gc,
					    0, 0,width-curr_offset,height,
					    GDK_RGB_DITHER_MAX,&rgb_buff[curr_offset2*3],true_buff_width*3);
			gdk_draw_rgb_image (gwidget->window,gc,
					    width-curr_offset, 0,curr_offset,height,
					    GDK_RGB_DITHER_MAX,&rgb_buff[0],true_buff_width*3);

		}

		curr_height=height-font_height;
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
			gdk_rgb_gc_set_foreground(gc,note_colours[note]);
			gdk_draw_string(gwidget->window,gfont,
					gc,note_vert_line_offset-font_width,curr_height,notebuf);
			gdk_draw_line(gwidget->window,gc,
			      0,curr_height,width,curr_height);
			curr_height-=font_height;
		}
		gdk_rgb_gc_set_foreground(gc,0x000000);
		gdk_draw_line(gwidget->window,gc,
			      note_vert_line_offset,0,note_vert_line_offset,height);
		gdk_window_end_paint(gwidget->window);
		//gdk_rgb_gc_set_foreground(gc,0x000000);
		gdk_rgb_gc_set_background(gc,0xffffff);
		if(gthread_lock)
			gdk_threads_leave ();
	}
}

void *scroll_buff_thread_func(void *unused)
{

	while(worker_state==playingback_resonance_analysis)
	{
		pthread_mutex_lock(&scroll_buff_mutex);
		draw_window(TRUE);
	}
	scroll_buff_thread_exited=TRUE;
	return NULL;
}

void queue_scroll_left()
{
	pthread_mutex_unlock(&scroll_buff_mutex);
}

#if 0
#define ONE_DAY (3600*24)
void scroll_thread_handler(int signal)
{
	pthread_mutex_unlock(&scroll_buffer_running);
}
#endif

void do_resonance_analysis();
void free_resonance_buff();
void playback_resonance_analysis(s32 start_offset,s32 end_offset);

void *worker_thread_func(void *unused)
{
	while(1)
	{
		worker_state=idle;
		pthread_mutex_lock(&worker_mutex);
		switch(worker_state)
		{
		case reading_wav:
			break;
		case writing_wav:
			break;
		case recording:
			record_dev_dsp();
			break;
		case playback:
			play_dev_dsp();
			break;
		case maximising_sample_volume:
			agc_sample();
			break;
		case generating_test_sample:
			generate_test_sample(low_note,high_note,sample_rate,10);
			break;
		case doing_resonance_analysis:
#ifndef PPALSA
			free_resonance_buff();
#endif
			do_resonance_analysis();
			break;
		case playingback_resonance_analysis:
#ifdef PPALSA
			if(record_head&&record_head->resonance_buff)
#endif
			{
				scroll_buff_thread_exited=FALSE;
				pthread_mutex_init(&scroll_buff_mutex,NULL);
				pthread_mutex_lock(&scroll_buff_mutex);
				pthread_create(&scroll_buff_thread,NULL,scroll_buff_thread_func,NULL);
				playback_resonance_analysis(0,0);
				worker_state=idle;
				while(scroll_buff_thread_exited==FALSE)
					sched_yield();
				pthread_mutex_destroy(&scroll_buff_mutex);

			}

#ifdef PPALSA
			else
				fprintf(stderr,"First record a sample & do resonance analysis\n");
#endif
			break;
		default:
			break;
		}
	}
}

#ifdef RESONANCE_AND_ENVELOPE_BUFF
void *scroll_thread_func(void *unused)
{
	struct itimerval ovalue;
	
	//signal(SIGVTALRM,scroll_thread_handler);
	//pthread_mutexattr_settype(&scroll_attr,PTHREAD_MUTEX_RECURSIVE);
	//pthread_mutex_init(&scroll_buffer_running,&scroll_attr);
	while(1)
	{
		//setitimer(ITIMER_VIRTUAL,&scroll_interval_itimer,&ovalue);
		scroll_buff_left();
		usleep(scroll_interval);
		//pthread_mutex_lock(&scroll_buffer_running);
	}
	return 0;
}
#endif
#ifdef RESONANCE_AND_ENVELOPE_BUFF
void resonance_analysis_callback(u32 buff_offset,u32 num_samples,
				   sine_table_struct **sine_tables,u32 sine_table_offset)
{
	struct itimerval end_resonance_time;
	struct timeval   result;
	long long        usec;
	u32              i,j,x1,y1,curr_sine_table_offset;
	double           x,y;
	u8               *curr_pixel;
	double           sine,cosine,envelope_val;
	sine_table_struct *curr_sine_table;
	struct itimerval  time_remaining;
	struct timeval    start_time;
	
	for(i=0,y=height-1;i<sine_tables_num_entries;i++,y-=pixels_per_portamento_inc)
	{
		curr_sine_table=sine_tables[i];
		y1=(u32)y;
	
		for(j=0,curr_sine_table_offset=sine_table_offset,x=width;j<num_samples;
		    j++,x+=x_per_sample,curr_sine_table_offset++)
		{
			x1=(u32)x;
			
			curr_pixel=&rgb_buff[(x1*3)+(y1*true_buff_width*3)];
			sine=curr_sine_table->sine_envelope_buff[curr_sine_table_offset]/(double)0x80000000;
			cosine=curr_sine_table->cosine_envelope_buff[curr_sine_table_offset]/(double)0x80000000;		
			envelope_val=sqrt((sine*sine)+(cosine*cosine))*256*10.0;
			if(envelope_val>10)
			{
				envelope_val=envelope_val+1;
				envelope_val=envelope_val-1;
			}
			if(envelope_val>255)
				envelope_val=255;
			if(curr_pixel[0]<(u8)envelope_val)
			{
				curr_pixel[0]=(u8)envelope_val;
				curr_pixel[1]=0;
				curr_pixel[2]=(u8)envelope_val;
			}
		}
	}
	if(first_resonance_call)
	{
		first_resonance_call=FALSE;

		getitimer(ITIMER_VIRTUAL,&time_remaining);
		start_time.tv_sec=ONE_DAY;
		start_time.tv_usec=0;
		timersub(&start_time,
			 &time_remaining.it_value,&result);
		usec=(result.tv_sec*1000000)+result.tv_usec;
		scroll_interval=(double)usec/((double)num_samples*x_per_sample);
		scroll_interval_itimer.it_interval.tv_sec=0;
		scroll_interval_itimer.it_interval.tv_usec=0;
		scroll_interval_itimer.it_value.tv_sec=scroll_interval/1000000;
		scroll_interval_itimer.it_value.tv_usec=scroll_interval%1000000;
		pthread_create(&scroll_thread,NULL,scroll_thread_func,NULL);
	}

}
#else

double get_envelope_val(sine_table_struct *sine_table)
{
	double sine,cosine;
	double envelope_val;
	sine=sine_table->sine_envelope_val/(double)0x80000000;
	cosine=sine_table->cosine_envelope_val/(double)0x80000000;		
	envelope_val=sqrt((sine*sine)+(cosine*cosine))*256*5.0;
	if(envelope_val>255)
		envelope_val=255;
	return envelope_val;
}

#ifndef PPALSA
typedef struct resonance_buff resonance_buff;

struct resonance_buff
{
	resonance_buff *next;
	u8 buff[0];
};

struct resonance_buff *resonance_buff_head=NULL,
	*resonance_buff_tail=NULL;
#endif

#ifndef PPALSA
void free_resonance_buff()
{
	resonance_buff *curr,*next;
	curr=resonance_buff_head;

	while(curr)
	{
		next=curr->next;
		free(curr);
		curr=next;
	}
	resonance_buff_head=NULL;
	resonance_buff_tail=NULL;

}

resonance_buff *alloc_resonance_buff()
{
	resonance_buff *curr;
	curr=myalloc("resonance_buff",
		     offsetof(struct resonance_buff,buff[height]));
	curr->next=NULL;
	if(resonance_buff_head==NULL)
	{
		resonance_buff_head=curr;
		resonance_buff_tail=curr;
	}
	else
	{
		resonance_buff_tail->next=curr;
		resonance_buff_tail=curr;
	}
	return curr;
}
#endif

#ifdef PPALSA
extern snd_pcm_uframes_t frames;
#endif

void playback_resonance_analysis(s32 start_offset,s32 end_offset)
{
#ifndef PPALSA
	resonance_buff *curr_sound;
#else
	struct soundbuff *curr_sound=NULL,*curr_scroll=NULL;
#endif
	u8  envelope_val;
	u8  *curr_pixel;
	s32 y;
#ifdef PPALSA
	snd_pcm_sframes_t 	avail,orig_avail;
	snd_pcm_t *handle;
	int rc;
	s32 x_offset,cnt;
	s32 sound_offset;

	handle=init_dev_dsp(0);
	orig_avail=snd_pcm_avail (handle);
#endif

#if PPALSA
	scroll_buff_offset=start_offset;
	curr_scroll=record_head;
	for(cnt=0;cnt<scroll_buff_offset&&curr_scroll;cnt++)
		curr_scroll=curr_scroll->next;
	curr_sound=curr_scroll;
	sound_offset=scroll_buff_offset-width+note_vert_line_offset;
#else
	curr=resonance_buff_head;
#endif
	while(curr_scroll||curr_sound)
	{
		x_offset=(scroll_buff_offset+buff_width)%true_buff_width;
#ifdef PPALSA
		while(1)
		{

			avail=snd_pcm_avail(handle);
			if(avail == -EPIPE)
				break;
			if((orig_avail-avail)<=frames)
				break;
			sched_yield();
		}
		if(sound_offset>=start_offset&&curr_sound)
		{
			fix_buffer(&curr_sound->sample_buff[0],curr_sound->num_samples);
			rc = snd_pcm_writei(handle,
					    &curr_sound->sample_buff,
					    curr_sound->num_samples);
			fix_buffer(&curr_sound->sample_buff[0],curr_sound->num_samples);
			curr_sound=curr_sound->next;
		}
		else
			rc = snd_pcm_writei(handle,
					    zero_sample_buff,
					    frames);
		if (rc == -EPIPE) {
			/* EPIPE means underrun */
			fprintf(stderr, "underrun occurred\n");
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			fprintf(stderr,
				"error from writei: %s\n",
				snd_strerror(rc));
		}  else if (rc != (int)frames) {
			fprintf(stderr,
				"short write, write %d frames\n", rc);
		}
#endif
		printf("snd pcm avail=%d\n",(int)snd_pcm_avail(handle));

		for(y=height-1;y>=0;y--)
		{
#ifdef PPALSA
			if(curr_scroll&&curr_scroll->resonance_buff)
				envelope_val=curr_scroll->resonance_buff[y];
			else
				envelope_val=0;
			
#else
			envelope_val=curr->buff[y];
#endif
			curr_pixel=&rgb_buff[(x_offset*3)+
					     (y*true_buff_width*3)];
			curr_pixel[0]=envelope_val;
			curr_pixel[1]=envelope_val;
			curr_pixel[2]=envelope_val;
		}
#ifdef PPALSA
		queue_scroll_left();
#else
		scroll_buff_left(1);
#endif
		if(curr_scroll)
			curr_scroll=curr_scroll->next;
		if(worker_state==stopping_current_work)
			break;
		scroll_buff_offset++;
		sound_offset++;
	}
	if(worker_state!=stopping_current_work)
		while(orig_avail-snd_pcm_avail(handle)>0)
			sched_yield();
#ifdef PPALSA
	alsa_close(handle);
#endif
}

void resonance_analysis_callback_common(u32 buff_offset,
#ifdef PPALSA
soundbuff_walker *walker
#endif
)
{
	s32 y;
#ifdef PPALSA
	u8 *curr=NULL;
#else
	resonance_buff *curr;
#endif
	u8  curr_pixel;
	u8  *curr_pixel_ptr;
	u32 curr_offset;
	curr_offset=(((buff_offset-1)/frames)+width)%true_buff_width;
#ifdef PPALSA
	if(!walker->u.ll.curr->resonance_buff)
		walker->u.ll.curr->resonance_buff=
			myalloc("resonance_buff",
				sizeof(*curr)*height);
	curr=walker->u.ll.curr->resonance_buff;	
#else
	curr=alloc_resonance_buff();
#endif
	for(y=height-1;y>=0;y--)
	{
		curr_pixel=rgb_buff[(curr_offset*3)+
				    (((s32)y)*true_buff_width*3)];
#if PPALSA
		curr[y]=curr_pixel;
#else
		curr->buff[y]=curr_pixel;
#endif
	}
	walker->u.ll.curr=walker->u.ll.curr->next;
	scroll_buff_offset=(buff_offset/frames);
	draw_window(TRUE);
	curr_offset=((buff_offset/frames)+width)%true_buff_width;
	for(y=0;y<height;y++)
	{
		curr_pixel_ptr=&rgb_buff[(curr_offset*3)+
				       (y*true_buff_width*3)];
		curr_pixel_ptr[0]=0;
		curr_pixel_ptr[1]=0;
		curr_pixel_ptr[2]=0;
	}

}



#ifdef LINEAR_PITCH
void resonance_analysis_callback(u32 buff_offset,
#ifdef PPALSA
				 soundbuff_walker *walker,
#endif
				 sine_table_struct **sine_tables)
{
	u8  *curr_pixel;
	double y,last_y,curr_y,fraction;
	u32 curr_sine,note_idx;
	double  envelope_val,curr_envelope_val,last_envelope_val;
	sine_table_struct *curr_sine_table;
	//resonance_buff *curr;	
#if PPALSA
	static soundbuff_walker last_walker;
#endif

#ifdef PPALSA
	if(buff_offset==0)
		last_walker=*walker;
#endif
	if((buff_offset!=0)&&
	   ((u32)(buff_offset*x_per_sample)!=(u32)((buff_offset-1)*x_per_sample)))
	{
		resonance_analysis_callback_common(buff_offset,
#ifdef PPALSA
			&last_walker
#endif
			);
#ifdef PPALSA
		last_walker=*walker;
#endif
	}

	last_y=height-1;
	last_envelope_val=0;
	for(curr_sine=0;curr_sine<sine_tables_num_entries;curr_sine++)
	{
		curr_sine_table=sine_tables[curr_sine];
		note_idx=curr_sine_table->note_idx;
		y=(height-1)-(pixels_per_portamento_inc*note_idx);
		curr_envelope_val=get_envelope_val(sine_tables[curr_sine]);
		for(curr_y=last_y;curr_y>=y;curr_y--)
		{
			if(curr_y<0)
				break;
			if(y==last_y)
				fraction=1;
			else
				fraction=(curr_y-last_y)/(y-last_y);
			curr_pixel=&rgb_buff[(buff_width*3)+
					     (((s32)curr_y)*true_buff_width*3)];
			envelope_val=(curr_envelope_val*fraction)+
				(last_envelope_val*(1-fraction));
			if(curr_pixel[0]<(u8)envelope_val)
			{
				curr_pixel[0]=(u8)envelope_val;
				curr_pixel[1]=0;
				curr_pixel[2]=(u8)envelope_val;
			}
		}
		last_y=y;
		last_envelope_val=envelope_val;
	}
}
#else
#ifdef DELTA_BUFF
void resonance_analysis_callback(u32 buff_offset,sine_table_struct **sine_tables)
{
	double envelope_val,float_y;
	u8     int_envelope_val;
	u32    curr_sine;
	sine_table_struct *prev_sine_table,*curr_sine_table,*next_sine_table;
	s32   int_y;
	u8  *curr_pixel;

	float_y=height-1-pixels_per_portamento_inc;
	for(curr_sine=1;curr_sine<sine_tables_num_entries-1;curr_sine++)
	{
		int_y=float_y;
		if(int_y<0)
			break;
		prev_sine_table=sine_tables[curr_sine-1];
	        curr_sine_table=sine_tables[curr_sine];
		next_sine_table=sine_tables[curr_sine+1];
		if(prev_sine_table->delta_envelope_val<=curr_sine_table->delta_envelope_val&&
		   next_sine_table->delta_envelope_val<=curr_sine_table->delta_envelope_val)
		{
			envelope_val=curr_sine_table->delta_envelope_val;
			if(envelope_val>255)
				envelope_val=255;
			int_envelope_val=(u8)envelope_val;
			curr_pixel=&rgb_buff[(buff_width*3)+
					     (int_y*true_buff_width*3)];
			if(curr_pixel[0]<(u8)int_envelope_val)
			{
				curr_pixel[0]=(u8)int_envelope_val;
				curr_pixel[1]=(u8)int_envelope_val;
				curr_pixel[2]=(u8)int_envelope_val;
			}

	
		}
		float_y-=pixels_per_portamento_inc;
	}
	if((buff_offset!=0)&&
	   ((u32)(buff_offset*x_per_sample)!=(u32)((buff_offset-1)*x_per_sample)))
		resonance_analysis_callback_common();
}
#else
void resonance_analysis_callback(u32 buff_offset,
#ifdef PPALSA
				 soundbuff_walker *walker,
#endif
				 sine_table_struct **sine_tables)
{
	s32 y;
	double idx;
	s32 int_idx,idx_ceil,int_y;
	double  envelope_val1,envelope_val2,envelope_val;
	u8  *curr_pixel;
	double fraction,float_y;
	u32    curr_sine;
#if PPALSA
	static soundbuff_walker last_walker;
#endif
	u32 curr_offset;
#ifdef PPALSA
	if(buff_offset==0)
		last_walker=*walker;
#endif
	if((buff_offset!=0)&&
	   ((u32)(buff_offset*x_per_sample)!=(u32)((buff_offset-1)*x_per_sample)))
	{
		resonance_analysis_callback_common(buff_offset,
#ifdef PPALSA
			&last_walker
#endif
			);
#ifdef PPALSA
		last_walker=*walker;
#endif

	}

	curr_offset=((buff_offset/frames)+width)%true_buff_width;
	if(height>sine_tables_num_entries)
	{
		idx=0.0;
		for(y=height-1;y>=0;y--)
		{
			
			curr_pixel=&rgb_buff[(curr_offset*3)+
					     (y*true_buff_width*3)];
			int_idx=idx;
			fraction=idx-int_idx;
			idx_ceil=(s32)ceil(idx);
			if(idx_ceil>=sine_tables_num_entries)
				break;
			envelope_val1=get_envelope_val(sine_tables[int_idx]);
			envelope_val2=get_envelope_val(sine_tables[idx_ceil]);
			envelope_val=(envelope_val2*fraction)+(envelope_val1*(1-fraction));
			if(curr_pixel[0]<(u8)envelope_val)
			{
				curr_pixel[0]=(u8)envelope_val;
				curr_pixel[1]=(u8)envelope_val;
				curr_pixel[2]=(u8)envelope_val;
			}
			idx+=notes_per_portamento_inc;
		}
	}
	else
	{
		float_y=height-1;
		for(curr_sine=0;curr_sine<sine_tables_num_entries;curr_sine++)
		{
			int_y=float_y;
			if(int_y<0)
				break;
			curr_pixel=&rgb_buff[(curr_offset*3)+
					     (int_y*true_buff_width*3)];
			envelope_val=get_envelope_val(sine_tables[curr_sine]);
#if 1
			if(curr_pixel[0]<(u8)envelope_val)
			{
				curr_pixel[0]=(u8)envelope_val;
				curr_pixel[1]=(u8)envelope_val;
				curr_pixel[2]=(u8)envelope_val;
			}
#else
			{
				int new_envelope_val=curr_pixel[0]+envelope_val;
				if(new_envelope_val>255)
					new_envelope_val=255;
				curr_pixel[0]=(u8)new_envelope_val;
				curr_pixel[1]=(u8)new_envelope_val;
				curr_pixel[2]=(u8)new_envelope_val;

			}
#endif
			float_y-=pixels_per_portamento_inc;
		}
	}
}
#endif
#endif /* LINEAR_PITCH */
#endif

void do_resonance_analysis()
{
	soundbuff_walker walker;
#if 0
	struct itimerval curr_time;
	struct itimerval ovalue;

	signal(SIGVTALRM,scroll_thread_handler);
	curr_time.it_value.tv_sec=ONE_DAY;
	curr_time.it_value.tv_usec=0;
	curr_time.it_interval.tv_sec=0;
	curr_time.it_interval.tv_usec=0;
	setitimer(ITIMER_VIRTUAL,&curr_time,&ovalue);
#endif
#ifndef PPALSA
	free_resonance_buff();
#endif
	initialise_walker_function_pointers();
	init_soundbuff_procptr(&walker);
	resonance_analysis(resonance_analysis_callback,
    			   &walker,0,sine_tables_num_entries);
}

gboolean
on_darea_expose (GtkWidget *widget,
		 GdkEventExpose *event,
		 gpointer user_data)
{
	gc=widget->style->fg_gc[GTK_STATE_NORMAL];
	gwidget=widget;
	draw_window(FALSE);
	return TRUE;
}


gboolean mouse_button_pressed (GtkWidget *widget,
			  GdkEventButton *event,
			  gpointer user_data)
{
	note_vert_line_offset=event->x;
	draw_window(FALSE);
	return TRUE;
}


void set_new_work_state(worker_state_enum new_state)
{
	if(worker_state==idle)
	{
		worker_state=new_state;
		pthread_mutex_unlock(&worker_mutex);
	}
}
static void
start_recording_func(GtkWidget *button, gpointer data)
{
        /*just print a string so that we know we got there*/
	set_new_work_state(recording);
}
static void
cancel_current_work_func(GtkWidget *button, gpointer data)
{
        /*just print a string so that we know we got there*/
	if(worker_state!=idle)
		worker_state=stopping_current_work;
}

static void
start_playback_func(GtkWidget *button, gpointer data)
{
        /*just print a string so that we know we got there*/
	set_new_work_state(playback);
}


static void agc_sample_func(GtkWidget *button, gpointer data)
{
	set_new_work_state(maximising_sample_volume);
}

static void
generate_test_sample_func(GtkWidget *button, gpointer data)
{
	set_new_work_state(generating_test_sample);

	
}



static void
start_resonance_analysis_func(GtkWidget *button, gpointer data)
{
	set_new_work_state(doing_resonance_analysis);
}

static void
playingback_resonance_analysis_func(GtkWidget *button, gpointer data)
{
	set_new_work_state(playingback_resonance_analysis);
}




// Menu Stuff
GnomeUIInfo file_menu[] = {
        GNOMEUIINFO_MENU_EXIT_ITEM(gtk_main_quit,NULL),
        GNOMEUIINFO_END
};

GnomeUIInfo sample_manipulation_menu[] = {
        GNOMEUIINFO_ITEM_NONE("Start Recording","",
                              start_recording_func),
        
	GNOMEUIINFO_ITEM_NONE("Start Playback","",
                              start_playback_func),
 	GNOMEUIINFO_ITEM_NONE("Maximise Sample Volume","",
			      agc_sample_func),
	GNOMEUIINFO_ITEM_NONE("Generate Test Sample","",
                              generate_test_sample_func),
	GNOMEUIINFO_ITEM_NONE("Stop Current Work","",
			      cancel_current_work_func),	
        GNOMEUIINFO_END
};


GnomeUIInfo resonance_analysis_menu[] = {
        GNOMEUIINFO_ITEM_NONE("Start Resonance Analysis","",
                              start_resonance_analysis_func),
	GNOMEUIINFO_ITEM_NONE("Playback Resonance Analysis","",
                              playingback_resonance_analysis_func),
	GNOMEUIINFO_END
};


GnomeUIInfo menubar[] = {
        GNOMEUIINFO_MENU_FILE_TREE(file_menu),
        GNOMEUIINFO_SUBTREE("Sample Manipulation",sample_manipulation_menu),
	GNOMEUIINFO_SUBTREE("Resonance Analysis",resonance_analysis_menu),
        GNOMEUIINFO_END
};


int
main (int argc, char *argv[])
{
	GtkWidget *app, *darea;
	GdkScreen* screen;
	u32 high_note_idx;

	/* init threads */
	g_thread_init (NULL);
	gdk_threads_init ();
	gdk_threads_enter ();

	//gtk_init (&argc, &argv);
	if(argc==2)
	{
		read_wav(argv[1]);
		process_wav();
	}
	gnome_init ("menu-basic-example", "0.1", argc, argv);
	//window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
//	app= gnome_mdi_new ("menu-basic-example",
//                             "Basic GNOME Application");
	app = gnome_app_new ("",
                             "Perfect Pitch");

	//darea = gtk_drawing_area_new ();
	darea=app;
	screen=gdk_screen_get_default();
	screen_width=gdk_screen_get_width(screen);
	screen_height=gdk_screen_get_height(screen);
	width=(screen_width/4)*3;
	height=(screen_height/4)*3;
	
	gtk_widget_set_size_request (darea,width,height);
	//gtk_container_add (GTK_CONTAINER (app), darea);
	//gtk_container_child_set(GTK_CONTAINER (app), darea,NULL);
	gtk_signal_connect (GTK_OBJECT (darea), "expose-event",
			    GTK_SIGNAL_FUNC (on_darea_expose), NULL);
	gtk_widget_add_events(darea, GDK_BUTTON_PRESS_MASK);
	gtk_signal_connect (GTK_OBJECT (darea), "button-press-event",
			    GTK_SIGNAL_FUNC (mouse_button_pressed), NULL);

	gfont = gdk_font_load ("-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*");
	font_height=gdk_string_height (gfont,"C#")+1;
	font_width=gdk_string_width (gfont,"10C#");
	num_notes=height/font_height;
	num_notes_div2=num_notes>>1;
	
	low_note=calc_note_from_middle_c(-num_notes_div2);
	high_note=calc_note_from_middle_c(num_notes_div2);
#if 0
	low_note=22;
	high_note=4000;
#endif
	sample_rate=11025;
#ifdef PPALSA
	x_per_sample=1.0/64.0;
	/* Set period size to 32 frames. */
	frames = 1.0/x_per_sample;
#else
	x_per_sample=(double)width/(double)(sample_rate*4);
#endif
	pthread_mutex_init(&worker_mutex,NULL);
	pthread_create(&worker_thread,NULL,worker_thread_func,NULL);
	
	portamento_scaler=16;
	calc_note_and_portamento_inc(portamento_scaler,high_note);
#ifdef LINEAR_PITCH
	generate_sine_tables(low_note,high_note,1.0,sample_rate);
#else
	generate_sine_tables(low_note,high_note,sample_rate);
#endif
#ifdef RESONANCE_AND_ENVELOPE_BUFF
	additional_buff_width=(double)triple_buff_envelope_buff_size*x_per_sample+1;
	buff_width=width+additional_buff_width;
	
#else
	buff_width=width;
#endif
	true_buff_width=buff_width*2;
	buff_size=true_buff_width*height*3;
	note_vert_line_offset=width/4;
	rgb_buff=myalloc("rgb_buff",buff_size);
	memset(rgb_buff,0,buff_size);
	zero_sample_buff=myalloc("zero_sample_buff",frames*sizeof(s16));
	memset(zero_sample_buff,0,frames*sizeof(s16));
#ifdef LINEAR_PITCH
	extern sine_table_struct **sine_tables;
	high_note_idx=sine_tables[sine_tables_num_entries-1]->note_idx;
#else
	high_note_idx=sine_tables_num_entries;
#endif
	pixels_per_portamento_inc=(double)height/(double)high_note_idx;
	notes_per_portamento_inc=(double)high_note_idx/(double)height;
	calculate_filter_coefficents(/*40*/80
#ifdef FILTER_POSITIVE_FEEDBACK
				     ,0,0/*100,400*/ /*80,340*/
#endif
		);

	//write_wav("out.wav");
	//resonant_analysis(NULL,0,sine_tables_num_entries);
	gtk_signal_connect (GTK_OBJECT (app), "delete_event",
                            GTK_SIGNAL_FUNC (gtk_main_quit),
                            NULL);
	//label = gtk_label_new(argv[0]);

        /*add the label as contents of the app*/
        //gnome_app_set_contents (GNOME_APP (app), label);
       /*create the menus for the application*/
        gnome_app_create_menus (GNOME_APP (app), menubar);

	gtk_widget_show_all (app);
	/* Set up the RGB buffer. */
	gtk_main ();
	gdk_threads_leave ();
	return 0;
}
