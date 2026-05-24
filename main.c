#include "simulation.h"
#include "file_io.h"

/* ─── Default configuration ───────────────────────────────────────────────── */
static void set_default_config(SimConfig *cfg)
{
    cfg->num_doctors             = 3;
    cfg->sim_duration            = DEFAULT_SIM_MIN;
    cfg->avg_arrival[CRITICAL]   = 15.0;
    cfg->avg_arrival[URGENT]     = 8.0;
    cfg->avg_arrival[NON_URGENT] = 5.0;
    cfg->avg_service[CRITICAL]   = 45.0;
    cfg->avg_service[URGENT]     = 25.0;
    cfg->avg_service[NON_URGENT] = 15.0;
}

/* ─── Interactive configuration ───────────────────────────────────────────── */
static void configure_simulation(SimConfig *cfg)
{
    int    n;
    double d;

    printf("\n--- Configure Simulation Parameters ---\n");

    printf("  Number of doctors (1-%d) [current: %d]: ",
           MAX_DOCTORS, cfg->num_doctors);
    if (scanf("%d", &n) == 1 && n >= 1 && n <= MAX_DOCTORS)
        cfg->num_doctors = n;

    printf("  Simulation duration in minutes [current: %.0f]: ", cfg->sim_duration);
    if (scanf("%lf", &d) == 1 && d > 0) cfg->sim_duration = d;

    printf("  Avg minutes between CRITICAL   arrivals [%.0f]: ",
           cfg->avg_arrival[CRITICAL]);
    if (scanf("%lf", &d) == 1 && d > 0) cfg->avg_arrival[CRITICAL] = d;

    printf("  Avg minutes between URGENT     arrivals [%.0f]: ",
           cfg->avg_arrival[URGENT]);
    if (scanf("%lf", &d) == 1 && d > 0) cfg->avg_arrival[URGENT] = d;

    printf("  Avg minutes between NON-URGENT arrivals [%.0f]: ",
           cfg->avg_arrival[NON_URGENT]);
    if (scanf("%lf", &d) == 1 && d > 0) cfg->avg_arrival[NON_URGENT] = d;

    printf("  Avg CRITICAL   treatment time (min) [%.0f]: ", cfg->avg_service[CRITICAL]);
    if (scanf("%lf", &d) == 1 && d > 0) cfg->avg_service[CRITICAL] = d;

    printf("  Avg URGENT     treatment time (min) [%.0f]: ", cfg->avg_service[URGENT]);
    if (scanf("%lf", &d) == 1 && d > 0) cfg->avg_service[URGENT] = d;

    printf("  Avg NON-URGENT treatment time (min) [%.0f]: ", cfg->avg_service[NON_URGENT]);
    if (scanf("%lf", &d) == 1 && d > 0) cfg->avg_service[NON_URGENT] = d;

    printf("Configuration updated successfully.\n");
}

/* ─── Run simulation ──────────────────────────────────────────────────────── */
static void run_simulation(SimConfig *cfg, SimStats **p_stats, Doctor **p_docs)
{
    srand((unsigned int)time(NULL));

    Event   *fel_head                = NULL;
    Patient *q_front[NUM_SEVERITIES] = {NULL, NULL, NULL};
    Patient *q_rear[NUM_SEVERITIES]  = {NULL, NULL, NULL};
    Doctor  *docs   = create_doctors(cfg->num_doctors);
    SimStats *stats = create_stats(cfg->sim_duration, cfg->num_doctors);
    int next_id = 1;

    FILE *log_fp = fopen(LOG_FILE, "w");
    if (!log_fp)
        printf("Warning: could not create log file. Continuing without it.\n");

    printf("\n+============================================+\n");
    printf("|    EMERGENCY ROOM SIMULATION - RUNNING     |\n");
    printf("+============================================+\n\n");
    if (log_fp) fprintf(log_fp, "=== ER Simulation Started ===\n\n");

    /* Seed first arrival event for each severity level */
    for (int s = 0; s < NUM_SEVERITIES; s++) {
        double first = RAND_EXP(cfg->avg_arrival[s]);
        if (first <= cfg->sim_duration)
            insert_event(&fel_head, first, PATIENT_ARRIVAL, -1, -1, (Severity)s);
    }

    /* Main DES loop: jump from event to event (no real-time waiting) */
    while (fel_head && fel_head->time <= cfg->sim_duration) {
        Event *e = pop_event(&fel_head);
        /* Route output: to log file when available, otherwise to stdout */
        FILE *out = log_fp ? log_fp : stdout;

        if (e->type == PATIENT_ARRIVAL)
            handle_arrival(e, docs, cfg->num_doctors,
                           q_front, q_rear, &fel_head,
                           stats, cfg, out, &next_id);
        else
            handle_completion(e, docs, q_front, q_rear,
                              &fel_head, stats, cfg, out);
        free(e);
    }

    /* Cleanup: free remaining FEL events and queued patients */
    free_fel(&fel_head);
    for (int s = 0; s < NUM_SEVERITIES; s++)
        while (q_front[s]) {
            Patient *p = dequeue(&q_front[s], &q_rear[s]);
            free_patient(p);
        }

    if (log_fp) {
        fclose(log_fp);
        printf("\nLog saved to '%s'\n", LOG_FILE);
    }

    printf("+============================================+\n");
    printf("|    SIMULATION COMPLETE                     |\n");
    printf("+============================================+\n");

    *p_stats = stats;
    *p_docs  = docs;
}

/* ─── Menu ────────────────────────────────────────────────────────────────── */
static void print_menu(void)
{
    printf("\n+==========================================+\n");
    printf("|    EMERGENCY ROOM SIMULATION  v1.0       |\n");
    printf("+==========================================+\n");
    printf("|  1. Configure simulation parameters      |\n");
    printf("|  2. Load configuration from file         |\n");
    printf("|  3. Run simulation                       |\n");
    printf("|  4. View results report                  |\n");
    printf("|  5. Sort & display doctor utilization    |\n");
    printf("|  6. Save all results to files            |\n");
    printf("|  7. Exit                                 |\n");
    printf("+==========================================+\n");
    printf("Choice: ");
}

/* ─── Main ────────────────────────────────────────────────────────────────── */
int main(void)
{
    SimConfig cfg;
    set_default_config(&cfg);

    SimStats *stats = NULL;
    Doctor   *docs  = NULL;
    int choice;

    printf("\nWelcome to the Emergency Room Discrete-Event Simulation!\n");
    printf("Default: %d doctors, %.0f-minute simulation.\n",
           cfg.num_doctors, cfg.sim_duration);

    do {
        print_menu();

        if (scanf("%d", &choice) != 1) {
            int c;
            while ((c = getchar()) != '\n' && c != EOF);
            choice = 0;
        }

        switch (choice) {

            case 1:
                configure_simulation(&cfg);
                break;

            case 2:
                load_config(&cfg, CONFIG_FILE);
                break;

            case 3:
                /* Free previous results before starting a new run */
                if (stats) { free_stats(stats);                  stats = NULL; }
                if (docs)  { free_doctors(docs, cfg.num_doctors); docs  = NULL; }
                run_simulation(&cfg, &stats, &docs);
                break;

            case 4:
                if (stats && docs)
                    print_report(stats, &cfg, docs);
                else
                    printf("No results yet. Run the simulation first (option 3).\n");
                break;

            case 5:
                if (docs) {
                    sort_doctors_by_utilization(docs, cfg.num_doctors);
                    printf("\nDoctors ranked by utilization (highest first):\n");
                    for (int i = 0; i < cfg.num_doctors; i++) {
                        double u = cfg.sim_duration > 0
                                   ? docs[i].total_busy_time
                                     / cfg.sim_duration * 100.0
                                   : 0.0;
                        printf("  %d. %-12s  patients: %3d  utilization: %.1f%%\n",
                               i + 1, docs[i].name,
                               docs[i].patients_served, u);
                    }
                } else {
                    printf("No results yet. Run the simulation first (option 3).\n");
                }
                break;

            case 6:
                if (stats && docs) {
                    save_config(&cfg, CONFIG_FILE);
                    save_report(stats, &cfg, docs, REPORT_FILE);
                    printf("All files saved: %s, %s, %s\n",
                           CONFIG_FILE, REPORT_FILE, LOG_FILE);
                } else {
                    printf("No results to save. Run the simulation first (option 3).\n");
                }
                break;

            case 7:
                printf("\nGoodbye!\n");
                break;

            default:
                printf("Invalid choice. Please enter a number between 1 and 7.\n");
        }

    } while (choice != 7);

    /* Final cleanup */
    if (stats) free_stats(stats);
    if (docs)  free_doctors(docs, cfg.num_doctors);

    return 0;
}
