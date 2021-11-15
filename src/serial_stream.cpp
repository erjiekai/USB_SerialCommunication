#include "serial_stream.hpp"


USBStream::USBStream(bool use_overlapped)
{
	#if defined(_WIN32) || defined(WIN32)
	USBStreamHandle = INVALID_HANDLE_VALUE;

	this->use_overlapped = use_overlapped;

	memset(&dcb, 0, sizeof(dcb));

	dcb.DCBlength = sizeof(dcb);
	dcb.BaudRate = CBR_115200; //CBR_115200
	dcb.Parity = NOPARITY;
	dcb.fParity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.ByteSize = 8;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	#elif defined(__unix__)
	//SerialPort USBStreamHandle;
	//SerialPort USBStreamHandle;
	#endif
}

USBStream::~USBStream()
{
	Close();
}

void USBStream::configurePort(int baudrate, int charsize, int parity, int stopbit, int flowcontrol)
{
	#if defined(_WIN32) || defined(WIN32)
	dcb.BaudRate = baudrate; //CBR_115200
	dcb.Parity = parity;
	dcb.fParity = parity;
	dcb.StopBits = stopbit;
	dcb.ByteSize = charsize;
	dcb.fDtrControl = flowcontrol;

	#elif defined(__unix__)
	this->baudrate = baudrate;
	this->charsize = charsize;
	this->parity = parity;
	this->stopbit = stopbit;
	this->flowcontrol = flowcontrol;
	#endif
}

void USBStream::setTimeouts(double ReadIntervalTime, double ReadTotalTime, double ReadTotalMultiplier, double WriteTotaleTime, double WriteTotalMultiplier)
{
#if defined(_WIN32) || defined(WIN32)
	// All time values are in milliseconds
	COMMTIMEOUTS USBStreamTimeouts = { 0 };
	USBStreamTimeouts.ReadIntervalTimeout = ReadIntervalTime;
	USBStreamTimeouts.ReadTotalTimeoutConstant = ReadTotalTime;
	USBStreamTimeouts.ReadTotalTimeoutMultiplier = ReadTotalMultiplier;
	USBStreamTimeouts.WriteTotalTimeoutConstant = WriteTotaleTime;
	USBStreamTimeouts.WriteTotalTimeoutMultiplier = WriteTotalMultiplier;
	SetCommTimeouts(USBStreamHandle, &USBStreamTimeouts);
#endif
}

/** @brief Opens a COM port in windows
*
* @param[in] device the name of the port (eg: COM1 or COM24)
*
* @return returns flag code [0 = everything successful, 1 = INVALID_HANDLE_VALUE, 2 = SetCommState failed ]
*/
int USBStream::Open(const char* device)
{
	int flag = 0;

	#if defined(_WIN32) || defined(WIN32)

	strcpy(comport, "\\\\.\\COM");
	strcat(comport, device);

	if (use_overlapped == true)
	{
		USBStreamHandle = CreateFile(comport,
			GENERIC_READ | GENERIC_WRITE, // access ( read and write)
			0,                            // (share) 0:cannot share the COM port
			0,                            // security  (None)
			OPEN_EXISTING,                // creation : open_existing
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,         // FILE_ATTRIBUTE_NORMAL use "FILE_FLAG_OVERLAPPED" for overlapped operation (event trigger/callback)
			0                             // no templates file for COM port...
		);
	}
	else
	{
		USBStreamHandle = CreateFile(comport,
			GENERIC_READ | GENERIC_WRITE,  // access ( read and write)
			0, // (share) 0:cannot share the COM port
			0,                           // security  (None)
			OPEN_EXISTING,               // creation : open_existing
			0,        // use "FILE_FLAG_OVERLAPPED" for overlapped operation (event trigger/callback)
			NULL                            // no templates file for COM port...
		);
	}

	// if handle successfully created, configure port, create event handle and get device number
	if (USBStreamHandle != INVALID_HANDLE_VALUE)
	{
		//SetCommState(USBStreamHandle, &dcb);
		//SetCommMask(USBStreamHandle, EV_RXCHAR);

		if (!SetCommState(USBStreamHandle, &dcb)) //configure the port comm protocol according to dcb (dcb configured in constructor)
			flag = 2; // set flag to 2 if SetCommState returns a false (0)

		SetCommMask(USBStreamHandle, EV_RXCHAR); // create event handle. EV_RXCHAR
		if (use_overlapped == true)
		{
			memset(&ov, 0, sizeof(ov));
			ov.hEvent = CreateEvent(
				NULL,  // default security attributes
				TRUE,  // manual-reset event
				FALSE, // not signaled
				NULL   // no name
			);
			// Initialize the rest of the OVERLAPPED structure to zero.
			ov.Internal = 0;
			ov.InternalHigh = 0;
			ov.Offset = 0;
			ov.OffsetHigh = 0;

			if (ov.hEvent == NULL)
				printf("flag creating overlapped event! abort now");
			//        assert(ov.hEvent);
		}
	}
	else
	{
		flag = 1;
	}

	if (flag != 0)
	{
		Close();
	}
	else
	{
		clearBuffer();
	}

	#elif defined(__unix__)

	std::string comport = "/dev/ttyACM" + std::string(device);
	USBStreamHandle.Open(comport);
	if(!USBStreamHandle.IsOpen())
	{
		std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
        << "Error: Could not open serial port." << comport << std::endl ;
        exit(1) ;
	}

		USBStreamHandle.SetBaudRate(getBaudRate(baudrate));
		USBStreamHandle.SetCharacterSize(getCharacterSize(charsize));
		USBStreamHandle.SetParity(getParity(parity));
		USBStreamHandle.SetStopBits(getStopBits(stopbit));
		USBStreamHandle.SetFlowControl(getFlowControl(flowcontrol));

	#endif

	return flag;
}

void USBStream::Close(void)
{
	#if defined(_WIN32) || defined(WIN32)
		CloseHandle(USBStreamHandle);
		USBStreamHandle = INVALID_HANDLE_VALUE;

	#elif defined(__unix__)
		USBStreamHandle.Close();
	#endif

	//printf("Port 1 has been CLOSED and %d is the file descriptionn", fileDescriptor);
}

bool USBStream::good()
{
	#if defined(_WIN32) || defined(WIN32)
	//return WaitNamedPipe(comport, 1); //wait for max 1ms 
	if (USBStreamHandle != INVALID_HANDLE_VALUE)
		return true;
	else
		return false;

	#elif defined(__unix__)
		return USBStreamHandle.IsOpen();
	#endif
}

void USBStream::write(char* buffer, int len)
{
	#if defined(_WIN32) || defined(WIN32)
	if (use_overlapped == true)
	{
		//Also Block other operations till writefile has finished, but can be rewrote to allow multithreading

		for (int i = 0; i < len; i++) {
			DWORD error_cc = WriteFile(USBStreamHandle, buffer + i, 1, NULL, &(ov));
			if (error_cc == FALSE && GetLastError() != ERROR_IO_PENDING)
			{
				printf("Cannot write to USB\n");
				i--;
			}
		}
	}
	else
	{
		//Block other operations till writefile has finished
		unsigned long result;

		if (USBStreamHandle != INVALID_HANDLE_VALUE)
			WriteFile(USBStreamHandle, buffer, len, &result, NULL);
		//printf("Written %d bytes\n", result);
		//return result;
	}
	#elif defined(__unix__)

	////////////// Method 1 ///////////////////
	// std::string str(tuffer);
	// USBStreamHandle.Write(str);

	////////////// Method 2 ///////////////////
	// // Write an array of data from the serial port.
	// for(int i=0; i<len; i++)
	// {
	// 	// printf("%d:%x \n", i, (unsigned char) buffer[i]);
	// 	USBStreamHandle.WriteByte((unsigned char)  buffer[i]);
	// 	// USBStreamHandle.WriteByte(buffer[i]);
	// }
	// // printf("\n");

	////////////// Method 3 ///////////////////
	// printf("isOpen: %d\n", USBStreamHandle.IsOpen());
	// printf("isDataAvailable: %d\n", USBStreamHandle.IsDataAvailable());
	std::vector<uint8_t> tbuffer;
	for(int i=0; i<len; i++)
	{
		tbuffer.push_back((uint8_t) buffer[i]);
	}
	USBStreamHandle.Write(tbuffer);

	#endif
}

int USBStream::read(char* buffer)
{
	#if defined(_WIN32) || defined(WIN32)
	if (use_overlapped == true)
	{

		unsigned long nbr = 0; //number of bytes that is read out

		//for (int i = 0; i < len; i++)//clear the output buffer
		//	buffer[i] = 0;

		BOOL fWaitingOnRead = FALSE;

		if (!ReadFile(USBStreamHandle, buffer, 1, &nbr, &(ov))) //try to read one byte and fail
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				printf("IO Pending");
				fWaitingOnRead = TRUE;
			}
			else
			{
				printf("Error in WaitCommEvent, abort");
			}
		}
		else //success reading one byte. can continue reading the rest of the buffer
		{
			/// code to read out 400 bytes at one go. This will stop the program until 400 bytes have been read. not useful.
			// need to start if else with "if (!WaitCommEvent (sp[0].USBStreamHandle, &event_flag, &(sp[0].ov)))"
			//                printf("WaitCommEvent returned immediatly\n");
			//                unsigned long nbr = 0; //number of bytes read
			//                ReadFile(sp[0].USBStreamHandle, output, 400, &nbr, &(sp[0].ov));

			/// code to read the rest of the buffer after 1 byte has been read
			DWORD errorcode = 0;
			COMSTAT mycomstat;
			ClearCommError(USBStreamHandle, &errorcode, &mycomstat); //use to obtain the number of bytes left over in the buffer
			if (mycomstat.cbInQue > 0)
			{
				ReadFile(USBStreamHandle, buffer + 1, mycomstat.cbInQue, &nbr, &(ov));
				std::string str(buffer); //convert the char array to string
				//printf("bytes: %d output: %s \n", nbr + 1, str.c_str());
			}
			else
			{
				PurgeComm(USBStreamHandle, PURGE_RXABORT | PURGE_TXABORT | PURGE_RXCLEAR | PURGE_TXCLEAR);
			}

			///this code tries to read the buffer until no bytes is left. Doesnt work properly with overlapped I/O
			//                int counter = 0;
			//                unsigned long nbr = 0; //number of bytes read
			//                char curr=0;
			//                do
			//                {
			//                    ReadFile(sp[0].USBStreamHandle, &curr, 1, &nbr, &(sp[0].ov));
			//                    if (nbr == 1)
			//                    {
			//                        output[counter]=curr;
			//                        counter++;
			//                    }
			//
			//                }while(nbr==1);
			//                std::string str(output);
			//                printf("bytes: %d output: %s",counter,str.c_str());
			//
		}
		//printf("\n");


		return ((int)nbr);
	}
	else
	{
		unsigned long nbr = 0; //number of bytes that is read out

		//for (int i = 0; i < len; i++)//clear the output buffer
		//	buffer[i] = 0;

		ReadFile(USBStreamHandle, buffer, 1, &nbr, NULL);
		if (nbr > 0)
		{
			DWORD errorcode = 0;
			COMSTAT mycomstat;
			ClearCommError(USBStreamHandle, &errorcode, &mycomstat); //use to obtain the number of bytes left over in the buffer
			if (mycomstat.cbInQue > 0)
			{
				if (USBStreamHandle != INVALID_HANDLE_VALUE)
				{
					ReadFile(USBStreamHandle, buffer + 1, mycomstat.cbInQue, &nbr, NULL);
				}
				//std::string str(buffer); //convert the char array to string
				//printf("bytes: %d output: %s\n", nbr + 1, str.c_str());
			}
		}
		return ((int)nbr);
	}

	#elif defined(__unix__)
	int count = 0;
	int timeout = 1;//ms
	while(USBStreamHandle.IsDataAvailable())
	{
		USBStreamHandle.ReadByte(buffer[count], timeout);
		count++;
	}
	return count;
	#endif
}

int USBStream::read(char* buffer, int len, int timeout)
{
	#if defined(_WIN32) || defined(WIN32)
	unsigned long nbr = 0; //number of bytes that is read out
	ReadFile(USBStreamHandle, buffer, len, &nbr, NULL);
	return (int)nbr;

	#elif defined(__unix__)
	LibSerial::DataBuffer data;
	// std::string str="";
	try
	{
	USBStreamHandle.Read(data, len, timeout);
	for (int i = 0; i < len; i++)
	{
		buffer[i] = data[i];
	}
	return len;
	}
	catch (LibSerial::ReadTimeout e)
	{
		return 0;
	}

	#endif
}

//read one byte
int USBStream::getOneByte(char& buffer, int timeout)
{
	#if defined(_WIN32) || defined(WIN32)
	unsigned long nbr = 0; //number of bytes that is read out
	ReadFile(USBStreamHandle, &buffer, 1, &nbr, NULL);
	return nbr;

	#elif defined(__unix__)
	try
	{
		USBStreamHandle.ReadByte(buffer, timeout);
		return 1;
	}
	catch (LibSerial::ReadTimeout e)
	{
		return 0;
	}
	
	#endif
}

void USBStream::clearBuffer()
{
	#if defined(_WIN32) || defined(WIN32)
	PurgeComm(USBStreamHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);

	#elif defined(__unix__)

	USBStreamHandle.FlushIOBuffers();
	//while (StreamStreamHandle.rdbuf()->in_avail() > 0)
	//{
	//	char next_byte;
	//	StreamStreamHandle.get(next_byte);
	//}
	#endif
}


bool USBStream::IsDataAvailable()
{
	#if defined(_WIN32) || defined(WIN32)
	DWORD errorcode = 0;
	COMSTAT mycomstat;
		ClearCommError(USBStreamHandle, &errorcode, &mycomstat); //use to obtain the number of bytes left over in the buffer
		if (mycomstat.cbInQue > 0)
		{
			return true;
		}
		else
		{
			return false;
		}
		
	#elif defined(__unix__)
	return USBStreamHandle.IsDataAvailable();
	#endif
}

#if defined(__unix__)
LibSerial::BaudRate USBStream::getBaudRate(int baud)
{
	switch (baud) {
	case 50:
		return LibSerial::BaudRate::BAUD_50;
	case 75:
		return LibSerial::BaudRate::BAUD_75;
	case 110:
		return LibSerial::BaudRate::BAUD_110;
	case 134:
		return LibSerial::BaudRate::BAUD_134;
	case 150:
		return LibSerial::BaudRate::BAUD_150;
	case 200:
		return LibSerial::BaudRate::BAUD_200;
	case 300:
		return LibSerial::BaudRate::BAUD_300;
	case 600:
		return LibSerial::BaudRate::BAUD_600;
	case 1200:
		return LibSerial::BaudRate::BAUD_1200;
	case 1800:
		return LibSerial::BaudRate::BAUD_1800;
	case 2400:
		return LibSerial::BaudRate::BAUD_2400;
	case 4800:
		return LibSerial::BaudRate::BAUD_4800;
	case 9600:
		return LibSerial::BaudRate::BAUD_9600;
	case 19200:
		return LibSerial::BaudRate::BAUD_19200;
	case 38400:
		return LibSerial::BaudRate::BAUD_38400;
	case 57600:
		return LibSerial::BaudRate::BAUD_57600;
	case 115200:
		return LibSerial::BaudRate::BAUD_115200;
	case 230400:
		return LibSerial::BaudRate::BAUD_230400;
	case 460800:
		return LibSerial::BaudRate::BAUD_460800;
	case 500000:
		return LibSerial::BaudRate::BAUD_500000;
	case 576000:
		return LibSerial::BaudRate::BAUD_576000;
	case 921600:
		return LibSerial::BaudRate::BAUD_921600;
	case 1000000:
		return LibSerial::BaudRate::BAUD_1000000;
	case 1152000:
		return LibSerial::BaudRate::BAUD_1152000;
	case 1500000:
		return LibSerial::BaudRate::BAUD_1500000;
	case 2000000:
		return LibSerial::BaudRate::BAUD_2000000;
	case 2500000:
		return LibSerial::BaudRate::BAUD_2500000;
	case 3000000:
		return LibSerial::BaudRate::BAUD_3000000;
	case 3500000:
		return LibSerial::BaudRate::BAUD_3500000;
	case 4000000:
		return LibSerial::BaudRate::BAUD_4000000;
	default:
		return LibSerial::BaudRate::BAUD_115200;
	}
}

LibSerial::CharacterSize USBStream::getCharacterSize(int csize)
{
	switch (csize) {
	case 5:
		return LibSerial::CharacterSize::CHAR_SIZE_5;
	case 6:
		return LibSerial::CharacterSize::CHAR_SIZE_6;
	case 7:
		return LibSerial::CharacterSize::CHAR_SIZE_7;
	case 8:
		return LibSerial::CharacterSize::CHAR_SIZE_8;
	default:
		return LibSerial::CharacterSize::CHAR_SIZE_DEFAULT;
	}
}

LibSerial::FlowControl USBStream::getFlowControl(int flowcontrol)
{
	switch (flowcontrol) {
	case 0:
		return LibSerial::FlowControl::FLOW_CONTROL_NONE;
	case 1:
		return LibSerial::FlowControl::FLOW_CONTROL_SOFTWARE;
	case 2:
		return LibSerial::FlowControl::FLOW_CONTROL_HARDWARE;
	default:
		return LibSerial::FlowControl::FLOW_CONTROL_NONE;
	}
}

LibSerial::Parity USBStream::getParity(int parity)
{
	switch (parity) {
	case 0:
		return LibSerial::Parity::PARITY_NONE;
	case 1:
		return LibSerial::Parity::PARITY_ODD;
	case 2:
		return LibSerial::Parity::PARITY_EVEN;
	default:
		return LibSerial::Parity::PARITY_NONE;
	}
}

LibSerial::StopBits USBStream::getStopBits(int stopbits)
{
	switch (stopbits) {
	case 1:
		return LibSerial::StopBits::STOP_BITS_1;
	case 2:
		return LibSerial::StopBits::STOP_BITS_2;
	default:
		return LibSerial::StopBits::STOP_BITS_1;
	}
}

#endif

/*
// main used for testing
void main()
{

#ifdef __unix__

	int numUSB = 1;
	USBStream* serial_port;
	serial_port = new USBStream[numUSB];


	for (int i = 0; i < numUSB; i++)
	{
		std::string comport = "/dev/ttyACM";
		comport = comport + std::to_string(i);

		printf("Connecting to %s\n", comport.c_str());
		serial_port[i].Open(comport.c_str());
		if (!serial_port[i].good())
		{
			std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
				<< "Error: Could not open serial port: " << comport
				<< std::endl;
			exit(1);
		}

	}

	for (int i = 0; i < numUSB; i++)
		serial_port[i].configurePort(BAUD_115200, 8, PARITY_NONE, 1, 0);

	// Write
	const int num_commd = 4;
	char output[num_commd];
	output[0] = 0xf7;
	output[1] = 0x7b;
	serial_port[0].write(output, num_commd);

	//Read
	int datasize = 44; //example
	char* data = new char[datasize]; // add 1 for button state
	for (int i = 0; i < numUSB; i++)
		serial_port[i].read(data, datasize); // all read data will be put into "data"

#else

	int numUSB = 1;
	USBStream* serial_port;
	serial_port = new USBStream[numUSB];

	std::string* comport = new std::string[numUSB];
	comport[0] = "6";

	for (int i = 0; i < numUSB; i++)
	{

		printf("Connecting to %s\n", comport[i].c_str());
		serial_port[i].Open(comport[i].c_str());
		if (!serial_port[i].good())
		{
			std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
				<< "Error: Could not open serial port: " << comport[i]
				<< std::endl;
			exit(1);
		}

	}

	for (int i = 0; i < numUSB; i++)
		serial_port[i].configurePort(BAUD_115200, 8, PARITY_NONE, 1, 0);

	// Write
	const int num_commd = 4;
	char output[num_commd];
	output[0] = 0xf7;
	output[1] = 0x7b;
	serial_port[0].write(output, num_commd);

	//Read
	int datasize = 44; //example
	char* data = new char[datasize]; // add 1 for button state
	for (int i = 0; i < numUSB; i++)
		serial_port[i].read(data, datasize); // all read data will be put into "data"


#endif


}

*/
