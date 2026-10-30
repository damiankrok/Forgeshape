// AUDIT PROBE STUB (FUNCTION-COUNCIL-R2 seat G): declarations only, never built into the product.
#pragma once
#include <cstdint>
extern "C" {
struct ANativeWindow;
typedef struct ANativeWindow ANativeWindow;
void ANativeWindow_acquire(ANativeWindow*);
void ANativeWindow_release(ANativeWindow*);
int32_t ANativeWindow_getWidth(ANativeWindow*);
int32_t ANativeWindow_getHeight(ANativeWindow*);
}
