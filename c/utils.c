//Copyright (c) 2014-2020 Assaf Stoler <assaf.stoler@gmail.com>
//All rights reserved.
//see LICENSE for more info

#include <inttypes.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

// filloing 3 includes are here to satisy logging
#include <pthread.h>
#include <rdb/rdb.h>
#include "rdbfw.h"
#include "log.h"

#include "utils.h"

/* Calculate and redurn the time delta in nano-seconds, between two supplied
 * timespec structures (from, to).
 *
 * Result is returned as a 64 bit integer, as well as set into the
 * result (*res) pointer, if suppplied.
 *
 * on error (null input), zero is returned, and *res is unchanged
 */


#define DIVIDER_1  1000000000
#define SEC_TO_MSEC 1000
#define NSEC_TO_MSEC 1000000

//int64_t clock_gettime_ms(struct timespec *res) ;
void profiling (int64_t start, int threshold, const char *where) {
    struct timespec ts;
    int64_t loop_end_ms = clock_gettime_ms(&ts);
    if (loop_end_ms - start >= threshold) {
        fwl (LOG_WARN, NULL, "%"PRIi64"ms delay @ %s\n", loop_end_ms - start,  where );
    }

}

int64_t clock_gettime_ms(struct timespec *res) {
    struct timespec ts;
    if (res == NULL) {
        res = &ts;
    }
    if (-1 != clock_gettime(CLOCK_REALTIME, res)) {
        return ts_to_ms(res);
    } else {
        return -1;
    }
}
    
void clock_settime_ms(int64_t new_time) {
#ifdef __MACH__
    // TODO: Only used on server... iOS only runs as client. give better log message / handle
    // - this is since iOS won't allow to set the clock
    return;
#else
    struct timespec ts;
    ms_to_ts(new_time, &ts);
    //ts.tv_sec = new_time / SEC_TO_MSEC;
    //ts.tv_nsec = (new_time % SEC_TO_MSEC) * NSEC_TO_MSEC;
    if (-1 == clock_settime(CLOCK_REALTIME, &ts)) {
        fwl (LOG_ERROR, NULL, "Faild to set clock\n");
    }
    return;
#endif
}

int64_t ts_to_ms(struct timespec *ts) {
    if (!ts) {
        return 0;
    }
    
    if (ts->tv_sec < 0 || ts->tv_nsec < 0) {
        return (-1 * (((int64_t) labs(ts->tv_sec) * SEC_TO_MSEC) + (labs(ts->tv_nsec) / NSEC_TO_MSEC)));
    }
    else {
        return (((int64_t) ts->tv_sec * SEC_TO_MSEC) + (ts->tv_nsec / NSEC_TO_MSEC));
    }
}

//UnitTested
void ms_to_ts (int64_t ms, struct timespec *res) {
    if (res != NULL) {
        res->tv_sec = (ms / SEC_TO_MSEC) ;//* ((ms < 0) ? -1 : 1);
        res->tv_nsec = (ms % SEC_TO_MSEC) * NSEC_TO_MSEC;// * ((res->tv_sec == 0 && (ms < 0)) ? -1 : 1);
    }
}

void clock_settime_ns(int64_t new_time) {
#ifdef __MACH__
    // TODO: Only used on server... iOS only runs as client. give better log message / handle
    // - this is since iOS won't allow to set the clock
    return;
#else
    struct timespec ts;
    ts.tv_sec = new_time / DIVIDER_1;
    ts.tv_nsec = new_time % DIVIDER_1;
    if (-1 == clock_settime(CLOCK_REALTIME, &ts)) {
        fwl (LOG_ERROR, NULL, "Faild to ste clock\n");
    }
    return;
#endif
}

uint64_t clock_gettime_uns(void) {
    struct timespec ts;
    if (-1 != clock_gettime(CLOCK_MONOTONIC, &ts)) {
        return (uint64_t) (ts.tv_sec) * 1000000000 + ts.tv_nsec;
    } else {
        return UINT64_MAX;
    }
}
int64_t clock_gettime_us(void) {
    struct timespec ts;
    if (-1 != clock_gettime(CLOCK_MONOTONIC, &ts)) {
        return ((int64_t) (ts.tv_sec) * 1000000000 + ts.tv_nsec) / 1000;
    } else {
        return -1;
    }
}

/*** clock_prep_abstime prepare a timer loaded with experation date in the future
 *   for usage by appropriate functions / fimeouts
 *
 *   Able to handle all signess conbinations
 *
 *   Result is stored in to ts 
 **/
//unitTested
int clock_prep_abstime(struct timespec *ts, int64_t delta, int read_clock) {
    if (!read_clock || (-1 != clock_gettime(CLOCK_REALTIME, ts))) {
        int64_t ns;
        if (ts->tv_sec >=0 && ts->tv_nsec >=0) {
            ns = ts->tv_nsec + delta;
            while ( ns >= 1000000000 ) {
                ts->tv_sec ++;
                ns -= 1000000000;
            }
            while ( ns < 0 ) {
                ts->tv_sec --;
                ns += 1000000000;
            }
            ts->tv_nsec = ns;
        } else { //Negative
            ns = labs(ts->tv_nsec) - delta;
            //ts->tv_nsec = labs(ts->tv_nsec) - delta;
            ts->tv_sec = labs(ts->tv_sec);

            while ( ns >= 1000000000 ) {
                ts->tv_sec ++;
                ns -= 1000000000;
            }
            while ( ns < 0 ) {
                ts->tv_sec --;
                ns += 1000000000;
            }

            if (ts->tv_sec) {
                ts->tv_sec *= -1;
            } else {
                ns *= -1;
            }
            ts->tv_nsec = ns;
        }

        return 0;
    } else {
        return -1;
    }
}
int is_ts_greater(struct timespec *subject, struct timespec *challenger) {
    if (subject->tv_sec < challenger->tv_sec) {
        return 1;
    }
    if (subject->tv_sec == challenger->tv_sec && subject->tv_nsec < challenger->tv_nsec) {
        return 1;
    }
    else {
        return 0;
    }
}
// UnitTested
int is_ts_greater_equal(struct timespec *subject, struct timespec *challenger) {
    if (subject->tv_sec < challenger->tv_sec) {
        return 1;
    }
    if (subject->tv_sec == challenger->tv_sec && subject->tv_nsec <= challenger->tv_nsec) {
        return 1;
    }
    else {
        return 0;
    }
}
// UnitTested
int is_ts_lesser(struct timespec *subject, struct timespec *challenger) {
    if (subject->tv_sec > challenger->tv_sec) {
        return 1;
    }
    if (subject->tv_sec == challenger->tv_sec && subject->tv_nsec > challenger->tv_nsec) {
        return 1;
    }
    else {
        return 0;
    }
}
// UnitTested
int is_ts_lesser_equal(struct timespec *subject, struct timespec *challenger) {
    if (subject->tv_sec > challenger->tv_sec) {
        return 1;
    }
    if (subject->tv_sec == challenger->tv_sec && subject->tv_nsec >= challenger->tv_nsec) {
        return 1;
    }
    else {
        return 0;
    }
}

/*uint64_t diff_time_ns(struct timespec *from, struct timespec *to,
                   struct timespec *res)
{
    struct timespec tmp_time;

    if (res == NULL) {
        res = &tmp_time;
    }

    if (from == NULL || to == NULL) {
        return 0LL;
    }

    res->tv_sec = to->tv_sec - from->tv_sec;

    if (to->tv_nsec > from->tv_nsec) {
        res->tv_nsec = to->tv_nsec - from->tv_nsec ;
    } else {
        res->tv_nsec = to->tv_nsec - from->tv_nsec  + 1000000000;
        res->tv_sec --;
    }

    return (uint64_t) res->tv_sec *  1000000000LL + res->tv_nsec;
}*/

int64_t s_ts_diff_time_ns(struct timespec *from, struct timespec *to,
                   struct timespec *res)
{
    struct timespec tmp_time;
    struct timespec tmp_to;
    int from_sign;
    int to_sign;

    if (res == NULL) {
        res = &tmp_time;
    }

    if (to == NULL) {
        to = &tmp_to;
        if(-1 == clock_gettime(CLOCK_REALTIME, &tmp_to)) {
            return 0LL;
        }
    }

    if (from == NULL) {
        return 0LL;
    }

    if (from->tv_sec < 0 /*|| from->tv_nsec < 0*/) {
        from_sign = -1;
    }
    else {
        from_sign = 1;
    }
    if (to->tv_sec < 0/* || to->tv_nsec < 0*/) {
        to_sign = -1;
    }
    else {
        to_sign = 1;
    }

    if (to->tv_sec > from->tv_sec ||
            (to->tv_sec == from->tv_sec && to->tv_nsec >= from->tv_nsec)) {
        // positive return

        res->tv_sec = to->tv_sec - from->tv_sec;

        if (/*to_sign */ to->tv_nsec >= /*from_sign */ from->tv_nsec) {
            res->tv_nsec = (to_sign * to->tv_nsec) - (from_sign * from->tv_nsec) ;
            while (res->tv_nsec >= 1000000000 ) {
                res->tv_nsec -= 1000000000;
                res->tv_sec++;
            }
        } else {
            res->tv_nsec = (to_sign * to->tv_nsec) - (from_sign * from->tv_nsec)  + 1000000000;
            res->tv_sec --;
            while (res->tv_nsec >= 1000000000 ) {
                res->tv_nsec -= 1000000000;
                res->tv_sec++;
            }
        }
        return (int64_t) res->tv_sec *  1000000000LL + res->tv_nsec;
    } else {
        // Negative return
        res->tv_sec = from->tv_sec - to->tv_sec;

        if (from->tv_nsec >= to->tv_nsec) {
            res->tv_nsec = (from_sign * from->tv_nsec) - (to_sign * to->tv_nsec) ;
            if (res->tv_nsec >= 1000000000 ) {
                res->tv_nsec -= 1000000000;
                res->tv_sec++;
            }
        } else {
            res->tv_nsec = (from_sign * from->tv_nsec) - (to_sign * to->tv_nsec)  + 1000000000;
            res->tv_sec --;
            if (res->tv_nsec >= 1000000000 ) {
                res->tv_nsec -= 1000000000;
                res->tv_sec++;
            }
        }
        if (res->tv_sec) {
            res->tv_sec = res->tv_sec * -1;
        }
        else {
            res->tv_nsec = res->tv_nsec * -1;
        }
        return (int64_t) res->tv_sec *  1000000000LL + res->tv_nsec;
    }
}

/* print time to stdout
 *
 */
void print_now (FILE *fp)
{
    struct timespec tmp_time;
    if (-1 != clock_gettime(CLOCK_REALTIME, &tmp_time)) {
        fprintf(fp, "%'lu.%09lu: ",
            tmp_time.tv_sec,
            tmp_time.tv_nsec);
    }

}
void print_time (struct timespec *ts)
{
    fprintf(logger, "%'lu.%09lu\n",
            ts->tv_sec,
            ts->tv_nsec);
}
void print_time_ns (int64_t time) 
{
    fprintf(logger, "%'"PRIi64".%09"PRIi64"\n",
           time / 1000000000, 
           time % 1000000000);
}

// print at most len-1 characters of time into string
// TODO: Unittest
char *snprint_time_ns (int64_t time, char *string, int len) 
{
    int64_t abs_time;
    int used = 0;
    char *ptr;
    if (len < 2) {
        *string = 0;
        return NULL;
    }

    if (time < 0) {
        snprintf(string,2,"-");
        used = 1;
        abs_time = time * -1;
    } 
    else {
       *string = 0;
       used = 0;
        abs_time = time;
    }

    ptr = string + used;

    snprintf(ptr, len - used, "%'"PRIi64".%09"PRIi64"",
           abs_time / 1000000000, 
           abs_time % 1000000000);
    return string;
}

// print at most len-1 characters of time into string
// *
char *snprint_ts_time (struct timespec *ts, char *string, int len) 
{
    //int mult = 1;
    int used = 0;
    char *ptr;

    if (len < 2) {
        *string = 0;
        return NULL;
    }

    if (ts->tv_sec < 0 || (ts->tv_sec == 0 && ts->tv_nsec < 0)) {
        snprintf(string,2,"-");
        used = 1;
        //mult = -1;
    } 
    else {
       *string = 0;
       used = 0;
    }

    ptr = string + used;

    snprintf(ptr, len - used, "%'ld.%09ld",
           labs(ts->tv_sec) /** mult*/,
           labs(ts->tv_nsec) /** mult*/);
    return string;
}

int fd_set_flag(int fd, int flag, int command) {

    int flags;
  
    flags = fcntl(fd, F_GETFL, 0);

    if (flags == -1) {
        return -0;
    }

    if (FLAG_SET) {
        flags &= ~flag;
    }

    else {
        flags |= flag;
    }

    return fcntl(fd, F_SETFL, flags) != -1;
}

int rdbfw_pthread_create(pthread_t *thread,
                          const pthread_attr_t *attr,
                          void *(*start_routine)(void *),
                          int max_attempts,
                          int terminate_on_fail,
                          int retry_delay,
                          plugins_t *p) {
    int retry_ct=0;
    int rc;
    while (1) {
        rc = pthread_create( thread, attr, start_routine, p);
        if (rc == 0) {
            return 0;
        }
        if (rc == EAGAIN) {
            if (retry_ct > max_attempts) {
                fwl (LOG_ERROR,p, "Thread creation failed, max attempts (%d) exusted\n", max_attempts);
                if (terminate_on_fail) p->state = RDBFW_STATE_STOPALL;
                return -1;
            } 
            else {
                retry_ct++;
                fwl (LOG_ERROR, p, "Thread creation failed, will retry\n");
                usleep (retry_delay);
                continue;
            }
        }
        else if (rc == EPERM) {
            if (terminate_on_fail) {
                fwl (LOG_ERROR, p, "Thread creation failed (%s) - missing permissions - aborting\n", p->uname);
                p->state = RDBFW_STATE_STOPALL;
            } 
            else fwl (LOG_ERROR, p, "Thread creation failed (%s) - missing permissions\n", p->uname);
            return -1;
        }
        else if (rc == EINVAL) {
            if (terminate_on_fail) {
                fwl (LOG_ERROR, p, "Thread creation failed (%s) - Invalid attribute - aborting\n", p->uname);
                p->state = RDBFW_STATE_STOPALL;
            }
            else fwl (LOG_ERROR, p, "Thread creation failed (%s) - Invalid attribute\n", p->uname);
            return -1;
        }
    }
}
    
