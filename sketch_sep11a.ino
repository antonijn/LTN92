#include <string.h>
#include <Bounce2.h>
#include <TFT_eSPI.h>

#include "my_font.h"

// Power up blink parameters
#define HAPPY_BLINK_INTERVAL    150
#define HAPPY_BLINK_NUM           2
#define LTN_LED_PIN              23

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

static void array_to_tft(char *buf)
{
	char line_buf[17];
	line_buf[16] = 0;
	tft.fillScreen(TFT_BLACK);
	tft.setCursor(10, 10, 4);
	for (int i = 0; i < 5; ++i) {
		memcpy(line_buf, buf + i * 16, 16);
		tft.println(line_buf);
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

	if (btn_a_n_overlong_press) {
		state = STATE_CDU_SELECT;
		return;
	}

	display_to_array(buf_old);

	FlightSim.update();

	if (!FlightSim.isEnabled() || ltn->unit_power == 0) {
		state = STATE_IDLE;
		return;
	}

	display_to_array(buf_new);

	analogWrite(LTN_LED_PIN, ltn->brightness * 255);
	if (prev_state != STATE_ACTIVE || memcmp(buf_old, buf_new, sizeof(buf_old)) != 0)
		array_to_tft(buf_new);

	update_annunciators();

	transmit_buttons(btn_letters, ltn->but_letters, 28);
	transmit_buttons(btn_side, ltn->but_side, 3);
}

static void cdu_select()
{
	if (1) {
		state = STATE_ACTIVE;

		// any timer running on button A_N must not be carried
		// over into next mode
		checking_a_n_overlong = false;
		return;
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
