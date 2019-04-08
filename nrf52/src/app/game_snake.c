#include <app_timer.h>

#include "drivers/controls.h"
#include "drivers/display.h"

#include "application.h"
#include "gfx_effect.h"

#include "images/external/snake_splash_10_bitmap.h"
#include "images/external/snake_splash_11_bitmap.h"
#include "images/external/snake_splash_12_bitmap.h"
#include "images/external/snake_splash_13_bitmap.h"
#include "images/external/snake_splash_14_bitmap.h"
#include "images/external/snake_splash_15_bitmap.h"
#include "images/external/snake_splash_1_bitmap.h"
#include "images/external/snake_splash_2_bitmap.h"
#include "images/external/snake_splash_3_bitmap.h"
#include "images/external/snake_splash_4_bitmap.h"
#include "images/external/snake_splash_5_bitmap.h"
#include "images/external/snake_splash_6_bitmap.h"
#include "images/external/snake_splash_7_bitmap.h"
#include "images/external/snake_splash_8_bitmap.h"
#include "images/external/snake_splash_9_bitmap.h"
#include "images/external/snake_splash_bitmap.h"

#define SNAKE_BUTTON_NONE 255

#define SNAKE_CONTENTS_EMPTY -1

#define SNAKE_GRID_CELL(x, y) (y * SNAKE_GRID_WIDTH + x)
#define SNAKE_GRID_CELL_X(z) (z % SNAKE_GRID_WIDTH)
#define SNAKE_GRID_CELL_Y(z) (z / SNAKE_GRID_WIDTH)
#define SNAKE_GRID_HEIGHT 13
#define SNAKE_GRID_PX_BORDER 1
#define SNAKE_GRID_PX_CELL 5
#define SNAKE_GRID_PX_OFFSET_X 1
#define SNAKE_GRID_PX_OFFSET_Y 1
#define SNAKE_GRID_WIDTH 24

APP_TIMER_DEF(snake_game_timer);

typedef struct SnakeGamePositionState {
    int8_t dx;
    int8_t dy;
    uint8_t growth;
    uint8_t head_x;
    uint8_t head_y;
    uint8_t prev_x;
    uint8_t prev_y;
    uint8_t shrink_delay;
    uint8_t tail_x;
    uint8_t tail_y;
} SnakeGamePositionState;

typedef struct SnakeGameState {
    SnakeGamePositionState position;

    bool delay;
    int16_t grid[SNAKE_GRID_WIDTH * SNAKE_GRID_HEIGHT];
} SnakeGameState;

static const struct bitmap_ext *splash_animation[] = {
    &snake_splash_1_bitmap,  &snake_splash_2_bitmap,  &snake_splash_3_bitmap,
    &snake_splash_4_bitmap,  &snake_splash_5_bitmap,  &snake_splash_6_bitmap,
    &snake_splash_7_bitmap,  &snake_splash_8_bitmap,  &snake_splash_9_bitmap,
    &snake_splash_10_bitmap, &snake_splash_11_bitmap, &snake_splash_12_bitmap,
    &snake_splash_13_bitmap, &snake_splash_14_bitmap, &snake_splash_15_bitmap,
};

static uint8_t snake_button_read_value = SNAKE_BUTTON_NONE;

static void snake_buttons_handle(button_t button)
{
    switch (button) {
    case BUTTON_BACK:
    case BUTTON_DOWN:
    case BUTTON_ENTER:
    case BUTTON_UP:
        snake_button_read_value = button;
        break;

    default:
        break;
    }
}

static uint8_t snake_buttons_read()
{
    if (snake_button_read_value != SNAKE_BUTTON_NONE) {
        uint8_t value = snake_button_read_value;
        snake_button_read_value = SNAKE_BUTTON_NONE;

        return value;
    }

    return SNAKE_BUTTON_NONE;
}

static void snake_boot_sequence(void (*service_device)())
{
    service_device();
    display_draw_16bit_ext_bitmap(0, 0, &snake_splash_bitmap, 0);

    snake_buttons_read();
    do {
        service_device();
    } while (snake_buttons_read() == SNAKE_BUTTON_NONE);

    for (uint8_t i = 0; i < 15; i++) {
        service_device();
        display_draw_16bit_ext_bitmap(40, 0, splash_animation[i], 0);
    }

    do {
        service_device();
    } while (snake_buttons_read() == SNAKE_BUTTON_NONE);

    gfx_fill_rect(0, 0, DISPLAY_HEIGHT, DISPLAY_WIDTH, DISPLAY_BLACK);
    gfx_update();
}

static void snake_initial_snake(SnakeGameState *p_state)
{
    uint8_t initial_length = 3;

    p_state->position = (SnakeGamePositionState){
        .dx = 1,
        .dy = 0,
        .growth = 0,
        .head_x = 0,
        .head_y = 0,
        .prev_x = 0,
        .prev_y = 0,
        .shrink_delay = 0,
        .tail_x = 0,
        .tail_y = 0,
    };

    for (uint16_t i = 0; i < SNAKE_GRID_WIDTH * SNAKE_GRID_HEIGHT; i++) {
        p_state->grid[i] = SNAKE_CONTENTS_EMPTY;
    }

    SnakeGamePositionState *p_pos = &((*p_state).position);

    p_pos->head_x = p_pos->tail_x = (SNAKE_GRID_WIDTH / 2) - initial_length;
    p_pos->head_y = p_pos->tail_y = SNAKE_GRID_HEIGHT / 2;

    for (uint8_t i = 1; i < initial_length; i++) {
        p_pos->head_x++;

        p_state->grid[SNAKE_GRID_CELL(p_pos->head_x - 1, p_pos->head_y)] =
            SNAKE_GRID_CELL(p_pos->head_x, p_pos->head_y);
    }
}

static void snake_game_loop(SnakeGameState *p_state)
{
    if (p_state->delay) {
        return;
    } else {
        p_state->delay = true;
    }
}

static void snake_game_timer_callback(void *p_context)
{
    SnakeGameState *p_state = (SnakeGameState *)p_context;

    p_state->delay = false;
}

void snake_application(void (*service_device)())
{
    SnakeGameState state = {.delay = true};

    nsec_controls_add_handler(snake_buttons_handle);

    snake_boot_sequence(service_device);
    snake_initial_snake(&state);

    app_timer_create(&snake_game_timer, APP_TIMER_MODE_REPEATED,
                     snake_game_timer_callback);
    app_timer_start(snake_game_timer, APP_TIMER_TICKS(150), &state);

    while (application_get() == snake_application) {
        snake_game_loop(&state);
        service_device();
    }

    ret_code_t err_code = app_timer_stop(snake_game_timer);
    APP_ERROR_CHECK(err_code);

    nsec_controls_suspend_handler(snake_buttons_handle);
}
