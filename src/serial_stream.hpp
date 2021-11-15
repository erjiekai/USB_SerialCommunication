#ifndef SERIAL_STREAM_HPP_
#define SERIAL_STREAM_HPP_

/** Reads data from a USB port for WINDOWS and Linux
*
* @note for linux, use LibSerial
* @see http://libserial.sourceforge.net/
*
* This class is written to mimic the SerialStream.h member functions. Hence, the member functions name are all the same, and not following google style guide
*
* basics on usb read write using Win32 API
* http://xanthium.in/Serial-Port-Programming-using-Win32-API
* For using Overlapped (Multithread). i.e. non blocking read/write
* https://www.dreamincode.net/forums/topic/165693-microsoft-working-with-overlapped-io/
*
* Created by:
* @author Er Jie Kai (EJK)
 */

#include <stdio.h>
#include <iostream>
#include <string>

#if defined(__unix__)

//#include "SerialStream.h"

#include <unistd.h>
// #include <stdio.h>
// #include <stdlib.h>
//using namespace LibSerial; // not ideal, but in order to make this class portable for both windows and linux, this is the easiest option

#include "libserial/SerialPort.h"
#include <libserial/SerialPortConstants.h>

#elif defined(_WIN32) || defined(WIN32)


#include <windows.h>
#include <ctime> // for timer

#endif /*Windows or Unix*/

class USBStream
{
private:
	char comport[15] = { 0 };
	

#if defined(_WIN32) || defined(WIN32)
	HANDLE USBStreamHandle;
	DCB dcb;
	OVERLAPPED ov;
	bool use_overlapped = false;
#elif defined(__unix__)
	int baudrate;
	int charsize;
	int parity;
	int stopbit;
	int flowcontrol;
	


	LibSerial::BaudRate getBaudRate(int baud);
	LibSerial::CharacterSize getCharacterSize(int csize);
	LibSerial::FlowControl getFlowControl(int flowcontrol);
	LibSerial::Parity getParity(int parity);
	LibSerial::StopBits getStopBits(int stopbits);

	LibSerial::SerialPort USBStreamHandle;

#endif

public:

	
	
	USBStream(bool use_overlapped = false);
	~USBStream();

	int Open(const char* device);
	void Close(void);

	void write(char* buffer, int len); //uses overlapped 
	int read(char* buffer); //uses overlapped
	int read(char* buffer, int len, int timeout=1);
	int getOneByte(char& buffer, int timeout = 0); //uses overlapped
	void configurePort(int baudrate, int charsize, int parity, int stopbit, int flowcontrol);
	void setTimeouts(double ReadIntervalTime, double ReadTotalTime, double ReadTotalMultiplier, double WriteTotaleTime, double WriteTotalMultiplier);
	bool good();
	void clearBuffer();
	bool IsDataAvailable();
};





#endif /*SERIAL_STREAM_HPP_*/
