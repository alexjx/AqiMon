#include "DSM501.h"
#include <avr/interrupt.h>

DSM501 DSM501::_instance;

/*
 * These PINs are hard coded to cope with interrupt
 * 	PM10 -> ADC3/PCINT11
 * 	PM25 -> ADC2/PCINT10
 */
const int DSM501::_pin[] = { PM10_PIN, PM25_PIN };

void DSM501::Isr() {
	DSM501* sensor = DSM501::getInst();
	sensor->_pinState[PM10_IDX] = digitalRead(sensor->_pin[PM10_IDX]);
	sensor->_pinState[PM25_IDX] = digitalRead(sensor->_pin[PM25_IDX]);
}

ISR(PCINT1_vect) {
	DSM501::Isr();
}

DSM501::DSM501() {
	for (int i = 0; i < 2; i++) {
		_win_start[i] = 0;
		_low_total[i] = 0;
		_state[i] = S_Idle;
		_pinState[i] = HIGH;
		_sig_start[i] = 0;
		_lastLowRatio[i] = NAN;
	}
}

void DSM501::begin() {
	_win_start[PM10_IDX] = millis() + _mS_By_S(60);	// Stable time
	pinMode(_pin[PM10_IDX], INPUT);

	_win_start[PM25_IDX] = millis() + _mS_By_S(60);
	pinMode(_pin[PM25_IDX], INPUT);

	cli();
	PCICR 	|= 	_BV(1);
	PCMSK1 	|=  _BV(2) | _BV(3);	// ENABLE PIN CHANGE
	sei();
}

void DSM501::update() {
	if (_state[PM10_IDX] == S_Idle && _pinState[PM10_IDX] == LOW) {
		signal_begin(PM10_IDX);
	} else if (_state[PM10_IDX] == S_Start && _pinState[PM10_IDX] == HIGH) {
		signal_end(PM10_IDX);
	}

	if (_state[PM25_IDX] == S_Idle && _pinState[PM25_IDX] == LOW) {
		signal_begin(PM25_IDX);
	} else if (_state[PM25_IDX] == S_Start && _pinState[PM25_IDX] == HIGH) {
		signal_end(PM25_IDX);
	}

#ifdef SERIAL_CHARTING
	static ulong_t _SC_lastReport = 0;
	ulong_t now = millis();
	if ((now - _SC_lastReport) >= 100) { //report every 0.1s
		char buf[32];
		sprintf(buf, "%d,%d", _pinState[PM10_IDX], _pinState[PM25_IDX]);
		Serial.println(buf);
	}
#endif
}


void DSM501::signal_begin(int i) {
	if (millis() >= _win_start[i]) {
		_sig_start[i] = millis();
		_state[i] = S_Start;
	}
}


void DSM501::signal_end(int i) {
	ulong_t now = millis();
	if (_sig_start[i]) { // we had a signal, and
		if ((now - _sig_start[i]) >= DSM501_MIN_SIG_SPAN) {	// this signal is not bouncing.
			_low_total[i] += now - _sig_start[i];
		}
		_sig_start[i] = 0;
	}
	_state[i] = S_Idle;
}

/*
 * Only return the stablized ratio
 */
double DSM501::getLowRatio(int i) {
	ulong_t now = millis();
	ulong_t span = now - _win_start[i];

	// special case if the device run too long, the millis() counter wrap back.
	if (now < _win_start[i]) {
		span = DSM501_MIN_WIN_SPAN;
	}

	if (span >= DSM501_MIN_WIN_SPAN) {
		_lastLowRatio[i] = (double)_low_total[i] / (double)span;

		_win_start[i] = now;
		_low_total[i] = 0;
	}

	return _lastLowRatio[i];
}


ulong_t DSM501::getParticalPcs(int i) {
	return (ulong_t)(getLowRatio(i) / 100.0 * 4.5 * 12500.0);
}


double DSM501::getParticalWeight(int i) {
	/*
	 * with data sheet... regression function is
	 * 	y=-0.158484+0.10085x
	 */
	double weight = -0.158484f + 0.10085f * getLowRatio(i);
	if (weight < 0.0) {
		weight = 0.0;
	}
	return weight;
}

int DSM501::getAQI() {
	// this works only under both pin configure
	int aqi = -1;

	double P25Weight = getParticalWeight(0) - getParticalWeight(1);
	if (P25Weight <= 15.4) {
		aqi = 0   +(int)(50.0 / 15.5 * P25Weight);
	} else if (P25Weight > 15.5 && P25Weight <= 40.5) {
		aqi = 50  + (int)(50.0 / 25.0 * (P25Weight - 15.5));
	} else if (P25Weight > 40.5 && P25Weight <= 65.5) {
		aqi = 100 + (int)(50.0 / 25.0 * (P25Weight - 40.5));
	} else if (P25Weight > 65.5 && P25Weight <= 150.5) {
		aqi = 150 + (int)(50.0 / 85.0 * (P25Weight - 65.5));
	} else if (P25Weight > 150.5 && P25Weight <= 250.5) {
		aqi = 200 + (int)(100.0 / 100.0 * (P25Weight - 150.5));
	} else if (P25Weight > 250.5 && P25Weight <= 350.5) {
		aqi = 300 + (int)(100.0 / 100.0 * (P25Weight - 250.5));
	} else if (P25Weight > 350.5 && P25Weight <= 500.0) {
		aqi = 400 + (int)(100.0 / 150.0 * (P25Weight - 350.5));
	} else if (P25Weight > 500.0) {
		aqi = 500 + (int)(500.0 / 500.0 * (P25Weight - 500.0)); // Extension
	} else {
		aqi = -1; // Initializing
	}

	return aqi;
}


void DSM501::debug(void)
{
	Serial.println("--- DSM501 BEGIN ---");

	Serial.println(_win_start[PM10_IDX]);
	Serial.println(DSM501_MIN_WIN_SPAN);
	Serial.println(DSM501_MIN_WIN_SPAN - (millis() - _win_start[PM10_IDX]));

	Serial.println(getLowRatio(PM10_IDX));
	Serial.println(getLowRatio(PM25_IDX));

	Serial.println(_low_total[PM10_IDX]);
	Serial.println(_low_total[PM25_IDX]);
}
