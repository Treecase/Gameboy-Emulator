/*
 * Alarms -- actions that must occur after n cycles/n times per second
 * NOTE: the CPU frequency is 4x realtime, so to get n times per second, multiply your frequency by four.
 */

#include "alarm.h"
#include "logging.h"

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>



static AlarmID  next_id     =    1;
static unsigned alarm_count =    0;
static Alarm   *alarms      = NULL;

static unsigned cpu_frequency;



/* free_alarms:  */
void free_alarms(void) {
    free (alarms);
    alarms      = NULL;
    alarm_count = 0;
    next_id     = 0;
}

/* init_alarms:  */
void init_alarms(unsigned f) {
    next_id     = 0;
    alarm_count = 0;
    alarms      = NULL;

    cpu_frequency = f;

    atexit (free_alarms);
}

/* mkalarm_freq: create an alarm that triggers n times per second */
AlarmID mkalarm_freq (double frequency, void (*fn)()) {

    long cycles = cpu_frequency / frequency;
    return mkalarm_cycle (cycles, fn);
}

/* mkalarm_cycle: create an alarm that triggers every n cycles */
AlarmID mkalarm_cycle (long cycles, void (*fn)()) {

    alarm_count++;
    alarms = realloc (alarms, alarm_count * sizeof(Alarm));
    memset (alarms + alarm_count-1, 0, sizeof(Alarm));

    Alarm *a = &alarms[alarm_count - 1];

    a->id          = next_id;
    a->cyclecount  = cycles;
    a->to_next_run = a->cyclecount;
    a->run = fn;

    next_id++;

    //debug ("created alarm %llu, triggers after %li cycles", a->id, a->cyclecount);

    return a->id;
}

/* rmalarm: remove an alarm */
void rmalarm (AlarmID id) {

    bool found = false;
    for (unsigned i = 0; !found && i < alarm_count; ++i)
        if (alarms[i].id == id) {
            //debug ("removing alarm %u", alarms[i].id);

            if (i < alarm_count - 1)
                memmove (alarms + i, alarms + i+1, sizeof(Alarm) * (alarm_count - (i+1)));

            found = true;
        }

    if (found) {
        alarm_count--;
        alarms = realloc (alarms, sizeof(Alarm) * alarm_count);
    }
    else
        error ("AlarmID %llu does not exist!", id);
}

/* update_alarms:  */
void update_alarms (unsigned num_cycles) {

    for (unsigned i = 0; i < alarm_count; ++i) {
        Alarm *a = &alarms[i];

        if (a->cyclecount >= 0) {
            a->to_next_run -= num_cycles;

            if (a->to_next_run <= 0) {
                a->to_next_run += a->cyclecount;
                if (a->run != NULL)
                    a->run();
            }
        }
    }
}

/* set_alarm_freq:  */
void set_alarm_freq (AlarmID id, double frequency) {

    for (unsigned i = 0; i < alarm_count; ++i)
        if (alarms[i].id == id) {
            alarms[i].cyclecount = cpu_frequency / frequency;
            alarms[i].to_next_run = alarms[i].cyclecount;
            break;
        }
}

/* set_alarm_cycles: set cyclecount, resetting to_next_run */
void set_alarm_cycles (AlarmID id, long cycles) {


    for (unsigned i = 0; i < alarm_count; ++i)
        if (alarms[i].id == id) {
            alarms[i].cyclecount = cycles;
            alarms[i].to_next_run = alarms[i].cyclecount;
            //debug ("alarm %llu has %li cycles until activation!", id, alarms[i].to_next_run);
            break;
        }
}

/* set_alarm_cycles_clean: set cyclecount, without resetting to_next_run */
void set_alarm_cycles_clean (AlarmID id, long cycles) {


    for (unsigned i = 0; i < alarm_count; ++i)
        if (alarms[i].id == id) {
            alarms[i].cyclecount = cycles;
            //debug ("alarm %llu has %li cycles until activation!", id, alarms[i].to_next_run);
            break;
        }
}

/* set_alarm_func:  */
void set_alarm_func (AlarmID id, void (*fn)()) {

    for (unsigned i = 0; i < alarm_count; ++i)
        if (alarms[i].id == id) {
            alarms[i].run = fn;
            break;
        }
}

/* get_alarm_remaining: return to_next_run */
long get_alarm_remaining (AlarmID id) {
    for (unsigned i = 0; i < alarm_count; ++i)
        if (alarms[i].id == id)
            return alarms[i].to_next_run;

    error ("Alarm %llu does not exist!", id);
    return -1;
}

