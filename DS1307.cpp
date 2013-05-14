/*
 * DS1307.cpp
 *
 *  Created on: May 9, 2013
 *      Author: xinj
 */

#include "DS1307.h"

#define DS_SEC_OFF	0
#define DS_MIN_OFF	1
#define DS_HOU_OFF	2
#define DS_DOW_OFF	3
#define DS_DAT_OFF	4
#define DS_MON_OFF	5
#define DS_YEA_OFF	6

DS1307::DS1307() {
}

void DS1307::begin() {
	Wire.begin();
}

void DS1307::setDateTimeBCD(int y, int M, int d, int h, int m, int s, bool pm) {
	byte buf[7];

	buf[DS_YEA_OFF] = y & 0x7fu;
	buf[DS_MON_OFF] = M & 0x1fu;
	buf[DS_DAT_OFF] = d & 0x3fu;
	buf[DS_HOU_OFF] = h & 0x1fu;
	if (pm)
		buf[DS_HOU_OFF] |= 0x20u;
	buf[DS_MIN_OFF] = m & 0x7fu;
	buf[DS_SEC_OFF] = s & 0x7fu;

	Wire.beginTransmission(DS1307_I2C_ADDR);
	Wire.write(0);
	Wire.write(buf, 7);
	Wire.endTransmission();

	updateDateTime();
}

void DS1307::debug() {
	char buf[32];
	Serial.println("--- DS1307 BEGIN ---");
	// reset internal address
	Wire.beginTransmission(DS1307_I2C_ADDR);
	Wire.write(0);
	Wire.endTransmission();

	Wire.requestFrom(DS1307_I2C_ADDR, 7);
	for (int i = 0; i < 7; ) {
		sprintf(buf, "%02x: %02x", i++, Wire.read());
		Serial.println(buf);
	}
}

void DS1307::readRawData(byte* buf) {
	// reset internal address
	Wire.beginTransmission(DS1307_I2C_ADDR);
	Wire.write(0);
	Wire.endTransmission();

	Wire.requestFrom(DS1307_I2C_ADDR, 7);
	for (int i = 0; i < 7; ) {
		buf[i++] = Wire.read();
	}
}

void DS1307::updateDateTime() {
	byte buf[7];
	readRawData(buf);
	parseData(buf);
}

void DS1307::makeStr(char* buf) {
	updateDateTime();

	sprintf(buf, "%02d/%02d %02d:%02d:%02d%s",
			month, day, hour, min, sec,
			(m == M_24) ? "" : (m == M_AM) ? "AM" : "PM");
}

#define BCD_Byte(x, h, l) (((x) & (l)) + (((x) & ((h) << 4)) >> 4) * 10)
#define IS_24H(x)	((x) & 0x40)
#define IS_PM(x)	((x) & 0x20)

void DS1307::parseData(byte *buf) {
	sec = BCD_Byte(buf[DS_SEC_OFF], 0x7u, 0xfu);
	min = BCD_Byte(buf[DS_MIN_OFF], 0x7u, 0xfu);
	hour = BCD_Byte(buf[DS_HOU_OFF],0x1u, 0xfu);
	if (IS_24H(buf[DS_HOU_OFF])) {
		m = M_24;
	} else if (IS_PM(buf[DS_HOU_OFF])){
		m = M_PM;
	} else {
		m = M_AM;
	}

	dow = buf[DS_DOW_OFF] & 0x7u;
	day = BCD_Byte(buf[DS_DAT_OFF], 0x3u, 0xfu);
	month = BCD_Byte(buf[DS_MON_OFF], 0x1u, 0xfu);
	year = BCD_Byte(buf[DS_YEA_OFF], 0xfu, 0xfu);
}
