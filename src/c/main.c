#include <pebble.h>

static Window *s_main_window;
static Layer *s_canvas_layer;
static TextLayer *s_earth_time_layer;
static TextLayer *s_date_layer;

// New pointers for the background image
static GBitmap *s_bg_bitmap;
static BitmapLayer *s_bg_layer;

// ----------------------------------------------------------------------
// SETTINGS FRAMEWORK
// ----------------------------------------------------------------------
typedef struct ClaySettings {
  bool show_human_time;
  bool show_human_date;
  bool show_eridian_secs;
  bool use_image_bg;
  GColor bg_colour;
  GColor human_colour;
  GColor eridian_colour;
} ClaySettings;

static ClaySettings settings;

static void default_settings() {
  settings.show_human_time = true;
  settings.show_human_date = true;
  settings.show_eridian_secs = false;
  
  // New Defaults!
  settings.use_image_bg = true;           // Background image ON by default
  settings.bg_colour = GColorBlack;       // Still keeping black as the solid fallback
  settings.human_colour = GColorYellow;   // Human text defaults to Yellow
  settings.eridian_colour = GColorWhite;  // Eridian text defaults to White
}

static void load_settings() {
  default_settings();
  persist_read_data(1, &settings, sizeof(ClaySettings));
}

static void save_settings() {
  persist_write_data(1, &settings, sizeof(ClaySettings));
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed);

static void update_tick_timer() {
  tick_timer_service_unsubscribe();
  if (settings.show_eridian_secs) {
    tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  } else {
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  }
}

static void apply_settings() {
  layer_set_hidden(text_layer_get_layer(s_earth_time_layer), !settings.show_human_time);
  layer_set_hidden(text_layer_get_layer(s_date_layer), !settings.show_human_date);
  
  // Hide or show the image background based on the toggle
  layer_set_hidden(bitmap_layer_get_layer(s_bg_layer), !settings.use_image_bg);
  
  text_layer_set_text_color(s_earth_time_layer, settings.human_colour);
  text_layer_set_text_color(s_date_layer, settings.human_colour);
  
  update_tick_timer();
  layer_mark_dirty(s_canvas_layer);
}

static void inbox_received_handler(DictionaryIterator *iter, void *context) {
  Tuple *time_tuple = dict_find(iter, MESSAGE_KEY_show_human_time);
  if(time_tuple) settings.show_human_time = time_tuple->value->int32 == 1;

  Tuple *date_tuple = dict_find(iter, MESSAGE_KEY_show_human_date);
  if(date_tuple) settings.show_human_date = date_tuple->value->int32 == 1;

  Tuple *sec_tuple = dict_find(iter, MESSAGE_KEY_show_eridian_secs);
  if(sec_tuple) settings.show_eridian_secs = sec_tuple->value->int32 == 1;

  Tuple *bg_img_tuple = dict_find(iter, MESSAGE_KEY_use_image_bg);
  if(bg_img_tuple) settings.use_image_bg = bg_img_tuple->value->int32 == 1;

  Tuple *bg_colour_tuple = dict_find(iter, MESSAGE_KEY_bg_colour);
  if(bg_colour_tuple) settings.bg_colour = GColorFromHEX(bg_colour_tuple->value->int32);

  Tuple *human_colour_tuple = dict_find(iter, MESSAGE_KEY_human_colour);
  if(human_colour_tuple) settings.human_colour = GColorFromHEX(human_colour_tuple->value->int32);

  Tuple *eridian_colour_tuple = dict_find(iter, MESSAGE_KEY_eridian_colour);
  if(eridian_colour_tuple) settings.eridian_colour = GColorFromHEX(eridian_colour_tuple->value->int32);

  save_settings();
  apply_settings();
}

// ----------------------------------------------------------------------
// ERIDIAN GLYPH DRAWING
// ----------------------------------------------------------------------

static int S(int val, bool big) {
  return big ? (val * 3) / 2 : val;
}

static void draw_eridian_digit(GContext *ctx, int digit, int x, int y, bool big) {
  graphics_context_set_stroke_width(ctx, big ? 8 : 5);
  graphics_context_set_stroke_color(ctx, settings.eridian_colour);
  
  switch(digit) {
    case 0: { 
      GPoint points[] = {
        GPoint(x + S(4, big), y + S(26, big)), GPoint(x + S(11, big), y + S(13, big)), 
        GPoint(x + S(15, big), y + S(6, big)), GPoint(x + S(15, big), y + S(2, big)), 
        GPoint(x + S(11, big), y + S(0, big)), GPoint(x + S(7, big), y + S(3, big)),
        GPoint(x + S(5, big), y + S(9, big)),  GPoint(x + S(7, big), y + S(18, big)), 
        GPoint(x + S(11, big), y + S(28, big)),GPoint(x + S(15, big), y + S(32, big)), 
        GPoint(x + S(19, big), y + S(30, big))
      };
      for (int i = 0; i < 10; i++) graphics_draw_line(ctx, points[i], points[i+1]);
      break;
    }
    case 1: 
      graphics_draw_line(ctx, GPoint(x + S(10, big), y + S(2, big)), GPoint(x + S(10, big), y + S(32, big)));
      break;
    case 2: 
      graphics_draw_line(ctx, GPoint(x + S(2, big), y + S(2, big)), GPoint(x + S(10, big), y + S(32, big)));
      graphics_draw_line(ctx, GPoint(x + S(18, big), y + S(2, big)), GPoint(x + S(10, big), y + S(32, big)));
      break;
    case 3: 
      graphics_draw_line(ctx, GPoint(x + S(10, big), y + S(2, big)), GPoint(x + S(2, big), y + S(32, big)));
      graphics_draw_line(ctx, GPoint(x + S(6, big), y + S(17, big)), GPoint(x + S(18, big), y + S(32, big)));
      break;
    case 4: 
      graphics_draw_line(ctx, GPoint(x + S(10, big), y + S(2, big)), GPoint(x + S(10, big), y + S(32, big)));
      graphics_draw_line(ctx, GPoint(x + S(2, big), y + S(17, big)), GPoint(x + S(18, big), y + S(17, big)));
      break;
    case 5: 
      graphics_draw_line(ctx, GPoint(x + S(2, big), y + S(10, big)), GPoint(x + S(10, big), y + S(32, big)));
      graphics_draw_line(ctx, GPoint(x + S(18, big), y + S(10, big)), GPoint(x + S(10, big), y + S(32, big)));
      graphics_draw_line(ctx, GPoint(x + S(6, big), y + S(17, big)), GPoint(x + S(14, big), y + S(17, big)));
      break;
  }
}

static void draw_base6_number(GContext *ctx, int value, int num_digits, int start_x, int y, bool big) {
  int divisors[] = {1, 6, 36}; 
  for (int i = 0; i < num_digits; i++) {
    int pos_from_right = num_digits - 1 - i;
    int divisor = divisors[pos_from_right];
    int digit = (value / divisor) % 6;
    
    int x = start_x + (i * S(30, big)); 
    draw_eridian_digit(ctx, digit, x, y, big);
  }
}

// ----------------------------------------------------------------------
// RENDER UI
// ----------------------------------------------------------------------
static void canvas_update_proc(Layer *layer, GContext *ctx) {
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);

  GRect bounds = layer_get_bounds(layer);
  
  // Only draw the solid colour if the image background is toggled OFF
  if (!settings.use_image_bg) {
    graphics_context_set_fill_color(ctx, settings.bg_colour);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  }

  bool big_mode = !settings.show_human_time && !settings.show_human_date;
  
  int digit_h = S(34, big_mode);
  int gap_x = S(10, big_mode);
  int gap_y = S(11, big_mode);
  
  int total_w_2 = 2 * S(20, big_mode) + gap_x;
  int total_w_3 = 3 * S(20, big_mode) + 2 * gap_x;
  
  int row_spacing = digit_h + gap_y;
  int centre_y = bounds.size.h / 2;

  int y_hr = centre_y - (digit_h / 2) - row_spacing;
  int y_min = centre_y - (digit_h / 2);
  int y_sec = centre_y - (digit_h / 2) + row_spacing;

  if (!settings.show_eridian_secs) {
    y_hr += row_spacing / 2;
    y_min += row_spacing / 2;
  }

  // Draw Hours
  int start_x_hr = (bounds.size.w - total_w_2) / 2;
  draw_base6_number(ctx, tick_time->tm_hour, 2, start_x_hr, y_hr, big_mode);

  // Draw Minutes
  int start_x_min = (bounds.size.w - total_w_3) / 2;
  draw_base6_number(ctx, tick_time->tm_min, 3, start_x_min, y_min, big_mode);

  // Draw Seconds
  if (settings.show_eridian_secs) {
    draw_base6_number(ctx, tick_time->tm_sec, 3, start_x_min, y_sec, big_mode);
  }
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  layer_mark_dirty(s_canvas_layer);

  if (settings.show_human_time) {
    static char s_time_buffer[16];
    if (clock_is_24h_style()) {
      strftime(s_time_buffer, sizeof(s_time_buffer), "%H:%M", tick_time);
    } else {
      strftime(s_time_buffer, sizeof(s_time_buffer), "%I:%M %p", tick_time);
    }
    text_layer_set_text(s_earth_time_layer, s_time_buffer);
  }

  if (settings.show_human_date) {
    static char s_date_buffer[16];
    strftime(s_date_buffer, sizeof(s_date_buffer), "%a %m/%d", tick_time);
    text_layer_set_text(s_date_layer, s_date_buffer);
  }
}

// ----------------------------------------------------------------------
// LIFECYCLE
// ----------------------------------------------------------------------
static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  // 1. Load the background image FIRST so it sits behind everything
  s_bg_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_ASTROPHAGE_BG);
  s_bg_layer = bitmap_layer_create(bounds);
  bitmap_layer_set_bitmap(s_bg_layer, s_bg_bitmap);
  layer_add_child(window_layer, bitmap_layer_get_layer(s_bg_layer));

  // 2. Add the drawing canvas
  s_canvas_layer = layer_create(bounds);
  layer_set_update_proc(s_canvas_layer, canvas_update_proc);
  layer_add_child(window_layer, s_canvas_layer);

  // 3. Add the human text layers on top
  s_earth_time_layer = text_layer_create(GRect(0, 5, bounds.size.w, 34));
  text_layer_set_background_color(s_earth_time_layer, GColorClear);
  text_layer_set_text_alignment(s_earth_time_layer, GTextAlignmentCenter);
  text_layer_set_font(s_earth_time_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_earth_time_layer));

  s_date_layer = text_layer_create(GRect(0, bounds.size.h - 38, bounds.size.w, 34));
  text_layer_set_background_color(s_date_layer, GColorClear);
  text_layer_set_text_alignment(s_date_layer, GTextAlignmentCenter);
  text_layer_set_font(s_date_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_date_layer));
}

static void main_window_unload(Window *window) {
  text_layer_destroy(s_earth_time_layer);
  text_layer_destroy(s_date_layer);
  layer_destroy(s_canvas_layer);
  
  // Clean up image memory
  bitmap_layer_destroy(s_bg_layer);
  gbitmap_destroy(s_bg_bitmap);
}

static void init() {
  app_message_register_inbox_received(inbox_received_handler);
  app_message_open(128, 128);
  
  load_settings();

  s_main_window = window_create();
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });
  window_stack_push(s_main_window, true);
  
  apply_settings(); 
  
  time_t temp = time(NULL);
  struct tm *tick_time = localtime(&temp);
  tick_handler(tick_time, SECOND_UNIT);
}

static void deinit() {
  window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}