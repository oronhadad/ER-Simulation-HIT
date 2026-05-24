#ifndef FILE_IO_H
#define FILE_IO_H

#include "types.h"

/* Save simulation configuration to a text file */
int save_config(SimConfig *cfg, const char *filename);

/* Load simulation configuration from a text file */
int load_config(SimConfig *cfg, const char *filename);

/* Save the full results report to a text file */
int save_report(SimStats *stats, SimConfig *cfg, Doctor *docs,
                const char *filename);

#endif /* FILE_IO_H */
