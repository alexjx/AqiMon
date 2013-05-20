/*
 * USBPort.h
 *
 *  Created on: May 20, 2013
 *      Author: xinj
 */

#ifndef USBPORT_H_
#define USBPORT_H_

class USBPort {
public:
	static void begin();
	static void poll();

public:
	USBPort();
};

extern USBPort UsbPort;

#endif /* USBPORT_H_ */
