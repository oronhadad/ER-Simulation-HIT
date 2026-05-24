#include "file_io.h"

/* ─── Save configuration ──────────────────────────────────────────────────── */

int save_config(SimConfig *cfg, const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: cannot open '%s' for writing.\n", filename);
        return 0;
    }
    fprintf(fp, "num_doctors            %d\n",   cfg->num_doctors);
    fprintf(fp, "sim_duration           %.2f\n", cfg->sim_duration);
    fprintf(fp, "avg_arrival_critical   %.2f\n", cfg->avg_arrival[CRITICAL]);
    fprintf(fp, "avg_arrival_urgent     %.2f\n", cfg->avg_arrival[URGENT]);
    fprintf(fp, "avg_arrival_nonurgent  %.2f\n", cfg->avg_arrival[NON_URGENT]);
    fprintf(fp, "avg_service_critical   %.2f\n", cfg->avg_service[CRITICAL]);
    fprintf(fp, "avg_service_urgent     %.2f\n", cfg->avg_service[URGENT]);
    fprintf(fp, "avg_service_nonurgent  %.2f\n", cfg->avg_service[NON_URGENT]);
    fclose(fp);
    printf("Configuration saved to '%s'\n", filename);
    return 1;
}

/* ─── Load configuration ──────────────────────────────────────────────────── */

int load_config(SimConfig *cfg, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("Error: cannot open '%s'. Use option 6 to save a config first.\n",
               filename);
        return 0;
    }
    char   key[64];
    double val;
    while (fscanf(fp, "%63s %lf", key, &val) == 2) {
        if      (strcmp(key, "num_doctors")           == 0) cfg->num_doctors             = (int)val;
        else if (strcmp(key, "sim_duration")          == 0) cfg->sim_duration            = val;
        else if (strcmp(key, "avg_arrival_critical")  == 0) cfg->avg_arrival[CRITICAL]   = val;
        else if (strcmp(key, "avg_arrival_urgent")    == 0) cfg->avg_arrival[URGENT]     = val;
        else if (strcmp(key, "avg_arrival_nonurgent") == 0) cfg->avg_arrival[NON_URGENT] = val;
        else if (strcmp(key, "avg_service_critical")  == 0) cfg->avg_service[CRITICAL]   = val;
        else if (strcmp(key, "avg_service_urgent")    == 0) cfg->avg_service[URGENT]     = val;
        else if (strcmp(key, "avg_service_nonurgent") == 0) cfg->avg_service[NON_URGENT] = val;
    }
    fclose(fp);
    printf("Configuration loaded from '%s'\n", filename);
    return 1;
}

/* ─── Save report ─────────────────────────────────────────────────────────── */

int save_report(SimStats *stats, SimConfig *cfg, Doctor *docs,
                const char *filename)
{
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("Error: cannot open '%s' for writing.\n", filename);
        return 0;
    }
    static const char *SEV[NUM_SEVERITIES] = {"CRITICAL  ", "URGENT    ", "NON-URGENT"};

    fprintf(fp, "+==========================================+\n");
    fprintf(fp, "|      EMERGENCY ROOM RESULTS REPORT       |\n");
    fprintf(fp, "+==========================================+\n\n");

    fprintf(fp, "Duration : %.0f min (%.1f hours)\n",
            cfg->sim_duration, cfg->sim_duration / 60.0);
    fprintf(fp, "Doctors  : %d\n\n", cfg->num_doctors);

    fprintf(fp, "Total served : %d\n", stats->total_served);
    for (int s = 0; s < NUM_SEVERITIES; s++) {
        double avg = stats->served_per_sev[s] > 0
                     ? stats->total_wait[s] / stats->served_per_sev[s] : 0.0;
        fprintf(fp, "[%s]  served: %3d  avg wait: %5.1f min  max queue: %d\n",
                SEV[s], stats->served_per_sev[s], avg, stats->max_queue_len[s]);
    }
    fprintf(fp, "Longest single wait : %.1f min\n\n", stats->max_wait);

    fprintf(fp, "Hourly Breakdown:\n");
    fprintf(fp, "  %-4s  %-9s  %-9s  %-12s\n",
            "Hour", "Arrived", "Served", "Avg Wait");
    for (int h = 0; h < stats->num_hours; h++) {
        if (stats->hourly_data[h][0] > 0 || stats->hourly_data[h][1] > 0)
            fprintf(fp, "  %3dh  %-9.0f  %-9.0f  %.1f min\n",
                    h + 1,
                    stats->hourly_data[h][0],
                    stats->hourly_data[h][1],
                    stats->hourly_data[h][2]);
    }

    fprintf(fp, "\nDoctor Utilization:\n");
    for (int i = 0; i < cfg->num_doctors; i++) {
        double util = cfg->sim_duration > 0
                      ? docs[i].total_busy_time / cfg->sim_duration * 100.0 : 0.0;
        fprintf(fp, "  %-12s  patients: %3d  busy: %6.1f min  util: %.1f%%\n",
                docs[i].name, docs[i].patients_served,
                docs[i].total_busy_time, util);
    }
    fprintf(fp, "+==========================================+\n");
    fclose(fp);
    printf("Report saved to '%s'\n", filename);
    return 1;
}
