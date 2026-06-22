#include "workspace_index/workspace_index.h"

#include "cli/cli.h"
#include "discover/discover.h"
#include "foundation/compat_fs.h"
#include "foundation/constants.h"
#include "foundation/log.h"
#include "foundation/mem.h"
#include "foundation/platform.h"
#include "foundation/str_util.h"
#include "pipeline/pipeline.h"
#include <yyjson/yyjson.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#endif

enum {
    WI_DEFAULT_LIMIT = 50000,
    WI_DEFAULT_STARTUP_BATCH_LIMIT = 1,
    WI_DEFAULT_MIN_AVAILABLE_MB = 4096,
    WI_DEFAULT_MAX_SWAP_USED_MB = 1024,
    WI_OWNER_LEASE_TTL_SECONDS = 300,
    WI_INITIAL_CAP = 16,
    WI_GROWTH = 2,
};

typedef struct {
    char path[CBM_PATH_MAX];
} wi_repo_t;

typedef struct {
    wi_repo_t *items;
    int count;
    int cap;
} wi_repo_list_t;

typedef struct {
    const char *path;
    const char *project;
    int tracked_files;
    int indexable_files;
    int memory_risk_score;
    long memory_available_mb;
    long swap_used_mb;
    const char *status;
    const char *reason;
    bool indexed;
} wi_repo_result_t;

typedef struct {
    char path[CBM_PATH_MAX];
    bool held;
} wi_process_lock_t;

typedef struct {
    bool available;
    long memory_available_mb;
    long swap_used_mb;
} wi_memory_sample_t;

static long wi_kb_to_mb(long kb) {
    if (kb <= 0) {
        return 0;
    }
    return kb / 1024;
}

#ifndef _WIN32
static bool wi_read_cgroup_long(const char *path, long *value) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return false;
    }
    char buf[CBM_SZ_128];
    if (!fgets(buf, sizeof(buf), fp)) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    if (strncmp(buf, "max", 3) == 0) {
        return false;
    }
    char *end = NULL;
    long v = strtol(buf, &end, CBM_DECIMAL_BASE);
    if (end == buf || v < 0) {
        return false;
    }
    *value = v;
    return true;
}
#endif

static bool wi_sample_memory(wi_memory_sample_t *sample) {
    if (!sample) {
        return false;
    }
    sample->available = false;
    sample->memory_available_mb = -1;
    sample->swap_used_mb = -1;
#ifdef _WIN32
    return false;
#else
    FILE *fp = fopen("/proc/meminfo", "r");
    long mem_available_kb = -1;
    long swap_total_kb = -1;
    long swap_free_kb = -1;
    if (fp) {
        char key[CBM_SZ_64];
        long value = 0;
        char unit[CBM_SZ_16];
        while (fscanf(fp, "%63[^:]: %ld %15s\n", key, &value, unit) == 3) {
            if (strcmp(key, "MemAvailable") == 0) {
                mem_available_kb = value;
            } else if (strcmp(key, "SwapTotal") == 0) {
                swap_total_kb = value;
            } else if (strcmp(key, "SwapFree") == 0) {
                swap_free_kb = value;
            }
        }
        fclose(fp);
    }
    if (mem_available_kb > 0) {
        sample->memory_available_mb = wi_kb_to_mb(mem_available_kb);
        sample->available = true;
    }
    if (swap_total_kb >= 0 && swap_free_kb >= 0) {
        sample->swap_used_mb = wi_kb_to_mb(swap_total_kb - swap_free_kb);
    }

    long cgroup_current = 0;
    long cgroup_max = 0;
    if (wi_read_cgroup_long("/sys/fs/cgroup/memory.current", &cgroup_current) &&
        wi_read_cgroup_long("/sys/fs/cgroup/memory.max", &cgroup_max) &&
        cgroup_max > cgroup_current) {
        long cgroup_available_mb = (cgroup_max - cgroup_current) / (1024 * 1024);
        if (sample->memory_available_mb < 0 || cgroup_available_mb < sample->memory_available_mb) {
            sample->memory_available_mb = cgroup_available_mb;
            sample->available = true;
        }
    }

    long cgroup_swap_current = 0;
    if (wi_read_cgroup_long("/sys/fs/cgroup/memory.swap.current", &cgroup_swap_current)) {
        long cgroup_swap_used_mb = cgroup_swap_current / (1024 * 1024);
        if (sample->swap_used_mb < 0 || cgroup_swap_used_mb > sample->swap_used_mb) {
            sample->swap_used_mb = cgroup_swap_used_mb;
        }
    }
    return sample->available;
#endif
}

static bool wi_memory_allows_index(const cbm_workspace_index_options_t *opts,
                                   wi_repo_result_t *r) {
    int min_available_mb = opts ? opts->min_available_mb : 0;
    int max_swap_used_mb = opts ? opts->max_swap_used_mb : 0;
    if (min_available_mb <= 0 && max_swap_used_mb <= 0) {
        return true;
    }

    wi_memory_sample_t sample = {0};
    if (!wi_sample_memory(&sample)) {
        r->memory_available_mb = -1;
        r->swap_used_mb = -1;
        r->status = "skipped_by_memory_gate";
        r->reason = "memory pressure could not be sampled";
        return false;
    }
    r->memory_available_mb = sample.memory_available_mb;
    r->swap_used_mb = sample.swap_used_mb;

    if (min_available_mb > 0 && sample.memory_available_mb >= 0 &&
        sample.memory_available_mb < min_available_mb) {
        r->status = "skipped_by_memory_gate";
        r->reason = "available memory below configured threshold";
        return false;
    }
    if (max_swap_used_mb > 0 && sample.swap_used_mb >= 0 &&
        sample.swap_used_mb > max_swap_used_mb) {
        r->status = "skipped_by_memory_gate";
        r->reason = "swap usage above configured threshold";
        return false;
    }
    return true;
}

static bool wi_try_process_lock(wi_process_lock_t *lock) {
    if (!lock) {
        return false;
    }
    lock->held = false;
    const char *cache_dir = cbm_resolve_cache_dir();
    if (!cache_dir) {
        return false;
    }
    snprintf(lock->path, sizeof(lock->path), "%s/_workspace_index.lock", cache_dir);
#ifdef _WIN32
    FILE *f = fopen(lock->path, "wx");
    if (!f) {
        return false;
    }
    fclose(f);
#else
    int fd = open(lock->path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0 && errno == EEXIST) {
        FILE *existing = fopen(lock->path, "r");
        long pid = 0;
        if (existing) {
            (void)fscanf(existing, "%ld", &pid);
            fclose(existing);
        }
        if (pid <= 0 || kill((pid_t)pid, 0) != 0) {
            (void)cbm_unlink(lock->path);
            fd = open(lock->path, O_WRONLY | O_CREAT | O_EXCL, 0600);
        }
    }
    if (fd < 0) {
        return false;
    }
    char pid_buf[CBM_SZ_64];
    int n = snprintf(pid_buf, sizeof(pid_buf), "%ld\n", (long)getpid());
    if (n > 0) {
        (void)write(fd, pid_buf, (size_t)n);
    }
    close(fd);
#endif
    lock->held = true;
    return true;
}

static void wi_release_process_lock(wi_process_lock_t *lock) {
    if (lock && lock->held) {
        (void)cbm_unlink(lock->path);
        lock->held = false;
    }
}

static bool wi_try_startup_owner_lease(void) {
    const char *cache_dir = cbm_resolve_cache_dir();
    if (!cache_dir) {
        return false;
    }
    char lease_path[CBM_PATH_MAX];
    snprintf(lease_path, sizeof(lease_path), "%s/_workspace_index_owner.lock", cache_dir);
#ifdef _WIN32
    FILE *f = fopen(lease_path, "wx");
    if (!f) {
        return false;
    }
    fclose(f);
    return true;
#else
    time_t now = time(NULL);
    int fd = open(lease_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0 && errno == EEXIST) {
        FILE *existing = fopen(lease_path, "r");
        long pid = 0;
        if (existing) {
            (void)fscanf(existing, "%ld", &pid);
            fclose(existing);
        }
        struct stat st;
        bool recent = stat(lease_path, &st) == 0 &&
                      now >= st.st_mtime &&
                      now - st.st_mtime < WI_OWNER_LEASE_TTL_SECONDS;
        bool live = pid > 0 && kill((pid_t)pid, 0) == 0;
        if (live || recent) {
            return false;
        }
        (void)cbm_unlink(lease_path);
        fd = open(lease_path, O_WRONLY | O_CREAT | O_EXCL, 0600);
    }
    if (fd < 0) {
        return false;
    }
    char pid_buf[CBM_SZ_64];
    int n = snprintf(pid_buf, sizeof(pid_buf), "%ld\n", (long)getpid());
    if (n > 0) {
        (void)write(fd, pid_buf, (size_t)n);
    }
    close(fd);
    return true;
#endif
}

static bool wi_dir_has_git(const char *path) {
    char git_path[CBM_PATH_MAX];
    snprintf(git_path, sizeof(git_path), "%s/.git", path);
    return cbm_file_exists(git_path);
}

static bool wi_add_repo(wi_repo_list_t *list, const char *path) {
    if (!list || !path || !path[0]) {
        return false;
    }
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].path, path) == 0) {
            return true;
        }
    }
    if (list->count >= list->cap) {
        int next_cap = list->cap > 0 ? list->cap * WI_GROWTH : WI_INITIAL_CAP;
        wi_repo_t *next = realloc(list->items, (size_t)next_cap * sizeof(*next));
        if (!next) {
            return false;
        }
        list->items = next;
        list->cap = next_cap;
    }
    snprintf(list->items[list->count].path, sizeof(list->items[list->count].path), "%s", path);
    list->count++;
    return true;
}

static void wi_expand_root(const char *root, char *out, size_t out_sz) {
    if (!root || !root[0]) {
        out[0] = '\0';
        return;
    }
    if (root[0] == '~' && root[SKIP_ONE] == '/') {
        const char *home = cbm_get_home_dir();
        if (home) {
            snprintf(out, out_sz, "%s/%s", home, root + CBM_SZ_2);
            return;
        }
    }
    snprintf(out, out_sz, "%s", root);
}

static void wi_discover_root(const char *root, wi_repo_list_t *repos) {
    char expanded[CBM_PATH_MAX];
    wi_expand_root(root, expanded, sizeof(expanded));
    if (!expanded[0] || !cbm_is_dir(expanded)) {
        return;
    }
    if (wi_dir_has_git(expanded)) {
        wi_add_repo(repos, expanded);
    }

    cbm_dir_t *dir = cbm_opendir(expanded);
    if (!dir) {
        return;
    }
    cbm_dirent_t *ent;
    while ((ent = cbm_readdir(dir)) != NULL) {
        if (!ent->is_dir || strcmp(ent->name, ".") == 0 || strcmp(ent->name, "..") == 0) {
            continue;
        }
        if (cbm_should_skip_dir(ent->name, CBM_MODE_FULL)) {
            continue;
        }
        char child[CBM_PATH_MAX];
        snprintf(child, sizeof(child), "%s/%s", expanded, ent->name);
        if (wi_dir_has_git(child)) {
            wi_add_repo(repos, child);
        }
    }
    cbm_closedir(dir);
}

static void wi_discover_roots(const char *roots, wi_repo_list_t *repos) {
    if (!roots || !roots[0]) {
        return;
    }
    char *copy = strdup(roots);
    if (!copy) {
        return;
    }
    char *save = NULL;
    char *tok = strtok_r(copy, ",;\n", &save);
    while (tok) {
        while (*tok == ' ' || *tok == '\t') {
            tok++;
        }
        wi_discover_root(tok, repos);
        tok = strtok_r(NULL, ",;\n", &save);
    }
    free(copy);
}

static const char *wi_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + SKIP_ONE : path;
}

static bool wi_path_has_skipped_component(const char *rel_path) {
    char part[CBM_PATH_MAX];
    int off = 0;
    for (const char *p = rel_path; ; p++) {
        if (*p == '/' || *p == '\0') {
            part[off] = '\0';
            if (off > 0 && cbm_should_skip_dir(part, CBM_MODE_FAST)) {
                return true;
            }
            off = 0;
            if (*p == '\0') {
                break;
            }
            continue;
        }
        if (off < (int)sizeof(part) - SKIP_ONE) {
            part[off++] = *p;
        }
    }
    return false;
}

static bool wi_path_matches_workspace_artifact_tree(const char *rel_path) {
    static const char *const patterns[] = {"local/runtime", "local/dev-runtime", NULL};
    if (!rel_path) {
        return false;
    }
    for (int i = 0; patterns[i]; i++) {
        size_t len = strlen(patterns[i]);
        if (strncmp(rel_path, patterns[i], len) == 0 &&
            (rel_path[len] == '\0' || rel_path[len] == '/')) {
            return true;
        }
    }
    return false;
}

static bool wi_is_indexable_source(const char *rel_path) {
    const char *base = wi_basename(rel_path);
    if (!base || !base[0]) {
        return false;
    }
    if (wi_path_has_skipped_component(rel_path)) {
        return false;
    }
    if (wi_path_matches_workspace_artifact_tree(rel_path)) {
        return false;
    }
    if (cbm_has_ignored_suffix(base, CBM_MODE_FAST) ||
        cbm_should_skip_filename(base, CBM_MODE_FAST) ||
        cbm_matches_fast_pattern(rel_path, CBM_MODE_FAST)) {
        return false;
    }
    return cbm_language_for_filename(base) != CBM_LANG_COUNT;
}

static void wi_count_git_files(const char *repo, int *tracked, int *indexable) {
    *tracked = 0;
    *indexable = 0;
    if (!cbm_validate_shell_arg(repo)) {
        return;
    }
    char cmd[CBM_PATH_MAX + CBM_SZ_128];
    snprintf(cmd, sizeof(cmd), "git -C '%s' ls-files 2>/dev/null", repo);
    FILE *fp = cbm_popen(cmd, "r");
    if (!fp) {
        return;
    }
    char line[CBM_PATH_MAX];
    while (fgets(line, sizeof(line), fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!line[0]) {
            continue;
        }
        (*tracked)++;
        if (wi_is_indexable_source(line)) {
            (*indexable)++;
        }
    }
    cbm_pclose(fp);
}

static bool wi_is_indexed(const char *project) {
    const char *cache_dir = cbm_resolve_cache_dir();
    if (!cache_dir || !project) {
        return false;
    }
    char db_path[CBM_PATH_MAX];
    snprintf(db_path, sizeof(db_path), "%s/%s.db", cache_dir, project);
    return cbm_file_exists(db_path);
}

static void wi_classify(wi_repo_result_t *r, int limit) {
    r->memory_risk_score = r->indexable_files;
    r->memory_available_mb = -1;
    r->swap_used_mb = -1;
    if (r->indexed) {
        r->status = "already_indexed";
        r->reason = "existing index found";
    } else if (r->indexable_files <= 0) {
        r->status = "no_indexable_files";
        r->reason = "no tracked source files passed policy filters";
    } else if (limit > 0 && r->indexable_files > limit) {
        r->status = "too_many_indexable_files";
        r->reason = "indexable source file count exceeds limit";
    } else {
        r->status = "would_index";
        r->reason = "eligible";
    }
}

static int wi_apply_repo(const wi_repo_result_t *r, const cbm_workspace_index_options_t *opts) {
    if (strcmp(r->status, "already_indexed") == 0) {
        (void)opts;
        return 0;
    }
    if (strcmp(r->status, "would_index") != 0) {
        return 0;
    }
    bool locked = false;
    if (opts->nonblocking_lock) {
        locked = cbm_pipeline_try_lock();
        if (!locked) {
            cbm_log_info("workspace_index.skip", "path", r->path, "reason", "pipeline_busy");
            return 0;
        }
    } else {
        cbm_pipeline_lock();
        locked = true;
    }

    cbm_pipeline_t *p = cbm_pipeline_new(r->path, NULL, CBM_MODE_FAST);
    if (!p) {
        if (locked) {
            cbm_pipeline_unlock();
        }
        return CBM_NOT_FOUND;
    }
    int rc = cbm_pipeline_run(p);
    cbm_pipeline_free(p);
    cbm_pipeline_unlock();
    cbm_mem_collect();
    if (rc == 0 && opts->watcher) {
        cbm_watcher_watch(opts->watcher, r->project, r->path);
    }
    return rc;
}

char *cbm_workspace_index_run(const cbm_workspace_index_options_t *opts) {
    int limit = opts && opts->limit > 0 ? opts->limit : WI_DEFAULT_LIMIT;
    int max_apply = opts && opts->max_apply > 0 ? opts->max_apply : 0;
    int min_available_mb = opts && opts->min_available_mb > 0 ? opts->min_available_mb : 0;
    int max_swap_used_mb = opts && opts->max_swap_used_mb > 0 ? opts->max_swap_used_mb : 0;
    const char *roots = opts ? opts->roots : NULL;
    bool apply = opts ? opts->apply : false;
    bool lock_busy = false;
    wi_process_lock_t process_lock = {0};

    if (apply && !wi_try_process_lock(&process_lock)) {
        if (opts && opts->nonblocking_lock) {
            lock_busy = true;
            apply = false;
        } else {
            yyjson_mut_doc *busy_doc = yyjson_mut_doc_new(NULL);
            yyjson_mut_val *busy_root = yyjson_mut_obj(busy_doc);
            yyjson_mut_doc_set_root(busy_doc, busy_root);
            yyjson_mut_obj_add_str(busy_doc, busy_root, "type", "workspace.index.preview.v1");
            yyjson_mut_obj_add_str(busy_doc, busy_root, "roots", roots ? roots : "");
            yyjson_mut_obj_add_int(busy_doc, busy_root, "limit", limit);
            yyjson_mut_obj_add_int(busy_doc, busy_root, "max_apply", max_apply);
            yyjson_mut_obj_add_int(busy_doc, busy_root, "min_available_mb", min_available_mb);
            yyjson_mut_obj_add_int(busy_doc, busy_root, "max_swap_used_mb", max_swap_used_mb);
            yyjson_mut_obj_add_bool(busy_doc, busy_root, "apply", false);
            yyjson_mut_obj_add_bool(busy_doc, busy_root, "lock_busy", true);
            yyjson_mut_obj_add_str(busy_doc, busy_root, "status", "workspace_index_lock_busy");
            size_t busy_len = 0;
            char *busy_out = yyjson_mut_write(busy_doc, YYJSON_WRITE_PRETTY, &busy_len);
            yyjson_mut_doc_free(busy_doc);
            return busy_out;
        }
    }

    wi_repo_list_t repos = {0};
    wi_discover_roots(roots, &repos);

    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    yyjson_mut_val *root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    yyjson_mut_obj_add_str(doc, root, "type", "workspace.index.preview.v1");
    yyjson_mut_obj_add_str(doc, root, "roots", roots ? roots : "");
    yyjson_mut_obj_add_int(doc, root, "limit", limit);
    yyjson_mut_obj_add_int(doc, root, "max_apply", max_apply);
    yyjson_mut_obj_add_int(doc, root, "min_available_mb", min_available_mb);
    yyjson_mut_obj_add_int(doc, root, "max_swap_used_mb", max_swap_used_mb);
    yyjson_mut_obj_add_bool(doc, root, "apply", apply);
    yyjson_mut_obj_add_bool(doc, root, "lock_busy", lock_busy);

    yyjson_mut_val *arr = yyjson_mut_arr(doc);
    int applied = 0;
    int eligible = 0;
    for (int i = 0; i < repos.count; i++) {
        char *project = cbm_project_name_from_path(repos.items[i].path);
        if (!project) {
            continue;
        }
        wi_repo_result_t r = {.path = repos.items[i].path, .project = project};
        wi_count_git_files(r.path, &r.tracked_files, &r.indexable_files);
        r.indexed = wi_is_indexed(r.project);
        wi_classify(&r, limit);
        if (lock_busy && strcmp(r.status, "would_index") == 0) {
            r.status = "skipped_by_active_lock";
            r.reason = "workspace index lock is held by another process";
        }
        if (strcmp(r.status, "would_index") == 0 || strcmp(r.status, "already_indexed") == 0) {
            eligible++;
        }
        if (opts && apply && strcmp(r.status, "would_index") == 0) {
            if (max_apply > 0 && applied >= max_apply) {
                r.status = "deferred_by_batch_limit";
                r.reason = "startup workspace batch limit reached";
            } else if (wi_memory_allows_index(opts, &r)) {
                if (wi_apply_repo(&r, opts) == 0) {
                    applied++;
                    r.status = "indexed";
                    r.reason = "indexed successfully";
                }
            }
        } else if (opts && apply && strcmp(r.status, "already_indexed") == 0) {
            (void)wi_apply_repo(&r, opts);
        }

        yyjson_mut_val *obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_strcpy(doc, obj, "path", r.path);
        yyjson_mut_obj_add_strcpy(doc, obj, "project", r.project);
        yyjson_mut_obj_add_int(doc, obj, "tracked_files", r.tracked_files);
        yyjson_mut_obj_add_int(doc, obj, "indexable_files", r.indexable_files);
        yyjson_mut_obj_add_int(doc, obj, "memory_risk_score", r.memory_risk_score);
        if (r.memory_available_mb >= 0) {
            yyjson_mut_obj_add_int(doc, obj, "memory_available_mb", (int)r.memory_available_mb);
        }
        if (r.swap_used_mb >= 0) {
            yyjson_mut_obj_add_int(doc, obj, "swap_used_mb", (int)r.swap_used_mb);
        }
        yyjson_mut_obj_add_str(doc, obj, "status", r.status);
        yyjson_mut_obj_add_str(doc, obj, "reason", r.reason);
        yyjson_mut_arr_add_val(arr, obj);
        free(project);
    }

    yyjson_mut_obj_add_int(doc, root, "repository_count", repos.count);
    yyjson_mut_obj_add_int(doc, root, "eligible_count", eligible);
    yyjson_mut_obj_add_int(doc, root, "applied_count", applied);
    yyjson_mut_obj_add_val(doc, root, "repositories", arr);

    size_t out_len = 0;
    char *out = yyjson_mut_write(doc, YYJSON_WRITE_PRETTY, &out_len);
    yyjson_mut_doc_free(doc);
    free(repos.items);
    wi_release_process_lock(&process_lock);
    return out;
}

void cbm_workspace_auto_index_run(const cbm_workspace_index_options_t *opts) {
    const char *roots = opts ? opts->roots : NULL;
    if (!roots || !roots[0]) {
        cbm_log_info("workspace_index.skip", "reason", "no_workspace_roots");
        return;
    }
    if (!wi_try_startup_owner_lease()) {
        cbm_log_info("workspace_index.skip", "reason", "startup_owner_lease_active");
        return;
    }
    char *summary = cbm_workspace_index_run(opts);
    if (summary) {
        cbm_log_info("workspace_index.done", "summary", "available");
        free(summary);
    }
}

void cbm_workspace_auto_index_from_config(cbm_config_t *cfg, cbm_watcher_t *watcher) {
    if (!cfg) {
        return;
    }
    if (!cbm_config_get_bool(cfg, CBM_CONFIG_WORKSPACE_AUTO_INDEX, false)) {
        return;
    }
    const char *roots = cbm_config_get(cfg, CBM_CONFIG_WORKSPACE_ROOTS, "");
    int limit = cbm_config_get_int(cfg, CBM_CONFIG_WORKSPACE_INDEX_LIMIT, WI_DEFAULT_LIMIT);
    int max_apply = cbm_config_get_int(cfg, CBM_CONFIG_WORKSPACE_INDEX_BATCH_LIMIT,
                                       WI_DEFAULT_STARTUP_BATCH_LIMIT);
    int min_available_mb = cbm_config_get_int(cfg, CBM_CONFIG_WORKSPACE_INDEX_MIN_AVAILABLE_MB,
                                              WI_DEFAULT_MIN_AVAILABLE_MB);
    int max_swap_used_mb = cbm_config_get_int(cfg, CBM_CONFIG_WORKSPACE_INDEX_MAX_SWAP_USED_MB,
                                              WI_DEFAULT_MAX_SWAP_USED_MB);
    cbm_workspace_index_options_t opts = {
        .roots = roots,
        .limit = limit,
        .max_apply = max_apply,
        .min_available_mb = min_available_mb,
        .max_swap_used_mb = max_swap_used_mb,
        .apply = true,
        .json = true,
        .nonblocking_lock = true,
        .watcher = NULL,
    };
    (void)watcher;
    cbm_workspace_auto_index_run(&opts);
}

int cbm_cmd_workspace_index(int argc, char **argv) {
    const char *roots = NULL;
    char roots_buf[CBM_PATH_MAX * CBM_SZ_4];
    roots_buf[0] = '\0';
    bool apply = false;
    bool json = false;
    int limit = WI_DEFAULT_LIMIT;
    int max_apply = 0;
    int min_available_mb = 0;
    int max_swap_used_mb = 0;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--apply") == 0) {
            apply = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if ((strcmp(argv[i], "--root") == 0 || strcmp(argv[i], "--roots") == 0) &&
                   i + SKIP_ONE < argc) {
            if (roots_buf[0]) {
                strncat(roots_buf, ",", sizeof(roots_buf) - strlen(roots_buf) - SKIP_ONE);
            }
            strncat(roots_buf, argv[++i], sizeof(roots_buf) - strlen(roots_buf) - SKIP_ONE);
        } else if (strcmp(argv[i], "--limit") == 0 && i + SKIP_ONE < argc) {
            limit = (int)strtol(argv[++i], NULL, CBM_DECIMAL_BASE);
        } else if (strcmp(argv[i], "--max-apply") == 0 && i + SKIP_ONE < argc) {
            max_apply = (int)strtol(argv[++i], NULL, CBM_DECIMAL_BASE);
        } else if (strcmp(argv[i], "--min-available-mb") == 0 && i + SKIP_ONE < argc) {
            min_available_mb = (int)strtol(argv[++i], NULL, CBM_DECIMAL_BASE);
        } else if (strcmp(argv[i], "--max-swap-used-mb") == 0 && i + SKIP_ONE < argc) {
            max_swap_used_mb = (int)strtol(argv[++i], NULL, CBM_DECIMAL_BASE);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: codebase-memory-mcp workspace-index [--root PATH] [--apply] [--json] [--limit N] [--max-apply N] [--min-available-mb N] [--max-swap-used-mb N]\n");
            return 0;
        } else {
            (void)fprintf(stderr, "Unknown workspace-index argument: %s\n", argv[i]);
            return 1;
        }
    }

    if (roots_buf[0]) {
        roots = roots_buf;
    } else {
        const char *home = cbm_get_home_dir();
        if (!home) {
            (void)fprintf(stderr, "error: HOME not set\n");
            return 1;
        }
        cbm_config_t *cfg = cbm_config_open(cbm_resolve_cache_dir());
        if (!cfg) {
            (void)fprintf(stderr, "error: cannot open config database\n");
            return 1;
        }
        roots = cbm_config_get(cfg, CBM_CONFIG_WORKSPACE_ROOTS, "");
        if (!roots || !roots[0]) {
            cbm_config_close(cfg);
            (void)fprintf(stderr, "error: no roots provided; use --root PATH or config set %s\n",
                          CBM_CONFIG_WORKSPACE_ROOTS);
            return 1;
        }
        snprintf(roots_buf, sizeof(roots_buf), "%s", roots);
        roots = roots_buf;
        cbm_config_close(cfg);
    }

    cbm_workspace_index_options_t opts = {
        .roots = roots,
        .limit = limit,
        .max_apply = max_apply,
        .min_available_mb = min_available_mb,
        .max_swap_used_mb = max_swap_used_mb,
        .apply = apply,
        .json = json,
        .nonblocking_lock = false,
        .watcher = NULL,
    };
    char *out = cbm_workspace_index_run(&opts);
    if (!out) {
        return 1;
    }
    printf("%s\n", out);
    free(out);
    return 0;
}
