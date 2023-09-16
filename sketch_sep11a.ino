#include <string.h>
#include <Bounce2.h>
#include <TFT_eSPI.h>

#include "my_font.h"

// Power up blink parameters
#define HAPPY_BLINK_INTERVAL    150
#define HAPPY_BLINK_NUM           2
#define LTN_LED_PIN              23

#define LTN_AMBER                  0xFEA9
#define LTN_GREEN                  0xAFC9
#define LTN_CYAN                   TFT_CYAN

// Keyboard debouncing interval
#define BTN_DEBOUNCE_INTERVAL    25

#define OVERLONG_THRESHOLD     2000

// Diode matrix row pins
#define ROW1_PIN                  7
#define ROW2_PIN                  4
#define ROW3_PIN                  3
#define ROW4_PIN                  2

// Diode matrix column pins
#define COL1_PIN                 17
#define COL2_PIN                 16
#define COL3_PIN                 15
#define COL4_PIN                 14
#define COL5_PIN                 10
#define COL6_PIN                  9
#define COL7_PIN                  8
#define COL8_PIN                  1

// Status LEDs
#define WRN_PIN                  19
#define BAT_PIN                  18
#define ALR_PIN                   5
#define OFS_PIN                   6

// Special buttons
#define BTN_SLEW_UP              17
#define BTN_SLEW_DOWN            24
#define BTN_A_N                   2
#define BTN_ENT                   0

TFT_eSPI tft;

FlightSimInteger ltn_color_option;

struct ltn_cdu {
	FlightSimInteger unit_power, input_active;
	FlightSimFloat brightness;
	FlightSimData display_text_lines[5];

	FlightSimInteger but_letters[28];
	FlightSimInteger but_side[3];

	FlightSimInteger annunciators[4];
};

struct ltn_cdu ltns[3];
struct ltn_cdu *ltn;

const int annunciator_pins[4] = {
	WRN_PIN, BAT_PIN, ALR_PIN, OFS_PIN,
};

const int row_pins[4] = {
	ROW1_PIN, ROW2_PIN, ROW3_PIN, ROW4_PIN,
};
const int col_pins[8] = {
	COL1_PIN, COL2_PIN, COL3_PIN, COL4_PIN,
	COL5_PIN, COL6_PIN, COL7_PIN, COL8_PIN,
};

Bounce btn_letters[28];
Bounce btn_side[3];
bool checking_a_n_overlong, btn_a_n_overlong_press;

enum program_state {
	STATE_OFF,
	STATE_IDLE,
	STATE_ACTIVE,
	STATE_CDU_SELECT,
};

program_state prev_state, state;

static char *dup_sprintf(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	char buf[64];
	vsprintf(buf, fmt, ap);

	va_end(ap);
	return strdup(buf);
}

static void ltn_init(int idx)
{
	ltn_cdu *cdu = &ltns[idx];

	cdu->unit_power   = XPlaneRef(dup_sprintf("B742/LTN_INT/unit_power[%d]", idx));
	cdu->input_active = XPlaneRef(dup_sprintf("B742/LTN/input_active[%d]", idx));
	cdu->brightness   = XPlaneRef(dup_sprintf("B742/LTN_INT/brightness[%d]", idx));

	cdu->annunciators[0] = XPlaneRef(dup_sprintf("B742/LTN%d/WRN_lamp", idx+1));
	cdu->annunciators[1] = XPlaneRef(dup_sprintf("B742/LTN%d/BAT_lamp", idx+1));
	cdu->annunciators[2] = XPlaneRef(dup_sprintf("B742/LTN%d/ALR_lamp", idx+1));
	cdu->annunciators[3] = XPlaneRef(dup_sprintf("B742/LTN%d/OFS_lamp", idx+1));

	for (int i = 0; i < 5; ++i) {
		char *ref = dup_sprintf("B742/LTN%d/display_text_line%d", idx+1, i+1);
		cdu->display_text_lines[i] = XPlaneRef(ref);
	}

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 7; ++j) {
			char *ref = dup_sprintf("B742/LTN%d/but_letters[%d]", idx+1, i * 7 + j);
			cdu->but_letters[i * 7 + j] = XPlaneRef(ref);
		}
	}

	cdu->but_side[0] = XPlaneRef(dup_sprintf("B742/LTN%d/but_ENT", idx+1));
	cdu->but_side[1] = XPlaneRef(dup_sprintf("B742/LTN%d/but_CLR", idx+1));
	cdu->but_side[2] = XPlaneRef(dup_sprintf("B742/LTN%d/but_A_N", idx+1));
}

static void keyboard_init()
{
	for (int i = 0; i < 4; ++i) {
		// high impedance mode
		pinMode(row_pins[i], INPUT);
	}

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 7; ++j) {
			btn_letters[i * 7 + j].attach(col_pins[j], INPUT_PULLUP);
			btn_letters[i * 7 + j].interval(BTN_DEBOUNCE_INTERVAL);
		}
	}

	for (int i = 0; i < 3; ++i) {
		btn_side[i].attach(col_pins[7], INPUT_PULLUP);
		btn_side[i].interval(BTN_DEBOUNCE_INTERVAL);
	}
}

static void happy_blink()
{
	bool state = 0;
	for (int i = 0; i < 2 * HAPPY_BLINK_NUM; ++i) {
		state = !state;
		for (int i = 0; i < 4; ++i)
			digitalWrite(annunciator_pins[i], state);
		delay(HAPPY_BLINK_INTERVAL);
	}
}

void setup()
{
	ltn_color_option = XPlaneRef("B742/LTN/color_option");
	for (int i = 0; i < 3; ++i)
		ltn_init(i);
	ltn = &ltns[0];

	for (int i = 0; i < 4; ++i)
		pinMode(annunciator_pins[i], OUTPUT);

	keyboard_init();

	prev_state = STATE_OFF;
	state = STATE_IDLE;

	tft.init();
	tft.setRotation(3);
	tft.fillScreen(TFT_BLACK);

	// Font 4
	tft.setCursor(0, 0, 4);
	tft.setTextColor(TFT_YELLOW,TFT_BLACK);
	tft.setTextSize(1);

	happy_blink();
}

static const uint8_t *glyph(char input)
{
	if (input >= 'A' && input <= 'Z')
		return LTR(input - 'A');

	if (input >= '0' && input <= '9')
		return NUM(input - '0');

	switch (input) {
	case ' ':
		return SYM(0);
	case '!':
		return SYM(1);
	case '"':
		return SYM(2);
	case '#':
		return SYM(3);
	case '\'':
		return SYM(4);
	case '*':
		return SYM(5);
	case '+':
		return SYM(6);
	case ',':
		return SYM(7);
	case '-':
		return SYM(8);
	case '.':
		return SYM(9);
	case '/':
		return SYM(10);
	case ':':
		return SYM(11);
	case ';':
		return SYM(12);
	case '<':
		return SYM(13);
	case '=':
		return SYM(14);
	case '>':
		return SYM(15);
	case '?':
		return SYM(16);
	case '[':
		return SYM(17);
	case '\\':
		return SYM(18);
	case ']':
		return SYM(19);
	case '_':
		return SYM(20);
	case 'a':
		return SYM(21);
	case 'b':
		return SYM(22);
	case 'f':
		return SYM(23);
	case 'l':
		return SYM(24);
	case 'r':
		return SYM(25);
	case 's':
		return SYM(26);
	case 't':
		return SYM(27);
	case '|':
		return SYM(28);
	default:
		return nullptr;
	}
}

static void power_off_annunciators()
{
	for (int i = 0; i < 4; ++i)
		digitalWrite(annunciator_pins[i], LOW);
}

static void idle()
{
	if (prev_state != STATE_IDLE) {
		digitalWrite(LTN_LED_PIN, LOW);
		power_off_annunciators();
	}

	if (btn_a_n_overlong_press) {
		state = STATE_CDU_SELECT;
		return;
	}

	FlightSim.update();

	if (FlightSim.isEnabled() && ltn->unit_power != 0)
		state = STATE_ACTIVE;
}

static void update_annunciators()
{
	for (int i = 0; i < 4; ++i)
		digitalWrite(annunciator_pins[i], ltn->annunciators[i] != 0);
}

static void display_to_array(char *buf)
{
	memset(buf, ' ', 5 * 16);
	for (int i = 0; i < 5; ++i) {
		size_t len = ltn->display_text_lines[i].len();
		if (len > 16)
			len = 16;
		len = strnlen(ltn->display_text_lines[i], len);
		memcpy(buf + i * 16, ltn->display_text_lines[i], len);
	}
}

static void diff_to_tft(char *buf_old, char *buf_new, uint32_t fg)
{
	const int dx = 7;
	const int dy = 16;
	const int offs_x = (480 - (GLYPH_WIDTH  + dx) * 16 + dx) / 2;
	const int offs_y = (320 - (GLYPH_HEIGHT + dy) *  5 + dy) / 2;

	for (int i = 0; i < 5; ++i) {
		for (int j = 0; j < 16; ++j) {
			int k = i * 16 + j;
			if (buf_old[k] == buf_new[k])
				continue;

			int x = offs_x + j * (GLYPH_WIDTH  + dx);
			int y = offs_y + i * (GLYPH_HEIGHT + dy);
			const uint8_t *g = glyph(buf_new[k]);
			if (g != nullptr)
			  tft.drawBitmap(x, y, g, GLYPH_WIDTH, GLYPH_HEIGHT, fg, TFT_BLACK);
		}
	}
}

static void draw_entry_line_marker(bool enabled)
{
	const int spot_x1 = 10;
	const int spot_x2 = 480 - spot_x1;
	const int spot_y = 320 / 2;
	const int spot_r = 5;
	if (enabled) {
		tft.drawSpot(spot_x1, spot_y, spot_r, LTN_AMBER, TFT_BLACK);
		tft.drawSpot(spot_x2, spot_y, spot_r, LTN_AMBER, TFT_BLACK);
	} else {
		tft.fillRect(spot_x1 - spot_r, spot_y - spot_r, spot_r*2+1, spot_r*2+1, TFT_BLACK);
		tft.fillRect(spot_x2 - spot_r, spot_y - spot_r, spot_r*2+1, spot_r*2+1, TFT_BLACK);
	}
}

static void transmit_buttons(Bounce *btns, FlightSimInteger *ltn_buts, int num)
{
	for (int i = 0; i < num; ++i) {
		if (btns[i].changed())
			ltn_buts[i] = !btns[i].read();
	}
}

static void update()
{
	char buf_old[5 * 16];
	char buf_new[5 * 16];
	static bool was_input_active;
	static int prev_color;

	if (btn_a_n_overlong_press) {
		state = STATE_CDU_SELECT;
		return;
	}

	if (prev_state == STATE_ACTIVE) {
		display_to_array(buf_old);
	} else {
		tft.fillScreen(TFT_BLACK);
		memset(buf_old, 0, sizeof(buf_old));
		was_input_active = false;
	}

	FlightSim.update();

	if (!FlightSim.isEnabled() || ltn->unit_power == 0) {
		state = STATE_IDLE;
		return;
	}

	display_to_array(buf_new);

	if (prev_color != ltn_color_option) {
		prev_color = ltn_color_option;

		// invalidate whole screen
		memset(buf_old, 0, sizeof(buf_old));
	}

	analogWrite(LTN_LED_PIN, ltn->brightness * 255);
	if (memcmp(buf_old, buf_new, sizeof(buf_old)) != 0)
		diff_to_tft(buf_old, buf_new, (ltn_color_option == 0) ? LTN_GREEN : LTN_AMBER);

	if (ltn->input_active != was_input_active) {
		draw_entry_line_marker(ltn->input_active);
		was_input_active = ltn->input_active;
	}

	update_annunciators();

	transmit_buttons(btn_letters, ltn->but_letters, 28);
	transmit_buttons(btn_side, ltn->but_side, 3);
}

static void cdu_select()
{
	static char buf_old[5 * 16];
	static char buf_new[] =
		" SELECT ACT CDU "
		"                "
		"                "
		"                "
		"                ";
	static int was_selected;
	static program_state return_state;

	FlightSim.update();

	if (prev_state != STATE_CDU_SELECT) {
		was_selected = (int)(ltn - ltns);
		tft.fillScreen(TFT_BLACK);
		memset(buf_old, 0, sizeof(buf_old));
		draw_entry_line_marker(true);
		return_state = prev_state;

		if (prev_state != STATE_ACTIVE)
			digitalWrite(LTN_LED_PIN, HIGH);
	}

	int selected = was_selected;

	if (btn_letters[BTN_SLEW_UP].fell()) {
		++selected;
		if (selected > 2)
			selected = 2;
	}

	if (btn_letters[BTN_SLEW_DOWN].fell()) {
		--selected;
		if (selected < 0)
			selected = 0;
	}

	if (btn_side[BTN_ENT].fell()) {
		state = return_state;
		ltn = &ltns[selected];

		// any timer running on button A_N must not be carried
		// over into next mode
		checking_a_n_overlong = false;
		return;
	}

	if (prev_state != STATE_CDU_SELECT || selected != was_selected) {
		// invalidate screen
		for (int i = 1; i < 5; ++i) {
			// offset 5 to centre
			char *target = buf_new + i * 16 + 5;

			int disp = selected + i - 1;
			if (disp >= 1 && disp <= 3) {
				memcpy(target, "CDU ", 4);
				target[4] = '0' + disp;
			} else {
				memcpy(target, "     ", 5);
			}
		}

		diff_to_tft(buf_old, buf_new, LTN_CYAN);
		memcpy(buf_old, buf_new, sizeof(buf_old));
		was_selected = selected;
	}
}

static void scan_row(int row, Bounce *btn_7)
{
	int pin = row_pins[row];
	pinMode(pin, OUTPUT);
	digitalWrite(pin, LOW);

	for (int j = 0; j < 7; ++j)
		btn_letters[row * 7 + j].update();

	if (btn_7 != nullptr)
		btn_7->update();

	digitalWrite(pin, HIGH);
	pinMode(pin, INPUT);
}

static void scan()
{
	static unsigned long time_ref;

	scan_row(0, nullptr);
	for (int i = 1; i < 4; ++i)
		scan_row(i, &btn_side[i - 1]);

	btn_a_n_overlong_press = false;

	if (checking_a_n_overlong) {
		if (btn_side[BTN_A_N].read()) {
			checking_a_n_overlong = false;
		} else if (millis() - time_ref > OVERLONG_THRESHOLD) {
			btn_a_n_overlong_press = true;
			checking_a_n_overlong = false;
		}
	} else if (btn_side[BTN_A_N].fell()) {
		checking_a_n_overlong = true;
		time_ref = millis();
	}
}

void loop()
{
	program_state temp = state;

	scan();

	switch (state)
	{
	case STATE_OFF:
		state = STATE_IDLE;
		break;
	case STATE_IDLE:
		idle();
		break;
	case STATE_ACTIVE:
		update();
		break;
	case STATE_CDU_SELECT:
		cdu_select();
		break;
	}

	prev_state = temp;
}
