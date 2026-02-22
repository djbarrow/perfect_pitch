#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <math.h>
#include "utils.h"
#include "unix_sound.h"
#include "perfect_pitch.h"


#define PI (3.14159265)
off_t filelen;
u8    *buff,*curr_buff;
ssize_t readlen;
u32     buff_size,buff_num_samples;
s16    *record_buffer;
u32    true_bytes_per_sample;
sine_table_struct **sine_tables=NULL;
u32 sine_tables_num_entries=0;

#if 0
u32     fifo_rd_ptr=0;
u32     fifo_wr_ptr=0;
typedef enum
{
	last_op_read,
	last_op_write
} last_op_t;
last_op_t last_op=last_op_write;
#endif
#ifdef RESONANCE_AND_ENVELOPE_BUFF
u32       envelope_buff_size;
u32       triple_buff_envelope_buff_size;
#endif

static inline void prefetch(const void *x)
{
	asm( "prefetchnta (%0)"
	     : "=r" (x) : );
}


off_t getfilelen(int fd)
{
   struct stat buf;

   if(fstat(fd,&buf))
      exit(errno);
   return buf.st_size;
}





s32 myround(double a)
{
  if(a>=0.0)
    return (s32)(a+0.5);
  else
    return (s32)(a-0.5);
}

u32    sample_rate;

u32 wavelength_factor;
void calculate_filter_coefficents(u32 factor
#ifdef FILTER_POSITIVE_FEEDBACK
				  ,u32 filter_positive_feedback1
	                          ,u32 filter_positive_feedback2
#endif

)
{
	u32 i,j;
	u32 wavelength,wavelength_coeff;
	u32 filter_coefficent;
	s32 num,oldnum;
	sine_table_struct *curr_sine_table;

	wavelength_factor=factor;
	filter_coefficent=65535;
	for(i=0;i<sine_tables_num_entries;i++)
	{
		curr_sine_table=sine_tables[i];
		wavelength=sample_rate/curr_sine_table->freq;
		wavelength_coeff=wavelength*wavelength_factor;
#ifdef RESONANCE_AND_ENVELOPE_BUFF
		if(wavelength_coeff>triple_buff_envelope_buff_size)
			wavelength_coeff=triple_buff_envelope_buff_size;
#else
#if 1
		//if(wavelength_coeff>(sample_rate/10))
		//	wavelength_coeff=(sample_rate/4);
		if(wavelength_coeff>(sample_rate/4))
			wavelength_coeff=(sample_rate/4);
#endif
#endif
		curr_sine_table->wavelength_coeff=wavelength_coeff;
		
		while(1)
		{
			num=65536*32677;
			for(j=0;num>0;j++)
			{
				oldnum=num;
				num=((num/65536)*filter_coefficent);
				if(oldnum==num)
					num=num-1;
			}
			if(j<wavelength_coeff)
			{
//#define DJ3
#ifdef DJ3
				filter_coefficent=65536;
#endif
				curr_sine_table->filter_coefficent1=filter_coefficent
#ifdef FILTER_POSITIVE_FEEDBACK
				       +filter_positive_feedback1
#endif
					;
				curr_sine_table->filter_coefficent2=(65536-filter_coefficent)
#ifdef FILTER_POSITIVE_FEEDBACK
					+filter_positive_feedback2
#endif
					;
				break;
			}
			else
				filter_coefficent--;
		}
	}
}

void generate_sine_table(double freq,s32 sine_table_idx,sine_table_t **tmp_table,u32 *tmp_table_size)
{
	sine_table_t last_entry_perfect,tmp_sine;
	sine_table_struct *sine_table;
	s32 k,cosine_idx;
	double sample_increment,j;
//#define DJ2
#ifdef DJ2
	long long testsin;
#endif
	sample_increment=(freq*2*PI)/sample_rate; 
	last_entry_perfect=
		sin(2*PI-sample_increment)
		*FLOAT_MAX_SINE_TABLE_T;
	cosine_idx=-1;
	for(j=0,k=0;;j+=sample_increment,k++)
	{
		if(k==*tmp_table_size)
		{
			*tmp_table_size<<=1;
			*tmp_table=(sine_table_t *)realloc(*tmp_table,
							  *tmp_table_size*sizeof(**tmp_table));
			if(*tmp_table==NULL)
				exit_error("couldn't allocate tmp_table\n");;
		}
		tmp_sine=sin(j)*
			FLOAT_MAX_SINE_TABLE_T;
#ifdef DJ2
		testsin=tmp_sine;
#endif
		(*tmp_table)[k]=tmp_sine;
		if(tmp_sine>=MAX_SINE_TABLE_T)
			cosine_idx=k;
		if(tmp_sine==last_entry_perfect&&cosine_idx!=-1)
		{
#ifdef DJ2
			printf("freq=%e testsin=%lld\n",freq,testsin);
#endif
			sine_table=(sine_table_struct *)myalloc("sine_table",
								offsetof(sine_table_struct,
									 sine[k]));
#ifdef RESONANCE_AND_ENVELOPE_BUFF
			sine_table->sine_envelope_buff=(s32 *)myalloc("envelope_buff",
								      sizeof(s32)*envelope_buff_size*4);
			sine_table->sine_resonance_buff=&sine_table->sine_envelope_buff[envelope_buff_size];
			sine_table->cosine_envelope_buff=&sine_table->sine_resonance_buff[envelope_buff_size];
			sine_table->cosine_resonance_buff=&sine_table->cosine_envelope_buff[envelope_buff_size];
#endif	
			sine_table->num_entries=k;
			sine_table->cosine_idx=cosine_idx;
			sine_tables[sine_table_idx]=sine_table;
			memcpy(sine_table->sine,*tmp_table,k*sizeof(sine_table_t));
			sine_table->freq=freq;
#if 0
			sine_table->power_factor=
				((freq*bandwidth_factor)-(freq/bandwidth_factor))/hi_freq2;
#endif
			break;
		}
	}
}


double note_inc,portamento_inc;
double loop_break_b,loop_break_c;
double hi_freq;

void calc_note_and_portamento_inc(u32 portamento_scaler,double high_note)
{
	note_inc=pow(2.0,1.0/12);
	portamento_inc=pow(2.0,(1.0/(12.0*portamento_scaler)));
	hi_freq=(high_note*LOOP_BREAKER)/portamento_inc;
	sine_tables_num_entries=0;
	loop_break_b=2*LOOP_BREAKER;
	loop_break_c=note_inc*LOOP_BREAKER;

}
#ifdef LINEAR_PITCH
u32 get_note_idx_from_freq(double low_note,double freq)
{
	u32 note_idx=0;
	double a,b,c,i;
	for(a=low_note;;a*=2)
	{
		for(b=1;b<loop_break_b;b*=note_inc)
		{
			for(c=1;c<loop_break_c;c*=portamento_inc)
			{
				i=a*b*c;
				if(i>=freq)
					return note_idx;
				note_idx++;
			}
		}
	}
}

void generate_sine_tables(double low_freq,double high_freq,
			 double freq_inc,u32 sample_rate)
{
	double curr_freq;
	u32 tmp_table_size=65536;
	sine_table_t *tmp_table;
	u32 idx;

	printf("generating sine tables\n");
	sine_tables_num_entries=0;
	for(curr_freq=low_freq;curr_freq<=high_freq;curr_freq+=freq_inc)
		sine_tables_num_entries++;
	// The +1 in the malloc is so we can use prefectching.
	sine_tables=(sine_table_struct **)myalloc("sine_tables",
						  (sine_tables_num_entries+1)*
						  sizeof(sine_table_struct *));
	tmp_table=(sine_table_t *)myalloc("tmp_table",tmp_table_size*sizeof(*tmp_table));
	idx=0;
	for(curr_freq=low_freq;curr_freq<=high_freq;curr_freq+=freq_inc)
	{
		generate_sine_table(curr_freq,idx,&tmp_table,&tmp_table_size);
		
		get_note_idx_from_freq(low_freq,curr_freq);
		sine_tables[idx]->note_idx=
			get_note_idx_from_freq(low_freq,curr_freq);
		idx++;
	}
	
}
#else
void generate_sine_tables(double low_freq,double high_freq,u32 sample_rate)
{
#if 0
	double bandwidth_factor
#endif
		;
	double a,b,c,i;
	u32 tmp_table_size=65536;
	sine_table_t *tmp_table;
	s32 l;
#ifdef RESONANCE_AND_ENVELOPE_BUFF
	u32 min_triple_buff_size;
#endif
	printf("generating sine tables\n");
#if 0
	bandwidth_factor=pow(2.0,(1.0/(12.0*(portamento_scaler/2))));
#endif
	sine_tables_num_entries=0;
#ifdef RESONANCE_AND_ENVELOPE_BUFF
	min_triple_buff_size=(sample_rate*4)/low_freq;
	triple_buff_envelope_buff_size=(sample_rate>>4)/3;
	if(triple_buff_envelope_buff_size<min_triple_buff_size)
		triple_buff_envelope_buff_size=min_triple_buff_size;
	envelope_buff_size=triple_buff_envelope_buff_size*3;
#endif

	for(a=low_freq;;a*=2)
	{
		for(b=1;b<loop_break_b;b*=note_inc)
		{
			for(c=1;c<loop_break_c;c*=portamento_inc)
			{
				i=a*b*c;
				if(i>hi_freq)
					goto loop_break;
				sine_tables_num_entries++;
			}
		}
	}
 loop_break:
	// The +1 in the malloc is so we can use prefectching.
	sine_tables=(sine_table_struct **)myalloc("sine_tables",
						  (sine_tables_num_entries+1)*
						  sizeof(sine_table_struct *));
	tmp_table=(sine_table_t *)myalloc("tmp_table",tmp_table_size*sizeof(*tmp_table));
	l=0;
	for(a=low_freq;;a*=2)
	{
		for(b=1;b<loop_break_b;b*=note_inc)
		{
			for(c=1;c<loop_break_c;c*=portamento_inc)
			{
				
				i=a*b*c;
				if(i>hi_freq)
					goto loop_break2;
				generate_sine_table(i,l,&tmp_table,&tmp_table_size);
				l++;
			}
		}
	}
loop_break2:
	return;
}
#endif /* LINEAR_PITCH */


#if 0
s32 get_y_val(s32 x1,s32 y1,s32 x2,s32 y2,s32 xpt)
{
	double x1ft=(double)x1,y1ft=(double)y1,x2ft=(double)x2,y2ft=(double)y2;
	double xptft=(double)xpt,y1factor;
	double slope;

	if(xptft>x2ft||xptft<x1ft)
	{
		exit_error("get_y_val illegal xptft\n");
		exit(-1);

	}
	if(xptft==x1ft)
		y1factor=0;
	else
		y1factor=(x2ft-x1ft)/(xptft-x1ft);
	return myround(((y2ft-y1ft)*y1factor)+y1ft);
	
}
#endif

//#define TEST_SAMPLE_TYPE (2)
#define TEST_SAMPLE_TYPE (0)
void generate_test_sample(double low_freq,double high_freq,u32 sample_rate,u32 num_seconds)
{
	u32 i;
	double note1,note2,note3,note_freq_inc,
		phase1,phase2,phase3;
#ifdef PPALSA
	soundbuff_walker walker;
#endif	
	free_sound_buffs();
	buff_num_samples=sample_rate*num_seconds;
#ifdef PPALSA
	record_head=alloc_record_buff();
	init_soundbuff_linkedlist(&walker);
#else
	buff_size=buff_num_samples*sizeof(s16*);

	record_buffer=(s16 *)myalloc("record_buffer",buff_size);
#endif
	note1=low_freq;
	note2=high_freq/2;
	note3=high_freq;
	note_freq_inc=((double)high_freq-(double)low_freq)/buff_num_samples;
	phase1=0;
	phase2=0;
	phase3=0;
#if TEST_SAMPLE_TYPE==2
	note1=low_freq*8;
#endif
#if TEST_SAMPLE_TYPE>0
	for(i=0;i<buff_num_samples;i++)
	{
#if TEST_SAMPLE_TYPE==1
		if(i<480)
		{
			if((i%80)<40)
#ifdef PPALSA
				write_sample_alsa(&walker,MAX_SINE_TABLE_T);
			else
				write_sample_alsa(&walker,-MAX_SINE_TABLE_T);
#endif
				record_buffer[i]=MAX_SINE_TABLE_T;
			else
				record_buffer[i]=-MAX_SINE_TABLE_T;
		}
#endif
#if TEST_SAMPLE_TYPE==2
#ifdef PPALSA
		write_sample_alsa(&walker,
				  sin(phase1)*(MAX_SINE_TABLE_T));			
#else
		record_buffer[i]=sin(phase1)*(MAX_SINE_TABLE_T);
#endif
		phase1+=(note1*2*PI)/sample_rate;
#endif
	}
	
#else
	for(i=0;i<buff_num_samples;i++)
	{
#ifdef PPALSA
		write_sample_alsa(&walker,
			sin(phase1)*(MAX_SINE_TABLE_T/4)+
			sin(phase2)*(MAX_SINE_TABLE_T/4)
			+ sin(phase3)*(MAX_SINE_TABLE_T/4)
			);
#else
		record_buffer[i]=
			sin(phase1)*(MAX_SINE_TABLE_T/4)+
			sin(phase2)*(MAX_SINE_TABLE_T/4)
			+ sin(phase3)*(MAX_SINE_TABLE_T/4)
			;
#endif
		phase1+=(note1*2*PI)/sample_rate;
		phase2+=(note2*2*PI)/sample_rate;
		phase3+=(note3*2*PI)/sample_rate;
		note1+=note_freq_inc;
		note3-=note_freq_inc;
	}
#endif
}


void resample_buffer(u32 new_sample_rate)
{
	s16 *new_record_buffer;
	u32 new_num_samples=((u64)buff_num_samples*(u64)new_sample_rate)/sample_rate;
	u32 new_buff_size=new_num_samples<<1;
	double j,sample_inc=(double)sample_rate/(double)new_sample_rate;
	u32 i;
	double fraction;

	new_record_buffer=(s16 *)myalloc("new_record_buffer",new_buff_size+sizeof(*new_record_buffer));
	new_record_buffer[new_buff_size]=0;
	for(i=0;i<new_num_samples;i++)
	{
		fraction=sample_inc-(int)sample_inc;
		new_record_buffer[i]=
			record_buffer[(int)(j+1)]*fraction+
			record_buffer[(int)j]*(1-fraction);
		j+=sample_inc;
		if((j+1)>new_num_samples)
			break;
	}
	free(record_buffer);
	buff_num_samples=new_num_samples;
	record_buffer=new_record_buffer;
	sample_rate=new_sample_rate;
}


void agc_sample()
{
	s16 max_sample=0,record_tmp;
	u32 i;
	soundbuff_walker walker;
	init_soundbuff_procptr(&walker);
	
	for(i=0;i<buff_num_samples;i++)
	{
		read_sample_procptr(&walker,&record_tmp);
		if(record_tmp<0&&-record_tmp>max_sample)
			max_sample=-record_tmp;
		if(record_tmp>0&&record_tmp>max_sample)
			max_sample=record_tmp;
	}
	if(max_sample!=0)
	{
		init_soundbuff_procptr(&walker);
		for(i=0;i<buff_num_samples;i++)
		{
			read_sample_noinc_procptr(&walker,&record_tmp);
			write_sample_procptr(&walker,
					     (s16)((((s32)record_tmp)<<15)
						   /max_sample));
		}
	}
}

riff_struct   *riff=NULL;
format_struct *format=NULL;
data_struct   *data=NULL;
fact_struct   *fact=NULL;


void read_wav(char *filename)
{
	int fd;

	fd=open(filename,O_RDONLY);
	if(fd==-1)
		exit_error("Couldn't open %s\n",filename);
	filelen=getfilelen(fd);
	buff=myalloc("buff",filelen);
	readlen=read(fd,buff,filelen);
	if(readlen!=filelen)
		exit_error("Couldn't read all of file read=%d bytes.\n",readlen);
	curr_buff=buff;
	riff=(riff_struct *)curr_buff;
	if(strncmp("RIFF",riff->riff_tag,sizeof(riff->riff_tag)))
		exit_error("RIFF tag not found\n");
	else
		curr_buff+=sizeof(riff_struct);
	format=NULL;
	fact=NULL;
	data=NULL;
	do
	{
		if(strncmp("fmt ",(char *)curr_buff,sizeof(format->fmt_tag))==0)
		{
			format=(format_struct *)curr_buff;
			curr_buff+=sizeof(format_struct);
			if(format->compression_tag!=1)
			{
				exit_error("We can't deal with data compressed using compression_tag=%x",
					(int)format->compression_tag);
			}
		}
		else if(strncmp("data",(char *)curr_buff,sizeof(data->data_tag))==0)
		{
			data=(data_struct *)curr_buff;
			curr_buff+=sizeof(data_struct);
		}
		else if(strncmp("fact",(char *)curr_buff,sizeof(fact->fact_tag))==0)
		{
			fact=(fact_struct *)curr_buff;
			curr_buff+=sizeof(fact_struct);
		}
		else
		{
			curr_buff[4]=0;
			exit_error("Unknown tag %s\n",curr_buff);
		}
	} while(data==NULL);
	if(format==NULL)
	{
		exit_error("fmt  tag not found\n");
	}
	if(format->bits_per_sample>16&&(format->bits_per_sample&7)!=0)
	{
		exit_error("illegal number of bytes per sample\n");
	}
	true_bytes_per_sample=format->bytes_per_sample/format->num_channels;
	buff_size=data->length/format->num_channels;
	buff_num_samples=buff_size/true_bytes_per_sample;
	sample_rate=format->sample_rate;
	if(true_bytes_per_sample!=(format->bits_per_sample>>3))
	{
		exit_error("true bytes per sample=%d"
			" format bits per sample=%d\n",
			true_bytes_per_sample,format->bits_per_sample);
	}
	close(fd);

}



void process_wav()
{
	u32     i,j,k;
	s32    record_tmp;

	free_sound_buffs();
	record_buffer=(s16 *)myalloc("record_buffer",buff_size);
	initialise_walker_function_pointers();
        for(i=0,k=0;i<buff_num_samples;i++,
		    k+=(format->num_channels*true_bytes_per_sample))
	{
		record_tmp=0;

		for(j=0;j<format->num_channels;j++)
		{
			if(true_bytes_per_sample==1)
				record_tmp+=data->databuf[k+j]<<8;
			else
				record_tmp+=*(s16 *)(&data->databuf[k+(j*2)]);
		}
		record_tmp/=format->num_channels;
		record_buffer[i]=record_tmp;
	}
}


int write_wav_header(char *filename)
{
	int fd;

	fd=open("out.wav",O_WRONLY|O_CREAT|O_TRUNC,755);
	format->num_channels=1;
	//format->sample_rate/=format->num_channels;
	format->bytes_per_sample=true_bytes_per_sample;
	format->average_bytes_per_second=format->sample_rate*format->bytes_per_sample;
	data->length=sizeof(*record_buffer)*buff_num_samples;
	write(fd,riff,sizeof(riff_struct));
	write(fd,format,sizeof(format_struct));
	if(fact)
		write(fd,fact,sizeof(fact_struct));
	write(fd,data,sizeof(data_struct));
	return fd;
}

void write_wav(char *filename)
{
	int fd;
	fd=write_wav_header(filename);
	write(fd,record_buffer,sizeof(*record_buffer)*buff_num_samples);
	close(fd);			

}

#ifdef RESONANCE_AND_ENVELOPE_BUFF
void clean_resonance_and_envelope_buffs(int triple_buff)
{
	u32 i,buff_offset;
	sine_table_struct *curr_sine_table;

	for(i=0;i<sine_tables_num_entries;i++)
	{
		buff_offset=triple_buff_envelope_buff_size*triple_buff;
		curr_sine_table=sine_tables[i];
		memset(&curr_sine_table->sine_envelope_buff[buff_offset],0,
		       sizeof(*curr_sine_table->sine_envelope_buff)*triple_buff_envelope_buff_size);
		memset(&curr_sine_table->sine_resonance_buff[buff_offset],0,
		       sizeof(*curr_sine_table->sine_resonance_buff)*triple_buff_envelope_buff_size);
		memset(&curr_sine_table->sine_envelope_buff[buff_offset],0,
		       sizeof(*curr_sine_table->cosine_envelope_buff)*triple_buff_envelope_buff_size);
		memset(&curr_sine_table->sine_resonance_buff[buff_offset],0,
		       sizeof(*curr_sine_table->cosine_resonance_buff)*triple_buff_envelope_buff_size);
	}
}
/*
 * start_sine_table_entry is usually 0
 * end_sine_table_entry is usually sine_tables_num_entries
 */
//#define BACKWARD_TIME_FILTER
#ifdef BACKWARD_TIME_FILTER
#define FILTER_SHIFT (17) 
#else
#define FILTER_SHIFT (16)
#endif
int resonance_analysis(resonance_analysis_callback_t callback,
			soundbuff_walker *walker,
			u32 start_sine_table_entry,u32 end_sine_table_entry)
{
	u32 i,j,buff_idx;
	s32 k;
#ifdef BACKWARD_TIME_FILTER 
	s32	end_curr_buff1,end_curr_buff2;
#endif
	u32 num_entries;
	sine_table_struct *curr_sine_table;
	s32    record_tmp;
	int    clean_buff;
	s32    last_sine_envelope_val;
	s32    last_cosine_envelope_val;
	u32    last_sine_table_offset,last_buff_offset,last_num_samples;
	u32    second_last_sine_table_offset,second_last_buff_offset,second_last_num_samples;


	printf("doing resonant analysis\n");
	buff_idx=0;
	clean_resonance_and_envelope_buffs(0);	
	clean_resonance_and_envelope_buffs(1);
	clean_resonance_and_envelope_buffs(2);
	clean_buff=2;
	for(j=start_sine_table_entry;j<end_sine_table_entry;j++)
	{
		curr_sine_table=sine_tables[j];
		curr_sine_table->curr_sine_idx=0;
		curr_sine_table->curr_cosine_idx=curr_sine_table->cosine_idx;
		curr_sine_table->last_sine_envelope_val=0;
		curr_sine_table->last_cosine_envelope_val=0;
	}
	second_last_sine_table_offset=last_sine_table_offset=buff_idx=0;
	second_last_buff_offset=last_buff_offset=0;
	second_last_num_samples=last_num_samples=buff_num_samples;
	if(last_num_samples>triple_buff_envelope_buff_size)
		last_num_samples=triple_buff_envelope_buff_size;
	for(i=0;;i++)
	{
		if(read_sample_procptr(walker,&record_tmp))
			break;
		for(j=start_sine_table_entry;j<end_sine_table_entry;j++)
		{
			curr_sine_table=sine_tables[j];
			//prefetch(sine_tables[j+1]);
			num_entries=curr_sine_table->num_entries;
			curr_sine_table->sine_resonance_buff[buff_idx]=
				curr_sine_table->sine[curr_sine_table->curr_sine_idx]
				*record_tmp;
			curr_sine_table->cosine_resonance_buff[buff_idx]=
				curr_sine_table->sine[curr_sine_table->curr_cosine_idx]
				*record_tmp;
			curr_sine_table->curr_sine_idx=curr_sine_table->curr_sine_idx+1;
			if(curr_sine_table->curr_sine_idx==num_entries)
				curr_sine_table->curr_sine_idx=0;
			curr_sine_table->curr_cosine_idx=curr_sine_table->cosine_idx+1;
			if(curr_sine_table->curr_cosine_idx==num_entries)
			   curr_sine_table->curr_cosine_idx=0;
		}
		if((buff_idx%triple_buff_envelope_buff_size)==(triple_buff_envelope_buff_size-1)||i==(buff_num_samples-1))
		{
			for(j=start_sine_table_entry;j<end_sine_table_entry;j++)
			{
				curr_sine_table=sine_tables[j];
				last_sine_envelope_val=curr_sine_table->last_sine_envelope_val;
				last_cosine_envelope_val=curr_sine_table->last_cosine_envelope_val;
				for(k=last_sine_table_offset;k<=buff_idx;k++)
				{
					last_sine_envelope_val=curr_sine_table->sine_envelope_buff[k]=
						((curr_sine_table->sine_resonance_buff[k]>>FILTER_SHIFT)
						 *curr_sine_table->filter_coefficent2)+
						((last_sine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1);
					last_cosine_envelope_val=curr_sine_table->cosine_envelope_buff[k]=
						((curr_sine_table->cosine_resonance_buff[k]>>FILTER_SHIFT)
						 *curr_sine_table->filter_coefficent2)+
						((last_cosine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1);				
				}
				curr_sine_table->last_sine_envelope_val=last_sine_envelope_val;
				curr_sine_table->last_cosine_envelope_val=last_cosine_envelope_val;
#ifdef BACKWARD_TIME_FILTER
				if(i>triple_buff_envelope_buff_size)
				{
					end_curr_buff1=last_sine_table_offset;
					end_curr_buff2=end_curr_buff1+curr_sine_table->wavelength_coeff;
					last_sine_envelope_val=last_cosine_envelope_val=0;
					for(k=end_curr_buff2;k>=end_curr_buff1;k--)
					{
						last_sine_envelope_val=
							((curr_sine_table->sine_resonance_buff[k]>>FILTER_SHIFT)
							 *curr_sine_table->filter_coefficent2)+
							((last_sine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1);
						last_cosine_envelope_val=
							((curr_sine_table->cosine_resonance_buff[k]>>FILTER_SHIFT)
							 *curr_sine_table->filter_coefficent2)+
							((last_cosine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1);
					}
					end_curr_buff1--;
					if(((s32)end_curr_buff1)<0)
						end_curr_buff1=envelope_buff_size-1;	
					for(k=0;k<triple_buff_envelope_buff_size;k++,end_curr_buff1--)
					{
						last_sine_envelope_val=
							(curr_sine_table->sine_envelope_buff[end_curr_buff1]+=
							((curr_sine_table->sine_resonance_buff[end_curr_buff1]
							  >>FILTER_SHIFT)
							 *curr_sine_table->filter_coefficent2)+
							 ((last_sine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1));
						last_cosine_envelope_val=
							(curr_sine_table->cosine_envelope_buff[end_curr_buff1]+=
							((curr_sine_table->cosine_resonance_buff[end_curr_buff1]
							  >>FILTER_SHIFT)
							 *curr_sine_table->filter_coefficent2)+
							 ((last_cosine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1));
					}
					if(callback)
					{
						callback_rc=callback(second_last_buff_offset,second_last_num_samples,
							 sine_tables,second_last_sine_table_offset);
						if(callback_rc)
							return callback_rc;
					}
				}			
				if(i==(buff_num_samples-1))
				{
					last_sine_envelope_val=
						last_cosine_envelope_val=0;
					for(k=buff_idx;k>=last_sine_table_offset;k--)
					{
						last_sine_envelope_val=
							(curr_sine_table->sine_envelope_buff[k]+=
							((curr_sine_table->sine_resonance_buff[k]
							  >>FILTER_SHIFT)
							 *curr_sine_table->filter_coefficent2)+
							 ((last_sine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1));

						last_cosine_envelope_val=
							(curr_sine_table->cosine_envelope_buff[end_curr_buff1]+=
							((curr_sine_table->cosine_resonance_buff[end_curr_buff1]
							  >>FILTER_SHIFT)
							 *curr_sine_table->filter_coefficent2)+
							 ((last_cosine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1));
					}
#ifdef BACKWARD_TIME_FILTER
					if(callback)
					{
						callback_rc=callback(last_buff_offset,last_num_samples,sine_tables,
							 last_sine_table_offset);
						if(callback_rc)
							return callback_rc;
						
					}
#endif
				}
#endif
			}
#ifdef BACKWARD_TIME_FILTER
			second_last_buff_offset=last_buff_offset;
			second_last_num_samples=last_num_samples;
			second_last_sine_table_offset=last_sine_table_offset;
#else
			if(callback)
			{
				callback_rc=callback(last_buff_offset,last_num_samples,sine_tables,last_sine_table_offset);
				if(callback_rc)
					return callback_rc;
			}
#endif
			last_buff_offset=i+1;
			last_num_samples=buff_num_samples-last_buff_offset;
			if(last_num_samples>triple_buff_envelope_buff_size)
				last_num_samples=triple_buff_envelope_buff_size;
			last_sine_table_offset=buff_idx+1;
			if(last_sine_table_offset>=envelope_buff_size)
				last_sine_table_offset=0;
			clean_resonance_and_envelope_buffs(clean_buff);
			clean_buff++;
			if(clean_buff==3)
				clean_buff=0;
		}
		buff_idx++;
		if(buff_idx==envelope_buff_size)
			buff_idx=0;
			
	}
	return 0;
}
#else
#define FILTER_SHIFT (16)
void resonance_analysis(resonance_analysis_callback_t callback,
			soundbuff_walker *walker,
			u32 start_sine_table_entry,u32 end_sine_table_entry)
{
	u32 i,j;
	u32 num_entries;
	sine_table_struct *curr_sine_table;
	s32    record_tmp;
	s16    record_tmp_s16;
#ifdef DELTA_BUFF
	double sine,cosine;
#endif

	for(j=start_sine_table_entry;j<end_sine_table_entry;j++)
	{
		curr_sine_table=sine_tables[j];
		curr_sine_table->curr_sine_idx=0;
		curr_sine_table->curr_cosine_idx=curr_sine_table->cosine_idx;
		curr_sine_table->sine_envelope_val=0;
		curr_sine_table->cosine_envelope_val=0;
	}
	for(i=0;;i++)
	{
		if(read_sample_procptr(walker,&record_tmp_s16))
			break;
		record_tmp=record_tmp_s16;
		for(j=start_sine_table_entry;j<end_sine_table_entry;j++)
		{
			curr_sine_table=sine_tables[j];
			//prefetch(sine_tables[j+1]);
			curr_sine_table->sine_resonance_val=
				curr_sine_table->sine[curr_sine_table->curr_sine_idx]
				*record_tmp;
			curr_sine_table->cosine_resonance_val=
				curr_sine_table->sine[curr_sine_table->curr_cosine_idx]
				*record_tmp;
			curr_sine_table->sine_envelope_val=
				((curr_sine_table->sine_resonance_val>>FILTER_SHIFT)
				 *curr_sine_table->filter_coefficent2)+
				((curr_sine_table->sine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1);
			curr_sine_table->cosine_envelope_val=
				((curr_sine_table->cosine_resonance_val>>FILTER_SHIFT)
				 *curr_sine_table->filter_coefficent2)+
				((curr_sine_table->cosine_envelope_val>>FILTER_SHIFT)*curr_sine_table->filter_coefficent1);			
#ifdef FILTER_POSITIVE_FEEDBACK
			if(curr_sine_table->sine_envelope_val>MAX_ENVELOPE_VAL)
				curr_sine_table->sine_envelope_val=MAX_ENVELOPE_VAL;
			else if(curr_sine_table->sine_envelope_val<(-MAX_ENVELOPE_VAL))
				curr_sine_table->sine_envelope_val=-MAX_ENVELOPE_VAL;
			if(curr_sine_table->cosine_envelope_val>MAX_ENVELOPE_VAL)
				curr_sine_table->cosine_envelope_val=MAX_ENVELOPE_VAL;
			else if(curr_sine_table->cosine_envelope_val<(-MAX_ENVELOPE_VAL))
				curr_sine_table->cosine_envelope_val=-MAX_ENVELOPE_VAL;
				
#endif
#ifdef DELTA_BUFF
		        sine=curr_sine_table->sine_envelope_val/(double)0x80000000;
			cosine=curr_sine_table->cosine_envelope_val/(double)0x80000000;		
			curr_sine_table->delta_envelope_val=sqrt((sine*sine)+(cosine*cosine))*256;
#endif
//#define DJ (1)
#if DJ
			double get_envelope_val(sine_table_struct *sine_table);
			if(j==start_sine_table_entry)
			{
				double envelope_val;
				envelope_val=get_envelope_val(curr_sine_table);
				printf("ev=%d rt=%d sin=%ld cos=%ld s_rv=%ld c_rv=%d s_ev=%d c_ev=%d\n ",
				       (int)envelope_val,(int)record_tmp,
				       curr_sine_table->sine[curr_sine_table->curr_sine_idx],
				       curr_sine_table->sine[curr_sine_table->curr_cosine_idx],
				       curr_sine_table->sine_resonance_val,
				       curr_sine_table->cosine_resonance_val,
				       curr_sine_table->sine_envelope_val,curr_sine_table->cosine_envelope_val);
			}
#endif
			curr_sine_table->curr_sine_idx=curr_sine_table->curr_sine_idx+1;
			num_entries=curr_sine_table->num_entries;
			if(curr_sine_table->curr_sine_idx==num_entries)
				curr_sine_table->curr_sine_idx=0;
			curr_sine_table->curr_cosine_idx=curr_sine_table->curr_cosine_idx+1;
			if(curr_sine_table->curr_cosine_idx==num_entries)
			   curr_sine_table->curr_cosine_idx=0;
			
		}
		if(callback)
		{
			callback(i,
#ifdef PPALSA
				 walker,
#endif
				 sine_tables);
			if(worker_state==stopping_current_work)
				return;
		}
	}
}
#endif



double calc_note_from_middle_c(int num_notes)
{
	int octave,i;
	double semitone,middle_c,retval;

	semitone=pow(2.0,1.0/12);
	middle_c=440*semitone*semitone*semitone;
	if(octave<0)
		octave=((double)num_notes/12.0)-0.99;
	else
		octave=num_notes/12;
	retval=middle_c;
	while(octave<0)
	{
		retval=retval/2.0;
		octave++;
		num_notes+=12;
	}
	while(octave>0)
	{
		retval=retval*2.0;
		octave--;
		num_notes-=12;
	}
	for(i=0;i<num_notes;i++)
		retval*=semitone;
	return retval;
}

