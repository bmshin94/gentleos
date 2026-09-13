/*
 * Copyright (c) 2026 luke8086
 * Distributed under the terms of GPL-2 License
 *
 * File: calc.c - Calculator app
 */

#include <gui.h>

enum {
    BUTTON_WIDTH = 24,
    BUTTON_HEIGHT = 24,
    BUTTON_COLS = 4,
    BUTTON_ROWS = 4,
    BUTTONS_COUNT = BUTTON_COLS * BUTTON_ROWS,

    DISPLAY_HEIGHT = BUTTON_HEIGHT,

    GRID_X = 1,
    GRID_Y = 1 + DISPLAY_HEIGHT + 1,
    GRID_WIDTH = GRID_WIDTH_SPACED(BUTTON_WIDTH, BUTTON_COLS),
    GRID_HEIGHT = GRID_HEIGHT_SPACED(BUTTON_HEIGHT, BUTTON_ROWS),

    WINDOW_WIDTH = GRID_X + GRID_WIDTH + 1,
    WINDOW_HEIGHT = GRID_Y + GRID_HEIGHT + 1,

    DISPLAY_X = GRID_X,
    DISPLAY_Y = 1,
    DISPLAY_WIDTH = GRID_WIDTH,
};

static window_st window;

static widget_st button_widgets[BUTTONS_COUNT];
static widget_st *pressed_button;

static grid_st grid;

static const char *button_labels[BUTTONS_COUNT] = {
    "7", "8", "9", "/",
    "4", "5", "6", "*",
    "1", "2", "3", "-",
    "0", "C", "=", "+"
};

static int32_t current_val;
static int32_t stored_val;
static int32_t last_operand;
static uint8_t current_op;
static uint8_t last_op;
static int new_number = 1;
static int error;

static void
exec_add(void)
{
    int32_t result;

    if (add32(&result, stored_val, current_val)) {
        error = 1;
        return;
    }

    current_val = result;
}

static void
exec_sub(void)
{
    int32_t result;

    if (sub32(&result, stored_val, current_val)) {
        error = 1;
        return;
    }

    current_val = result;
}

static void
exec_mul(void)
{
    int32_t result;

    if (mul32(&result, stored_val, current_val)) {
        error = 1;
        return;
    }

    current_val = result;
}

static void
exec_div(void)
{
    int32_t result;

    if (div32(&result, stored_val, current_val)) {
        error = 1;
        return;
    }

    current_val = result;
}

static void
exec_current_op(void)
{
    switch (current_op) {
    case '+': exec_add(); break;
    case '-': exec_sub(); break;
    case '*': exec_mul(); break;
    case '/': exec_div(); break;
    }

    current_op = 0;
    new_number = 1;
}

static void
update_display(void)
{
    static char buf[32];
    rect_st rect;
    font_st *font;
    int text_width, text_x, text_y;

    if (error) {
        snprintf(buf, sizeof(buf), "ERR");
    } else {
        snprintf(buf, sizeof(buf), "%ld", current_val);
    }

    gui_rect_init(&rect, DISPLAY_X, DISPLAY_Y, DISPLAY_WIDTH, DISPLAY_HEIGHT);

    gui_surface_draw_rect(&window.origin, &rect, gui_color_bg);

    font = &fonts[0];
    text_width = (uint16_t)strlen(buf) * font->size.width;
    text_x = rect.x + rect.width - text_width - 10;
    text_y = rect.y + (rect.height - font->size.height) / 2;
    gui_surface_draw_str(&window.origin, text_x, text_y, font,
        buf, gui_color_fg, gui_color_bg);

    gui_surface_mark_dirty(&window.origin, &rect);
}

static void
on_button_press(widget_st *widget)
{
    uint8_t op;
    int val;

    op = widget->label[0];

    if (error && op != 'C') {
        return;
    }

    if (op >= '0' && op <= '9') {
        val = op - '0';
        if (new_number) {
            current_val = val;
            new_number = 0;
        } else {
            (void)append32(&current_val, current_val, val);
        }
    } else if (op == 'C') {
        current_val = 0;
        stored_val = 0;
        current_op = 0;
        last_op = 0;
        last_operand = 0;
        new_number = 1;
        error = 0;
    } else if (op == '=') {
        if (current_op) {
            last_op = current_op;
            last_operand = current_val;
            exec_current_op();
        } else if (last_op) {
            stored_val = current_val;
            current_val = last_operand;
            current_op = last_op;
            exec_current_op();
        }
    } else if (op == '+' || op == '-' || op == '*' || op == '/') {
        if (current_op && !new_number) {
            exec_current_op();
        }
        stored_val = current_val;
        current_op = op;
        last_op = 0;
        new_number = 1;
    }

    update_display();
}

static widget_st *
button_for_char(int ch)
{
    int i;

    if (ch == 'c') {
        ch = 'C';
    }

    if (ch == '\n') {
        ch = '=';
    }

    for (i = 0; i < BUTTONS_COUNT; ++i) {
        if (button_widgets[i].label[0] == ch) {
            return &button_widgets[i];
        }
    }

    return NULL;
}

static void
on_key_up(uint8_t key_code, uint8_t key_mods)
{
    widget_st *prev_pressed_button = pressed_button;

    if (prev_pressed_button) {
        pressed_button = NULL;
        prev_pressed_button->active = 0;
        prev_pressed_button->draw(prev_pressed_button);
    }
}

static void
on_key_down(uint8_t key_code, uint8_t key_mods)
{
    widget_st *prev_pressed_button = pressed_button;
    widget_st *button = button_for_char(key_char_for_code(key_code, key_mods));

    if (!button) {
        return;
    }

    if (button == prev_pressed_button) {
        return;
    }

    if (prev_pressed_button) {
        pressed_button = 0;
        prev_pressed_button->active = 0;
        prev_pressed_button->draw(prev_pressed_button);
    }

    pressed_button = button;
    button->active = 1;
    button->draw(button);
    on_button_press(button);
}

static void
init_buttons(void)
{
    int row, col, idx;
    widget_st *button;

    grid.cell_width = BUTTON_WIDTH;
    grid.cell_height = BUTTON_HEIGHT;
    grid.cols = BUTTON_COLS;
    grid.rows = BUTTON_ROWS;
    grid.x = GRID_X;
    grid.y = GRID_Y;

    for (row = 0; row < BUTTON_ROWS; ++row) {
        for (col = 0; col < BUTTON_COLS; ++col) {
            idx = row * BUTTON_COLS + col;
            button = &button_widgets[idx];

            button->origin = &window.origin;
            gui_grid_cell_rect(&grid, col, row, &button->rect);
            button->hide_border = 1;
            button->label = button_labels[idx];
            button->draw = gui_button_draw;
        }
    }
}

static void
on_show(void)
{
    int i;

    gui_window_draw(&window, gui_color_fg, 1);

    for (i = 0; i < BUTTONS_COUNT; ++i) {
        button_widgets[i].draw(&button_widgets[i]);
    }

    update_display();
}

static void
on_init(void)
{
    gui_window_init(&window, WINDOW_WIDTH, WINDOW_HEIGHT);

    init_buttons();

    app_calc.on_show = on_show;
    app_calc.on_key_down = on_key_down;
    app_calc.on_key_up = on_key_up;
}

global app_st app_calc = {
    "Calculator",
    &icon_calc,
    DEFAULT_TICK_FREQUENCY,
    on_init,
};
