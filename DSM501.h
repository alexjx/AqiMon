#ifndef DSM501_H
#define DSM501_H
#if ARDUINO >= 100
 #include "Arduino.h"
#else
 #include "WProgram.h"
#endif

typedef unsigned long ulong_t;

#define _mS_By_S(x)	((x) * 1000ul)
#define _S_By_S(x) 	(x)

#define DSM501_MIN_SIG_SPAN	(10ul)			// 10mS
#define DSM501_MIN_WIN_SPAN	_mS_By_S(60ul)	// 60S

#define PM10_PIN	A3
#define PM25_PIN	A2

#define PM10_IDX	0
#define PM25_IDX	1

#define SAF_WIN_MAX 60	// 60 mins

enum State {
	S_Idle,
	S_Start,
};

class DSM501 {
public:
	static DSM501* getInst(void) {
		return &_instance;
	}
	void begin();
	void update(); // called in the loop function for update

	double 	getLowRatio(int i = 0, bool filtered = false);
	double  getParticalWeight(int i = 0);
	int 	getAQI();

	void 	debug();

	static void Isr();

protected:
	DSM501();
	void signal_begin(int i);
	void signal_end(int i);

protected:
	static DSM501 _instance;

private:
	const static int _pin[2];
	ulong_t _low_total[2];  // total low in microsec
	ulong_t	_win_start[2];	// start time in millisec
	ulong_t _sig_start[2];  // period start time in millisec

	// Sliding Averaging Filtering
	ulong_t _saf_sum[2];
	ulong_t _saf_ent[2][SAF_WIN_MAX];
	ulong_t	_saf_idx[2];

	double 	_lastLowRatio[2];

	volatile State _state[2];
	volatile int _pinState[2];
};

#endif
