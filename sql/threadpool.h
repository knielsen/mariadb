
#define MAX_THREAD_GROUPS 128

/* Threadpool parameters */
extern uint threadpool_min_threads;  /* Minimum threads in pool */
extern uint threadpool_idle_timeout; /* Shutdown idle worker threads  after this timeout */
extern uint threadpool_size; /* Number of parallel executing threads */
extern uint threadpool_stall_limit;  /* time interval in 10 ms units for stall checks*/
extern uint threadpool_max_threads;  /* Maximum threads in pool */
extern uint threadpool_oversubscribe;  /* Maximum active threads in group */


/*
  Functions used by scheduler. 
  OS-specific implementations are in
  threadpool_unix.cc or threadpool_win.cc
*/
extern bool tp_init();
extern void tp_add_connection(THD*);
extern void tp_wait_begin(THD *, int);
extern void tp_wait_end(THD*);
extern void tp_post_kill_notification(THD *thd);
extern void tp_end(void);

/*
  Threadpool statistics
*/
struct TP_STATISTICS
{
  /* Current number of worker thread. */
  volatile int32 num_worker_threads;
  /* Current number of idle threads. */
  volatile int32 num_waiting_threads;
};

extern TP_STATISTICS tp_stats;


/* Functions to set threadpool parameters */
extern void tp_set_min_threads(uint val);
extern void tp_set_max_threads(uint val);
extern int tp_set_threadpool_size(uint val);
extern void tp_set_threadpool_stall_limit(uint val);

/* Activate threadpool scheduler */
extern void tp_scheduler(void);

