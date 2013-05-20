#include "DSM501.h"
#include <avr/interrupt.h>


uint8_t dsm501_coeff = 1;

DSM501::DSM501(int pin10, int pin25) {
	_pin[PM10_IDX] = pin10;
	_pin[PM25_IDX] = pin25;

	for (int i = 0; i < 2; i++) {
		_win_start[i] = 0;
		_low_total[i] = 0;
		_state[i] = S_Idle;
		_sig_start[i] = 0;
		_lastLowRatio[i] = NAN;

		_saf_sum[i] = 0;
		memset(_saf_ent[i], 0, SAF_WIN_MAX * sizeof(uint32_t));
		_saf_idx[i] = 0;
	}
}


void DSM501::begin() {
	_coeff = dsm501_coeff;

	_win_start[PM10_IDX] = millis();
	pinMode(_pin[PM10_IDX], INPUT);

	_win_start[PM25_IDX] = millis();
	pinMode(_pin[PM25_IDX], INPUT);
}


void DSM501::reset() {
	_win_start[PM10_IDX] = millis();
	_win_start[PM25_IDX] = millis();
}

uint8_t DSM501::setCoeff(uint8_t coeff) {
	_coeff = coeff;
	return _coeff;
}


void DSM501::update() {
	if (_state[PM10_IDX] == S_Idle && digitalRead(_pin[PM10_IDX]) == LOW) {
		signal_begin(PM10_IDX);
	} else if (_state[PM10_IDX] == S_Start && digitalRead(_pin[PM10_IDX]) == HIGH) {
		signal_end(PM10_IDX);
	}

	if (_state[PM25_IDX] == S_Idle && digitalRead(_pin[PM25_IDX]) == LOW) {
		signal_begin(PM25_IDX);
	} else if (_state[PM25_IDX] == S_Start && digitalRead(_pin[PM25_IDX]) == HIGH) {
		signal_end(PM25_IDX);
	}
}


void DSM501::signal_begin(int i) {
	if (millis() >= _win_start[i]) {
		_sig_start[i] = millis();
		_state[i] = S_Start;
	}
}


void DSM501::signal_end(int i) {
	uint32_t now = millis();
	if (_sig_start[i]) { // we had a signal, and
		if ((now - _sig_start[i]) <= DSM501_MAX_SIG_SPAN &&
			(now - _sig_start[i]) >= DSM501_MIN_SIG_SPAN) {	// this signal is not bouncing.
			_low_total[i] += now - _sig_start[i];
		}
		_sig_start[i] = 0;
	}
	_state[i] = S_Idle;
}


/*
 * Only return the stabilized ratio
 */
double DSM501::getLowRatio(int i) {
	uint32_t now = millis();
	uint32_t span = now - _win_start[i];

	// special case if the device run too long, the millis() counter wrap back.
	if (now < _win_start[i]) {
		span = DSM501_MIN_WIN_SPAN;
	}

	if (span >= DSM501_MIN_WIN_SPAN) {
		int idx = (++_saf_idx[i]) % SAF_WIN_MAX;
		_saf_sum[i] -= _saf_ent[i][idx];
		_saf_ent[i][idx] = _low_total[i];
		_saf_sum[i] += _low_total[i];

		if (_saf_idx[i] < SAF_WIN_MAX) {
			_lastLowRatio[i] = (double)_saf_sum[i] * 100.0 / (double)(DSM501_MIN_WIN_SPAN * _saf_idx[i]);
		} else {
			_lastLowRatio[i] = (double)_saf_sum[i] * 100.0 / (double)(DSM501_MIN_WIN_SPAN * SAF_WIN_MAX);
		}

		_win_start[i] = now;
		_low_total[i] = 0;
	}

	return _lastLowRatio[i] / (double)_coeff;
}


double DSM501::getParticalWeight(int i) {
	/*
	 * with data sheet...regression function is
	 * 	y=0.30473*x^3-2.63943*x^2+102.60291*x-3.49616
	 */
	double r = getLowRatio(i);
	double weight = 0.30473 * pow(r, 3) - 2.63943 * pow(r, 2) + 102.60291 * r - 3.49616;
	return weight < 0.0 ? 0.0 : weight;
}


int DSM501::getAQI() {
	// this works only under both pin configure
	int aqi = -1;

	double P25Weight = getParticalWeight(0) - getParticalWeight(1);
	if (P25Weight>= 0 && P25Weight <= 15.4) {
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


#ifdef DEBUG
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
#endif
