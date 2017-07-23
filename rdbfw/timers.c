/*
 * The Timers module:
 *
 * This module implement a serious of timers using the select's call timeout 
 * option for efficient time-keeping.
 *
 * each requested timers causes in invocation of a thread that loops around a 
 * single timer. 
 *
 * Timers are requested via the messaging system rdbmsg. available are
 * RDBMSG_ID_TIMER_START (data is frequency (1/HZ)) 
 * RDBMSG_ID_TIMER_STOP (data is which timer - index)
 * RDBMSG_ID_TIMER_ACK (data is which timer index will be used with 'last' request.
 * - since messages are always delivered in order, it is a valid usa case for a module to
 *   ask for several timers, and only then proccess the replies. which will corolate in order
 *   to the requests.
 * - ack messages are sent with the 'to' field filled with the 'from value used during the 
 *   TIMER_START request. modues requesting timers should listen to TIMER_ACK message with the "to"
 *   field populated with their own identefication to avoid picking up ack's intended for other modules.
 *
 * RDBMSG_ID_TIMER_TICK_0
 * RDBMSG_ID_TIMER_TICK_2
 * ...
 * RDBMSG_ID_TIMER_TICK_15
 *
 * timers may be private or public.
 * */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <dlfcn.h>
#include <errno.h>
#ifdef USE_PRCTL
#include <sys/prctl.h>
#endif

#include "rDB.h"
#include "messaging.h"
#include "rdbfw.h"
#include "log.h"

#define MAX_TIMERS 16

typedef struct t_info_s {
    uint32_t hz;
    int id;
    int counter;
} t_info_t;

static t_info_t t_info[MAX_TIMERS];

static pthread_t timers_main_thread;
static pthread_t timers[MAX_TIMERS];
static pthread_attr_t attr;
    
static plugins_t *ctx;
static int break_requested = 0;

static void *timer_thread(void *p);


static int find_free_timer() {
    int i;

    for (i = 0; i < MAX_TIMERS; i++) {
        if (t_info[i].hz == 0) {
            return i;
        }
    }
    return -1;
}

static void timers_stop_all(void){
    int i;

    for (i = 0; i < MAX_TIMERS; i++) {
        if (t_info[i].hz != 0) {
            log (LOG_INFO, "Stoping timer thread %d\n", i);
            pthread_cancel(timers[i]);
            pthread_join(timers[i], NULL);
            memset(&t_info[i],0, sizeof (t_info[i]));
            memset(&timers[i],0, sizeof (timers[i]));
        }
    }
    log (LOG_DEBUG_MORE, "Done\n");
}

static void *timers_main(void *p) {
    rdbmsg_queue_t *q;

    int timer_id;
    static int first_entry = 1;

#ifdef USE_PRCTL
    prctl(PR_SET_NAME,"timers_main\0",NULL,NULL,NULL);
#endif

    pthread_mutex_unlock(&ctx->startup_mutex);

    pthread_mutex_lock(&ctx->msg_mutex);
    while (break_requested == 0) {
        if (first_entry) {
            // first time we want to skip the condition as there may already be something in the Q, 
            // for which we ay have already missed the wakeup signal
            first_entry = 0;
        } else {
            pthread_cond_wait(&ctx->msg_condition, &ctx->msg_mutex);
#ifdef WAKEUP_ACCOUNTING
            ctx->wakeup_count++;
#endif
        }
        
        // in case we woke up to a stop single, we want to emit no more. break out now.
        if (break_requested) break;

        pthread_mutex_unlock(&ctx->msg_mutex);
        do {
            log (LOG_DEBUG_MORE, "%s: Timed Work\n", ctx->name);                  // do some work...

            rdb_lock(ctx->msg_q_pool,__FUNCTION__);
            q = rdb_delete(ctx->msg_q_pool, 0, NULL);
            rdb_unlock(ctx->msg_q_pool, __FUNCTION__);

            if (q) {    // process message
                log (LOG_DEBUG_MORE, "%s: Received message grp.id = %d.%d\n",
                        ctx->name, q->msg.group, q->msg.id);
                if (q->msg.id == RDBMSG_ID_TIMER_STOP) {
                    timer_id = q->msg.len - RDBMSG_ID_TIMER_TICK_0;
                    pthread_cancel(timers[timer_id]);
                    pthread_join(timers[timer_id], NULL);
                    memset(&t_info[timer_id],0, sizeof (t_info[timer_id]));
                    memset(&timers[timer_id],0, sizeof (timers[timer_id]));
                }
                if (q->msg.id == RDBMSG_ID_TIMER_START) {
                    timer_id = find_free_timer();
                    // we reply to the request before starting the timer, so receiver has a change to set up listener before ticks accomulate
                    rdbmsg_emit_simple(RDBMSG_ROUTE_MDL_TIMERS, 
                            q->msg.from,    // replying directly to the requester
                            RDBMSG_GROUP_TIMERS,
                            RDBMSG_ID_TIMER_ACK,
                            timer_id);
                    if (timer_id != -1) {   // we got a timer
                        int rc;
                        int cnt = 0;
                        t_info[timer_id].hz = q->msg.len;
                        t_info[timer_id].id = timer_id + RDBMSG_ID_TIMER_TICK_0;
                        while (1) {
                            rc = pthread_create( &timers[timer_id], &attr, timer_thread, (void *)&t_info[timer_id]);
                            if (rc == 0) {
                                break;
                            }
                            if (rc == EAGAIN) {
                                if (cnt > MAX_THREAD_RETRY) {
                                    log (LOG_ERROR, "Timer thread creation failed, MAX_THREAD_RETRY exusted\n");
                                    ctx->state = RDBFW_STATE_STOPALL;
                                } 
                                else {
                                    cnt++;
                                    log (LOG_ERROR, "Timer tread creation failed, will retry\n");
                                    usleep (10000);
                                    continue;
                                }
                            }
                            else if (rc == EPERM) {
                                log (LOG_ERROR, "Timer thread creation failed - missing permissions - aborting\n");
                                ctx->state = RDBFW_STATE_STOPALL;
                            }
                            else if (rc == EINVAL) {
                                log (LOG_ERROR, "Timer thread creation failed - Invalid attribute - aborting\n");
                                ctx->state = RDBFW_STATE_STOPALL;
                            }
                        }
                    }                    
                }
                rdbmsg_free(ctx, q);
            } else {
                break;
            }
        } while (1);
        pthread_mutex_lock(&ctx->msg_mutex);

    }
    pthread_mutex_unlock(&ctx->msg_mutex);

    pthread_exit(NULL);
}

static void timers_init(void *p) {
    ctx = p;

//    pthread_mutex_init(&ctx->msg_mutex, NULL);
//    pthread_cond_init(&ctx->msg_condition, NULL);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    ctx->state = RDBFW_STATE_INITILIZED;

    if (0 != rdbmsg_request(p, RDBMSG_ROUTE_NA, RDBMSG_ROUTE_MDL_TIMERS, RDBMSG_GROUP_NA, RDBMSG_ID_TIMER_START)) {
        log (LOG_ERROR, "rdbmsg_request failed. events may not fire. Aborting (%d.%d.%d.%d)",
                RDBMSG_ROUTE_NA, RDBMSG_ROUTE_MDL_TIMERS, RDBMSG_GROUP_NA, RDBMSG_ID_TIMER_START);
        ctx->state = RDBFW_STATE_STOPALL;
        return;
    }
    if (0 != rdbmsg_request(p, RDBMSG_ROUTE_NA, RDBMSG_ROUTE_MDL_TIMERS, RDBMSG_GROUP_NA, RDBMSG_ID_TIMER_STOP)) {
        log (LOG_ERROR, "rdbmsg_request failed. events may not fire. Aborting (%d.%d.%d.%d)",
                RDBMSG_ROUTE_NA, RDBMSG_ROUTE_MDL_TIMERS, RDBMSG_GROUP_NA, RDBMSG_ID_TIMER_STOP);
        ctx->state = RDBFW_STATE_STOPALL;
        return;
    }

}

static void timers_de_init(void *p) {
    ctx = p;
    
    ctx->state = RDBFW_STATE_LOADED;
    


}

static void timers_stop(void *P) {
    break_requested = 1;
    
    // even though we set break_requested to one we also need to
    // make sure it's awake after that moment, to it can be processed.
    // the join will ensure we dont quit until out internal threads did.
    pthread_mutex_lock(&ctx->msg_mutex);
    pthread_cond_signal(&ctx->msg_condition);
    pthread_mutex_unlock(&ctx->msg_mutex);
    pthread_join(timers_main_thread, NULL);
    
    // now that we surely stopped the timer main thread, safe to kill
    // all children with no worry of a re-spawn
    timers_stop_all();
    ctx->state = RDBFW_STATE_STOPPED;
}

static void timers_start(void *P) {
    int rc;
    int cnt = 0;

    pthread_mutex_lock(&ctx->startup_mutex);

    memset(t_info,0,sizeof(t_info));

    while (1) {
        rc = pthread_create( &timers_main_thread, &attr, timers_main, NULL);
        if (rc == 0) {
            break;
        }
        if (rc == EAGAIN) {
            if (cnt > MAX_THREAD_RETRY) {
                log (LOG_ERROR, "Thread creation failed, MAX_THREAD_RETRY exusted\n");
                ctx->state = RDBFW_STATE_STOPALL;
                return;
            } 
            else {
                cnt++;
                log (LOG_ERROR, "Thread creation failed, will retry\n");
                usleep (100000);
                continue;
            }
        }
        else if (rc == EPERM) {
            log (LOG_ERROR, "Thread creation failed - missing permissions - aborting\n");
            ctx->state = RDBFW_STATE_STOPALL;
            return;
        }
        else if (rc == EINVAL) {
            log (LOG_ERROR, "Thread creation failed - Invalid attribute - aborting\n");
            ctx->state = RDBFW_STATE_STOPALL;
            return;
        }
    }

    pthread_mutex_lock(&ctx->startup_mutex);
    ctx->state = RDBFW_STATE_RUNNING;
    pthread_mutex_unlock(&ctx->startup_mutex);
}

static void *timer_thread(void *p)
{
    t_info_t *t_info;
    t_info = p;
    log (LOG_INFO, "Starting timer thread @ %dHz\n",t_info->hz);
    
    struct timeval tv;
    int rc;
    while (!break_requested) {
        if (t_info->hz == 1) {
            tv.tv_sec=1;
            tv.tv_usec= 0;
        } else {
            tv.tv_sec=0;
            tv.tv_usec = 1000000/t_info->hz;
        }
        // portable, simple way to wait for timer. 
        rc=select(0, NULL, NULL, NULL, &tv);
        if (rc != 0) {
            log (LOG_DEBUG, "rc = %d during select. Sperious timer may occured\n", rc);
        }
        // we do NOT want to emit if we are canceled 
        pthread_testcancel();
        log(LOG_TRACE, "CORE_EMIT\n");
        rdbmsg_emit_simple(RDBMSG_ROUTE_NA, 
                RDBMSG_ROUTE_NA,
                RDBMSG_GROUP_TIMERS,
                t_info->id,
                t_info->counter++);

        if (t_info->counter>=1000) {
            t_info->counter=0;
        }
    }

    pthread_exit(NULL); 
}

const rdbfw_plugin_api_t timers_rdbfw_fns = {
    .init = timers_init,
    .de_init = timers_de_init,
    .start = timers_start,
    .stop = timers_stop,
};