#ifdef __cplusplus
extern "C" 
{
#endif
void read_wav(char *filename);
void process_wav();

#define FILTER_POSITIVE_FEEDBACK
#define MAX_ENVELOPE_VAL 0x70000000
void calculate_filter_coefficents(u32 factor
#ifdef FILTER_POSITIVE_FEEDBACK
				  ,u32 filter_positive_feedback1
	                          ,u32 filter_positive_feedback2
#endif
);
void generate_test_sample(double low_freq,double high_freq,u32 sample_rate,u32 num_seconds);
extern u32 sample_rate;
extern u8  *buff;
extern u32 buff_size,buff_num_samples;
extern s16 *record_buffer;
#define LOOP_BREAKER       (0.99999)
extern u32 sine_tables_num_entries;

typedef struct
{
	char riff_tag[4];
	u32 length;
	char wave_tag[4];
}  __attribute__ ((packed)) riff_struct;

typedef struct
{
	char fmt_tag[4];
	u32 length;
	u16 compression_tag;
	u16 num_channels;
	u32 sample_rate;
	u32 average_bytes_per_second;
	u16 bytes_per_sample;
	u16 bits_per_sample;
	u16 extra_format_bytes;
}  __attribute__ ((packed)) format_struct;

typedef struct
{
	char data_tag[4];
	u32  length;
	s8   databuf[0];
}__attribute__ ((packed)) data_struct;

typedef struct
{
	char fact_tag[4];
	u32  chunk_data;
	u32  format_dependant_data;
} __attribute__ ((packed)) fact_struct;

extern riff_struct   *riff;
extern format_struct *format;
extern data_struct   *data;
extern fact_struct   *fact;


typedef s16 sine_table_t;
#define MAX_SINE_TABLE_T (32677)
#define FLOAT_MAX_SINE_TABLE_T (32677.5)

void calc_note_and_portamento_inc(u32 portamento_scaler,double high_note);
//#define LINEAR_PITCH
//#define RESONANCE_AND_ENVELOPE_BUFF
//#define DELTA_BUFF
#ifdef LINEAR_PITCH
void generate_sine_tables(double low_freq,double high_freq,
			 double freq_inc,u32 sample_rate);
#else
void generate_sine_tables(double low_freq,double high_freq,u32 sample_rate);
#endif
typedef struct
{
	double freq;
#ifdef LINEAR_PITCH
	u32 note_idx;
#endif
#if 0
	double power_factor;
#endif
	u32    filter_coefficent1;
	u32    filter_coefficent2;
	u32    wavelength_coeff;
	u32    num_entries;
	s32    cosine_idx;
	s32    curr_sine_idx;
	s32    curr_cosine_idx;
#ifdef RESONANCE_AND_ENVELOPE_BUFF
	s32    last_sine_envelope_val;
	s32    last_cosine_envelope_val;
	s32    *sine_envelope_buff;
	s32    *sine_resonance_buff;
	s32    *cosine_envelope_buff;
	s32    *cosine_resonance_buff;
#else
	s32    sine_envelope_val;
	s32    sine_resonance_val;
	s32    cosine_envelope_val;
	s32    cosine_resonance_val;
#ifdef DELTA_BUFF
	double delta_envelope_val;
#endif
#endif
	sine_table_t sine[0];
} sine_table_struct;

typedef int  (*read_sample_func_t)(void *this_ptr,s16 *sample);
#ifdef RESONANCE_AND_ENVELOPE_BUFF
extern u32 triple_buff_envelope_buff_size;
typedef void (*resonance_analysis_callback_t)(u32 buff_offset,u32 num_samples,
					     sine_table_struct **sine_tables,u32 sine_table_offset);
#else
typedef void (*resonance_analysis_callback_t)(u32 buff_offset,
#ifdef PPALSA
					      soundbuff_walker *walker,
#endif
					      sine_table_struct **sine_tables);

#endif
extern void resonance_analysis(resonance_analysis_callback_t callback,
			      soundbuff_walker *walker,
			       u32 start_sine_table_entry,
			       u32 end_sine_table_entry);
double calc_note_from_middle_c(int num_notes);
void agc_sample();
void resample_buffer(u32 new_sample_rate);
#ifdef __cplusplus
}
#endif
