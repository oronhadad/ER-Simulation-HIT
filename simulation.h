#ifndef SIMULATION_H
#define SIMULATION_H

#include "types.h"

/* ─── Future Event List (FEL) — sorted linked list ────────────────────────── */
void   insert_event(Event **head, double time, EventType type,
                    int patient_id, int doctor_id, Severity sev);
Event *pop_event(Event **head);
void   free_fel(Event **head);

/* ─── Waiting queues — one linked list per severity ───────────────────────── */
void     enqueue(Patient **front, Patient **rear, Patient *p);
Patient *dequeue(Patient **front, Patient **rear);

/* ─── Patient helpers ──────────────────────────────────────────────────────── */
Patient *create_patient(int id, Severity sev, double arrival_time);
void     free_patient(Patient *p);

/* ─── Doctor helpers ───────────────────────────────────────────────────────── */
Doctor *create_doctors(int n);
int     find_idle_doctor(Doctor *docs, int n);           /* linear search  */
void    assign_patient_to_doctor(Doctor *doc, Patient *p, double t,
                                 Event **fel_head, SimConfig *cfg);
void    sort_doctors_by_utilization(Doctor *docs, int n); /* selection sort */
void    free_doctors(Doctor *docs, int n);

/* ─── Statistics ───────────────────────────────────────────────────────────── */
SimStats *create_stats(double sim_duration_min, int num_doctors);
void      record_service_complete(SimStats *stats, Patient *p, double current_time);
void      update_queue_max(SimStats *stats, Patient *q_front[]);
void      free_stats(SimStats *stats);

/* ─── Event handlers ───────────────────────────────────────────────────────── */
void handle_arrival(Event *e, Doctor *docs, int num_docs,
                    Patient *q_front[], Patient *q_rear[],
                    Event **fel_head, SimStats *stats,
                    SimConfig *cfg, FILE *log_fp, int *next_id);

void handle_completion(Event *e, Doctor *docs,
                       Patient *q_front[], Patient *q_rear[],
                       Event **fel_head, SimStats *stats,
                       SimConfig *cfg, FILE *log_fp);

/* ─── Report (printed to console) ─────────────────────────────────────────── */
void print_report(SimStats *stats, SimConfig *cfg, Doctor *docs);

#endif /* SIMULATION_H */
