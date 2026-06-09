// calculator.c
// VesperaOS - operating system for the x86_64 architecture
//
// Copyright (c) 2026 Linus Genz <linuslinuxgenz@gmail.com>
//
// Created by Linus Genz on 07.06.26.
//
// This file is part of VesperaOS.
//
// VesperaOS is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// VesperaOS is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with VesperaOS. If not, see <https://www.gnu.org/licenses/>.

/**
 * calculator.c — VesperaOS Calculator
 *
 * Copyright (c) 2026 VesperaOS project
 *
 * Style:   Adwaita / GNOME HIG  (dark palette)
 * Build:   cc -std=c99 -Wall -o calculator calculator.c -lstella -lm
 * Encoding: UTF-8 (symbol literals use explicit \xNN escape sequences)
 */

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stella.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Colour palette  (GNOME HIG — 0xRRGGBB)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define C_WIN_BG     0x241f31u   /* Dark 4   – window / display background  */
#define C_BTN_NUM    0x3d3846u   /* Dark 3   – digit / dot / ± buttons       */
#define C_BTN_NUM_H  0x5e5c64u   /* Dark 2   – digit hover                   */
#define C_BTN_UTIL   0x3d3846u   /* Dark 3   – utility: %, 1/x, x², √       */
#define C_BTN_UTIL_H 0x5e5c64u   /* Dark 2   – utility hover                 */
#define C_BTN_DEST_H 0xa51d2du   /* Red 5    – CE / C / ← hover (danger hint)*/
#define C_BTN_OP     0x5e5c64u   /* Dark 2   – operators (slightly elevated) */
#define C_BTN_OP_H   0x1c71d8u   /* Blue 4   – operator hover               */
#define C_BTN_EQ     0x3584e4u   /* Blue 3   – equals accent                */
#define C_BTN_EQ_H   0x62a0eau   /* Blue 2   – equals hover                 */
#define C_TEXT       0xffffffu   /* Light 1  – primary text                 */
#define C_TEXT_DIM   0x9a9996u   /* Light 5  – expression / secondary text  */
#define C_SEPARATOR  0x3d3846u   /* Dark 3   – display / grid border        */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Layout constants
 *
 *  Grid math (verified):
 *    Horizontal:  4 × BTN_W  +  3 × BTN_GAP  =  4×81  + 3×6  = 342 px
 *                 WIN_W  -  2 × GRID_PAD_H    =  360   - 18   = 342 px  ✓
 *    Vertical:    6 × BTN_H  +  5 × BTN_GAP  =  6×72  + 5×6  = 462 px
 *                 (WIN_H - DISP_H) - 2×GRID_PAD_V = 480 - 18 = 462 px  ✓
 * ═══════════════════════════════════════════════════════════════════════════ */

#define WIN_W       360
#define WIN_H       580
#define DISP_H      100   /* display panel height         */
#define BTN_W        81   /* button width                 */
#define BTN_H        72   /* button height                */
#define BTN_GAP       6   /* gap between buttons          */
#define GRID_PAD_H    9   /* grid horizontal padding      */
#define GRID_PAD_V    9   /* grid vertical padding        */

/* ═══════════════════════════════════════════════════════════════════════════
 *  UTF-8 operator / symbol literals
 * ═══════════════════════════════════════════════════════════════════════════ */

#define SYM_MINUS  "\xe2\x88\x92"   /* U+2212  −  (proper minus sign)   */
#define SYM_TIMES  "\xc3\x97"       /* U+00D7  ×  (multiplication sign) */
#define SYM_DIV    "\xc3\xb7"       /* U+00F7  ÷  (division sign)       */
#define SYM_SQRT   "\xe2\x88\x9a"   /* U+221A  √  (square root)         */
#define SYM_SUP2   "\xc2\xb2"       /* U+00B2  ²  (superscript two)     */
#define SYM_PM     "\xc2\xb1"       /* U+00B1  ±  (plus-minus sign)     */
#define SYM_BACK   "\xe2\x86\x90"   /* U+2190  ←  (backspace arrow)     */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Calculator state
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum { OP_NONE, OP_ADD, OP_SUB, OP_MUL, OP_DIV } op_t;

static double s_accum    = 0.0;        /* left-hand operand / running total  */
static op_t   s_pending  = OP_NONE;    /* operator waiting for right operand  */
static bool   s_newinput = true;       /* next digit starts a fresh entry    */
static bool   s_hasdot   = false;      /* decimal point already typed        */
static bool   s_afteq    = false;      /* = was the last evaluated action    */
static bool   s_error    = false;      /* error state (÷0, √neg, …)         */

static char   s_main[64] = "0";        /* primary display string             */
static char   s_expr[96] = "";         /* expression / history line          */

static stella_widget_t s_lbl_expr;     /* secondary (expression) label       */
static stella_widget_t s_lbl_main;     /* main (number) label                */

/* ─── Small helpers ────────────────────────────────────────────────────── */

/** Format a double for display: integer when exact, scientific for huge
 *  values, %g otherwise. */
static void dbl_fmt(double v, char *out, size_t n) {
    if (v != v)                          { strncpy(out, "NaN",   n); return; }
    if (v >  9.999e14 || v < -9.999e14)  { snprintf(out, n, "%.4e",  v); return; }
    if (v == (long long)v)               { snprintf(out, n, "%lld", (long long)v); }
    else                                 { snprintf(out, n, "%.10g", v); }
}

static void refresh(void) {
    stella_label_update(s_lbl_main, s_error ? "Error" : s_main);
    stella_label_update(s_lbl_expr, s_expr);
}

static const char *op_sym(op_t op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return SYM_MINUS;
        case OP_MUL: return SYM_TIMES;
        case OP_DIV: return SYM_DIV;
        default:     return "";
    }
}

/** Apply a binary operator; sets s_error on division-by-zero. */
static double apply_op(double a, op_t op, double b) {
    switch (op) {
        case OP_ADD: return a + b;
        case OP_SUB: return a - b;
        case OP_MUL: return a * b;
        case OP_DIV:
            if (b == 0.0) { s_error = true; return 0.0; }
            return a / b;
        default: return b;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Button identifiers  (digits 0-9 must be first and in order — see handler)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    ID_0, ID_1, ID_2, ID_3, ID_4,
    ID_5, ID_6, ID_7, ID_8, ID_9,  /* must equal their numeric values */
    ID_DOT, ID_PM,
    ID_ADD, ID_SUB, ID_MUL, ID_DIV,
    ID_EQ,
    ID_C, ID_CE, ID_BACK,
    ID_PCT, ID_RECIP, ID_SQ, ID_SQRT,
    BTN_COUNT                        /* must be last — used for array size */
} btn_id_t;

typedef struct { btn_id_t id; } btn_ctx_t;
static btn_ctx_t s_ctx[BTN_COUNT];

/* ═══════════════════════════════════════════════════════════════════════════
 *  Central click handler
 * ═══════════════════════════════════════════════════════════════════════════ */

static void on_click(stella_widget_t w, void *ud) {
    (void)w;
    btn_id_t id = ((btn_ctx_t *)ud)->id;

    /* In error state only C (all-clear) is accepted */
    if (s_error && id != ID_C) return;

    /* ── Digits 0-9 ──────────────────────────────────────────────────── */
    if (id <= ID_9) {
        int d = (int)id;   /* enum value equals digit value by design */
        if (s_newinput || s_afteq) {
            snprintf(s_main, sizeof(s_main), "%d", d);
            s_newinput = s_hasdot = s_afteq = false;
        } else if (strlen(s_main) < 15) {
            size_t l   = strlen(s_main);
            s_main[l]     = (char)('0' + d);
            s_main[l + 1] = '\0';
        }
        refresh();
        return;
    }

    /* ── Decimal point ───────────────────────────────────────────────── */
    if (id == ID_DOT) {
        if (s_newinput || s_afteq) {
            strncpy(s_main, "0.", sizeof(s_main));
            s_newinput = s_afteq = false;
            s_hasdot = true;
        } else if (!s_hasdot) {
            strncat(s_main, ".", sizeof(s_main) - strlen(s_main) - 1);
            s_hasdot = true;
        }
        refresh();
        return;
    }

    /* ── All-clear ───────────────────────────────────────────────────── */
    if (id == ID_C) {
        s_accum = 0.0;  s_pending = OP_NONE;
        s_newinput = true;  s_hasdot = s_afteq = s_error = false;
        strncpy(s_main, "0", sizeof(s_main));
        s_expr[0] = '\0';
        refresh();
        return;
    }

    /* ── Clear entry ─────────────────────────────────────────────────── */
    if (id == ID_CE) {
        s_newinput = true;  s_hasdot = false;
        strncpy(s_main, "0", sizeof(s_main));
        refresh();
        return;
    }

    /* ── Backspace ───────────────────────────────────────────────────── */
    if (id == ID_BACK) {
        if (!s_newinput) {
            size_t l = strlen(s_main);
            if (l > 1) {
                if (s_main[l - 1] == '.') s_hasdot = false;
                s_main[l - 1] = '\0';
            } else {
                strncpy(s_main, "0", sizeof(s_main));
                s_newinput = true;  s_hasdot = false;
            }
        }
        refresh();
        return;
    }

    /* ── Sign toggle ─────────────────────────────────────────────────── */
    if (id == ID_PM) {
        dbl_fmt(-atof(s_main), s_main, sizeof(s_main));
        refresh();
        return;
    }

    /* ── Unary functions ─────────────────────────────────────────────── */
    if (id == ID_PCT || id == ID_RECIP || id == ID_SQ || id == ID_SQRT) {
        double v = atof(s_main);
        double r = v;
        switch (id) {
            case ID_PCT:
                r = (s_pending != OP_NONE) ? s_accum * (v / 100.0) : v / 100.0;
                break;
            case ID_RECIP:
                if (v == 0.0) { s_error = true; refresh(); return; }
                r = 1.0 / v;
                break;
            case ID_SQ:
                r = v * v;
                break;
            case ID_SQRT:
                if (v < 0.0) { s_error = true; refresh(); return; }
                r = sqrt(v);
                break;
            default: break;
        }
        dbl_fmt(r, s_main, sizeof(s_main));
        s_newinput = true;
        refresh();
        return;
    }

    /* ── Binary operators (+, −, ×, ÷) ──────────────────────────────── */
    op_t nop = OP_NONE;
    if      (id == ID_ADD) nop = OP_ADD;
    else if (id == ID_SUB) nop = OP_SUB;
    else if (id == ID_MUL) nop = OP_MUL;
    else if (id == ID_DIV) nop = OP_DIV;

    if (nop != OP_NONE) {
        double cur = atof(s_main);
        /* Chain: fold the pending op if user hasn't started a new number */
        if (!s_newinput && s_pending != OP_NONE) {
            s_accum = apply_op(s_accum, s_pending, cur);
            if (s_error) { refresh(); return; }
        } else if (!s_newinput || s_afteq) {
            /* First operator press, or continuing after = */
            s_accum = cur;
        }
        s_pending  = nop;
        s_newinput = true;  s_hasdot = s_afteq = false;

        char tmp[32];
        dbl_fmt(s_accum, tmp, sizeof(tmp));
        snprintf(s_expr, sizeof(s_expr), "%s %s", tmp, op_sym(nop));
        dbl_fmt(s_accum, s_main, sizeof(s_main));
        refresh();
        return;
    }

    /* ── Equals ──────────────────────────────────────────────────────── */
    if (id == ID_EQ && s_pending != OP_NONE) {
        double rhs = atof(s_main);
        char   la[32], lb[32];
        dbl_fmt(s_accum, la, sizeof(la));
        dbl_fmt(rhs,     lb, sizeof(lb));

        double res = apply_op(s_accum, s_pending, rhs);
        if (!s_error) {
            snprintf(s_expr, sizeof(s_expr), "%s %s %s =",
                     la, op_sym(s_pending), lb);
            dbl_fmt(res, s_main, sizeof(s_main));
            s_accum = res;
        }
        s_pending  = OP_NONE;
        s_newinput = true;  s_hasdot = false;  s_afteq = true;
        refresh();
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Button grid definition  (6 rows × 4 columns)
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const char *label;
    btn_id_t    id;
    uint32_t    bg;
    uint32_t    bg_hover;
} btn_def_t;

static const btn_def_t s_layout[6][4] = {
    /* row 0 — clear / utility */
    { { "%",              ID_PCT,  C_BTN_UTIL, C_BTN_UTIL_H },
      { "CE",             ID_CE,   C_BTN_UTIL, C_BTN_DEST_H },
      { "C",              ID_C,    C_BTN_UTIL, C_BTN_DEST_H },
      { SYM_BACK,         ID_BACK, C_BTN_UTIL, C_BTN_DEST_H } },
    /* row 1 — scientific */
    { { "1/x",            ID_RECIP, C_BTN_UTIL, C_BTN_UTIL_H },
      { "x" SYM_SUP2,     ID_SQ,    C_BTN_UTIL, C_BTN_UTIL_H },
      { SYM_SQRT "x",     ID_SQRT,  C_BTN_UTIL, C_BTN_UTIL_H },
      { SYM_DIV,          ID_DIV,   C_BTN_OP,   C_BTN_OP_H   } },
    /* row 2 */
    { { "7", ID_7, C_BTN_NUM, C_BTN_NUM_H },
      { "8", ID_8, C_BTN_NUM, C_BTN_NUM_H },
      { "9", ID_9, C_BTN_NUM, C_BTN_NUM_H },
      { SYM_TIMES, ID_MUL, C_BTN_OP, C_BTN_OP_H } },
    /* row 3 */
    { { "4", ID_4, C_BTN_NUM, C_BTN_NUM_H },
      { "5", ID_5, C_BTN_NUM, C_BTN_NUM_H },
      { "6", ID_6, C_BTN_NUM, C_BTN_NUM_H },
      { SYM_MINUS, ID_SUB, C_BTN_OP, C_BTN_OP_H } },
    /* row 4 */
    { { "1", ID_1, C_BTN_NUM, C_BTN_NUM_H },
      { "2", ID_2, C_BTN_NUM, C_BTN_NUM_H },
      { "3", ID_3, C_BTN_NUM, C_BTN_NUM_H },
      { "+", ID_ADD, C_BTN_OP, C_BTN_OP_H } },
    /* row 5 */
    { { SYM_PM, ID_PM,  C_BTN_NUM, C_BTN_NUM_H },
      { "0",    ID_0,   C_BTN_NUM, C_BTN_NUM_H },
      { ".",    ID_DOT, C_BTN_NUM, C_BTN_NUM_H },
      { "=",    ID_EQ,  C_BTN_EQ,  C_BTN_EQ_H  } },
};

/* ═══════════════════════════════════════════════════════════════════════════
 *  UI builder
 * ═══════════════════════════════════════════════════════════════════════════ */

static void make_btn(stella_widget_t parent,
                     const btn_def_t *d, btn_ctx_t *ctx) {
    stella_widget_t b = stella_button_create(parent, d->label, BTN_W, BTN_H);
    stella_widget_set_bg      (b, stella_hex(d->bg),       STELLA_OPA_COVER);
    stella_widget_set_hover_bg(b, stella_hex(d->bg_hover), STELLA_OPA_COVER);
    stella_widget_no_border   (b);
    stella_widget_set_radius  (b, 12);
    stella_text_set_font      (b, STELLA_FONT_20_MATH);
    stella_text_set_color     (b, stella_hex(C_TEXT));
    ctx->id = d->id;
    stella_widget_on_click(b, on_click, ctx);
}

static void build_ui(stella_window_t *win) {
    stella_widget_t scr = stella_window_get_screen(win);
    stella_widget_set_bg   (scr, stella_hex(C_WIN_BG), STELLA_OPA_COVER);
    stella_widget_no_border(scr);
    stella_widget_no_scroll(scr);

    /* ── Root flex column ───────────────────────────────────────────── */
    stella_widget_t root = stella_container_create(scr);
    stella_widget_set_size   (root, STELLA_SIZE_FULL, STELLA_SIZE_FULL);
    stella_widget_flex_col   (root, STELLA_FLEX_START, STELLA_FLEX_CENTER,
                              STELLA_FLEX_START);
    stella_widget_set_bg_transp(root);
    stella_widget_no_border  (root);
    stella_widget_no_scroll  (root);
    stella_widget_set_pad_all(root, 0);

    /* ── Display panel ──────────────────────────────────────────────── */
    stella_widget_t disp = stella_container_create(root);
    stella_widget_set_size       (disp, STELLA_SIZE_FULL, DISP_H);
    stella_widget_set_bg         (disp, stella_hex(C_WIN_BG), STELLA_OPA_COVER);
    stella_widget_set_border_bottom(disp, stella_hex(C_SEPARATOR), 1);
    stella_widget_no_scroll      (disp);
    stella_widget_set_pad_hor    (disp, 18);
    stella_widget_set_pad_top    (disp, 10);
    stella_widget_flex_col       (disp, STELLA_FLEX_END, STELLA_FLEX_END,
                                  STELLA_FLEX_END);

    /* Secondary label — expression history, right-aligned, dimmed */
    s_lbl_expr = stella_label_create(disp, "");
    stella_widget_set_width(s_lbl_expr, STELLA_SIZE_FULL);
    stella_text_set_font   (s_lbl_expr, STELLA_FONT_14);
    stella_text_set_color  (s_lbl_expr, stella_hex(C_TEXT_DIM));
    stella_text_set_align  (s_lbl_expr, STELLA_TEXT_ALIGN_RIGHT);

    /* Primary label — current number, large, right-aligned */
    s_lbl_main = stella_label_create(disp, "0");
    stella_widget_set_width(s_lbl_main, STELLA_SIZE_FULL);
    stella_text_set_font   (s_lbl_main, STELLA_FONT_24);
    stella_text_set_color  (s_lbl_main, stella_hex(C_TEXT));
    stella_text_set_align  (s_lbl_main, STELLA_TEXT_ALIGN_RIGHT);

    /* ── Button grid ────────────────────────────────────────────────── */
    stella_widget_t grid = stella_container_create(root);
    stella_widget_set_size   (grid, STELLA_SIZE_FULL, STELLA_SIZE_FULL);
    stella_widget_set_bg     (grid, stella_hex(C_WIN_BG), STELLA_OPA_COVER);
    stella_widget_no_border  (grid);
    stella_widget_no_scroll  (grid);
    stella_widget_set_pad_hor(grid, GRID_PAD_H);
    stella_widget_set_pad_ver(grid, GRID_PAD_V);
    stella_widget_set_pad_row(grid, BTN_GAP);
    stella_widget_flex_col   (grid, STELLA_FLEX_START,
                              STELLA_FLEX_CENTER, STELLA_FLEX_START);

    /* Build 6 rows */
    int idx = 0;
    for (int r = 0; r < 6; r++) {
        stella_widget_t row = stella_container_create(grid);
        stella_widget_set_size   (row, STELLA_SIZE_FULL, STELLA_SIZE_CONTENT);
        stella_widget_set_bg_transp(row);
        stella_widget_no_border  (row);
        stella_widget_no_scroll  (row);
        stella_widget_set_pad_all(row, 0);
        stella_widget_set_pad_col(row, BTN_GAP);
        stella_widget_flex_row   (row, STELLA_FLEX_CENTER,
                                  STELLA_FLEX_CENTER, STELLA_FLEX_CENTER);

        for (int c = 0; c < 4; c++)
            make_btn(row, &s_layout[r][c], &s_ctx[idx++]);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Entry point
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    if (stella_init() != 0) return 1;

    const stella_config_t cfg = {
        .width  = WIN_W,
        .height = WIN_H,
        .flags  = 0,
        .title  = "Calculator",
    };

    stella_window_t *win = stella_window_create(&cfg);
    if (!win) return 1;

    build_ui(win);

    /* Main loop — ~5 ms tick matches LVGL's internal timer resolution */
    while (!stella_window_should_close(win)) {
        stella_process_events(win);
        stella_tick(5);
        /* platform sleep(5ms) goes here */
    }

    stella_window_destroy(win);
    return 0;
}