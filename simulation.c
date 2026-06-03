#include "simulation.h"

/* ─── Name / Diagnosis tables ─────────────────────────────────────────────── */

static const char *PATIENT_NAMES[] = {
    "James", "Emma", "Oliver", "Sophia", "Noah", "Ava", "Liam", "Isabella",
    "William", "Mia", "Benjamin", "Charlotte", "Elijah", "Amelia", "Lucas",
    "Harper", "Henry", "Evelyn", "Alexander", "Abigail"
};
#define NUM_PNAMES  ((int)(sizeof(PATIENT_NAMES) / sizeof(PATIENT_NAMES[0])))

static const char *DOCTOR_NAMES[] = {
    "Dr. Cohen", "Dr. Levi", "Dr. Brown", "Dr. Chen", "Dr. Davis",
    "Dr. Evans", "Dr. Fisher", "Dr. Green", "Dr. Hall", "Dr. Irving"
};

static const char *DIAGNOSES[NUM_SEVERITIES][5] = {
    /* CRITICAL   */ {"Cardiac Arrest",  "Stroke",        "Resp. Failure",  "Severe Trauma",    "Internal Bleeding"},
    /* URGENT     */ {"Appendicitis",    "Broken Bone",   "Deep Laceration","Severe Infection",  "Kidney Stone"},
    /* NON_URGENT */ {"Sprained Ankle",  "High Fever",    "Headache",       "Mild Contusion",    "Stomach Pain"}
};
#define NUM_DIAG 5

/* ─── FEL operations ──────────────────────────────────────────────────────── */

/* Insert event into the FEL in ascending time order (sorted linked list) */
void insert_event(Event **head, double time, EventType type,
                  int patient_id, int doctor_id, Severity sev)
{
    Event *e = (Event *)malloc(sizeof(Event));
    if (!e) { fputs("Error: malloc failed\n", stderr); exit(1); }

    e->time       = time;
    e->type       = type;
    e->patient_id = patient_id;
    e->doctor_id  = doctor_id;
    e->severity   = sev;    
    e->next       = NULL;

    /* Insert before first node with a larger time */
    if (!*head || (*head)->time > time) {
        e->next = *head;
        *head = e;
        return;
    }
    Event *curr = *head;
    while (curr->next && curr->next->time <= time)
        curr = curr->next;
    e->next    = curr->next;
    curr->next = e;
}

/* Remove and return the event with the smallest time (always at head) */
Event *pop_event(Event **head)
{
    if (!*head) return NULL;
    Event *e = *head;
    *head    = e->next;
    e->next  = NULL;
    return e;
}

/* Free all remaining events in the FEL */
void free_fel(Event **head)
{
    while (*head) {
        Event *tmp = *head;
        *head = (*head)->next;
        free(tmp);
    }
}

/* ─── Waiting queue operations (FIFO linked list) ─────────────────────────── */

/* Add patient to the rear of a FIFO waiting queue */
void enqueue(Patient **front, Patient **rear, Patient *p)
{
    p->next = NULL;
    if (!*rear) {
        *front = *rear = p;
        return;
    }
    (*rear)->next = p;
    *rear = p;
}

/* Remove and return the patient at the front of the waiting queue */
Patient *dequeue(Patient **front, Patient **rear)
{
    if (!*front) return NULL;
    Patient *p = *front;
    *front = p->next;
    if (!*front) *rear = NULL;
    p->next = NULL;
    return p;
}

/* ─── Patient helpers ─────────────────────────────────────────────────────── */

/* Allocate and initialise a new patient with random name and diagnosis */
Patient *create_patient(int id, Severity sev, double arrival_time)
{
    Patient *p = (Patient *)malloc(sizeof(Patient));
    if (!p) { fputs("Error: malloc failed\n", stderr); exit(1); }

    p->id             = id;
    p->severity       = sev;
    p->arrival_time   = arrival_time;
    p->service_start  = 0.0;
    p->departure_time = 0.0;
    p->doctor_id      = -1;
    p->next           = NULL;

    snprintf(p->name, MAX_NAME_LEN, "%s-%03d",
             PATIENT_NAMES[rand() % NUM_PNAMES], id);
    strncpy(p->diagnosis, DIAGNOSES[sev][rand() % NUM_DIAG], MAX_DIAG_LEN - 1);
    p->diagnosis[MAX_DIAG_LEN - 1] = '\0';

    return p;
}

/* Free a single patient node */
void free_patient(Patient *p) { free(p); }

/* ─── Doctor helpers ──────────────────────────────────────────────────────── */

/* Allocate an array of n doctors, all starting IDLE with zero stats */
Doctor *create_doctors(int n)
{
    Doctor *docs = (Doctor *)calloc(n, sizeof(Doctor));
    if (!docs) { fputs("Error: calloc failed\n", stderr); exit(1); }
    for (int i = 0; i < n; i++) {
        docs[i].id              = i;
        docs[i].status          = IDLE;
        docs[i].current_patient = NULL;
        docs[i].total_busy_time = 0.0;
        docs[i].patients_served = 0;
        strncpy(docs[i].name, DOCTOR_NAMES[i % 10], MAX_NAME_LEN - 1);
    }
    return docs;
}

/* Linear search: return index of first idle doctor, or -1 if all busy */
int find_idle_doctor(Doctor *docs, int n)
{
    for (int i = 0; i < n; i++)
        if (docs[i].status == IDLE) return i;
    return -1;
}

/* Assign patient to doctor and schedule a SERVICE_COMPLETION event */
void assign_patient_to_doctor(Doctor *doc, Patient *p, double t,
                               Event **fel_head, SimConfig *cfg)
{
    doc->status          = BUSY;
    doc->current_patient = p;
    p->service_start     = t;
    p->doctor_id         = doc->id;

    double duration = RAND_EXP(cfg->avg_service[p->severity]);
    if (duration < 1.0) duration = 1.0;   /* minimum 1 minute */
    insert_event(fel_head, t + duration, SERVICE_COMPLETION,
                 p->id, doc->id, p->severity);
}

/* Selection sort: rank doctors by descending total_busy_time */
void sort_doctors_by_utilization(Doctor *docs, int n)
{
    for (int i = 0; i < n - 1; i++) {
        int best = i;
        for (int j = i + 1; j < n; j++)
            if (docs[j].total_busy_time > docs[best].total_busy_time)
                best = j;
        if (best != i) {
            Doctor tmp  = docs[i];
            docs[i]     = docs[best];
            docs[best]  = tmp;
        }
    }
}

/* Free doctor array; also frees any patient still in treatment */
void free_doctors(Doctor *docs, int n)
{
    for (int i = 0; i < n; i++)
        if (docs[i].current_patient)
            free_patient(docs[i].current_patient);
    free(docs);
}

/* ─── Statistics ──────────────────────────────────────────────────────────── */

/* Allocate stats struct including the dynamic 2-D matrix hourly_data */
SimStats *create_stats(double sim_duration_min, int num_doctors)
{
    SimStats *s = (SimStats *)calloc(1, sizeof(SimStats));
    if (!s) { fputs("Error: calloc failed\n", stderr); exit(1); }

    s->num_hours   = (int)(sim_duration_min / 60.0) + 1;
    s->num_doctors = num_doctors;

    /* Dynamic matrix: [num_hours][HOURLY_COLS]
       col 0 = patients arrived, col 1 = patients served, col 2 = avg wait */
    s->hourly_data = (double **)malloc(s->num_hours * sizeof(double *));
    if (!s->hourly_data) { fputs("Error: malloc failed\n", stderr); exit(1); }
    for (int i = 0; i < s->num_hours; i++) {
        s->hourly_data[i] = (double *)calloc(HOURLY_COLS, sizeof(double));
        if (!s->hourly_data[i]) { fputs("Error: calloc failed\n", stderr); exit(1); }
    }
    return s;
}

/* Record a completed service: update stats and the hourly matrix */
void record_service_complete(SimStats *stats, Patient *p, double current_time)
{
    double wait = p->service_start - p->arrival_time;

    stats->total_served++;
    stats->total_wait[p->severity]      += wait;
    stats->served_per_sev[p->severity]++;
    if (wait > stats->max_wait) stats->max_wait = wait;
    p->departure_time = current_time;

    int hour = (int)(current_time / 60.0);
    if (hour < stats->num_hours) {
        double n = ++stats->hourly_data[hour][1];   /* patients served this hour */
        /* Running average of wait time for this hour */
        stats->hourly_data[hour][2] =
            ((n - 1) * stats->hourly_data[hour][2] + wait) / n;
    }
}

/* Update the max queue length observed for each severity */
void update_queue_max(SimStats *stats, Patient *q_front[])
{
    for (int s = 0; s < NUM_SEVERITIES; s++) {
        int len = 0;
        for (Patient *p = q_front[s]; p; p = p->next) len++;
        if (len > stats->max_queue_len[s]) stats->max_queue_len[s] = len;
    }
}

/* Free the dynamic matrix and the stats struct itself */
void free_stats(SimStats *stats)
{
    if (stats->hourly_data) {
        for (int i = 0; i < stats->num_hours; i++)
            free(stats->hourly_data[i]);
        free(stats->hourly_data);
    }
    free(stats);
}

/* ─── Event handlers ──────────────────────────────────────────────────────── */

/* Event handler: patient arrives — assign to idle doctor or enqueue by severity */
void handle_arrival(Event *e, Doctor *docs, int num_docs,
                    Patient *q_front[], Patient *q_rear[],
                    Event **fel_head, SimStats *stats,
                    SimConfig *cfg, FILE *log_fp, int *next_id)
{
    Severity sev = e->severity;
    double   t   = e->time;

    Patient *p = create_patient((*next_id)++, sev, t);

    /* Track arrivals in the hourly matrix */
    int hour = (int)(t / 60.0);
    if (hour < stats->num_hours)
        stats->hourly_data[hour][0]++;

    int doc_idx = find_idle_doctor(docs, num_docs);

    if (doc_idx >= 0) {
        assign_patient_to_doctor(&docs[doc_idx], p, t, fel_head, cfg);
        fprintf(log_fp,  "T=%7.2f | %-16s [%s] arrives -> assigned to %-12s\n",
                t, p->name, SEV_NAME(sev), docs[doc_idx].name);
        printf(       "T=%7.2f | %-16s [%s] arrives -> assigned to %-12s\n",
                t, p->name, SEV_NAME(sev), docs[doc_idx].name);
    } else {
        enqueue(&q_front[sev], &q_rear[sev], p);
        int total_q = 0;
        for (int s = 0; s < NUM_SEVERITIES; s++)
            for (Patient *cur = q_front[s]; cur; cur = cur->next) total_q++;
        fprintf(log_fp,  "T=%7.2f | %-16s [%s] arrives -> queued (total waiting: %d)\n",
                t, p->name, SEV_NAME(sev), total_q);
        printf(       "T=%7.2f | %-16s [%s] arrives -> queued (total waiting: %d)\n",
                t, p->name, SEV_NAME(sev), total_q);
    }
    update_queue_max(stats, q_front);

    /* Schedule next arrival for the same severity */
    double next_t = t + RAND_EXP(cfg->avg_arrival[sev]);
    if (next_t <= cfg->sim_duration)
        insert_event(fel_head, next_t, PATIENT_ARRIVAL, -1, -1, sev);
}

/* Event handler: treatment ends — free doctor, pull next patient by priority */
void handle_completion(Event *e, Doctor *docs,
                       Patient *q_front[], Patient *q_rear[],
                       Event **fel_head, SimStats *stats,
                       SimConfig *cfg, FILE *log_fp)
{
    Doctor  *doc = &docs[e->doctor_id];
    Patient *p   = doc->current_patient;
    double   t   = e->time;

    /* Finalize patient stats and doctor busy time */
    doc->total_busy_time += t - p->service_start;
    doc->patients_served++;
    record_service_complete(stats, p, t);

    fprintf(log_fp, "T=%7.2f | %-12s finishes  %-16s [%s]  wait=%.1f min\n",
            t, doc->name, p->name, SEV_NAME(p->severity),
            p->service_start - p->arrival_time);
    printf(      "T=%7.2f | %-12s finishes  %-16s [%s]  wait=%.1f min\n",
            t, doc->name, p->name, SEV_NAME(p->severity),
            p->service_start - p->arrival_time);

    free_patient(p);
    doc->current_patient = NULL;

    /* Pick next patient: CRITICAL > URGENT > NON_URGENT */
    Patient *next_p = NULL;
    for (int s = CRITICAL; s <= NON_URGENT && !next_p; s++)
        next_p = dequeue(&q_front[s], &q_rear[s]);

    if (next_p) {
        assign_patient_to_doctor(doc, next_p, t, fel_head, cfg);
        fprintf(log_fp, "         | %-12s now treating %-16s [%s]  waited %.1f min\n",
                doc->name, next_p->name, SEV_NAME(next_p->severity),
                next_p->service_start - next_p->arrival_time);
        printf(      "         | %-12s now treating %-16s [%s]  waited %.1f min\n",
                doc->name, next_p->name, SEV_NAME(next_p->severity),
                next_p->service_start - next_p->arrival_time);
    } else {
        doc->status = IDLE;
        fprintf(log_fp, "         | %-12s is now IDLE\n", doc->name);
        printf(      "         | %-12s is now IDLE\n", doc->name);
    }
}

/* ─── Console report ──────────────────────────────────────────────────────── */

/* Print the full results report to stdout: patient stats, hourly breakdown, doctor utilization */
void print_report(SimStats *stats, SimConfig *cfg, Doctor *docs)
{
    static const char *SEV[NUM_SEVERITIES] = {"CRITICAL  ", "URGENT    ", "NON-URGENT"};

    printf("\n+==========================================+\n");
    printf("|      EMERGENCY ROOM RESULTS REPORT       |\n");
    printf("+==========================================+\n");

    printf("\n--- Configuration ---\n");
    printf("  Duration : %.0f min (%.1f hours)\n",
           cfg->sim_duration, cfg->sim_duration / 60.0);
    printf("  Doctors  : %d\n", cfg->num_doctors);

    printf("\n--- Patient Statistics ---\n");
    printf("  Total served : %d\n", stats->total_served);
    for (int s = 0; s < NUM_SEVERITIES; s++) {
        double avg = stats->served_per_sev[s] > 0
                     ? stats->total_wait[s] / stats->served_per_sev[s] : 0.0;
        printf("  [%s]  served: %3d  avg wait: %5.1f min  max queue: %d\n",
               SEV[s], stats->served_per_sev[s], avg, stats->max_queue_len[s]);
    }
    printf("  Longest single wait : %.1f min\n", stats->max_wait);

    printf("\n--- Hourly Breakdown (from dynamic matrix) ---\n");
    printf("  %-4s  %-9s  %-9s  %-12s\n",
           "Hour", "Arrived", "Served", "Avg Wait");
    for (int h = 0; h < stats->num_hours; h++) {
        if (stats->hourly_data[h][0] > 0 || stats->hourly_data[h][1] > 0)
            printf("  %3dh  %-9.0f  %-9.0f  %.1f min\n",
                   h + 1,
                   stats->hourly_data[h][0],
                   stats->hourly_data[h][1],
                   stats->hourly_data[h][2]);
    }

    printf("\n--- Doctor Utilization ---\n");
    for (int i = 0; i < cfg->num_doctors; i++) {
        double util = cfg->sim_duration > 0
                      ? docs[i].total_busy_time / cfg->sim_duration * 100.0 : 0.0;
        printf("  %-12s  patients: %3d  busy: %6.1f min  utilization: %.1f%%\n",
               docs[i].name, docs[i].patients_served,
               docs[i].total_busy_time, util);
    }
    printf("+==========================================+\n\n");
}
