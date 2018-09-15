/*
 * alarms
 *
 */

#ifndef __ALARM_HPP
#define __ALARM_HPP


typedef unsigned long long AlarmID;

typedef struct Alarm {
    AlarmID id;

    long   to_next_run;
    long   cyclecount;
    void (*run)();
} Alarm;



void init_alarms (unsigned f);
void update_alarms (unsigned num_cycles);

AlarmID mkalarm_freq (double frequency, void (*fn)());
AlarmID mkalarm_cycle (long cycles, void (*fn)());

void rmalarm (AlarmID id);

void set_alarm_freq (AlarmID id, double frequency);
void set_alarm_cycles (AlarmID id, long cycles);
void set_alarm_cycles_clean (AlarmID id, long cycles);
void set_alarm_func (AlarmID id, void (*fn)());

long get_alarm_remaining (AlarmID id);


#endif

