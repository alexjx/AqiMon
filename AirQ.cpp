// Do not remove the include below
#include "AirQ.h"
#include "DHT22.h"
#include "DSM501.h"
#include "LiquidCrystal.h"
#include "DS1307.h"
#include "SD.h"
#include "USBPort.h"

/*
 * Pin definition:
 * 	DHT22	DATA 	-> A1
 * 	DSM501	PM10 	-> A3
 * 			PM25 	-> A2
 * 	DS1307	SDA		-> SDL
 * 			SCL		-> SCL
 * 	LCD1602	RS		-> 10
 * 			RW		-> 9
 *  		E		-> A0
 *  		LED		-> VCC
 *  		D4		-> 5
 *  		D5		-> 6
 *  		D6		-> 7
 *  		D7		-> 8
 *  SD		SS		-> 4
 *  		MISO	-> MISO
 *  		MOSI	-> MOSI
 *  		SCK		-> SCK
 *  UART	RX		-> 0
 *  		TX		-> 1
 */

#define DHT22_PIN	A1
#define DSM501_PM10	A3
#define DSM501_PM25	A2
#define DS1307_SDA	SDA
#define DS1307_SCL	SCL
#define LCD_RS		10
#define LCD_RW		9
#define LCD_E		A0
#define LCD_D4		5
#define LCD_D5		6
#define LCD_D6		7
#define LCD_D7		8
#define SD_CS		4
#define SD_MISO		MISO
#define SD_MOSI		MOSI
#define SD_SCK		SCK

// data
struct AirQMsg {
	float temperature;
	float humidity;
	int   dust;
};


/***********************************************
 * Global Control variable
 ***********************************************/
#define SSPEED 	115200

uint32_t lcd_lu_time = 0u;
uint32_t lcd_tm_Intv = 1000ul;
uint32_t lcd_lu_aqdat = 0u;
uint32_t lcd_ad_Intv = 5000ul;

uint32_t lastLog = 0u;
uint32_t logIntv = DSM501_MIN_WIN_SPAN;

union _FB {
	struct {
		char line1[32]; // time buffer
		char line2[64]; // display buffer
	};
} FB;

uint8_t sd_initialized = false;

/***********************************************
 * Components
 ***********************************************/
DS1307 ds1307;
DHT22 dht(DHT22_PIN);
LiquidCrystal lcd(LCD_RS, LCD_RW, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
DSM501 dsm501(DSM501_PM10, DSM501_PM25);

/***********************************************
 * Report function
 ***********************************************/
int genReports(char *buf, bool detail = false) {
	char buf1[16];
	int n = 0;

	dtostrf(dht.readTemperature(), 0, detail ? 2 : 0, buf1);
	n += sprintf(buf + n, "T:%sC ", buf1);

	dtostrf(dht.readHumidity(), 0, detail ? 2 : 0, buf1);
	n += sprintf(buf + n, "H:%s%% ", buf1);

	if (detail) {
		dtostrf(dsm501.getParticalWeight(0), 0, 2, buf1);
		n += sprintf(buf + n, "P10:%sug/m3 ", buf1);

		dtostrf(dsm501.getParticalWeight(1), 0, 2, buf1);
		n += sprintf(buf + n, "P25:%sug/m3 ", buf1);
	}

	n += sprintf(buf + n, "%4d", dsm501.getAQI());

	return n;
}

void displayTime() {
	lcd.setCursor(0, 0);
	ds1307.makeStr(FB.line1, 31);
	lcd.print(FB.line1);
}

void displaySdState() {
	lcd.setCursor(15, 0);
	if (sd_initialized) {
		lcd.print("*");
	} else {
		lcd.print(" ");
	}
}

void displayAirData() {
	lcd.setCursor(0, 1);
	genReports(FB.line2);
	lcd.print(FB.line2);
}

void lcd_ref_line(int line) {
	if (line == 1) {
		displayTime();
		displaySdState();
	} else if (line == 2) {
		displayAirData();
	}
}

void log2Sd() {
	File dataFile = SD.open("aqi_log.txt", FILE_WRITE);

	if (dataFile) {
		// since LCD is updating this every sec, we skip this
//			ds1307.makeStr(buf_t);
		dataFile.print(FB.line1);

		dataFile.print(" "); // Delimiter

		// log more detail info
		genReports(FB.line2, true);
		dataFile.println(FB.line2);

		dataFile.close();

		sd_initialized = true;
	} else {
		sd_initialized = false;
	}
}

/***********************************************
 * Serial
 ***********************************************/
#define AQI_SER_OK		'O'
#define AQI_SER_ERROR 	'E'
#define AQI_SER_EOP		' '

uint8_t getch() {
	while(!Serial.available());
	return Serial.read();
}

void fill(uint8_t* buff, int n) {
	for (int x = 0; x < n; x++) {
		buff[x] = getch();
	}
}

bool ch_sync() {
	if (AQI_SER_EOP != getch()) {
		Serial.print(AQI_SER_ERROR);
		return false;
	}
	Serial.print(AQI_SER_OK);
	return true;
}

int SerBcdParseByte(uint8_t *&p) {
	int v = (*p++ - '0') << 4;
	v |= (*p++ - '0');
	return v;
}

void procSerial()
{
	uint8_t val = getch();
	switch(val) {
	case 'r':
		ds1307.makeStr(FB.line1, 32);
		Serial.print(FB.line1);
		Serial.print(" ");
		genReports(FB.line2, true);
		Serial.println(FB.line2);
		break;

	case 'C':
		{
			if (!ch_sync())
				return;

			dsm501.setCoeff(Serial.parseInt());

			if (!ch_sync())
				return;

			break;
		}
	case 'c':
		Serial.print("Current DSM501 COEFF:");
		Serial.println(dsm501.getCoeff());
		break;

#ifdef DEBUG
	case 'd':
		dsm501.debug();
		break;
#endif

	case 'T':
		{
			if (!ch_sync())
				return;

			uint8_t buf[12];
			int Y, M, D, h, m, s;

			fill(buf, 12);
			if (!ch_sync())
				return;

			uint8_t *p = buf;
			Y = SerBcdParseByte(p);
			M = SerBcdParseByte(p);
			D = SerBcdParseByte(p);
			h = SerBcdParseByte(p);
			m = SerBcdParseByte(p);
			s = SerBcdParseByte(p);

			ds1307.setDateTimeBCD(Y, M, D, h, m, s);

			ds1307.makeStr(FB.line1, 32);
			Serial.println(FB.line1);
			Serial.print(AQI_SER_EOP);

			break;
		}
	} // end of switch
}

/***********************************************
 * Setup
 ***********************************************/
void setup() {
	// Initialize DSM501
	dsm501.begin();

	// Initialize DS
	ds1307.begin();

	// LCD
	lcd.begin(16, 2);
	lcd.noAutoscroll();

	// SD
	pinMode(SD_CS, OUTPUT);
	if (SD.begin(SD_CS)) {
		sd_initialized = true;
	} else {
		sd_initialized = false;
	}

	// Serial
	Serial.begin(SSPEED);

#ifdef EN_USB
	// USB
	UsbPort.begin();
#endif

	lcd.setCursor(0, 1);
	lcd.print("Initializing...");

	// wait 60s for DSM to warm up
	for (uint32_t now = millis(); now < 60000ul; now = millis()) {
		lcd_ref_line(1);
		delay(10);

		if (Serial.available())
			procSerial();

#ifdef EN_USB
		UsbPort.poll();
#endif
	}

	// Need to reset counter here...
	dsm501.reset();
}


/***********************************************
 * Main Loop
 ***********************************************/
void loop() {
	// call dsm501 to handle updates.
	dsm501.update();

#ifdef EN_USB
	// usb update
	UsbPort.poll();
#endif

	if (Serial) {
		if (Serial.available()) {
			procSerial();
		}
	} // Serial

	uint32_t now = millis();
	/*
	 * Update LCD every seconds
	 */
	if (now - lcd_lu_time > lcd_tm_Intv) {
		lcd_ref_line(1);
		lcd_lu_time = now;
	}

	if (now - lcd_lu_aqdat > lcd_ad_Intv) {
		lcd_ref_line(2);
		lcd_lu_aqdat = now;
	}

	/*
	 * Log data to SD card if possible.
	 */
	if (sd_initialized && now - lastLog > logIntv) {
		log2Sd();
		lastLog = now;
	}
}
