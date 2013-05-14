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
 * 	DS1307	SDL		-> SDL
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

#define SSPEED 	115200

DS1307 ds1307;
DHT22 dht(A1);
DSM501* dsm501 = NULL;
LiquidCrystal lcd(10, 9, A0, 5, 6, 7, 8);

ulong_t lastUpd = 0u;
ulong_t lcdIntv = 1050u;	// 1s

ulong_t lastLog = 0u;
ulong_t logIntv = DSM501_MIN_RATIO_SPAN;

//The setup function is called once at startup of the sketch
void setup() {
	// Initialize DSM501
	if (dsm501 == NULL) {
		dsm501 = DSM501::getInst();
	}
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

// data
struct AirQMsg {
	float temperature;
	float humidity;
	int   dust;
};

int Report(char *buf, bool detail = false) {
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

// The loop function is called in an endless loop
char buf[64]; // display buffer

void loop() {
	// call dsm501 to handle updates.
	dsm501->update();

	if (Serial) {
		if (Serial.available()) {

			int val = Serial.read();
			switch(val) {
			case 'r':
				ds1307.makeStr(buf);
				Serial.print(buf);
				Serial.print(" ");
				Report(buf, true);
				Serial.println(buf);
				break;
			case 't':
				ds1307.makeStr(buf);
				Serial.println(buf);
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

		if (millis() - lastUpd > lcdIntv) {
			lcd.setCursor(0, 0);
			ds1307.makeStr(buf);
			lcd.print(buf);

			lcd.setCursor(0, 1);
			Report(buf);
			lcd.print(buf);

			lastUpd = millis();
		}

		if (millis() - lastLog > logIntv) {
			File dataFile = SD.open("aqi_log.txt", FILE_WRITE);
			if (dataFile) {
				ds1307.makeStr(buf);
				dataFile.print(buf);

				dataFile.print(" "); // delim

				Report(buf, true);
				dataFile.println(buf);

				dataFile.close();
			}
			lastLog = millis();
		}
	}
}
