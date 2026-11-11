/*
 * What every Home Assistant entity shows the same way, whoever keeps the
 * entities (the remote's WebSocket client, the emulator's demo house): its
 * domain's icon and the line under its name.
 */
#include <stdio.h>
#include "ha.h"
#include "ui/icons.h"

const char *ha_domain_icon(ha_domain_t d)
{
    static const char *const icons[HA_DOMAIN_COUNT] = {
        [HA_LIGHT] = ICON_BULB, [HA_SWITCH] = ICON_SWITCH, [HA_ENERGY] = ICON_ENERGY,
        [HA_BUTTON] = ICON_BUTTON, [HA_CLIMATE] = ICON_THERMO, [HA_FAN] = ICON_FAN,
        [HA_HUMIDIFIER] = ICON_HUMIDIFIER, [HA_WATER_HEATER] = ICON_WATER_HEATER,
        [HA_AIR_QUALITY] = ICON_AIR_QUALITY, [HA_WEATHER] = ICON_WEATHER, [HA_COVER] = ICON_COVER,
        [HA_VALVE] = ICON_VALVE, [HA_LOCK] = ICON_LOCK, [HA_ALARM] = ICON_ALARM, [HA_SIREN] = ICON_SIREN,
        [HA_SENSOR] = ICON_SENSOR, [HA_BINARY] = ICON_DOOR, [HA_CAMERA] = ICON_CAMERA,
        [HA_EVENT] = ICON_EVENT, [HA_MEDIA] = ICON_MEDIA_PLAYER, [HA_REMOTE] = ICON_REMOTE,
        [HA_VACUUM] = ICON_VACUUM, [HA_MOWER] = ICON_LAWN_MOWER, [HA_SCENE] = ICON_SCENE,
        [HA_SCRIPT] = ICON_SCRIPT, [HA_AUTOMATION] = ICON_AUTOMATION, [HA_INPUT_BOOLEAN] = ICON_TOGGLE,
        [HA_INPUT_NUMBER] = ICON_NUMBER, [HA_INPUT_SELECT] = ICON_SELECT, [HA_INPUT_TEXT] = ICON_TEXT,
        [HA_TIMER] = ICON_TIMER, [HA_COUNTER] = ICON_COUNTER, [HA_PERSON] = ICON_PERSON,
        [HA_SUN] = ICON_SUN, [HA_CALENDAR] = ICON_CALENDAR, [HA_TODO] = ICON_TODO,
        [HA_UPDATE] = ICON_UPDATE, [HA_NOTIFY] = ICON_NOTIFY,
    };
    return icons[d];
}

static void fmt_time(char *buf, int len, int32_t s)
{
    if (s >= 3600) snprintf(buf, len, "%d:%02d:%02d", (int)(s / 3600), (int)(s / 60 % 60), (int)(s % 60));
    else snprintf(buf, len, "%d:%02d", (int)(s / 60), (int)(s % 60));
}

/* The secondary line under an entity's name */
const char *ha_state_text(const ha_entity_t *e, char *buf, int len)
{
    if (e->unavailable) return snprintf(buf, len, "Unavailable"), buf;
    if (e->sub[0]) return snprintf(buf, len, "%s", e->sub), buf;
    switch (e->domain) {
    case HA_LIGHT:
        if (!e->on) snprintf(buf, len, "Off");
        else if (e->features & LIGHT_RGB) snprintf(buf, len, "On · RGB · %d%%", e->value);
        else snprintf(buf, len, "%d%%", e->value);
        break;
    case HA_SWITCH:
    case HA_INPUT_BOOLEAN:
    case HA_AUTOMATION:
        snprintf(buf, len, "%s", e->on ? "On" : "Off");
        break;
    case HA_FAN:
        if (e->on) snprintf(buf, len, "On · speed %d", e->value);
        else snprintf(buf, len, "Off");
        break;
    case HA_COVER:
        if (e->value == 0) snprintf(buf, len, "Closed");
        else if (e->value == 100) snprintf(buf, len, "Open");
        else snprintf(buf, len, "Open %d%%", e->value);
        break;
    case HA_VALVE: snprintf(buf, len, "%s", e->on ? "Open" : "Closed"); break;
    case HA_LOCK: snprintf(buf, len, "%s", e->on ? "Locked" : "Unlocked"); break;
    case HA_MEDIA:
        if (!e->on && !e->title[0]) snprintf(buf, len, "Off");
        else snprintf(buf, len, "%s%s%s", e->on ? "Playing" : "Paused", e->artist[0] ? " · " : "", e->artist);
        break;
    case HA_TIMER: {
        char t[16];
        fmt_time(t, sizeof(t), e->pos);
        snprintf(buf, len, "Timer · %s", e->on ? "active" : e->pos ? "paused" : "idle");
        break;
    }
    default: buf[0] = 0; break;
    }
    return buf;
}
