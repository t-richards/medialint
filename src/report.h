#pragma once

#include <glib.h>

#include "linter.h"

// ReportingContext is a thread-safe context for storing linter messages.
typedef struct ReportingContext
{
    /*< private >*/
    GMutex mutex;
    GHashTable *reports;
} ReportingContext;

// Creates a new ReportingContext for storing linter messages.
ReportingContext *reporting_context_new();

// Adds a new lint message to the context.
void reporting_context_add(ReportingContext *ctx, const char *path_key, const LinterClass class_id, const char *report_message);

// Prints all lint messages for display.
//
// @return The number of files that produced lint messages.
int reporting_context_print(ReportingContext *ctx);

// Frees the ReportingContext.
void reporting_context_free(ReportingContext *ctx);
