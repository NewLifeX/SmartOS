#ifndef __TEST_CONF__H__
#define __TEST_CONF__H__

#include "Sys.h"
#include "Port.h"

void TestSerial();
void TestAT45DB();
void TestFlash();
void TestCrc();
void TestEnc28j60();
void TestEthernet();
void TestTimer(OutputPort& leds);
void TestThread(OutputPort& leds);

#include "NRF24L01.h"
void TestNRF24L01();
NRF24L01* Create2401();

void TestMessage(OutputPort* leds);

#endif
