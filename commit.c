
#include "commit.h"
#include "index.h"
#include "tree.h"
#include "pes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <unistd.h>

// forward declarations (from object.c)
int object_write(ObjectType type, const void *data, size_t len, ObjectID *id_out);
int object_read(const ObjectID *id, ObjectType *type_out, void **data_out, size_t *len_out);

int commit_create(const char *message, ObjectID *commit_id_out) {
    if (!message || !commit_id_out) return -1;

    Commit commit;
    memset(&commit, 0, sizeof(Commit));

    Index index;
    if (index_load(&index) != 0) {
        fprintf(stderr, "error: failed to load index\n");
        return -1;
    }

    if (index.count == 0) {
        fprintf(stderr, "nothing to commit\n");
        return -1;
    }

    if (tree_from_index(&commit.tree) != 0) {
        fprintf(stderr, "error: failed to create tree\n");
        return -1;
    }

    if (head_read(&commit.parent) == 0) {
        commit.has_parent = 1;
    } else {
        commit.has_parent = 0;
    }

    const char *author = pes_author();
    if (!author) author = "unknown";

    snprintf(commit.author, sizeof(commit.author), "%s", author);

    commit.timestamp = (uint64_t)time(NULL);

    snprintf(commit.message, sizeof(commit.message), "%s", message);

    void *buffer = NULL;
    size_t len = 0;

    if (commit_serialize(&commit, &buffer, &len) != 0) {
        fprintf(stderr, "error: serialize failed\n");
        return -1;
    }

    if (object_write(OBJ_COMMIT, buffer, len, commit_id_out) != 0) {
        free(buffer);
        fprintf(stderr, "error: object write failed\n");
        return -1;
    }

    free(buffer);

    if (head_update(commit_id_out) != 0) {
        fprintf(stderr, "error: failed to update HEAD\n");
        return -1;
    }

    return 0;
}

int commit_serialize(const Commit *commit, void **data_out, size_t *len_out) {
    if (!commit || !data_out || !len_out) return -1;

    char tree_hex[HASH_HEX_SIZE + 1];
    char parent_hex[HASH_HEX_SIZE + 1];

    hash_to_hex(&commit->tree, tree_hex);

    if (commit->has_parent) {
        hash_to_hex(&commit->parent, parent_hex);
    }

    char *buffer = malloc(8192);
    if (!buffer) return -1;

    int written = 0;

    written += sprintf(buffer + written, "tree %s\n", tree_hex);

    if (commit->has_parent) {
        written += sprintf(buffer + written, "parent %s\n", parent_hex);
    }

    written += sprintf(buffer + written, "author %s\n", commit->author);
    written += sprintf(buffer + written, "timestamp %" PRIu64 "\n", commit->timestamp);
    written += sprintf(buffer + written, "\n%s\n", commit->message);

    *data_out = buffer;
    *len_out = written;

    return 0;
}

int commit_parse(const void *data, size_t len, Commit *commit_out) {
    if (!data || !commit_out) return -1;

    memset(commit_out, 0, sizeof(Commit));

    char *buffer = malloc(len + 1);
    if (!buffer) return -1;

    memcpy(buffer, data, len);
    buffer[len] = '\0';

    char *line = strtok(buffer, "\n");

    while (line) {
        if (strncmp(line, "tree ", 5) == 0) {
            hex_to_hash(line + 5, &commit_out->tree);
        } else if (strncmp(line, "parent ", 7) == 0) {
            hex_to_hash(line + 7, &commit_out->parent);
            commit_out->has_parent = 1;
        } else if (strncmp(line, "author ", 7) == 0) {
            strncpy(commit_out->author, line + 7, sizeof(commit_out->author) - 1);
        } else if (strncmp(line, "timestamp ", 10) == 0) {
            commit_out->timestamp = strtoull(line + 10, NULL, 10);
        } else if (strlen(line) == 0) {
            line = strtok(NULL, "");
            if (line) {
                strncpy(commit_out->message, line, sizeof(commit_out->message) - 1);
            }
            break;
        }

        line = strtok(NULL, "\n");
    }

    free(buffer);
    return 0;
}

int head_read(ObjectID *id_out) {
    FILE *head = fopen(HEAD_FILE, "r");
    if (!head) return -1;

    char line[256];
    if (!fgets(line, sizeof(line), head)) {
        fclose(head);
        return -1;
    }
    fclose(head);

    if (strncmp(line, "ref: ", 5) != 0) return -1;

    char ref_path[256];
    snprintf(ref_path, sizeof(ref_path), "%s/%s", PES_DIR, line + 5);

    char *newline = strchr(ref_path, '\n');
    if (newline) *newline = '\0';

    FILE *ref = fopen(ref_path, "r");
    if (!ref) return -1;

    char hex[HASH_HEX_SIZE + 1];
    if (!fgets(hex, sizeof(hex), ref)) {
        fclose(ref);
        return -1;
    }
    fclose(ref);

    char *nl = strchr(hex, '\n');
    if (nl) *nl = '\0';

    return hex_to_hash(hex, id_out);
}

int head_update(const ObjectID *new_commit) {
    FILE *head = fopen(HEAD_FILE, "r");
    if (!head) return -1;

    char line[256];
    if (!fgets(line, sizeof(line), head)) {
        fclose(head);
        return -1;
    }
    fclose(head);

    if (strncmp(line, "ref: ", 5) != 0) return -1;

    char ref_path[256];
    snprintf(ref_path, sizeof(ref_path), "%s/%s", PES_DIR, line + 5);

    char *newline = strchr(ref_path, '\n');
    if (newline) *newline = '\0';

    FILE *ref = fopen(ref_path, "w");
    if (!ref) return -1;

    char hex[HASH_HEX_SIZE + 1];
    hash_to_hex(new_commit, hex);

    fprintf(ref, "%s\n", hex);
    fclose(ref);

    return 0;
}

int commit_walk(commit_walk_fn callback, void *ctx) {
    ObjectID current;

    if (head_read(&current) != 0) {
        return -1;
    }

    while (1) {
        ObjectType type;
        void *data = NULL;
        size_t len = 0;

        if (object_read(&current, &type, &data, &len) != 0) {
            return -1;
        }

        Commit commit;
        if (commit_parse(data, len, &commit) != 0) {
            free(data);
            return -1;
        }

        free(data);

        callback(&current, &commit, ctx);

        if (!commit.has_parent) {
            break;
        }

        current = commit.parent;
    }

    return 0;
}


