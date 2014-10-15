#include <pebble.h>
#include <pebble_fonts.h>
#include "NotificationCenter.h"

#define LIST_STORAGE_SIZE 10

Window* listWindow;

uint16_t numEntries = 0;

int8_t arrayCenterPos = 0;
int16_t centerIndex = 0;

int16_t pickedEntry = -1;
uint8_t pickedMode = 0;

bool ending = false;
char** titles;
char** subtitles;
uint8_t* types;
char** dates;

MenuLayer* listMenuLayer;
static InverterLayer* inverterLayer;

GBitmap* normalNotification;
GBitmap* ongoingNotification;

int8_t convertToArrayPos(uint16_t index)
{
	int16_t indexDiff = index - centerIndex;
	if (indexDiff > LIST_STORAGE_SIZE / 2 || indexDiff < -LIST_STORAGE_SIZE / 2)
		return -1;

	int8_t arrayPos = arrayCenterPos + indexDiff;
	if (arrayPos < 0)
		arrayPos += LIST_STORAGE_SIZE;
	if (arrayPos > LIST_STORAGE_SIZE - 1)
		arrayPos -= LIST_STORAGE_SIZE;

	return arrayPos;
}

char* getTitle(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return titles[arrayPos];
}

void setTitle(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(titles[arrayPos],name);
}

char* getSubtitle(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return subtitles[arrayPos];
}

void setSubtitle(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(subtitles[arrayPos],name);
}

uint8_t getType(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return 0;

	return types[arrayPos];
}

void setType(uint16_t index, uint8_t type)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	types[arrayPos] = type;
}

char* getDate(uint16_t index)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return "";

	return dates[arrayPos];
}

void setDate(uint16_t index, char *name)
{
	int8_t arrayPos = convertToArrayPos(index);
	if (arrayPos < 0)
		return;

	strcpy(dates[arrayPos],name);
}

void shiftArray(int newIndex)
{
	int8_t clearIndex;

	int16_t diff = newIndex - centerIndex;
	if (diff == 0)
		return;

	centerIndex+= diff;
	arrayCenterPos+= diff;

	if (diff > 0)
	{
		if (arrayCenterPos > LIST_STORAGE_SIZE - 1)
			arrayCenterPos -= LIST_STORAGE_SIZE;

		clearIndex = arrayCenterPos - LIST_STORAGE_SIZE / 2;
		if (clearIndex < 0)
			clearIndex += LIST_STORAGE_SIZE;
	}
	else
	{
		if (arrayCenterPos < 0)
			arrayCenterPos += LIST_STORAGE_SIZE;

		clearIndex = arrayCenterPos + LIST_STORAGE_SIZE / 2;
		if (clearIndex > LIST_STORAGE_SIZE - 1)
			clearIndex -= LIST_STORAGE_SIZE;
	}

	*titles[clearIndex] = 0;
	*subtitles[clearIndex] = 0;
	types[clearIndex] = 0;
	*dates[clearIndex] = 0;

}

uint8_t getEmptySpacesDown()
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i <= centerIndex + LIST_STORAGE_SIZE / 2; i++)
	{
		if (i >= numEntries)
			return LIST_STORAGE_SIZE;

		if (*getTitle(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

uint8_t getEmptySpacesUp()
{
	uint8_t spaces = 0;
	for (int i = centerIndex; i >= centerIndex - LIST_STORAGE_SIZE / 2; i--)
	{
		if (i < 0)
			return LIST_STORAGE_SIZE;

		if (*getTitle(i) == 0)
		{
			break;
		}

		spaces++;
	}

	return spaces;
}

void allocateData()
{
	titles = malloc(sizeof(int*) * LIST_STORAGE_SIZE);
	subtitles = malloc(sizeof(int*) * LIST_STORAGE_SIZE);
	dates = malloc(sizeof(int*) * LIST_STORAGE_SIZE);
	types = malloc(sizeof(uint8_t) * LIST_STORAGE_SIZE);

	for (int i = 0; i < LIST_STORAGE_SIZE; i++)
	{
		titles[i] = malloc(sizeof(char) * 21);
		subtitles[i] = malloc(sizeof(char) * 21);
		dates[i] = malloc(sizeof(char) * 21);

		*titles[i] = 0;
		*subtitles[i] = 0;
		*dates[i] = 0;
	}
}

void freeData()
{
	for (int i = 0; i < LIST_STORAGE_SIZE; i++)
	{
		free(titles[i]);
		free(subtitles[i]);
		free(dates[i]);
	}

	free(titles);
	free(subtitles);
	free(dates);
	free(types);
}

void requestNotification(uint16_t pos)
{
	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 4);
	dict_write_uint16(iterator, 1, pos);
	app_message_outbox_send();

	ending = true;
}

void sendpickedEntry(int16_t pos, uint8_t mode)
{
	if (ending)
	{
		pickedEntry = pos;
		pickedMode = mode;

		return;
	}

	app_comm_set_sniff_interval(SNIFF_INTERVAL_REDUCED);

	DictionaryIterator *iterator;
	app_message_outbox_begin(&iterator);
	dict_write_uint8(iterator, 0, 5);
	dict_write_int16(iterator, 1, pos);
	app_message_outbox_send();
}

void requestAdditionalEntries()
{
	if (ending)
		return;

	int emptyDown = getEmptySpacesDown();
	int emptyUp = getEmptySpacesUp();

	if (emptyDown < 6 && emptyDown <= emptyUp)
	{
		uint8_t startingIndex = centerIndex + emptyDown;
		requestNotification(startingIndex);
	}
	else if (emptyUp < 6)
	{
		uint8_t startingIndex = centerIndex - emptyUp;
		requestNotification(startingIndex);
	}
}

uint16_t menu_get_num_sections_callback(MenuLayer *me, void *data) {
	return 1;
}

uint16_t menu_get_num_rows_callback(MenuLayer *me, uint16_t section_index, void *data) {
	return numEntries;
}


int16_t menu_get_row_height_callback(MenuLayer *me,  MenuIndex *cell_index, void *data) {
	return 56;
}

void menu_pos_changed(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context)
{
	shiftArray(new_index.row);
	requestAdditionalEntries();
}


void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data) {
	graphics_context_set_text_color(ctx, GColorBlack);

	graphics_draw_text(ctx, getTitle(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD), GRect(34, 0, 144 - 35, 21), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
	graphics_draw_text(ctx, getSubtitle(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_14), GRect(34, 21, 144 - 35, 17), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
	graphics_draw_text(ctx, getDate(cell_index->row), fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD), GRect(34, 39, 144 - 35, 18), GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

	GBitmap* image;
	switch (getType(cell_index->row))
	{
	case 1:
		image = ongoingNotification;
		break;
	default:
		image = normalNotification;
		break;
	}

	graphics_draw_bitmap_in_rect(ctx, image, GRect(1, 14, 31, 31));
}


void menu_select_callback(MenuLayer *me, MenuIndex *cell_index, void *data) {
	sendpickedEntry(cell_index->row, 0);
}

void receivedEntries(DictionaryIterator* data)
{
	uint16_t offset = dict_find(data, 1)->value->uint16;
	numEntries = dict_find(data, 2)->value->uint16;

	setType(offset, dict_find(data, 3)->value->uint8);
	setTitle(offset, dict_find(data, 4)->value->cstring);
	setSubtitle(offset, dict_find(data, 5)->value->cstring);
	setDate(offset, dict_find(data, 6)->value->cstring);

	menu_layer_reload_data(listMenuLayer);
	ending = false;

	if (pickedEntry >= 0)
	{
		sendpickedEntry(pickedEntry, pickedMode);
		return;
	}
	requestAdditionalEntries();
}

void list_data_received(int packetId, DictionaryIterator* data)
{
	switch (packetId)
	{
	case 2:
		receivedEntries(data);
		break;

	}

}

void list_window_load(Window *me) {
	normalNotification = gbitmap_create_with_resource(RESOURCE_ID_ICON);
	ongoingNotification = gbitmap_create_with_resource(RESOURCE_ID_COGWHEEL);

	Layer* topLayer = window_get_root_layer(listWindow);

	listMenuLayer = menu_layer_create(GRect(0, 0, 144, 168 - 16));

	// Set all the callbacks for the menu layer
	menu_layer_set_callbacks(listMenuLayer, NULL, (MenuLayerCallbacks){
		.get_num_sections = menu_get_num_sections_callback,
				.get_num_rows = menu_get_num_rows_callback,
				.get_cell_height = menu_get_row_height_callback,
				.draw_row = menu_draw_row_callback,
				.select_click = menu_select_callback,
				.selection_changed = menu_pos_changed
	});

	menu_layer_set_click_config_onto_window(listMenuLayer, listWindow);

	layer_add_child(topLayer, (Layer*) listMenuLayer);

	if (config_invertColors)
	{
		inverterLayer = inverter_layer_create(layer_get_frame(topLayer));
		layer_add_child(topLayer, (Layer*) inverterLayer);
	}

	arrayCenterPos = 0;
	centerIndex = 0;
}

void list_window_unload(Window *me) {
	gbitmap_destroy(normalNotification);
	gbitmap_destroy(ongoingNotification);

	menu_layer_destroy(listMenuLayer);

	if (inverterLayer != NULL)
		inverter_layer_destroy(inverterLayer);

	window_destroy(me);
}

void list_window_appear(Window* me)
{
	allocateData();
	setCurWindow(2);

	ending = false;
	pickedEntry = -1;
	pickedMode = 0;

	requestAdditionalEntries();
}

void list_window_disappear(Window* me)
{
	freeData();
}

void init_notification_list_window()
{
	listWindow = window_create();

	window_set_window_handlers(listWindow, (WindowHandlers){
		.appear = list_window_appear,
	    .disappear = list_window_disappear,
		.load = list_window_load,
		.unload = list_window_unload


	});

	window_set_fullscreen(listWindow, false);

	window_stack_push(listWindow, true /* Animated */);
}

