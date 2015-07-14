#include <pebble.h>
#include <stdio.h>

#include "player.h"
#include "opponent.h"
#include "bullet.h"

#define ARRAYSIZE(x) (sizeof(x)/sizeof(*x))
#define BACKGROUND_COLOR GColorBlack

enum {
  GAME_ACTIVE,
  GAME_PAUSED,
  GAME_OVER
} game_state = GAME_ACTIVE;

static Window *window;
static Layer *render_layer;

static Player player;
static Opponent opponent;
static Bullet bullet;

static void game_pause();
static void game_unpause();
static void game_over();

TextLayer *overlay_text_layer;

static void overlay_open(const char * text) {
  GRect game_bounds = layer_get_bounds(render_layer);

  overlay_text_layer = text_layer_create((GRect) {
      .origin = { 0, game_bounds.size.h / 2 - 20 },
      .size = { game_bounds.size.w, 20 }
  });

  text_layer_set_text(overlay_text_layer, text);
  text_layer_set_text_alignment(overlay_text_layer, GTextAlignmentCenter);

#ifdef PBL_COLOR
  text_layer_set_text_color(overlay_text_layer, GColorWhite);
  text_layer_set_background_color(overlay_text_layer, GColorDarkCandyAppleRed);
#else
  text_layer_set_text_color(overlay_text_layer, GColorBlack);
  text_layer_set_background_color(overlay_text_layer, GColorWhite);
#endif

  layer_add_child(render_layer, text_layer_get_layer(overlay_text_layer));
}

static void overlay_close() {
  if (overlay_text_layer) {
    text_layer_destroy(overlay_text_layer);
  }
}

// Singleton for simplicity
struct {
  int points;
  char text[15];
  TextLayer *text_layer;
} points;

static void points_update(int delta) {
  points.points += delta;
  snprintf(points.text, ARRAYSIZE(points.text), "Points: %i", points.points);
}

static void click_back_handler(ClickRecognizerRef recognizer, void *ctx) {
  switch (game_state) {
  case GAME_ACTIVE:
    game_pause();
    break;
  case GAME_PAUSED:
  case GAME_OVER:
    window_stack_pop(true);
    break;
  }
}

static void game_init();
static void start_select_handler(ClickRecognizerRef recognizer, void *ctx) {
  switch (game_state) {
  case GAME_ACTIVE:
    bullet_respawn(&bullet, player.obj.x_pos + player.obj.size, player.obj.y_pos, 10, 0);
    break;
  case GAME_PAUSED:
    game_unpause();
    break;
  case GAME_OVER:
    game_init();
    break;
  }
}

static void start_up_handler(ClickRecognizerRef recognizer, void *ctx) {
  player.obj.y_vel = -5;
}

static void end_up_handler(ClickRecognizerRef recognizer, void *ctx) {
  player.obj.y_vel = 0;
}

static void start_down_handler(ClickRecognizerRef recognizer, void *ctx) {
  player.obj.y_vel = 5;
}

static void end_down_handler(ClickRecognizerRef recognizer, void *ctx) {
  player.obj.y_vel = 0;
}

static void render_layer_update_callback(Layer *layer, GContext *ctx) {
  switch (game_state) {
  case GAME_ACTIVE:
    player_update(&player);
    opponent_update(&opponent);
    bullet_update(&bullet);

    if (object_collides(&bullet.obj, &opponent.obj)) {
      points_update(1);
      bullet_hide(&bullet);
      opponent_kill(&opponent);
    }

    // Opponent passes screen boundary
    if (opponent.obj.x_pos + opponent.obj.size < 0) {
      points_update(-1);
      opponent_respawn(&opponent);
      vibes_short_pulse();
    }

    if (object_collides(&player.obj, &opponent.obj)) {
      player_health_update(&player, -opponent.obj.size);
      opponent_kill(&opponent);
      vibes_short_pulse();
    }

    player_draw(&player, ctx);
    opponent_draw(&opponent, ctx);
    bullet_draw(&bullet, ctx);
    player_health_draw(&player, ctx);
    break;
  case GAME_PAUSED:
    break;
  case GAME_OVER:
    break;
  }
}

static void click_config_provider(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_BACK, click_back_handler);
  window_raw_click_subscribe(BUTTON_ID_SELECT, start_select_handler, NULL, NULL);
  window_raw_click_subscribe(BUTTON_ID_UP, start_up_handler, end_up_handler, NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN, start_down_handler, end_down_handler, NULL);
}

static void timer_callback(void *data) {
  layer_mark_dirty(render_layer);

  switch (game_state) {
  case GAME_ACTIVE:
  case GAME_PAUSED:
    app_timer_register(1000 / 30, timer_callback, NULL);
    break;
  case GAME_OVER:
    break;
  }
}

static void game_init() {
  game_state = GAME_ACTIVE;

  overlay_close();

  player.health = 100;
  player.obj.x_pos = 0,
  player.obj.y_pos = 84,
  player.obj.x_vel = 0,
  player.obj.y_vel = 0,
  player.obj.size = 20;

  opponent_respawn(&opponent);

  bullet.obj.size = 5;
  bullet_hide(&bullet);

  points.points = 0;
  points_update(0);

  timer_callback(NULL);
}

static void game_over() {
  game_state = GAME_OVER;
  overlay_open("GAME OVER");
}

static void game_pause() {
  game_state = GAME_PAUSED;
  overlay_open("PAUSED");
}

static void game_unpause() {
  game_state = GAME_ACTIVE;
  overlay_close();
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);

  // Initialize the render layer
  render_layer = layer_create(layer_get_frame(window_layer));
  layer_set_update_proc(render_layer, render_layer_update_callback);
  layer_add_child(window_layer, render_layer);

  GRect game_bounds = layer_get_bounds(render_layer);

  // Initialize the points text layer
  points.text_layer = text_layer_create((GRect) {
      .origin = { 0, game_bounds.size.h - 20 },
      .size = { game_bounds.size.w, 20 }
  });
  text_layer_set_text(points.text_layer, points.text);
  text_layer_set_text_alignment(points.text_layer, GTextAlignmentCenter);

#ifdef PBL_COLOR
  text_layer_set_text_color(points.text_layer, GColorWhite);
  text_layer_set_background_color(points.text_layer, GColorWindsorTan);
#else
  text_layer_set_text_color(points.text_layer, GColorBlack);
  text_layer_set_background_color(points.text_layer, GColorWhite);
#endif

  layer_add_child(window_layer, text_layer_get_layer(points.text_layer));

  game_init();
}

static void window_unload(Window *window) {
  text_layer_destroy(points.text_layer);
  overlay_close();
  layer_destroy(render_layer);
}

static void init(void) {
  window = window_create();

  window_set_background_color(window, BACKGROUND_COLOR);
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

#ifdef PBL_PLATFORM_APLITE
  window_set_fullscreen(window, true);
#endif

  window_stack_push(window, true);
}

static void deinit(void) {
  window_destroy(window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
