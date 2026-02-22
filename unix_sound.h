#ifdef __cplusplus
extern "C"
{
#endif
#ifdef PPALSA
/* Use the newer ALSA API */
#define ALSA_PCM_NEW_HW_PARAMS_API
/* All of the ALSA library API is defined
 * in this header */
#include <alsa/asoundlib.h>
#else
#include <linux/soundcard.h>
#endif
typedef struct soundbuff soundbuff;

struct soundbuff
{
	soundbuff *next;
#ifdef PPALSA
	u8     *resonance_buff;
#endif
	u32    num_samples;
	s16    sample_buff[0];
};

typedef struct 
{
	struct soundbuff *curr;
	u32       curr_sample_idx;
} soundbuff_walker_linkedlist;


typedef struct
{
	union 
	{
		soundbuff_walker_linkedlist ll;
		u32 curr_sample_idx;
	} u;
} soundbuff_walker;

struct soundbuff *alloc_record_buff(
#ifndef PPALSA
	u32 *buff_offset
#endif
);
void free_sound_buffs();
void record_dev_dsp();
void play_dev_dsp();
void init_soundbuff_linkedlist(soundbuff_walker *walker);
extern struct soundbuff *record_head;
typedef void (*init_soundbuff_t)(soundbuff_walker *walker);
typedef int (*read_sample_noinc_t)(soundbuff_walker *walker,s16 *sample);
typedef int (*read_sample_t)(soundbuff_walker *walker,s16 *sample);
typedef int (*write_sample_noinc_t)(soundbuff_walker *walker,s16 sample);
typedef int (*write_sample_t)(soundbuff_walker *walker,s16 sample);
#ifdef PPALSA
void write_sample_alsa(soundbuff_walker *walker,s16 sample);
#endif

extern init_soundbuff_t init_soundbuff_procptr;
extern read_sample_noinc_t  read_sample_noinc_procptr;
extern read_sample_t  read_sample_procptr;
extern write_sample_noinc_t write_sample_noinc_procptr;
extern write_sample_t write_sample_procptr;
void initialise_walker_function_pointers(void);

typedef enum
{
	idle,
	stopping_current_work,
	reading_wav,
	writing_wav,
	recording,
	playback,
	maximising_sample_volume,
	generating_test_sample,
	doing_resonance_analysis,
	playingback_resonance_analysis
} worker_state_enum;
void set_worker_state(worker_state_enum new_worker_state);


extern worker_state_enum worker_state;

void fix_buffer(s16 *buff,u32 num_samples);
#ifdef PPALSA
snd_pcm_t *init_dev_dsp(int recording);
void alsa_close(snd_pcm_t *handle);
#endif
#ifdef __cplusplus
}
#endif
