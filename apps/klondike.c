/*
 * Copyright (c) 2026 luke8086
 * Distributed under the terms of GPL-2 License
 *
 * File: klondike.c - Klondike game
 */

#include <gui.h>

enum {
    CARD_WIDTH = 30,
    CARD_HEIGHT = 24,

    CARD_COUNT = 52,
    FOUND_COUNT = 4,
    COLUMN_COUNT = 7,

    GAP_Y = 6,
    GAP_X = 6,

    COLUMN_CARDS_STEP = 10,
    COLUMN_CARDS_MAX = 24,

    WINDOW_WIDTH = COLUMN_COUNT * CARD_WIDTH + (COLUMN_COUNT + 1) * GAP_X,
    WINDOW_HEIGHT = GUI_HEIGHT - 2 * STATUS_HEIGHT,

    TOP_PILES_Y = GAP_Y,
    COLUMNS_Y = TOP_PILES_Y + CARD_HEIGHT + GAP_Y,
    COLUMNS_H = WINDOW_HEIGHT - COLUMNS_Y - GAP_Y,

    PILE_STOCK = 1,
    PILE_WASTE = 2,
    PILE_FOUNDS = 3,
    PILE_COLUMNS = 4,

    STATE_DEFAULT = 0,
    STATE_WON = 1,
    STATE_AUTO_PENDING = 2,

    AUTO_MOVE_HIGHLIGHT_TICKS = TICK_FREQUENCY * 10 / 100, /* 0.10s */
    AUTO_MOVE_EXECUTE_TICKS = TICK_FREQUENCY * 30 / 100,   /* 0.30s */
};

static window_st window;

static card_t stock_cards[CARD_COUNT];
static card_pile_st stock;

static card_t waste_cards[CARD_COUNT];
static card_pile_st waste;

static card_t founds_cards[FOUND_COUNT];
static card_pile_st founds[FOUND_COUNT];

static card_t columns_cards[COLUMN_COUNT][COLUMN_CARDS_MAX];
static card_pile_st columns[COLUMN_COUNT];

static card_game_st game;
static int state;
static int ticks_waited = 0;

static void
draw_all_piles(void)
{
    int i;

    card_pile_draw(&game, &stock);
    card_pile_draw(&game, &waste);

    for (i = 0; i < FOUND_COUNT; ++i) {
        card_pile_draw(&game, &founds[i]);
    }

    for (i = 0; i < COLUMN_COUNT; ++i) {
        card_pile_draw(&game, &columns[i]);
    }
}

static int
remaining_cards(void)
{
    int i, ret;

    ret = stock.count + waste.count;

    for (i = 0; i < COLUMN_COUNT; ++i) {
        ret += columns[i].count;
    }

    return ret;
}

static void
deal_cards(void)
{
    card_t deck[CARD_COUNT];
    int i, j, k;

    card_deck_init(deck, CARD_COUNT);
    card_deck_shuffle(deck, CARD_COUNT);

    stock.count = 0;
    waste.count = 0;

    for (i = 0; i < FOUND_COUNT; ++i) {
        founds[i].count = 0;
    }

    for (i = 0; i < COLUMN_COUNT; ++i) {
        columns[i].count = 0;
        columns[i].face_up_from = 0;
    }

    k = 0;
    for (i = 0; i < COLUMN_COUNT; ++i) {
        for (j = 0; j <= i; ++j) {
            card_pile_push(&columns[i], deck[k++]);
        }
        columns[i].face_up_from = i;
    }

    while (k < CARD_COUNT) {
        card_pile_push(&stock, deck[k++]);
    }

    game.cur_move.src = NULL;
    state = STATE_DEFAULT;
}

static void
update_status(void)
{
    if (state == STATE_WON) {
        gui_status_set("You Won! Press R to restart");
        return;
    }

    gui_status_set("Remaining cards: %d", remaining_cards());
}

static void
check_win(void)
{
    int i;

    for (i = 0; i < FOUND_COUNT; ++i) {
        if (founds[i].count == 0 || CARD_RANK(CARD_PILE_TOP(&founds[i])) != 12) {
            return;
        }
    }

    state = STATE_WON;
    update_status();
}

static int
get_max_valid_sequence_len(card_pile_st *p)
{
    int i;
    card_t curr, prev;

    if (p->count == 0) {
        return 0;
    }

    for (i = p->count - 1; i > p->face_up_from; --i) {
        curr = p->cards[i];
        prev = p->cards[i - 1];

        if (CARD_RANK(prev) != CARD_RANK(curr) + 1) {
            break;
        }
        if (CARD_COLOR(prev) == CARD_COLOR(curr)) {
            break;
        }
    }

    return p->count - i;
}

static int
card_should_auto_promote(card_t card)
{
    int rank = CARD_RANK(card);
    int suit = CARD_SUIT(card);
    int color = CARD_COLOR(card);
    int i;

    if (founds[suit].count == 0) {
        if (rank != 0) {
            return 0;
        }
    } else if (rank != CARD_RANK(CARD_PILE_TOP(&founds[suit])) + 1) {
        return 0;
    }

    if (rank <= 1) {
        return 1;
    }

    for (i = 0; i < FOUND_COUNT; ++i) {
        if (CARD_COLOR(i * 13) == color) {
            continue;
        }

        if (founds[i].count == 0 || CARD_RANK(CARD_PILE_TOP(&founds[i])) < rank - 1) {
            return 0;
        }
    }

    return 1;
}

static void
set_auto_move(card_pile_st *src, card_pile_st *dst)
{
    game.cur_move.src = src;
    game.cur_move.dst = dst;
    game.cur_move.count = 1;
    state = STATE_AUTO_PENDING;
    ticks_waited = 0;
}

static void
check_auto_move(void)
{
    int i;
    card_t card;

    card = CARD_PILE_TOP(&waste);

    if (card != CARD_EMPTY && card_should_auto_promote(card)) {
        set_auto_move(&waste, &founds[CARD_SUIT(card)]);
        return;
    }

    for (i = 0; i < COLUMN_COUNT; ++i) {
        card = CARD_PILE_TOP(&columns[i]);

        if (card != CARD_EMPTY && card_should_auto_promote(card)) {
            set_auto_move(&columns[i], &founds[CARD_SUIT(card)]);
            return;
        }
    }
}

static void
exec_move(void)
{
    card_game_exec_cur_move(&game);
    update_status();
    check_win();

    if (state != STATE_WON) {
        check_auto_move();
    }
}

static void
start_move(void)
{
    if (game.cur_pile->type != PILE_WASTE && game.cur_pile->type != PILE_COLUMNS) {
        return;
    }

    if (game.cur_pile->count == 0) {
        return;
    }

    game.cur_move.src = game.cur_pile;
    card_pile_draw(&game, game.cur_pile);
    update_status();
}

static void
cancel_move(void)
{
    card_pile_st *old = game.cur_move.src;

    game.cur_move.src = NULL;

    if (old != NULL) {
        card_pile_draw(&game, old);
    }

    update_status();
}

static void
show_error(const char *msg)
{
    cancel_move();
    gui_status_set("%s", msg);
}

static int
can_move_to_column(card_pile_st *col, card_t src_bottom)
{
    card_t dst_top;

    if (col->count == 0) {
        return CARD_RANK(src_bottom) == 12;
    }

    dst_top = CARD_PILE_TOP(col);

    if (CARD_RANK(dst_top) != CARD_RANK(src_bottom) + 1) {
        return 0;
    }

    if (CARD_COLOR(dst_top) == CARD_COLOR(src_bottom)) {
        return 0;
    }

    return 1;
}

static void
request_move_to_column(void)
{
    card_pile_st *src = game.cur_move.src;
    card_pile_st *dst = game.cur_move.dst;
    int count, max_count;
    card_t src_bottom;

    if (src->type == PILE_WASTE) {
        src_bottom = CARD_PILE_TOP(src);

        if (!can_move_to_column(dst, src_bottom)) {
            show_error("Invalid move");
            return;
        }

        game.cur_move.count = 1;
        exec_move();
        return;
    } else if (src->type == PILE_COLUMNS) {
        max_count = get_max_valid_sequence_len(src);

        for (count = max_count; count >= 1; --count) {
            src_bottom = src->cards[src->count - count];

            if (can_move_to_column(dst, src_bottom)) {
                game.cur_move.count = count;
                exec_move();
                return;
            }
        }
    }

    show_error("Invalid move");
}

static int
can_move_to_found(card_pile_st *found, card_t card)
{
    if (CARD_SUIT(card) != found->index) {
        return 0;
    }

    if (found->count == 0) {
        return CARD_RANK(card) == 0;
    }

    return CARD_RANK(CARD_PILE_TOP(found)) + 1 == CARD_RANK(card);
}

static void
request_move(void)
{
    game.cur_move.dst = game.cur_pile;

    if (game.cur_pile->type == PILE_COLUMNS) {
        request_move_to_column();
    } else {
        show_error("Invalid move");
    }
}

static void
request_promote_to_found(void)
{
    card_t card;
    card_pile_st *found;

    if (game.cur_move.src == NULL) {
        start_move();
    }

    if (game.cur_move.src == NULL) {
        return;
    }

    card = CARD_PILE_TOP(game.cur_move.src);
    if (card == CARD_EMPTY) {
        cancel_move();
        return;
    }

    found = &founds[CARD_SUIT(card)];
    if (!can_move_to_found(found, card)) {
        show_error("Invalid move");
        return;
    }

    game.cur_move.dst = found;
    game.cur_move.count = 1;
    exec_move();
}

static void
draw_card_from_stock(void)
{
    if (stock.count > 0) {
        card_pile_push(&waste, card_pile_pop(&stock));
    } else if (waste.count > 0) {
        while (waste.count > 0) {
            card_pile_push(&stock, card_pile_pop(&waste));
        }
    }

    card_pile_draw(&game, &stock);
    card_pile_draw(&game, &waste);
    update_status();
    check_auto_move();
}

static void
handle_space(void)
{
    if (game.cur_pile->type == PILE_STOCK) {
        if (game.cur_move.src == NULL) {
            draw_card_from_stock();
        } else {
            cancel_move();
        }

        return;
    }

    if (game.cur_move.src == NULL) {
        start_move();
    } else if (game.cur_move.src == game.cur_pile) {
        cancel_move();
    } else {
        request_move();
    }
}

static void
move_cursor(int dx, int dy)
{
    card_pile_st *p = game.cur_pile;
    int col;

    card_cursor_draw(&game, 0);

    if (p->type == PILE_COLUMNS) {
        if (dy < 0) {
            game.cur_pile = (p->index == 0) ? &stock : &waste;
        } else if (dx != 0) {
            col = MAX(0, MIN(COLUMN_COUNT - 1, p->index + dx));
            game.cur_pile = &columns[col];
        }
    } else if (dy > 0) {
        game.cur_pile = &columns[(p->type == PILE_STOCK) ? 0 : 1];
    } else if (dx > 0 && p->type == PILE_STOCK) {
        game.cur_pile = &waste;
    } else if (dx < 0 && p->type == PILE_WASTE) {
        game.cur_pile = &stock;
    }

    card_cursor_draw(&game, 1);
}

static void
restart_game(void)
{
    deal_cards();
    draw_all_piles();
    card_cursor_draw(&game, 1);
    update_status();
}

static void
on_key_down(uint8_t key_code, uint8_t key_mods)
{
    if (state == STATE_AUTO_PENDING) {
        return;
    }

    if (state == STATE_WON) {
        if (key_code == KEY_R) {
            restart_game();
        }
        return;
    }

    switch (key_code) {
        case KEY_LEFT: move_cursor(-1, 0); return;
        case KEY_RIGHT: move_cursor(1, 0); return;
        case KEY_UP: move_cursor(0, -1); return;
        case KEY_DOWN: move_cursor(0, 1); return;
        case KEY_SPACE: handle_space(); return;
        case KEY_F: request_promote_to_found(); return;
        case KEY_R: restart_game(); return;
        case KEY_ESC: cancel_move(); return;
    }
}

static void
on_tick(void)
{
    if (state != STATE_AUTO_PENDING) {
        return;
    }

    ++ticks_waited;

    if (ticks_waited == AUTO_MOVE_HIGHLIGHT_TICKS) {
        card_pile_draw(&game, game.cur_move.src);
        return;
    }

    if (ticks_waited >= AUTO_MOVE_EXECUTE_TICKS) {
        state = STATE_DEFAULT;
        exec_move();
    }
}

static void
init_game(void)
{
    int i;

    game.origin = &window.origin;
    game.size = &window.size;

    game.card_width = CARD_WIDTH;
    game.card_height = CARD_HEIGHT;
    game.card_step = COLUMN_CARDS_STEP;

    game.cur_pile = &columns[0];

    stock.type = PILE_STOCK;
    stock.index = 0;
    stock.capacity = CARD_COUNT;
    stock.count = 0;
    stock.face_up_from = CARD_PILE_ALL_FACE_DOWN;
    stock.cards = stock_cards;
    stock.is_cascade = 0;
    stock.replace_on_push = 0;
    gui_rect_init(&stock.rect, GAP_X, TOP_PILES_Y, CARD_WIDTH, CARD_HEIGHT);

    waste.type = PILE_WASTE;
    waste.index = 0;
    waste.capacity = CARD_COUNT;
    waste.count = 0;
    waste.cards = waste_cards;
    waste.is_cascade = 0;
    waste.replace_on_push = 0;
    gui_rect_init(&waste.rect,
        GAP_X + (CARD_WIDTH + GAP_X), TOP_PILES_Y, CARD_WIDTH, CARD_HEIGHT);

    for (i = 0; i < FOUND_COUNT; ++i) {
        founds[i].type = PILE_FOUNDS;
        founds[i].index = i;
        founds[i].capacity = 1;
        founds[i].count = 0;
        founds[i].cards = &founds_cards[i];
        founds[i].is_cascade = 0;
        founds[i].replace_on_push = 1;

        gui_rect_init(&founds[i].rect,
            GAP_X + (i + 3) * (CARD_WIDTH + GAP_X), TOP_PILES_Y,
            CARD_WIDTH, CARD_HEIGHT);
    }

    for (i = 0; i < COLUMN_COUNT; ++i) {
        columns[i].type = PILE_COLUMNS;
        columns[i].index = i;
        columns[i].capacity = COLUMN_CARDS_MAX;
        columns[i].count = 0;
        columns[i].face_up_from = 0;
        columns[i].cards = columns_cards[i];
        columns[i].is_cascade = 1;
        columns[i].replace_on_push = 0;

        gui_rect_init(&columns[i].rect,
            GAP_X + i * (CARD_WIDTH + GAP_X), COLUMNS_Y,
            CARD_WIDTH, COLUMNS_H);
    }
}

static void
on_show(void)
{
    gui_status_set_br("F: Promote  R: Restart");
    restart_game();
}

static void
on_init(void)
{
    gui_window_init(&window, WINDOW_WIDTH, WINDOW_HEIGHT);
    init_game();

    app_klondike.on_show = on_show;
    app_klondike.on_key_down = on_key_down;
    app_klondike.on_tick = on_tick;
}

global app_st app_klondike = {
    "Klondike",
    &icon_klondike,
    on_init,
};
