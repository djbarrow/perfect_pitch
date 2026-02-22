#include "unix_sound.h"
#ifdef __cplusplus
extern "C"
{
#endif
	void *worker_thread_func(void *unused);
	void set_new_work_state(worker_state_enum new_state);
	void init_worker();
#ifdef __cplusplus
}
#endif
