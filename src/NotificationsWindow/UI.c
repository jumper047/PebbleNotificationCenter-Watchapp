//
// Created by Matej on 20.10.2015.
//

#include <pebble.h>
#include "UI.h"
#include "NotificationStorage.h"
#include "NotificationsWindow.h"
#include "../NotificationCenter.h"
#include "BackgroundLighterLayer.h"

static const int16_t WINDOW_HEIGHT = 168 - 16;

#ifdef PBL_SDK_2
    static InverterLayer* inverterLayer;
#endif

#ifdef PBL_COLOR
static BitmapLayer* notificationBitmapLayer;
static Layer* bitmapShadingLayer;
#endif

static Layer* statusbar;
static TextLayer* statusClock;
static Layer* circlesLayer;
static GBitmap* busyIndicator;
static char clockText[9];

static ScrollLayer* scroll;
static Layer* proxyScrollLayer;
static TextLayer* title;
static TextLayer* subTitle;
static TextLayer* text;

void nw_ui_refresh_notification(void)
{
    char* titleText;
    char* subtitleText = "";
    char* bodyText = "";

    Notification* notification;

    unsigned short additionalYOffset = 0;

    if (numOfNotifications < 1)
    {
        titleText = "No notifications";
        notification = NULL;
    }
    else
    {
        notification = nw_get_displayed_notification();
        titleText = notification->text;

        if (notification->currentTextLength > notification->subtitleStart)
            subtitleText = &notification->text[notification->subtitleStart];

        if (notification->currentTextLength > notification->bodyStart)
            bodyText = &notification->text[notification->bodyStart];

        text_layer_set_font(title, fonts_get_system_font(config_getFontResource(notification->fontTitle)));
        text_layer_set_font(subTitle, fonts_get_system_font(config_getFontResource(notification->fontSubtitle)));
        text_layer_set_font(text, fonts_get_system_font(config_getFontResource(notification->fontBody)));

#ifdef PBL_COLOR
        if (notification->imageSize > 0)
            additionalYOffset = WINDOW_HEIGHT;
#endif
    }


    text_layer_set_text(title, titleText);
    text_layer_set_text(subTitle, subtitleText);
    text_layer_set_text(text, bodyText);

    layer_set_frame(text_layer_get_layer(title), GRect(0, 0, 144 - 4, 30000));
    layer_set_frame(text_layer_get_layer(subTitle), GRect(0, 0, 144 - 4, 30000));
    layer_set_frame(text_layer_get_layer(text), GRect(0, 0, 144 - 4, 30000));

    struct GSize titleSize = text_layer_get_content_size(title);
    struct GSize subtitleSize = text_layer_get_content_size(subTitle);
    struct GSize textSize = text_layer_get_content_size(text);

    titleSize.h += 3;
    subtitleSize.h += 3;
    textSize.h += 5;

    layer_set_frame(text_layer_get_layer(title), GRect(3, additionalYOffset, 144 - 6, titleSize.h));
    layer_set_frame(text_layer_get_layer(subTitle),
                    GRect(3, titleSize.h + 1 + additionalYOffset, 144 - 6, subtitleSize.h));
    layer_set_frame(text_layer_get_layer(text),
                    GRect(3, titleSize.h + 1 + subtitleSize.h + 1 + additionalYOffset, 144 - 6, textSize.h));

    short verticalSize = titleSize.h + 1 + subtitleSize.h + 1 + textSize.h + 5 + additionalYOffset;

    if (additionalYOffset != 0 && verticalSize < WINDOW_HEIGHT * 2)
    {
        verticalSize = WINDOW_HEIGHT * 2;
    }

    layer_set_frame(proxyScrollLayer, GRect(0, 0, 144 - 4, verticalSize));
    scroll_layer_set_content_size(scroll, GSize(144 - 4, verticalSize));

    nw_ui_refresh_picked_indicator();
}

void nw_ui_refresh_picked_indicator(void)
{
    layer_mark_dirty(circlesLayer);
}

void nw_ui_set_busy_indicator(bool value)
{
    busy = value;
    layer_mark_dirty(statusbar);
}


void nw_ui_scroll_to_notification_start(void)
{
    int16_t scrollTo = 0;

    Notification* notification = nw_get_displayed_notification();
    if (notification != NULL)
    {
        if (notification->scrollToEnd)
            scrollTo = -scroll_layer_get_content_size(scroll).h;

#ifdef PBL_COLOR
        if (notification->imageSize > 0)
            scrollTo -= WINDOW_HEIGHT;
#endif
    }

    //First scroll with animation to override animation caused by pressing buttons. Then scroll without animation to speed it up.
    scroll_layer_set_content_offset(scroll, GPoint(0, scrollTo), true);
    scroll_layer_set_content_offset(scroll, GPoint(0, scrollTo), false);
}

void nw_ui_scroll_notification(bool down)
{
    GSize size = scroll_layer_get_content_size(scroll);
    GPoint point = scroll_layer_get_content_offset(scroll);
    point.y += down ? -50 : 50;

    if (point.y < -size.h)
        point.y = -size.h;

    scroll_layer_set_content_offset(scroll, point, true);
}

#ifdef PBL_COLOR

void nw_ui_set_notification_image(GBitmap* image)
{
    bitmap_layer_set_bitmap(notificationBitmapLayer, image);
}

static void on_scroll_changed(ScrollLayer* scrollLayer, void* context)
{
    layer_set_hidden(bitmapShadingLayer, scroll_layer_get_content_offset(scrollLayer).y == 0);
}

#endif

static void statusbarback_paint(Layer* layer, GContext* ctx)
{
    GColor backgroundColor = GColorBlack;
#ifdef PBL_COLOR
    Notification* curNotification = nw_get_displayed_notification();
    if (curNotification != NULL)
        backgroundColor = curNotification->notificationColor;
#endif

    graphics_context_set_fill_color(ctx, backgroundColor);
    graphics_fill_rect(ctx, GRect(0, 0, 144, 16), 0, GCornerNone);

    if (busy)
    {
        graphics_context_set_compositing_mode(ctx, PNG_COMPOSITING_MODE);
        graphics_draw_bitmap_in_rect(ctx, busyIndicator, GRect(80, 3, 9, 10));
    }
}


static void circles_paint(Layer* layer, GContext* ctx)
{
    GColor circlesColor = GColorWhite;
    GColor backgroundColor = GColorBlack;

#ifdef PBL_COLOR
    Notification* curNotification = nw_get_displayed_notification();
    if (curNotification != NULL)
    {
        backgroundColor = curNotification->notificationColor;
        circlesColor = getTextColor(backgroundColor);
    }
#endif


    graphics_context_set_stroke_color(ctx, circlesColor);
    graphics_context_set_fill_color(ctx, circlesColor);

    int x;
    int xDiff;
    int diameter;

    if (numOfNotifications < 7)
    {
        x = 9;
        xDiff = 11;
        diameter = 4;
    }
    else if (numOfNotifications < 9)
    {
        x = 7;
        xDiff = 9;
        diameter = 3;
    }
    else
    {
        x = 5;
        xDiff = 7;
        diameter = 2;
    }

    for (int i = 0; i < numOfNotifications; i++)
    {
        if (pickedNotification == i)
            graphics_fill_circle(ctx, GPoint(x, 8), diameter);
        else
            graphics_draw_circle(ctx, GPoint(x, 8), diameter);

        x += xDiff;
    }

    text_layer_set_background_color(statusClock, backgroundColor);
    text_layer_set_text_color(statusClock, circlesColor);
}


void nw_ui_update_statusbar_clock()
{
    time_t now = time(NULL);
    struct tm* lTime = localtime(&now);

    char* formatString;
    if (clock_is_24h_style())
        formatString = "%H:%M";
    else
        formatString = "%I:%M %p";

    strftime(clockText, 9, formatString, lTime);
    //text_layer_set_text(statusClock, "99:99 PM");
    text_layer_set_text(statusClock, clockText);
}

static TextLayer* init_text_layer()
{
    TextLayer* layer = text_layer_create(
            GRect(0, 0, 0, 0)); //Size is set by notification_refresh() so it is not important here
    text_layer_set_overflow_mode(layer, GTextOverflowModeWordWrap);
    text_layer_set_text_alignment(layer, GTextAlignmentLeft);
    text_layer_set_background_color(layer, GColorClear);
    layer_add_child(proxyScrollLayer, text_layer_get_layer(layer));

    return layer;
}

void nw_ui_load(Window* window)
{
    busyIndicator = gbitmap_create_with_resource(RESOURCE_ID_INDICATOR_BUSY);

    Layer* topLayer = window_get_root_layer(window);

    statusbar = layer_create(GRect(0, 0, 144, 16));
    layer_set_update_proc(statusbar, statusbarback_paint);
    layer_add_child(topLayer, statusbar);

    circlesLayer = layer_create(GRect(0, 0, 144 - 65, 16));
    layer_set_update_proc(circlesLayer, circles_paint);
    layer_add_child(statusbar, circlesLayer);

    statusClock = text_layer_create(GRect(144 - 53, 0, 50, 16));
    text_layer_set_background_color(statusClock, GColorBlack);
    text_layer_set_text_color(statusClock, GColorWhite);
    text_layer_set_font(statusClock, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(statusClock, GTextAlignmentRight);
    layer_add_child(statusbar, (Layer*) statusClock);

#ifdef  PBL_COLOR
    notificationBitmapLayer = bitmap_layer_create(GRect(0, STATUSBAR_Y_OFFSET, 144, WINDOW_HEIGHT));
    bitmap_layer_set_alignment(notificationBitmapLayer, GAlignCenter);
    layer_add_child(topLayer, bitmap_layer_get_layer(notificationBitmapLayer));

    bitmapShadingLayer = layer_create(GRect(0, STATUSBAR_Y_OFFSET, 144, WINDOW_HEIGHT));
    layer_set_update_proc(bitmapShadingLayer, backgroud_lighter_layer_update);
    layer_add_child(topLayer, bitmapShadingLayer);
#endif


    scroll = scroll_layer_create(GRect(0, 16, 144, 168 - 16));
    scroll_layer_set_shadow_hidden(scroll, !config_displayScrollShadow);
    layer_add_child(topLayer, (Layer*) scroll);
#ifdef PBL_COLOR
    scroll_layer_set_callbacks(scroll, (ScrollLayerCallbacks) {.content_offset_changed_handler = on_scroll_changed});
#endif


    proxyScrollLayer = layer_create(
            GRect(0, 0, 0, 0)); //Size is set by notification_refresh() so it is not important here
    scroll_layer_add_child(scroll, proxyScrollLayer);

    title = init_text_layer();
    subTitle = init_text_layer();
    text = init_text_layer();

    #ifdef PBL_SDK_2
        if (config_invertColors)
        {
            inverterLayer = inverter_layer_create(layer_get_frame(topLayer));
            layer_add_child(topLayer, (Layer*) inverterLayer);
        }
    #endif
}

void nw_ui_unload()
{
    layer_destroy(statusbar);
    layer_destroy(circlesLayer);
    text_layer_destroy(title);
    text_layer_destroy(subTitle);
    text_layer_destroy(text);
    text_layer_destroy(statusClock);
    scroll_layer_destroy(scroll);
    gbitmap_destroy(busyIndicator);

    #ifdef PBL_SDK_2
        if (inverterLayer != NULL)
            inverter_layer_destroy(inverterLayer);
    #endif

    #ifdef PBL_COLOR
        bitmap_layer_destroy(notificationBitmapLayer);
    #endif
}