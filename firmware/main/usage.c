#include "usage.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static usage_t s_usage;
static bool s_known;
static SemaphoreHandle_t s_lock;
static void (*s_on_change)(void);

void usage_init(void)
{
    s_lock = xSemaphoreCreateMutex();
}

// Read one length-prefixed string into `dst` (NUL-terminated, at most `cap` bytes with NUL).
static bool read_string(const uint8_t **p, const uint8_t *end, char *dst, size_t cap)
{
    if (*p >= end) {
        return false;
    }
    const size_t len = **p;
    (*p)++;
    if (len >= cap || *p + len > end) {
        return false;
    }
    memcpy(dst, *p, len);
    dst[len] = '\0';
    *p += len;
    return true;
}

// [name len][name][window count] then per window [label len][label][percent][reset minutes LE16].
bool usage_replace(const uint8_t *data, size_t len)
{
    const uint8_t *p = data;
    const uint8_t *end = data + len;
    usage_t parsed = { 0 };
    if (!read_string(&p, end, parsed.name, sizeof(parsed.name)) || p >= end || *p > USAGE_WINDOWS) {
        return false;
    }
    parsed.count = *p++;
    for (int i = 0; i < parsed.count; i++) {
        usage_window_t *w = &parsed.windows[i];
        if (!read_string(&p, end, w->label, sizeof(w->label)) || end - p < 3 ||
            (p[0] > 100 && p[0] != USAGE_UNKNOWN)) {
            return false;
        }
        w->percent = p[0];
        w->reset_minutes = p[1] | (p[2] << 8);
        p += 3;
    }
    if (p != end) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_usage = parsed;
    s_known = true;
    xSemaphoreGive(s_lock);
    if (s_on_change) {
        s_on_change();
    }
    return true;
}

bool usage_get(usage_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_usage;
    const bool known = s_known;
    xSemaphoreGive(s_lock);
    return known;
}

void usage_on_change(void (*callback)(void))
{
    s_on_change = callback;
}
