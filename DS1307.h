/*
 * DS1307.h
 *
 *  Created on: May 9, 2013
 *      Author: xinj
 */

#ifndef DS1307_H_
#define DS1307_H_
#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

#include <Wire.h>

#define DS1307_I2C_ADDR 0x68

class DS1307 {
public:
	enum {
		M_AM,
		M_PM,
		M_24 = 0x100,
	};

public:
	DS1307();
	void begin();

	void setDateTimeBCD(int y, int M, int d, int h, int m, int s, bool pm);
	void updateDateTime();
	void makeStr(char* buf);

	void debug();

//	void store(int addr, uint8_t v);
//	void retrieve(int addr, uint8_t v);

public:
	int sec;
	int min;
	int hour;
	int dow;
	int day;
	int month;
	int year;

	int m;

protected:
	void parseData(byte* buf);
	void readRawData(byte* buf);
};

#endif /* DS1307_H_ */
