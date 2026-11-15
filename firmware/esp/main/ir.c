/*
 * IR (hal.h): sending on the RMT with the carrier, and learning from the
 * IRM-H638T receiver.
 *
 * Out: a frame is built as mark/space pairs in microseconds and sent; while
 * the key is held it repeats at the protocol's own rate (NEC: the short
 * repeat code every 108 ms; Sony, RC5, RC6, Samsung, Kaseikyo: the whole
 * frame). stop never cuts a frame and keeps Sony's minimum of 3. Codes are
 * as Flipper-IRDB writes them (app/ir_library.c).
 * In: the receiver is powered from GPIO1 only while learning; its output is
 * demodulated and active low. NEC, NECext, Samsung32, Samsung36 and SIRC are
 * decoded; a frame that doesn't match one exactly (its length too) is turned
 * down rather than stored as something it isn't.
 *
 * Not on the board yet, so checked against the protocols' specs only.
 * IR_RAW codes (no timings stored) aren't sent.
 */
#include "board.h"

#include <string.h>
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "model.h"

static const char *TAG = "ir";

#define MAX_SYMBOLS 64
#define RX_SYMBOLS 128

static rmt_channel_handle_t tx, rx;
static rmt_encoder_handle_t copy;
static esp_timer_handle_t repeat_timer;

/* ---- building a frame ---- */

/* A frame: marks and spaces in microseconds, merged where two of the same
 * level meet (Manchester codes), then packed into RMT symbols, whose halves
 * hold at most 32767 us each (long gaps are split) */
#define MAX_SEGS 120
typedef struct {
    bool mark[MAX_SEGS];
    uint32_t us[MAX_SEGS];
    int n;
    rmt_symbol_word_t s[MAX_SYMBOLS];
    int ns;
} frame_t;

static frame_t frame, repeat;    /* the first frame, and what repeats (NEC: the repeat code) */

static void put(frame_t *f, bool mark, uint32_t us)
{
    if (!us) return;
    if (f->n && f->mark[f->n - 1] == mark) f->us[f->n - 1] += us;
    else if (f->n < MAX_SEGS) f->mark[f->n] = mark, f->us[f->n++] = us;
}

static void finish(frame_t *f, uint32_t gap_us)
{
    put(f, false, gap_us);
    int half = 0;
    f->ns = 0;
    for (int i = 0; i < f->n; i++) {
        for (uint32_t left = f->us[i]; left && f->ns < MAX_SYMBOLS;) {
            uint32_t d = left > 32000 ? 32000 : left;
            left -= d;
            rmt_symbol_word_t *w = &f->s[f->ns];
            if (!half) w->level0 = f->mark[i], w->duration0 = d;
            else w->level1 = f->mark[i], w->duration1 = d, f->ns++;
            half = !half;
        }
    }
    if (half && f->ns < MAX_SYMBOLS) f->s[f->ns].level1 = 0, f->s[f->ns].duration1 = 1, f->ns++;
}

static void pulse_bits(frame_t *f, uint32_t v, int bits, uint32_t mark, uint32_t zero, uint32_t one)
{
    for (int i = 0; i < bits; i++) {   /* LSB first */
        put(f, true, mark);
        put(f, false, (v >> i) & 1 ? one : zero);
    }
}

static void pulse_bits_msb(frame_t *f, uint32_t v, int bits, uint32_t mark, uint32_t zero, uint32_t one)
{
    for (int i = bits - 1; i >= 0; i--) {
        put(f, true, mark);
        put(f, false, (v >> i) & 1 ? one : zero);
    }
}

static void width_bits(frame_t *f, uint32_t v, int bits, uint32_t space, uint32_t zero, uint32_t one)
{
    for (int i = 0; i < bits; i++) {   /* Sony: the mark's width is the bit, LSB first */
        put(f, true, (v >> i) & 1 ? one : zero);
        put(f, false, space);
    }
}

static void manchester(frame_t *f, uint32_t v, int bits, uint32_t half, bool one_is_mark_first)
{
    for (int i = bits - 1; i >= 0; i--) {   /* MSB first */
        bool b = (v >> i) & 1;
        bool first = one_is_mark_first ? b : !b;
        put(f, first, half);
        put(f, !first, half);
    }
}

static struct {
    uint8_t proto;
    uint32_t period_ms, carrier_hz;
    int min_frames, sent;
    bool stop_asked, rc_toggle;
} cur;

/* Builds frame (and repeat); false if the protocol can't be sent */
static bool build(uint8_t proto, uint32_t a, uint32_t b)
{
    memset(&frame, 0, sizeof(frame));
    memset(&repeat, 0, sizeof(repeat));
    cur.carrier_hz = 38000;
    cur.min_frames = 1;
    switch (proto) {
    case IR_NEC:
    case IR_NECEXT: {
        uint32_t addr = proto == IR_NEC ? (a & 0xFF) | ((~a & 0xFF) << 8) : a & 0xFFFF;
        uint32_t cmd = proto == IR_NEC ? (b & 0xFF) | ((~b & 0xFF) << 8) : b & 0xFFFF;
        put(&frame, true, 9000), put(&frame, false, 4500);
        pulse_bits(&frame, addr | (cmd << 16), 32, 560, 560, 1690);
        put(&frame, true, 560);
        finish(&frame, 40000);
        put(&repeat, true, 9000), put(&repeat, false, 2250), put(&repeat, true, 560);
        finish(&repeat, 96000);
        cur.period_ms = 108;
        return true;
    }
    case IR_SAMSUNG32:
        put(&frame, true, 4500), put(&frame, false, 4500);
        pulse_bits(&frame, (a & 0xFF) | ((a & 0xFF) << 8) | ((b & 0xFF) << 16) | ((~b & 0xFF) << 24), 32, 560, 560,
                   1690);
        put(&frame, true, 560);
        finish(&frame, 46000);
        repeat = frame;
        cur.period_ms = 108;
        return true;
    case IR_SAMSUNG36:   /* 16 bits, a header-long gap, 20 bits */
        put(&frame, true, 4500), put(&frame, false, 4500);
        pulse_bits_msb(&frame, a & 0xFFFF, 16, 560, 560, 1690);
        put(&frame, true, 560), put(&frame, false, 4500);
        pulse_bits_msb(&frame, b & 0xFFFFF, 20, 560, 560, 1690);
        put(&frame, true, 560);
        finish(&frame, 46000);
        repeat = frame;
        cur.period_ms = 108;
        return true;
    case IR_SIRC:
    case IR_SIRC15:
    case IR_SIRC20: {
        int abits = proto == IR_SIRC ? 5 : proto == IR_SIRC15 ? 8 : 13;
        put(&frame, true, 2400), put(&frame, false, 600);
        width_bits(&frame, (b & 0x7F) | ((a & ((1u << abits) - 1)) << 7), 7 + abits, 600, 600, 1200);
        finish(&frame, 10000);
        repeat = frame;
        cur.period_ms = 45;
        cur.carrier_hz = 40000;
        cur.min_frames = 3;
        return true;
    }
    case IR_RC5: {
        /* start bits 1 and !cmd bit 6, toggle, 5 address, 6 command; a 1 is space then mark */
        uint32_t v = (1u << 13) | ((~b >> 6 & 1) << 12) | ((uint32_t)cur.rc_toggle << 11) | ((a & 0x1F) << 6) | (b & 0x3F);
        manchester(&frame, v, 14, 889, false);
        finish(&frame, 90000);
        repeat = frame;
        cur.period_ms = 114;
        cur.carrier_hz = 36000;
        return true;
    }
    case IR_RC6:
        /* leader, start 1, mode 000, toggle (double width), 8 address, 8 command; a 1 is mark then space */
        put(&frame, true, 2666), put(&frame, false, 889);
        manchester(&frame, 0x8, 4, 444, true);
        put(&frame, cur.rc_toggle, 889), put(&frame, !cur.rc_toggle, 889);
        manchester(&frame, ((a & 0xFF) << 8) | (b & 0xFF), 16, 444, true);
        finish(&frame, 83000);
        repeat = frame;
        cur.period_ms = 107;
        cur.carrier_hz = 36000;
        return true;
    case IR_KASEIKYO: {
        /* 16-bit vendor, its parity nibble, 12-bit address, 8-bit command, a parity byte */
        uint32_t vendor = a & 0xFFFF, addr = (a >> 16) & 0xFFF, cmd = b & 0xFF;
        uint8_t vp = (vendor ^ (vendor >> 8)) & 0xFF;
        vp = (vp ^ (vp >> 4)) & 0x0F;
        uint8_t d0 = vp | ((addr & 0x0F) << 4), d1 = addr >> 4, d2 = cmd, d3 = d0 ^ d1 ^ d2;
        put(&frame, true, 3456), put(&frame, false, 1728);
        pulse_bits(&frame, vendor, 16, 432, 432, 1296);
        pulse_bits(&frame, d0 | (d1 << 8) | (d2 << 16) | ((uint32_t)d3 << 24), 32, 432, 432, 1296);
        put(&frame, true, 432);
        finish(&frame, 74000);
        repeat = frame;
        cur.period_ms = 130;
        return true;
    }
    default:
        return false;   /* IR_RAW: no timings kept */
    }
}

static void send(const frame_t *f)
{
    rmt_transmit_config_t t = {.loop_count = 0};
    rmt_transmit(tx, copy, f->s, f->ns * sizeof(rmt_symbol_word_t), &t);
}

static void repeat_cb(void *arg)
{
    if (cur.stop_asked && cur.sent >= cur.min_frames) {
        esp_timer_stop(repeat_timer);
        return;
    }
    send(&repeat);
    cur.sent++;
}

bool hal_ir_start(uint8_t protocol, uint32_t address, uint32_t command)
{
    if (!tx) return false;
    esp_timer_stop(repeat_timer);   /* one code at a time */
    rmt_tx_wait_all_done(tx, 200);
    cur.rc_toggle = !cur.rc_toggle;
    if (!build(protocol, address, command)) return false;
    rmt_carrier_config_t c = {.frequency_hz = cur.carrier_hz, .duty_cycle = 0.33f};
    rmt_apply_carrier(tx, &c);
    cur.proto = protocol;
    cur.sent = 1;
    cur.stop_asked = false;
    send(&frame);
    esp_timer_start_periodic(repeat_timer, cur.period_ms * 1000);
    return true;
}

void hal_ir_stop(void)
{
    cur.stop_asked = true;
    if (cur.sent >= cur.min_frames) esp_timer_stop(repeat_timer);
}

/* ---- learning ---- */

static rmt_symbol_word_t rx_buf[RX_SYMBOLS];
static QueueHandle_t rx_queue;
static bool rx_on;

static bool IRAM_ATTR rx_done(rmt_channel_handle_t ch, const rmt_rx_done_event_data_t *e, void *ctx)
{
    BaseType_t woken = pdFALSE;
    xQueueSendFromISR(rx_queue, e, &woken);
    return woken == pdTRUE;
}

static rmt_receive_config_t rx_cfg = {.signal_range_min_ns = 1000, .signal_range_max_ns = 12000000};

void hal_ir_rx_power(bool on)
{
    gpio_set_level(PIN_IR_RX_POWER, on);
    if (on == rx_on || !rx) return;
    rx_on = on;
    if (on) {
        rmt_enable(rx);
        rmt_receive(rx, rx_buf, sizeof(rx_buf), &rx_cfg);
    } else {
        rmt_disable(rx);
    }
}

static bool near(uint32_t us, uint32_t want) { return us > want * 7 / 10 && us < want * 13 / 10; }

/* The receiver's output is low during a mark: level 0 = mark */
static bool decode(const rmt_symbol_word_t *s, int n, uint8_t *proto, uint32_t *addr, uint32_t *cmd)
{
    if (n < 2) return false;
    uint32_t m0 = s[0].duration0, s0 = s[0].duration1;
    /* Samsung36 (soundbars): the 4.5 ms header, 16 bits, a 4.5 ms gap, 20 bits.
     * Read as Samsung32 it gave a wrong code (the gap taken for a bit). */
    if (near(m0, 4500) && near(s0, 4500) && n >= 38 && near(s[17].duration0, 560) && near(s[17].duration1, 4500)) {
        uint32_t a = 0, b = 0;
        for (int i = 0; i < 16; i++) a = a << 1 | (s[1 + i].duration1 > 1120);
        for (int i = 0; i < 20; i++) b = b << 1 | (s[18 + i].duration1 > 1120);
        *proto = IR_SAMSUNG36, *addr = a, *cmd = b;
        return true;
    }
    /* 32 bits and the stop mark, nothing more (the RX ends a frame on its gap) */
    if ((near(m0, 9000) || near(m0, 4500)) && near(s0, 4500) && n >= 33 && n <= 35) {
        uint32_t v = 0;
        for (int i = 0; i < 32; i++) v |= (uint32_t)(s[1 + i].duration1 > 1120) << i;
        uint8_t a0 = v, a1 = v >> 8, c0 = v >> 16, c1 = v >> 24;
        if (near(m0, 4500)) {   /* Samsung: the address twice */
            *proto = IR_SAMSUNG32, *addr = a0, *cmd = c0;
        } else if ((uint8_t)~a0 == a1 && (uint8_t)~c0 == c1) {
            *proto = IR_NEC, *addr = a0, *cmd = c0;
        } else {
            *proto = IR_NECEXT, *addr = v & 0xFFFF, *cmd = v >> 16;
        }
        return true;
    }
    if (near(m0, 2400) && n >= 13) {   /* Sony: 12, 15 or 20 bits */
        int bits = n - 1 >= 20 ? 20 : n - 1 >= 15 ? 15 : 12;
        uint32_t v = 0;
        for (int i = 0; i < bits; i++) v |= (uint32_t)(s[1 + i].duration0 > 900) << i;
        *cmd = v & 0x7F;
        *addr = v >> 7;
        *proto = bits == 12 ? IR_SIRC : bits == 15 ? IR_SIRC15 : IR_SIRC20;
        return true;
    }
    return false;
}

bool hal_ir_receive(uint8_t *protocol, uint32_t *address, uint32_t *command)
{
    rmt_rx_done_event_data_t e;
    if (!rx_on || !rx_queue || xQueueReceive(rx_queue, &e, 0) != pdTRUE) return false;
    bool ok = decode(e.received_symbols, e.num_symbols, protocol, address, command);
    rmt_receive(rx, rx_buf, sizeof(rx_buf), &rx_cfg);   /* listen for the next */
    if (!ok) ESP_LOGI(TAG, "%d symbols, not a protocol it knows", (int)e.num_symbols);
    return ok;
}

/* ---- setup ---- */

void ir_init(void)
{
    gpio_config_t p = {.pin_bit_mask = 1ULL << PIN_IR_RX_POWER, .mode = GPIO_MODE_OUTPUT};
    gpio_config(&p);
    gpio_set_level(PIN_IR_RX_POWER, 0);

    rmt_tx_channel_config_t tc = {
        .gpio_num = PIN_IR_TX,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };
    if (rmt_new_tx_channel(&tc, &tx) != ESP_OK) {
        ESP_LOGE(TAG, "no RMT channel to send on");
        tx = NULL;
        return;
    }
    rmt_copy_encoder_config_t cc = {};
    rmt_new_copy_encoder(&cc, &copy);
    rmt_enable(tx);
    esp_timer_create_args_t ta = {.callback = repeat_cb, .name = "ir"};
    esp_timer_create(&ta, &repeat_timer);

    rmt_rx_channel_config_t rc = {
        .gpio_num = PIN_IR_RX,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 1000000,
        .mem_block_symbols = 64,
    };
    if (rmt_new_rx_channel(&rc, &rx) == ESP_OK) {
        rx_queue = xQueueCreate(2, sizeof(rmt_rx_done_event_data_t));
        rmt_rx_event_callbacks_t cb = {.on_recv_done = rx_done};
        rmt_rx_register_event_callbacks(rx, &cb, NULL);
    } else {
        rx = NULL;
    }
}
