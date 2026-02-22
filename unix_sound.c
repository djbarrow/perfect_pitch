#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "utils.h"
#include "unix_sound.h"
#include "perfect_pitch.h"
#include <portaudio.h>

extern double x_per_sample;

worker_state_enum worker_state;

#define SIZE 16      /* sample size: 8 or 16 bits */
#define CHANNELS 1  /* 1 = mono 2 = stereo */

#ifdef PPALSA
snd_pcm_sframes_t frames;

void alsa_close(snd_pcm_t *handle)
{
	snd_pcm_drain(handle);
	snd_pcm_close(handle);
}

snd_pcm_t *init_dev_dsp(int recording)
{
	int rc;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	int dir;
	snd_pcm_uframes_t new_frames;

	rc = snd_pcm_open(&handle, "default",
			  recording ? SND_PCM_STREAM_CAPTURE :
			  SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) {
		fprintf(stderr,
			"unable to open pcm device: %s\n",
			snd_strerror(rc));
		exit(1);
	}

	/* Allocate a hardware parameters object. */
	snd_pcm_hw_params_alloca(&params);

	/* Fill it in with default values. */
	snd_pcm_hw_params_any(handle, params);

	/* Set the desired hardware parameters. */

	/* Interleaved mode */
	snd_pcm_hw_params_set_access(handle, params,
                      SND_PCM_ACCESS_RW_NONINTERLEAVED);

	/* Signed 16-bit little-endian format */
	snd_pcm_hw_params_set_format(handle, params,
				     SND_PCM_FORMAT_S16_LE);

	/* Two channels (stereo) */
	snd_pcm_hw_params_set_channels(handle, params, 1);

	/* 44100 bits/second sampling rate (CD quality) */
	val = sample_rate;
	snd_pcm_hw_params_set_rate_near(handle, params,
                                  &val, &dir);
	snd_pcm_hw_params_set_period_size_near(handle,
                              params, &frames, &dir);

	/* Write the parameters to the driver */
	rc = snd_pcm_hw_params(handle, params);
	if (rc < 0) {
		fprintf(stderr,
			"unable to set hw parameters: %s\n",
			snd_strerror(rc));
		exit(1);
	}

	/* Use a buffer large enough to hold one period */
	snd_pcm_hw_params_get_period_size(params, &new_frames,
                                    &dir);
	if(new_frames!=frames)
	{
		fprintf(stderr,"frames!=new_frames\n");
		exit(-1);
	}
	return handle;
}
#else
int init_dev_dsp()
{
	int fd;	/* sound device file descriptor */
	int arg;	/* argument for ioctl calls */
	int status;   /* return status of system calls */

	/* open sound device */
	fd = open("/dev/dsp", O_RDWR);
	if (fd < 0) {
		perror("open of /dev/dsp failed");
		exit(1);
	}
	 arg = SIZE;	   /* sample size */
	 status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
	 if (status == -1)
		 perror("SOUND_PCM_WRITE_BITS ioctl failed");
	 if (arg != SIZE)
		 perror("unable to set sample size");

	 arg = CHANNELS;  /* mono or stereo */
	 status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
	 if (status == -1)
		 perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
	 if (arg != CHANNELS)
		 perror("unable to set number of channels");
	 arg = sample_rate;	   /* sampling rate */
	 status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
	 if (status == -1)
		 perror("SOUND_PCM_WRITE_WRITE ioctl failed");
	 sample_rate=arg;
	 printf("rate=%d\n",sample_rate);
	 return fd;
}
#endif


void fix_buffer(s16 *buff,u32 num_samples)
{
#ifdef PPALSA
	(void) buff;
	(void) num_samples;
	u32 sample_idx;

	for(sample_idx=0;sample_idx<num_samples;sample_idx++)
		buff[sample_idx]+=0x8000;
#endif	
}

struct soundbuff *alloc_record_buff(
#ifndef PPALSA
u32 *buff_offset
#endif
)
{
	int num_samples=0;
	struct soundbuff *soundbuff;
	extern double x_per_sample;
#ifdef PPALSA
	num_samples=frames;
#else
	num_samples=0;
	while(1)
	{
		*buff_offset+=1;
		num_samples++;
		if((u32)(*buff_offset*x_per_sample)!=
		   (u32)((*buff_offset-1)*x_per_sample))
			break;
	
	}
#endif
	soundbuff=myalloc("soundbuff",offsetof(struct soundbuff,sample_buff[num_samples]));
	soundbuff->next=NULL;
#ifdef PPALSA
	soundbuff->resonance_buff=NULL;
#endif
	soundbuff->num_samples=num_samples;
	return soundbuff;

}

struct soundbuff *record_head;
void free_record_buff()
{
	soundbuff *curr,*next;
	curr=record_head;
	while(curr)
	{
		next=curr->next;
#ifdef PPALSA
		if(curr->resonance_buff)
			free(curr->resonance_buff);
#endif
		free(curr);
		curr=next;
	}
	record_head=NULL;

}

void free_sound_buffs()
{
	free_record_buff();
	if(record_buffer)
	{
		free(record_buffer);
		record_buffer=NULL;
	}
	
}

#ifndef PORTAUDIO
void record_dev_dsp()
{
#ifdef PPALSA
	snd_pcm_t *handle;
	int rc;
#else
	int fd;
	u32 buff_offset=0;
#endif
	soundbuff *curr;

	free_sound_buffs();
#ifdef PPALSA
	handle=init_dev_dsp(1);
#else
	fd=init_dev_dsp();
#endif
	record_head=alloc_record_buff(
#ifndef PPALSA

		&buff_offset
#endif
);
	initialise_walker_function_pointers();
	curr=record_head;
	while(1)
	{
#ifdef PPALSA
		rc = snd_pcm_readi(handle,&curr->sample_buff,frames);
		if (rc == -EPIPE) {
			/* EPIPE means overrun */
			fprintf(stderr, "overrun occurred\n");
			snd_pcm_prepare(handle);
		} else if (rc < 0) {
			fprintf(stderr,
				"error from read: %s\n",
				snd_strerror(rc));
		} else if (rc != (int)frames) {
			fprintf(stderr, "short read, read %d frames\n", rc);
		}
#else
		myread(fd,&curr->sample_buff,curr->num_samples*2);
#endif
		fix_buffer(&curr->sample_buff[0],curr->num_samples);
		num_samples+=curr->num_samples;
		if(worker_state==stopping_current_work)
			break;
		curr->next=alloc_record_buff(
#ifndef PPALSA
			&buff_offset
#endif
);
		curr=curr->next;
	}
#ifdef PPALSA
	alsa_close(handle);
#else
	close(fd);
#endif
}

void play_dev_dsp()
{
#ifdef PPALSA
	snd_pcm_t *handle;
	int rc;
#else
	int fd;
#endif
	soundbuff *curr;
#ifdef PPALSA
	handle=init_dev_dsp(0);
#else
	fd=init_dev_dsp();
	if(record_buffer)
	{
		fix_buffer((s16 *)record_buffer,num_samples);
		mywrite(fd,record_buffer,buff_size);
	}
	else
#endif
	if(record_head)
	{
		curr=record_head;
		while(curr)
		{
			fix_buffer(&curr->sample_buff[0],curr->num_samples);
#ifdef PPALSA
			rc = snd_pcm_writei(handle,
					    &curr->sample_buff,
					    curr->num_samples);
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

#else
			mywrite(fd,&curr->sample_buff,curr->num_samples*2);
#endif
			fix_buffer(&curr->sample_buff[0],curr->num_samples);
		
		}
	}
#ifdef PPALSA
	alsa_close(handle);
#else
	close(fd);
#endif
}
#endif
#ifdef PORTAUDIO
void record_dev_dsp()
{
	PaStreamParameters  inputParameters;
	PaStream*           stream;
	PaError             err = paNoError;
	soundbuff *curr;

	free_sound_buffs();
	buff_num_samples=0;

	err = Pa_Initialize();
	if( err != paNoError ) 
		goto exit4;
	    inputParameters.device = Pa_GetDefaultInputDevice(); /* default input device */
	    inputParameters.channelCount = 1;                    /* stereo input */
	    inputParameters.sampleFormat = paInt16;
	    inputParameters.suggestedLatency = Pa_GetDeviceInfo( inputParameters.device )->defaultHighInputLatency;
	    inputParameters.hostApiSpecificStreamInfo = NULL;

    /* Record some audio. -------------------------------------------- */
	    err = Pa_OpenStream(
		    &stream,
		    &inputParameters,
		    NULL,                  /* &outputParameters, */
		    sample_rate,
		    frames,
		    paClipOff,      /* we won't output out of range samples so don't bother clipping them */
		    NULL,
		    NULL );
	    if( err != paNoError ) 
		    goto exit3;
	    err=Pa_StartStream(stream);
	    if( err != paNoError ) 
		    goto exit2;
	    record_head=alloc_record_buff(
#ifndef PPALSA

		&buff_offset
#endif
		    );
	    initialise_walker_function_pointers();
	    curr=record_head;
	    while(1)
	    {
		    err = Pa_ReadStream(stream,&curr->sample_buff,frames);
		    if(err)
			    goto exit1;
		    buff_num_samples+=curr->num_samples;
		    if(worker_state==stopping_current_work)
			    break;
		    curr->next=alloc_record_buff(
#ifndef PPALSA
			    &buff_offset
#endif
);
		    curr=curr->next;
	    }
exit1:
	    Pa_StopStream(stream);
exit2:
	    Pa_CloseStream(stream);
exit3:
	    Pa_Terminate();
exit4:
	    return;
}

void play_dev_dsp()
{
	PaStreamParameters  outputParameters;
	PaStream*           stream;
	PaError             err = paNoError;
	soundbuff *curr;

	err = Pa_Initialize();
	if( err != paNoError ) 
		goto exit4;
	outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
	outputParameters.channelCount = 1;                     /* stereo output */
	outputParameters.sampleFormat =  paInt16;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
	outputParameters.hostApiSpecificStreamInfo = NULL;
	
	err = Pa_OpenStream(
		&stream,
		NULL, /* no input */
		&outputParameters,
		sample_rate,
		frames,
		paClipOff,      /* we won't output out of range samples so don't bother clipping them */
		NULL,
		NULL);
	if( err != paNoError ) 
		goto exit3;

	err = Pa_StartStream( stream );
	if( err != paNoError ) 
		goto exit2;
	curr=record_head;
	while(curr)
	{
		err = Pa_WriteStream( stream, 
				      &curr->sample_buff,
				      curr->num_samples);
		if( err != paNoError ) 
			goto exit1;
		if(worker_state==stopping_current_work)
			break;
		curr=curr->next;
	}
exit1:
	    Pa_StopStream(stream);
exit2:
	    Pa_CloseStream(stream);
exit3:
	    Pa_Terminate();
exit4:
	    return;
}
#endif


void init_soundbuff_linkedlist(soundbuff_walker *walker)
{
	walker->u.ll.curr=record_head;
	walker->u.ll.curr_sample_idx=0;
}

int read_sample_noinc_linkedlist(soundbuff_walker *walker,s16 *sample)
{
	
	if(walker->u.ll.curr==NULL)
	{
		*sample=0;
		return TRUE;
	}
	*sample=walker->u.ll.curr->sample_buff[walker->u.ll.curr_sample_idx];
	return FALSE;
}


int read_sample_linkedlist(soundbuff_walker *walker,s16 *sample)
{
	
	if(walker->u.ll.curr==NULL)
	{
		*sample=0;
		return TRUE;
	}
	*sample=walker->u.ll.curr->sample_buff[walker->u.ll.curr_sample_idx++];
	if(walker->u.ll.curr_sample_idx>=walker->u.ll.curr->num_samples)
	{
		walker->u.ll.curr=walker->u.ll.curr->next;
		walker->u.ll.curr_sample_idx=0;
	}
	return FALSE;
}


int write_sample_noinc_linkedlist(soundbuff_walker *walker,s16 sample)
{
	
	if(walker->u.ll.curr==NULL||walker->u.ll.curr_sample_idx>=walker->u.ll.curr->num_samples)
	{
		return TRUE;
	}
	walker->u.ll.curr->sample_buff[walker->u.ll.curr_sample_idx]=sample;
	return FALSE;
}


#ifdef PPALSA
void write_sample_alsa(soundbuff_walker *walker,s16 sample)
{
	if(record_head==NULL)
	{
		record_head=alloc_record_buff();
	}
	else if(walker->u.ll.curr_sample_idx>=walker->u.ll.curr->num_samples)
	{
		walker->u.ll.curr->next=alloc_record_buff();
		walker->u.ll.curr=walker->u.ll.curr->next;
		walker->u.ll.curr_sample_idx=0;
	}
	walker->u.ll.curr->sample_buff[walker->u.ll.curr_sample_idx++]=sample;
}
#endif

int write_sample_linkedlist(soundbuff_walker *walker,s16 sample)
{
	
	if(walker->u.ll.curr==NULL)
	{
		return TRUE;
	}
	walker->u.ll.curr->sample_buff[walker->u.ll.curr_sample_idx++]=sample;
	if(walker->u.ll.curr_sample_idx>=walker->u.ll.curr->num_samples)
	{
		walker->u.ll.curr=walker->u.ll.curr->next;
		walker->u.ll.curr_sample_idx=0;
	}
	return FALSE;
}



void init_soundbuff_buff(soundbuff_walker *walker)
{
	walker->u.curr_sample_idx=0;
}

int read_sample_noinc_buff(soundbuff_walker *walker,s16 *sample)
{
	
	if(walker->u.curr_sample_idx>=buff_num_samples)
	{
		*sample=0;
		return TRUE;
	}
	*sample=record_buffer[walker->u.curr_sample_idx];
	return FALSE;
}

int read_sample_buff(soundbuff_walker *walker,s16 *sample)
{
	
	if(walker->u.curr_sample_idx>=buff_num_samples)
	{
		*sample=0;
		return TRUE;
	}
	*sample=record_buffer[walker->u.curr_sample_idx++];
	return FALSE;
}

int write_sample_noinc_buff(soundbuff_walker *walker,s16 sample)
{
	if(walker->u.curr_sample_idx>=buff_num_samples)
	{
		return TRUE;
	}
	record_buffer[walker->u.curr_sample_idx]=sample;
	return FALSE;
}


int write_sample_buff(soundbuff_walker *walker,s16 sample)
{
	if(walker->u.curr_sample_idx>=buff_num_samples)
	{
		return TRUE;
	}
	record_buffer[walker->u.curr_sample_idx++]=sample;
	return FALSE;
}

init_soundbuff_t init_soundbuff_procptr=NULL;
read_sample_noinc_t  read_sample_noinc_procptr=NULL;
read_sample_t  read_sample_procptr=NULL;
write_sample_noinc_t write_sample_noinc_procptr=NULL;
write_sample_t write_sample_procptr=NULL;

void initialise_walker_function_pointers()
{
	if(record_buffer)
	{
		if(record_head)
		{
			exit_error("initialise_walker_function_pointers "
				   " record_buffer & record_head != NULL");
		}
		else
		{
			init_soundbuff_procptr=
				init_soundbuff_buff;
			read_sample_noinc_procptr=read_sample_noinc_buff;
			read_sample_procptr=read_sample_buff;
			write_sample_noinc_procptr=write_sample_noinc_buff;
			write_sample_procptr=write_sample_buff;
		}
	} else if(record_head)
	{
		init_soundbuff_procptr=
			init_soundbuff_linkedlist;
		read_sample_noinc_procptr=read_sample_noinc_linkedlist;
		read_sample_procptr=read_sample_linkedlist;
		write_sample_noinc_procptr=write_sample_noinc_linkedlist;
		write_sample_procptr=write_sample_linkedlist;
	}
	else
	{
		exit_error("initialise_walker_function_pointers "
				   " record_buffer & record_head != NULL");
	}
	
}
