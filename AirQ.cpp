// Do not remove the include below
#include "AirQ.h"
#include "DHT22.h"
#include "DSM501.h"
#include "LiquidCrystal.h"
#include "DS1307.h"
#include "SD.h"

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

ulong_t lastUpd = 0u;
ulong_t lcdIntv = 1000u;	// 1s

ulong_t lastLog = 0u;
ulong_t logIntv = DSM501_MIN_WIN_SPAN;

char buf_t[32]; // time buffer
char buf_d[32]; // display buffer


/***********************************************
 * Components
 ***********************************************/
DS1307 ds1307;
DHT22 dht(DHT22_PIN);
DSM501* dsm501 = NULL;
LiquidCrystal lcd(LCD_RS, LCD_RW, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

/***********************************************
 * Report function
 ***********************************************/
int genReports(char *buf, bool detail = false) {
	char buf1[16];
	int n = 0;

	dtostrf(dht.readTemperature(), 0, 0, buf1);
	n += sprintf(buf + n, "T:%sC ", buf1);

	dtostrf(dht.readHumidity(), 0, 0, buf1);
	n += sprintf(buf + n, "H:%s%% ", buf1);

	if (detail) {
		dtostrf(dsm501->getParticalWeight(0), 0, 2, buf1);
		n += sprintf(buf + n, "P10:%sug/m3 ", buf1);

		dtostrf(dsm501->getParticalWeight(1), 0, 2, buf1);
		n += sprintf(buf + n, "P25:%sug/m3 ", buf1);
	}

	n += sprintf(buf + n, "%4d", dsm501->getAQI());

	return n;
}



/***********************************************
 * Setup
 ***********************************************/
void setup() {
	// Initialize DSM501
	dsm501 = DSM501::getInst();
	dsm501->begin();

	// Initialize DS
	ds1307.begin();

	// LCD
	lcd.begin(16, 2);
	lcd.noAutoscroll();

	// SD
	SD.begin(4);

	// Serial
	Serial.begin(SSPEED);
}


/***********************************************
 * Main Loop
 ***********************************************/
void loop() {
	// call dsm501 to handle updates.
	dsm501->update();

	if (Serial) {
		if (Serial.available()) {

			int val = Serial.read();
			switch(val) {
			case 'r':
				ds1307.makeStr(buf_t);
				Serial.print(buf_t);
				Serial.print(" ");
				genReports(buf_d, true);
				Serial.println(buf_d);
				break;

			case 't':
				ds1307.makeStr(buf_t);
				Serial.println(buf_t);
				break;

			case 'T':
			{

#define ParseOne(v) {		\
	char p = Serial.read();	\
	v = (p - '0') << 4;		\
	p = Serial.read();		\
	v |= (p - '0');			\
	p = Serial.read();		\
}
				int Y, M, D, h, m, s;
				char pm;
				ParseOne(Y);
				ParseOne(M);
				ParseOne(D);
				ParseOne(h);
				ParseOne(m);
				ParseOne(s);
				pm = Serial.read();
				Serial.read();

				ds1307.setDateTimeBCD(Y, M, D, h, m, s, pm == 'P');
				break;
			}

			case 'd':
				Serial.println("--- DEBUG INFO BEGIN ---");
				dsm501->debug();
				ds1307.debug();
				Serial.println("--- DEBUG INFO END ---");
				break;
			}
		}
	} // Serial

	/*
	 * Update LCD every seconds
	 */
	if (millis() - lastUpd > lcdIntv) {
		lcd.setCursor(0, 0);
		ds1307.makeStr(buf_t);
		lcd.print(buf_t);

		lcd.setCursor(0, 1);
		genReports(buf_d);
		lcd.print(buf_d);

		lastUpd = millis();
	}

	/*
	 * Log data to SD card if possible.
	 */
	if (millis() - lastLog > logIntv) {
		File dataFile = SD.open("aqi_log.txt", FILE_WRITE);

		if (dataFile) {
			// since LCD is updating this every sec, we skip this
//			ds1307.makeStr(buf_t);
			dataFile.print(buf_t);

			dataFile.print(" "); // Delimiter

			// log more detail info
			genReports(buf_d, true);
			dataFile.println(buf_d);

			dataFile.close();
		}

		lastLog = millis();
	}
}
