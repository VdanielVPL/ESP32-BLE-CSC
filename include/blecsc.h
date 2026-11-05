#pragma once
#include <Arduino.h>

void BLE_init(const char *deviceName);
void CSC_send(uint32_t wheelRevolutions, uint16_t lastWheelEventTime,
			  uint16_t crankRevolutions, uint16_t lastCrankEventTime);
