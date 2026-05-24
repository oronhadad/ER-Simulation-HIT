#ifndef TYPES_H
#define TYPES_H

/* suppress Visual Studio security warnings for scanf, sprintf, etc. */
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

/* ─── Constants & Preprocessor Directives ─────────────────────────────────── */
#define MAX_DOCTORS       10
#define MAX_NAME_LEN      50
#define MAX_DIAG_LEN     100
#define NUM_SEVERITIES     3
#define HOURLY_COLS        5
#define DEFAULT_SIM_MIN  480.0   /* 8-hour ER shift */

#define LOG_FILE     "er_log.txt"
#define CONFIG_FILE  "er_config.txt"
#define REPORT_FILE  "er_report.txt"

/* Exponential random variable — models real-world inter-arrival & service times */
#define RAND_EXP(avg)  (-(avg) * log((double)rand() / RAND_MAX + 1e-9))

/* Severity name lookup macro for log formatting */
#define SEV_NAME(s)  ((s) == CRITICAL ? "CRITICAL" : (s) == URGENT ? "URGENT  " : "NON-URG ")

/* ─── Enums ────────────────────────────────────────────────────────────────── */
typedef enum { CRITICAL = 0, URGENT = 1, NON_URGENT = 2 } Severity;
typedef enum { IDLE = 0, BUSY = 1 }                        DoctorStatus;
typedef enum { PATIENT_ARRIVAL = 0, SERVICE_COMPLETION = 1 } EventType;

/* ─── Structs ──────────────────────────────────────────────────────────────── */

/* Event — node in the Future Event List (FEL) linked list, sorted by time */
typedef struct Event {
    double    time;
    EventType type;
    int       patient_id;
    int       doctor_id;
    Severity  severity;   /* used by PATIENT_ARRIVAL to schedule next arrival */
    struct Event *next;
} Event;

/* Patient — node in per-severity waiting queues (linked list) */
typedef struct Patient {
    int      id;
    char     name[MAX_NAME_LEN];
    Severity severity;
    double   arrival_time;
    double   service_start;
    double   departure_time;
    int      doctor_id;
    char     diagnosis[MAX_DIAG_LEN];
    struct Patient *next;
} Patient;

/* Doctor — stored in a dynamically allocated array */
typedef struct {
    int          id;
    char         name[MAX_NAME_LEN];
    DoctorStatus status;
    Patient     *current_patient;
    double       total_busy_time;
    int          patients_served;
} Doctor;

/* SimConfig — holds all tunable parameters; saved to / loaded from file */
typedef struct {
    int    num_doctors;
    double sim_duration;                    /* total simulated time (minutes) */
    double avg_arrival[NUM_SEVERITIES];     /* avg minutes between arrivals   */
    double avg_service[NUM_SEVERITIES];     /* avg treatment duration         */
} SimConfig;

/* SimStats — runtime statistics, includes a dynamic 2-D matrix for hourly data */
typedef struct {
    double **hourly_data;               /* [num_hours][HOURLY_COLS] — malloc'd */
    int    num_hours;
    int    num_doctors;                 /* stored here so caller can free docs */
    int    total_served;
    double total_wait[NUM_SEVERITIES];
    int    served_per_sev[NUM_SEVERITIES];
    double max_wait;
    int    max_queue_len[NUM_SEVERITIES];
} SimStats;

#endif /* TYPES_H */
