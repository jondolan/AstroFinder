#include "pebble.h"

#define KEY_NAME 0
#define KEY_ELEVATION 1
#define KEY_HEADING 2

static Window *mainWindow;
static Layer *ballLayer, *barLayer;
static TextLayer *footerLayer, *feedbackLayer, *angleLayer;
static GPath *arrowToObject;
bool verticalMode = false;

static GPathInfo pointsForArrowToObject = {
  3,
  (GPoint[]) { { -30, 0 }, { 30, 0 }, { 0, -46 } }
};

uint32_t samplesPerBatch = 10;//, batchCounter = 0;//, previousX[9], previousY[9], previousZ[9];  // 10 samples per accelerometer batch
static GRect outerRing, innerRing, hollowBallLoc, hollowCollisionRect, hollowCollisionRectLarge;
GPoint ballLoc;
const int innerRingOffset = 60, ringWidth = 4, ballRad = 10, hollowBallRad = 32, testElevation = 75, arrowLength = 80;
int targetElevation = 0;
long currentHeading = 0.0, currentAngle = 0.0;
float d = 0;

static void compassEventHandler(CompassHeadingData compassHeading) {
  // bounds for the display
  GRect bounds = layer_get_frame(window_get_root_layer(mainWindow));
  
  // cos and sin of the angle to determine the ball loc for compass
  currentHeading = compassHeading.magnetic_heading;
  long cosAl = cos_lookup(currentHeading - TRIG_MAX_ANGLE / 4);
  float cosAf = (float) cosAl / TRIG_MAX_ANGLE;
  long sinAl = sin_lookup(currentHeading - TRIG_MAX_ANGLE / 4);
  float sinAf = (float) sinAl / TRIG_MAX_ANGLE;
  
  // radius of ball location, related to the upper left corner of screen
  int ballLocR = (float) ((bounds.size.w / 2) + (innerRingOffset / 2) + (ringWidth * 2)) / 2;
  // the location of the ball
  ballLoc = GPoint((int) (bounds.size.w / 2 + ballLocR * cosAf), (int) (bounds.size.h / 2 + ballLocR * sinAf));
  // location of the hollow ball (TODO - set through menu, select at open of app and available again by middle press)
  hollowBallLoc = GRect(bounds.size.w / 2 - (hollowBallRad / 2), innerRingOffset / 2 - (hollowBallRad / 2), hollowBallRad, hollowBallRad);
  // rectangle for collision detection with ball as it rotates
  hollowCollisionRect =  GRect(bounds.size.w / 2 - (hollowBallRad / 4), innerRingOffset / 2 - (hollowBallRad / 4), hollowBallRad / 2, hollowBallRad / 2);
  hollowCollisionRectLarge = GRect(bounds.size.w / 2 - (hollowBallRad * 2), innerRingOffset / 2 - (hollowBallRad * 2), hollowBallRad * 4, hollowBallRad * 4);
  
  GRect alert_bounds;
  /*if(compassHeading.compass_status == CompassStatusDataInvalid) {
    // Tell user to move their arm
    alert_bounds = GRect(0, 0, bounds.size.w, bounds.size.h);
    text_layer_set_background_color(feedbackLayer, GColorBlack);
    text_layer_set_text_color(feedbackLayer, GColorWhite);
    text_layer_set_font(feedbackLayer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text(feedbackLayer, "Compass is calibrating!\n\nMove your arm to aid calibration.");
  } else if (compassHeading.compass_status == CompassStatusCalibrating) {
    // Show status at the top*/
    alert_bounds = GRect(0, -3, bounds.size.w, bounds.size.h / 7);
    text_layer_set_background_color(feedbackLayer, GColorClear);
    text_layer_set_text_color(feedbackLayer, GColorBlack);
    text_layer_set_font(feedbackLayer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_text(feedbackLayer, "Tuning...");
 // }
  text_layer_set_text_alignment(feedbackLayer, GTextAlignmentCenter);
  layer_set_frame(text_layer_get_layer(feedbackLayer), alert_bounds);

  // display heading in degrees and radians
  static char headingBuffer[64];
  
  // display heading in degrees and radians
  snprintf(headingBuffer, sizeof(headingBuffer),
    " %ld°\n%ld.%02ldpi",
    TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE - currentHeading),
    // display radians, units digit
    (TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE - currentHeading) * 2) / 360,
    // radians, digits after decimal
    ((TRIGANGLE_TO_DEG(TRIG_MAX_ANGLE - currentHeading) * 200) / 360) % 100
  );
  text_layer_set_text(footerLayer, headingBuffer);

  // trigger layer for refresh
  layer_mark_dirty(ballLayer);
}

static void accelBatchHandler(AccelData *data, uint32_t num_samples) {  
  int x = data[0].x;
  int y = data[0].y;
  int z = data[0].z;
  
  int atan = atan2_lookup(y, z);
  atan -= TRIG_MAX_ANGLE / 2;
  float atanf = (float) atan;
  atanf /= 0x10000;
  atanf *= 360;
  currentAngle = atanf;
  int atand = (int) atanf;
  
  if (abs(atand - testElevation) > 100) {
    
  }
  
  static char headingBuffer[64];
  snprintf(headingBuffer, sizeof(headingBuffer),
    " %d %d %d \n%d %d",
    x, y,
    z,
    atand, targetElevation
  );
  text_layer_set_text(angleLayer, headingBuffer);
  
}

static void switchToVerticalMode() {
  verticalMode = true;
  accel_data_service_subscribe(samplesPerBatch, accelBatchHandler);
  layer_set_hidden(barLayer, false);
}

static void returnFromVeritcalMode() {
  verticalMode = false;
  accel_data_service_unsubscribe();
  layer_set_hidden(barLayer, true);
}

static void updateBallDisplay(Layer *path, GContext *ctx) {
  
  // fill the background with red to make it easy on the eyes
  graphics_context_set_fill_color(ctx, GColorFolly);
  graphics_fill_rect(ctx, layer_get_bounds(path), 0, GCornerNone);
  
  // make the inner and outer rings in black
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_radial(ctx, outerRing, GOvalScaleModeFitCircle, ringWidth, DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(360));
  graphics_fill_radial(ctx, innerRing, GOvalScaleModeFitCircle, ringWidth, DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(360));
  
  // make the hollow ball
  graphics_fill_radial(ctx, hollowBallLoc, GOvalScaleModeFitCircle, ringWidth, DEG_TO_TRIGANGLE(0), DEG_TO_TRIGANGLE(360));
  
  if (!verticalMode && grect_contains_point(&hollowCollisionRect, &ballLoc)) { // if we're not in vertical mode, check if there is a collison
    // color the ball green if it's in the hollow ball, black otherwise
      graphics_context_set_fill_color(ctx, GColorGreen);
      switchToVerticalMode();
  } else if (verticalMode) { // if we are in vertical mode, check if we are within tolerance for the original horizontal mode
      if (!grect_contains_point(&hollowCollisionRectLarge, &ballLoc) && currentAngle < 45.0) {
        returnFromVeritcalMode();
        graphics_context_set_fill_color(ctx, GColorBlue);
      }
  }
  graphics_fill_circle(ctx, ballLoc, ballRad);
}

static void updateBarDisplay(Layer *path, GContext *ctx) {
  graphics_context_set_fill_color(ctx, GColorBlue);
  graphics_fill_rect(ctx, layer_get_bounds(path), 0, GCornerNone);
  
  graphics_context_set_fill_color(ctx, GColorBlack);
  if (currentAngle < 45.0)
    gpath_rotate_to(arrowToObject, currentHeading);
  else
    gpath_rotate_to(arrowToObject, currentHeading-180.0);
  gpath_draw_filled(ctx, arrowToObject);
}

static void loadMainWin(Window *window) {  
  
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  GPoint center = GPoint(bounds.size.w / 2, bounds.size.h / 2);
  
  
  // positions for outer and inner rings
  outerRing = GRect(0, 0, bounds.size.w, bounds.size.h);
  innerRing = GRect(innerRingOffset / 2, innerRingOffset / 2, bounds.size.w - innerRingOffset, bounds.size.h - innerRingOffset);

  // Layer to draw the balls on
  ballLayer = layer_create(bounds);
  barLayer = layer_create(bounds);

  //  Define the draw callback to use for this layer
  layer_set_update_proc(ballLayer, updateBallDisplay);
  layer_add_child(window_layer, ballLayer);
  
  layer_set_update_proc(barLayer, updateBarDisplay);
  layer_add_child(window_layer, barLayer);
  layer_set_hidden(barLayer, true);
  
  arrowToObject = gpath_create(&pointsForArrowToObject);
  gpath_move_to(arrowToObject, center);
  
  angleLayer = text_layer_create(
    GRect(PBL_IF_ROUND_ELSE(40, 12), bounds.size.h * 3 / 4, bounds.size.w, bounds.size.h / 5));
  text_layer_set_text(angleLayer, "No Data");
  layer_add_child(barLayer, text_layer_get_layer(angleLayer));

  // Place text layers onto screen: one for the heading and one for calibration status
  footerLayer = text_layer_create(
    GRect(PBL_IF_ROUND_ELSE(10, 12), bounds.size.h * 3 / 4, bounds.size.w / 2, bounds.size.h / 5));
  text_layer_set_text(footerLayer, "No Data");
  layer_add_child(ballLayer, text_layer_get_layer(footerLayer));

  feedbackLayer = text_layer_create(GRect(0, 0, bounds.size.w, bounds.size.h / 7));
  text_layer_set_text_alignment(feedbackLayer, GTextAlignmentLeft);
  text_layer_set_background_color(feedbackLayer, GColorClear);

  layer_add_child(ballLayer, text_layer_get_layer(feedbackLayer));
}


static void unloadMainWin(Window *window) {
  text_layer_destroy(footerLayer);
  text_layer_destroy(feedbackLayer);
  text_layer_destroy(angleLayer);
  gpath_destroy(arrowToObject);
  layer_destroy(ballLayer);
  layer_destroy(barLayer);
}

static void inbox_dropped_callback(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Message dropped!");
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed!");
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Outbox send success!");
}

static void inbox_received_callback(DictionaryIterator *iterator, void *context) {
  // Store incoming information
  static char name_buffer[32];
  static char elevation_buffer[8];
  static char heading_buffer[8];
  
  // Read tuples for data
  Tuple *name_tuple = dict_find(iterator, KEY_NAME);
  //Tuple *elevation_tuple = dict_find(iterator, KEY_ELEVATION);
  Tuple *heading_tuple = dict_find(iterator, KEY_HEADING);
  
  targetElevation = heading_tuple->value->int8;
  // If all data is available, use it
  //snprintf(name_buffer, sizeof(name_buffer), "%s", name_tuple->value->cstring);
    //snprintf(conditions_buffer, sizeof(conditions_buffer), "%s", conditions_tuple->value->cstring);
  
}

static void initApp() {
  // initialize compass and set a filter to 2 degrees
  compass_service_set_heading_filter(DEG_TO_TRIGANGLE(120));
  compass_service_subscribe(&compassEventHandler);
  
  // Register callbacks
  app_message_register_inbox_received(inbox_received_callback);
  app_message_register_inbox_dropped(inbox_dropped_callback);
  app_message_register_outbox_failed(outbox_failed_callback);
  app_message_register_outbox_sent(outbox_sent_callback);
  
  mainWindow = window_create();
  window_set_window_handlers(mainWindow, (WindowHandlers) {
    .load = loadMainWin,
    .unload = unloadMainWin,
  });
  window_stack_push(mainWindow, true);
}

static void closeApp() {
  compass_service_unsubscribe();
  window_destroy(mainWindow);
}

int main() {
  light_enable(true);
  initApp();
  app_event_loop();
  closeApp();
}