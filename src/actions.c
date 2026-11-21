#include "actions.h"

#include <strings.h>

/** @brief Configuration-file names indexed by Action. */
static const char *const ACTION_NAMES[ACTION_COUNT] = {
#define INK_ACTION_NAME(id, name) name,
    INK_ACTION_LIST(INK_ACTION_NAME)
#undef INK_ACTION_NAME
};

const char *action_name(Action action)
{
    return (unsigned)action < ACTION_COUNT ? ACTION_NAMES[action] : "unknown";
}

bool action_from_name(const char *name, Action *action)
{
    if (name == NULL || action == NULL) {
        return false;
    }
    for (int i = 0; i < ACTION_COUNT; i++) {
        if (strcasecmp(name, ACTION_NAMES[i]) == 0) {
            *action = (Action)i;
            return true;
        }
    }
    return false;
}
