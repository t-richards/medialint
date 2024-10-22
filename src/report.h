#pragma once

#include <glib-unix.h>
#include <glib.h>

// ReportingContext is a thread-safe context for storing linter messages.
typedef struct ReportingContext
{
    /*< private >*/
    GMutex mutex;
    GHashTable *reports;
} ReportingContext;

ReportingContext *reporting_context_new();
void reporting_context_free(ReportingContext *ctx);
void reporting_context_add(ReportingContext *ctx, const char *path_key, const char *report_class, const char *report_message);
int reporting_context_print(ReportingContext *ctx);
