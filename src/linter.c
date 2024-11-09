#include "linter.h"

const char *linter_class_name(LinterClass class_id)
{
    switch (class_id)
    {
    case CLASS_FORMAT_UNSUPPORTED:
        return "Format/Unsupported";
    case CLASS_NAMING_FORBIDDEN:
        return "Naming/Forbidden";
    case CLASS_NAMING_MOVIE:
        return "Naming/Movie";
    case CLASS_NAMING_TV:
        return "Naming/TV";
    case CLASS_SUBTITLES_PRESENCE:
        return "Subtitles/Presence";
    case CLASS_VIDEO_BITRATE:
        return "Video/Bitrate";
    case CLASS_VIDEO_CODEC:
        return "Video/Codec";
    case CLASS_VIDEO_RESOLUTION:
        return "Video/Resolution";
    default:
        return "Unknown";
    }
}
