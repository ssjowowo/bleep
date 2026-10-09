/* Routine editor and its step editor. Running and the progress window are
 * routine.c and overlays.c; the tiles are on the Activities tab. */
#include <stdio.h>
#include <string.h>
#include "../ha.h"
#include "../model.h"
#include "../routine.h"
#include "ui.h"

/* ================= Routine editor ================= */

/* Like the activity editor, it works on a draft: nothing is saved until
 * Create or Update, and leaving with changes asks first. The step pages
 * change the draft directly. */
static int ed;                      /* routine being edited, -1 = a new one */
static routine_t draft, orig;

static bool ed_dirty(void) { return memcmp(&draft, &orig, sizeof(draft)) != 0; }

void ui_routine_edit(int idx)
{
    ed = idx;
    if (idx >= 0) {
        draft = g_model.routines[idx];
    } else {
        memset(&draft, 0, sizeof(draft));
        snprintf(draft.name, sizeof(draft.name), "New routine");
        draft.icon = AICON_SCENE;
    }
    orig = draft;
    ui_open(&page_routine_edit, 0);
}

static void ed_changed(void)
{
    if (ui_current() == &page_routine_edit) ui_refresh();
}

static void ed_save_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!draft.n_steps || (ed >= 0 && !ed_dirty())) return;
    if (ed < 0) {
        routine_t *r = model_add_routine();
        if (!r) {
            ui_toast("No room for more routines");
            return;
        }
        *r = draft;
        ui_toast("Routine created");
    } else {
        g_model.routines[ed] = draft;
        ui_toast("Routine updated");
    }
    orig = draft;
    app_buzz(HAPTIC_CONFIRM);
    app_notify(EV_DEVICES, 0);
    ui_back();
}

static void ed_name_done(const char *text, void *ctx)
{
    LV_UNUSED(ctx);
    if (!*text) return;
    snprintf(draft.name, sizeof(draft.name), "%s", text);
}

static void ed_name_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_text_entry("Routines · Name", "Routine name", "Name", draft.name, "Shown on its tile", KB_NAME, NAME_LEN - 1,
                  ed_name_done, NULL);
}

static void ed_icon_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ui_pick_icon(draft.name, &draft.icon, ed_changed);
}

static void ed_step_cb(lv_event_t *e) { ui_open(&page_routine_step, ARG_INT(e)); }

/* ---- adding a step ---- */

static const fn_t preferred_fns[] = {FN_POWER_ON, FN_POWER, FN_INPUT_1, FN_OK};

static fn_t first_fn(int dev)
{
    const device_t *d = &g_model.devices[dev];
    for (unsigned i = 0; i < sizeof(preferred_fns) / sizeof(preferred_fns[0]); i++)
        if (d->fn[preferred_fns[i]].transport) return preferred_fns[i];
    for (int f = FN_NONE + 1; f < FN_COUNT; f++)
        if (d->fn[f].transport) return (fn_t)f;
    return FN_NONE;
}

static int first_ha_entity(void)
{
    ha_op_t ops[HAOP_COUNT];
    for (int i = 0; i < ha_count(); i++)
        if (ha_ops_for(i, ops)) return i;
    return -1;
}

static void add_kind_done(int kind, void *ctx)
{
    LV_UNUSED(ctx);
    rstep_t s = {.kind = (uint8_t)kind, .target = -1, .repeat = 1, .wait = 2};
    switch (kind) {
    case RS_DEVICE: {
        int dev = -1;
        for (int d = 0; d < g_model.n_devices && dev < 0; d++)
            if (first_fn(d) != FN_NONE) dev = d;
        if (dev < 0) {
            ui_toast("Add a device first");
            return;
        }
        s.target = dev;
        s.fn = first_fn(dev);
        break;
    }
    case RS_HA: {
        int e = first_ha_entity();
        if (e < 0) {
            ui_toast("Sign in to Home Assistant first");
            return;
        }
        ha_op_t ops[HAOP_COUNT];
        ha_ops_for(e, ops);
        snprintf(s.entity, sizeof(s.entity), "%s", ha_get(e)->id);
        s.op = ops[0];
        break;
    }
    case RS_ACTIVITY_START:
        if (!g_model.n_activities) {
            ui_toast("Create an activity first");
            return;
        }
        s.target = 0;
        break;
    default:
        break;
    }
    draft.steps[draft.n_steps++] = s;
    ui_open(&page_routine_step, draft.n_steps - 1);   /* straight to its settings */
}

static void ed_add_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (draft.n_steps >= MAX_RSTEPS) {
        ui_toast("A routine can have up to %d steps", MAX_RSTEPS);
        return;
    }
    static const char *const kinds[] = {"Press a device button", "Home Assistant action", "Start an activity",
                                        "End the running activity"};
    ui_choice(draft.name, "Add a step", kinds, 4, -1, add_kind_done, NULL);
}

static void ed_delete_yes(void *ctx)
{
    LV_UNUSED(ctx);
    orig = draft;
    model_delete_routine(ed);
    app_notify(EV_DEVICES, 0);
    ui_back();
}

static void ed_delete_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    ov_confirm("Delete routine?", draft.name, "Delete", true, ed_delete_yes, NULL);
}

static void ed_build(lv_obj_t *root, int arg)
{
    LV_UNUSED(arg);
    w_header(root, ed < 0 ? "New routine" : "Routines", draft.name, ui_back_guarded_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);
    w_on_click(w_list_row(col, ICON_EDIT, "Name", draft.name, NULL), ed_name_cb, 0);
    w_on_click(w_list_row(col, activity_icons[draft.icon].glyph, "Icon", activity_icons[draft.icon].name, NULL),
               ed_icon_cb, 0);

    w_section(col, "STEPS");
    char text[64], buf[80], w[16];
    for (int i = 0; i < draft.n_steps; i++) {
        const rstep_t *s = &draft.steps[i];
        routine_step_text(s, text, sizeof(text));
        snprintf(buf, sizeof(buf), "%d. %s", i + 1, text);
        bool last = i == draft.n_steps - 1;
        lv_obj_t *r = w_list_row(col, NULL, buf, s->wait && !last ? routine_wait_text(s->wait, w, sizeof(w)) : NULL,
                                 NULL);
        if (s->wait && !last) w_icon(r, ICON_TIMER, 14, T->text2);
        w_on_click(r, ed_step_cb, i);
    }
    if (!draft.n_steps) w_label(col, F_LABEL, T->text2, "No steps yet.");
    lv_obj_t *add = w_button(col, "Add step", ICON_PLUS, BTN_OUTLINE);
    lv_obj_set_width(add, lv_pct(100));
    lv_obj_set_style_margin_top(add, 10, 0);
    lv_obj_add_event_cb(add, ed_add_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *n = w_label(col, F_CAPTION, T->text2,
                          "Tap a step to change it, its wait or its place. Each step waits the time shown before "
                          "the next one starts.");
    lv_obj_set_width(n, lv_pct(100));
    lv_obj_set_style_pad_top(n, 8, 0);

    if (ed >= 0) {
        lv_obj_t *del = w_button(col, "Delete routine", ICON_TRASH, BTN_OUTLINE);
        lv_obj_set_width(del, lv_pct(100));
        lv_obj_set_style_margin_top(del, 16, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(del, 0), T->warning, 0);
        lv_obj_set_style_text_color(lv_obj_get_child(del, 1), T->warning, 0);
        lv_obj_add_event_cb(del, ed_delete_cb, LV_EVENT_CLICKED, NULL);
    }

    bool can = draft.n_steps > 0 && (ed < 0 || ed_dirty());
    w_save_bar_why(root, ed < 0 ? "Create" : "Update", can, draft.n_steps ? NULL : "Add a step to it first", ed_save_cb);
}

/* Unsaved changes: asked about by every way out (ui.c) */
static const char *ed_dirty_q(void)
{
    if (!ed_dirty()) return NULL;
    return ed < 0 ? "Discard new routine?" : "Discard changes to this routine?";
}

const page_t page_routine_edit = {"Edit routine", -1, ed_build, NULL, NULL, false, ed_dirty_q};

/* ================= Step editor ================= */

static int sp;      /* step being edited, in the draft */
static rstep_t *step(void) { return &draft.steps[sp]; }

/* choice lists: built when a picker opens */
#define MAX_OPTS 128             /* big Home Assistant installs */
static char opt_text[MAX_OPTS][48];
static const char *opts[MAX_OPTS];
static int opt_map[MAX_OPTS];

static void device_done(int i, void *ctx)
{
    LV_UNUSED(ctx);
    rstep_t *s = step();
    s->target = opt_map[i];
    if (!g_model.devices[s->target].fn[s->fn].transport) s->fn = first_fn(s->target);
}

static void fn_done(int i, void *ctx) { LV_UNUSED(ctx); step()->fn = opt_map[i]; }
static void activity_done(int i, void *ctx) { LV_UNUSED(ctx); step()->target = opt_map[i]; }
static void op_done(int i, void *ctx) { LV_UNUSED(ctx); step()->op = opt_map[i]; }

static void entity_done(int i, void *ctx)
{
    LV_UNUSED(ctx);
    rstep_t *s = step();
    int e = opt_map[i];
    snprintf(s->entity, sizeof(s->entity), "%s", ha_get(e)->id);
    ha_op_t ops[HAOP_COUNT];
    int n = ha_ops_for(e, ops);
    bool keep = false;
    for (int k = 0; k < n; k++) keep |= ops[k] == s->op;
    if (!keep) s->op = ops[0];
    int lo, hi;
    const char *unit;
    ha_op_range(e, &lo, &hi, &unit);
    if (s->value < lo || s->value > hi) s->value = (lo + hi) / 2;
}

static void pick_cb(lv_event_t *e)
{
    rstep_t *s = step();
    int n = 0, sel = 0;
    switch (ARG_INT(e)) {
    case 0:   /* device */
        for (int d = 0; d < g_model.n_devices && n < MAX_OPTS; d++) {
            if (first_fn(d) == FN_NONE) continue;
            snprintf(opt_text[n], sizeof(opt_text[n]), "%s", g_model.devices[d].name);
            if (d == s->target) sel = n;
            opt_map[n++] = d;
        }
        for (int i = 0; i < n; i++) opts[i] = opt_text[i];
        ui_choice(draft.name, "Device", opts, n, sel, device_done, NULL);
        break;
    case 1: { /* button */
        const device_t *d = &g_model.devices[s->target];
        for (int f = FN_NONE + 1; f < FN_COUNT && n < MAX_OPTS; f++) {
            if (!d->fn[f].transport) continue;
            snprintf(opt_text[n], sizeof(opt_text[n]), "%s", fn_name((fn_t)f));
            if (f == s->fn) sel = n;
            opt_map[n++] = f;
        }
        for (int i = 0; i < n; i++) opts[i] = opt_text[i];
        ui_choice(d->name, "Button", opts, n, sel, fn_done, NULL);
        break;
    }
    case 2: { /* HA entity */
        ha_op_t ops[HAOP_COUNT];
        for (int i = 0; i < ha_count() && n < MAX_OPTS; i++) {
            if (!ha_ops_for(i, ops)) continue;
            const ha_entity_t *en = ha_get(i);
            if (strlen(en->id) >= sizeof(s->entity)) continue;   /* can't be stored whole */
            snprintf(opt_text[n], sizeof(opt_text[n]), "%s · %s", en->name, ha_room_name(en->room));
            if (!strcmp(en->id, s->entity)) sel = n;
            opt_map[n++] = i;
        }
        for (int i = 0; i < n; i++) opts[i] = opt_text[i];
        ui_choice(draft.name, "Home Assistant", opts, n, sel, entity_done, NULL);
        break;
    }
    case 3: { /* HA action */
        int en = ha_find(s->entity);
        if (en < 0) {   /* gone from HA since the page was built */
            ui_toast("Not in Home Assistant right now");
            break;
        }
        ha_op_t ops[HAOP_COUNT];
        int k = ha_ops_for(en, ops);
        for (int i = 0; i < k; i++) {
            snprintf(opt_text[n], sizeof(opt_text[n]), "%s", ha_op_name(ops[i], en));
            if (ops[i] == s->op) sel = n;
            opt_map[n++] = ops[i];
        }
        for (int i = 0; i < n; i++) opts[i] = opt_text[i];
        ui_choice(ha_get(en)->name, "Action", opts, n, sel, op_done, NULL);
        break;
    }
    case 4:   /* activity */
        for (int a = 0; a < g_model.n_activities && n < MAX_OPTS; a++) {
            snprintf(opt_text[n], sizeof(opt_text[n]), "%s", g_model.activities[a].name);
            if (a == s->target) sel = n;
            opt_map[n++] = a;
        }
        for (int i = 0; i < n; i++) opts[i] = opt_text[i];
        ui_choice(draft.name, "Activity", opts, n, sel, activity_done, NULL);
        break;
    }
}

static void presses_add(int d)
{
    rstep_t *s = step();
    int v = s->repeat + d;
    s->repeat = v < 1 ? 1 : v > 10 ? 10 : v;
    ui_refresh();
}
static void presses_minus(lv_event_t *e) { LV_UNUSED(e); presses_add(-1); }
static void presses_plus(lv_event_t *e) { LV_UNUSED(e); presses_add(1); }

static void value_add(int d)
{
    rstep_t *s = step();
    int lo, hi;
    const char *unit;
    ha_op_range(ha_find(s->entity), &lo, &hi, &unit);
    int v = s->value + d * (*unit == ' ' ? 5 : 1);   /* 5 % steps, 1 degree steps */
    s->value = v < lo ? lo : v > hi ? hi : v;
    ui_refresh();
}
static void value_minus(lv_event_t *e) { LV_UNUSED(e); value_add(-1); }
static void value_plus(lv_event_t *e) { LV_UNUSED(e); value_add(1); }

static lv_obj_t *wait_value, *wait_slider;

static void wait_text_set(void)
{
    char w[16];
    lv_label_set_text(wait_value, routine_wait_text(step()->wait, w, sizeof(w)));
}

static void wait_slider_cb(lv_event_t *e)
{
    step()->wait = lv_slider_get_value(lv_event_get_target_obj(e));
    wait_text_set();
}

/* - / + move half a second; the slider follows */
static void wait_add(int d)
{
    int v = step()->wait + d;
    step()->wait = v < 0 ? 0 : v > RSTEP_WAIT_MAX ? RSTEP_WAIT_MAX : v;
    lv_slider_set_value(wait_slider, step()->wait, LV_ANIM_OFF);
    wait_text_set();
}
static void wait_minus(lv_event_t *e) { LV_UNUSED(e); wait_add(-1); }
static void wait_plus(lv_event_t *e) { LV_UNUSED(e); wait_add(1); }

static void move_cb(lv_event_t *e)
{
    int to = sp + ARG_INT(e);
    if (to < 0 || to >= draft.n_steps) return;
    rstep_t t = draft.steps[sp];
    draft.steps[sp] = draft.steps[to];
    draft.steps[to] = t;
    ui_replace(&page_routine_step, to);   /* follow the step to its new place */
}

static void step_delete_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    memmove(&draft.steps[sp], &draft.steps[sp + 1], (draft.n_steps - sp - 1) * sizeof(rstep_t));
    draft.n_steps--;
    ui_back();
}

static void picker_row(lv_obj_t *col, const char *icon, const char *label, const char *value, int which)
{
    lv_obj_t *r = w_list_row(col, icon, label, value, NULL);
    w_on_click(r, pick_cb, which);
}

static void step_build(lv_obj_t *root, int idx)
{
    sp = idx;
    rstep_t *s = step();
    char title[24], buf[48];
    snprintf(title, sizeof(title), "Step %d", sp + 1);
    w_header(root, draft.name, title, ui_back_cb);
    lv_obj_t *col = w_col(root, 0);
    lv_obj_set_style_pad_hor(col, PAD, 0);

    switch (s->kind) {
    case RS_DEVICE: {
        const device_t *d = &g_model.devices[s->target];
        picker_row(col, dev_kind_icon(d->kind), "Device", d->name, 0);
        picker_row(col, ICON_REMOTE, "Button", fn_name((fn_t)s->fn), 1);
        lv_obj_t *r = w_list_row(col, ICON_NUMBER, "Presses", NULL, NULL);
        lv_obj_set_clickable(r, false);
        snprintf(buf, sizeof(buf), "%d", s->repeat);
        w_row_stepper(r, buf, presses_minus, presses_plus);
        break;
    }
    case RS_HA: {
        int en = ha_find(s->entity);
        picker_row(col, en >= 0 ? (ha_get(en)->icon ? ha_get(en)->icon : ha_domain_icon(ha_get(en)->domain)) : ICON_HOME,
                   "Entity", en >= 0 ? ha_get(en)->name : s->entity, 2);
        if (en < 0) {
            w_label(col, F_LABEL, T->warning, "Not in Home Assistant right now. Sign in, or pick another.");
            break;
        }
        picker_row(col, ICON_SLIDERS, "Action", ha_op_name(s->op, en), 3);
        if (s->op == HAOP_SET) {
            int lo, hi;
            const char *unit;
            ha_op_range(en, &lo, &hi, &unit);
            lv_obj_t *r = w_list_row(col, ICON_NUMBER, "Value", NULL, NULL);
            lv_obj_set_clickable(r, false);
            snprintf(buf, sizeof(buf), "%d%s", s->value, unit);
            w_row_stepper(r, buf, value_minus, value_plus);
        }
        break;
    }
    case RS_ACTIVITY_START:
        picker_row(col, ICON_PLAY, "Activity",
                   s->target >= 0 && s->target < g_model.n_activities ? g_model.activities[s->target].name : "None", 4);
        w_label(col, F_CAPTION, T->text2, "Its own start sequence runs first, then the wait.");
        break;
    default:
        w_label(col, F_LABEL, T->text, "Ends the running activity and switches its devices off.");
        break;
    }

    /* wait after the step: title and value, then a slider in half seconds,
     * laid out like the stepped sliders in Settings */
    lv_obj_t *wb = w_col(col, 8);
    lv_obj_set_style_pad_top(wb, 12, 0);
    lv_obj_t *head = w_row(wb, 10);
    w_icon(head, ICON_TIMER, 18, T->text2);
    w_label(head, F_BODY, T->text, "Wait after");
    w_spacer(head);
    wait_value = w_row_stepper(head, "", wait_minus, wait_plus);
    lv_obj_set_style_text_color(wait_value, T->accent, 0);
    wait_text_set();
    lv_obj_t *sl = wait_slider = w_slider(wb, s->wait, 0, RSTEP_WAIT_MAX);
    lv_obj_set_style_margin_left(sl, 28, 0);   /* lines up with the title; the right inset leaves room for the knob */
    lv_obj_set_style_margin_right(sl, 9, 0);
    lv_obj_set_style_margin_top(sl, 6, 0);
    lv_obj_add_event_cb(sl, wait_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *ends = w_row(wb, 0);
    lv_obj_set_style_pad_left(ends, 26, 0);
    w_label(ends, F_CAPTION, T->text2, "0 s");
    w_spacer(ends);
    w_label(ends, F_CAPTION, T->text2, "60 s");
    bool last = sp == draft.n_steps - 1;
    lv_obj_t *wn = w_label(col, F_CAPTION, last ? T->warning : T->text2,
                           last ? "This is the last step, so its wait is skipped."
                                : "Up to 60 s, in half seconds. Long enough for a TV to start: about 5 s.");
    lv_obj_set_width(wn, lv_pct(100));
    lv_obj_set_style_pad_top(wn, 6, 0);

    lv_obj_t *mv = w_row(col, 10);
    lv_obj_set_style_pad_top(mv, 16, 0);
    lv_obj_t *up = w_button(mv, "Move up", ICON_CHEV_UP, sp > 0 ? BTN_DEFAULT : BTN_DISABLED);
    lv_obj_set_flex_grow(up, 1);
    if (sp > 0) lv_obj_add_event_cb(up, move_cb, LV_EVENT_CLICKED, ARG(-1));
    lv_obj_t *dn = w_button(mv, "Move down", ICON_CHEV_DOWN, !last ? BTN_DEFAULT : BTN_DISABLED);
    lv_obj_set_flex_grow(dn, 1);
    if (!last) lv_obj_add_event_cb(dn, move_cb, LV_EVENT_CLICKED, ARG(1));

    lv_obj_t *del = w_button(col, "Delete step", ICON_TRASH, BTN_OUTLINE);
    lv_obj_set_width(del, lv_pct(100));
    lv_obj_set_style_margin_top(del, 10, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(del, 0), T->warning, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(del, 1), T->warning, 0);
    lv_obj_add_event_cb(del, step_delete_cb, LV_EVENT_CLICKED, NULL);
}

const page_t page_routine_step = {"Routine step", -1, step_build, NULL, NULL};
