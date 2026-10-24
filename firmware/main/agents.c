#include "agents.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static agent_t s_agents[AGENT_MAX];
static int s_count;
static bool s_known;
static SemaphoreHandle_t s_lock;
static void (*s_on_change)(void);

void agents_init(void)
{
    s_lock = xSemaphoreCreateMutex();
}

static void lock(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
}

static void unlock(void)
{
    xSemaphoreGive(s_lock);
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

// [page][page count][count] then `count` agents. Pages arrive in order on the NimBLE host task;
// they are collected here and replace the list together, so the screen never shows half of one.
bool agents_replace(const uint8_t *data, size_t len)
{
    const uint8_t *p = data;
    const uint8_t *end = data + len;
    if (len < 3) {
        return false;
    }
    const int page = *p++;
    const int pages = *p++;
    const int count = *p++;
    // Static rather than on the stack: this runs on the NimBLE host task, whose stack is small.
    // Only that task writes the list, so one buffer is enough.
    static agent_t parsed[AGENT_MAX];
    static int s_next_page;
    if (pages < 1 || pages * AGENT_PAGE > AGENT_MAX || page >= pages || count > AGENT_PAGE ||
        (page > 0 && page != s_next_page) || (page < pages - 1 && count != AGENT_PAGE)) {
        return false;
    }

    agent_t *dst = &parsed[page * AGENT_PAGE];
    for (int i = 0; i < count; i++) {
        if (p >= end || *p > AGENT_UNKNOWN) {
            return false;
        }
        dst[i].status = *p++;
        if (!read_string(&p, end, dst[i].id, AGENT_ID_LEN) ||
            !read_string(&p, end, dst[i].title, AGENT_TITLE_LEN) ||
            !read_string(&p, end, dst[i].detail, AGENT_DETAIL_LEN)) {
            return false;
        }
    }
    if (p != end) {
        return false;
    }
    s_next_page = page + 1;
    if (page < pages - 1) {
        return true;
    }

    const int total = page * AGENT_PAGE + count;
    lock();
    memcpy(s_agents, parsed, sizeof(agent_t) * total);
    s_count = total;
    s_known = true;
    unlock();
    if (s_on_change) {
        s_on_change();
    }
    return true;
}

bool agents_known(void)
{
    return s_known;
}

int agents_copy(agent_t *out, int max)
{
    lock();
    const int n = s_count < max ? s_count : max;
    memcpy(out, s_agents, sizeof(agent_t) * n);
    unlock();
    return n;
}

int agents_attention(void)
{
    int n = 0;
    lock();
    for (int i = 0; i < s_count; i++) {
        if (s_agents[i].status == AGENT_BLOCKED || s_agents[i].status == AGENT_DONE) {
            n++;
        }
    }
    unlock();
    return n;
}

int agents_dominant(void)
{
    int dominant = -1;
    lock();
    for (int i = 0; i < s_count; i++) {
        if (dominant < 0 || (int)s_agents[i].status < dominant) {
            dominant = s_agents[i].status;
        }
    }
    unlock();
    return dominant;
}

void agents_on_change(void (*callback)(void))
{
    s_on_change = callback;
}
