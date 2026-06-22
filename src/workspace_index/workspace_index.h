#ifndef CBM_WORKSPACE_INDEX_H
#define CBM_WORKSPACE_INDEX_H

#include "watcher/watcher.h"
#include <stdbool.h>

typedef struct cbm_config cbm_config_t;

typedef struct {
    const char *roots; /* comma, semicolon, or newline separated */
    int limit;
    bool apply;
    bool json;
    bool nonblocking_lock;
    cbm_watcher_t *watcher; /* optional, not owned */
} cbm_workspace_index_options_t;

int cbm_cmd_workspace_index(int argc, char **argv);
char *cbm_workspace_index_run(const cbm_workspace_index_options_t *opts);
void cbm_workspace_auto_index_from_config(cbm_config_t *cfg, cbm_watcher_t *watcher);

#endif /* CBM_WORKSPACE_INDEX_H */
