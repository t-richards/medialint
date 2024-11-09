#pragma once

typedef enum LinterClass
{
    CLASS_FORMAT_UNSUPPORTED,
    CLASS_NAMING_FORBIDDEN,
    CLASS_NAMING_MOVIE,
    CLASS_NAMING_TV,
    CLASS_SUBTITLES_PRESENCE,
    CLASS_VIDEO_BITRATE,
    CLASS_VIDEO_CODEC,
    CLASS_VIDEO_RESOLUTION
} LinterClass;

/**
 * Get the name of a linter class.
 * 
 * @param class_id The linter class.
 * @return A static string with the name of the class, never NULL.
 */
const char *linter_class_name(LinterClass class_id);
