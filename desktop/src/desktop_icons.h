// The Android vector drawables, embedded into the desktop build at configure
// time (desktop/CMakeLists.txt writes the table) so an icon has one source.
#pragma once

#include <cstddef>

namespace fsdesktop {

struct EmbeddedIcon {
    const char* name;
    const char* xml;
};

extern const EmbeddedIcon kEmbeddedIcons[];
extern const size_t kEmbeddedIconCount;

}  // namespace fsdesktop
