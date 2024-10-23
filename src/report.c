#include <stdio.h>
#include "report.h"

typedef struct Report
{
    char *report_class;
    char *report_message;
} Report;


// Each report has copied strings.
void report_free(gpointer data)
{
    struct Report *report = data;
    g_free(report->report_class);
    g_free(report->report_message);
    g_free(report);
}

// Hash table items are queues of reports.
void item_free(gpointer data)
{
    GQueue *queue = data;
    g_queue_free_full(queue, report_free);
}

ReportingContext *reporting_context_new()
{
    ReportingContext *ctx = g_new0(ReportingContext, 1);
    g_mutex_init(&ctx->mutex);
    ctx->reports = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, item_free);
    return ctx;
}

// lol
void reporting_context_free(ReportingContext *ctx)
{
    g_hash_table_destroy(ctx->reports);
    g_mutex_clear(&ctx->mutex);
    g_free(ctx);
}

void reporting_context_add(ReportingContext *ctx, const char *path_key, const char *report_class, const char *report_message)
{
    struct Report *report = g_new0(struct Report, 1);
    report->report_class = g_strdup(report_class);
    report->report_message = g_strdup(report_message);

    g_mutex_lock(&ctx->mutex);

    // First, check if the key already exists.
    gpointer report_queue = g_hash_table_lookup(ctx->reports, path_key);
    if (report_queue != NULL)
    {
        // The key already has a report queue. Append the new report to it.
        g_queue_push_tail(report_queue, report);
    }
    else
    {
        // The key does not exist. Insert a new queue with the report.
        GQueue *report_queue = g_queue_new();
        g_queue_push_tail(report_queue, report);
        g_hash_table_insert(ctx->reports, g_strdup(path_key), report_queue);
    }

    g_mutex_unlock(&ctx->mutex);
}

gint report_compare(const Report *a, const Report *b, gpointer _user_data)
{
    return g_ascii_strcasecmp(a->report_class, b->report_class);
}

int reporting_context_print(ReportingContext *ctx)
{
    g_mutex_lock(&ctx->mutex);

    GList *keys = g_hash_table_get_keys(ctx->reports);
    keys = g_list_sort(keys, (GCompareFunc)g_ascii_strcasecmp);
    int num_keys = g_list_length(keys);

    for (GList *key = keys; key != NULL; key = key->next)
    {
        const char *path_key = key->data;

        GQueue *report_queue = g_hash_table_lookup(ctx->reports, path_key);
        g_queue_sort(report_queue, (GCompareDataFunc)report_compare, NULL);

        for (GList *report = report_queue->head; report != NULL; report = report->next)
        {
            struct Report *r = report->data;
            printf("%s: %s: %s\n", path_key, r->report_class, r->report_message);
        }
    }

    g_list_free(keys);

    g_mutex_unlock(&ctx->mutex);

    return num_keys;
}
