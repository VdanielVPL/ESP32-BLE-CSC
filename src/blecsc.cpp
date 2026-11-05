#include "blecsc.h"
#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID "1816"		// UUID for Cycling Speed and Cadence Service
#define CSC_MEASUREMENT_UUID "2A5B"	// UUID for CSC Measurement
#define CSC_FEATURE_UUID "2A5C"		// UUID for CSC Feature
#define APPEARANCE_SPEED_CADENCE 0x0485	// Cycling: Speed and Cadence Sensor

static NimBLECharacteristic *pMeasurement;	// CSC Measurement

class GapCB : public NimBLEServerCallbacks {
	void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
		// Closing an older connection from the same device
		for (uint16_t handle : pServer->getPeerDevices()) {
			if (handle != connInfo.getConnHandle() &&
			    pServer->getPeerInfoByHandle(handle).getAddress() == connInfo.getAddress()) {
				pServer->disconnect(handle);
			}
		}
		// Restarting advertising (it stops on connect)
		NimBLEDevice::startAdvertising();
	};
	void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
		NimBLEDevice::startAdvertising();
	};
};

void BLE_init(const char *deviceName) {
	NimBLEDevice::init(deviceName);
	NimBLEDevice::setSecurityAuth(false, false, false);

	NimBLEServer *server = NimBLEDevice::createServer();
	server->setCallbacks(new GapCB());

	NimBLEService *csc = server->createService(CSC_SERVICE_UUID);

	// Measurement characteristic
	pMeasurement = csc->createCharacteristic(
		CSC_MEASUREMENT_UUID,	// UUID for CSC Measurement
		NIMBLE_PROPERTY::NOTIFY
	);

	// Feature characteristic
	NimBLECharacteristic *pFeature = csc->createCharacteristic(
		CSC_FEATURE_UUID,		// UUID for CSC Feature
		NIMBLE_PROPERTY::READ
	);

	pFeature->setValue<uint16_t>(0x0003);	// wheel + crank

	NimBLEAdvertising *adv = server->getAdvertising();
	adv->addServiceUUID(CSC_SERVICE_UUID);
	adv->setName(deviceName);
	adv->setAppearance(APPEARANCE_SPEED_CADENCE);
	adv->start();
	// Serial.println("Advertising started!");
}

void CSC_send(uint32_t wheelRevolutions, uint16_t lastWheelEventTime,
		uint16_t crankRevolutions, uint16_t lastCrankEventTime) {
	// Restarting advertising if it stopped
	if (!NimBLEDevice::getAdvertising()->isAdvertising()) {
		NimBLEDevice::startAdvertising();
	}

	// CSC data buffer - 11 bytes
	uint8_t cscData[11];
	int offset = 0;

	// Flags: wheel and crank data
	cscData[offset++] = 0x01 | 0x02;

	// Cumulative Wheel Revolutions (4 bytes, little-endian)
	cscData[offset++] = wheelRevolutions & 0xFF;
	cscData[offset++] = (wheelRevolutions >> 8) & 0xFF;
	cscData[offset++] = (wheelRevolutions >> 16) & 0xFF;
	cscData[offset++] = (wheelRevolutions >> 24) & 0xFF;

	// Last Wheel Event Time (2 bytes, little-endian)
	cscData[offset++] = lastWheelEventTime & 0xFF;
	cscData[offset++] = (lastWheelEventTime >> 8) & 0xFF;

	// Cumulative Crank Revolutions (2 bytes, little-endian)
	cscData[offset++] = crankRevolutions & 0xFF;
	cscData[offset++] = (crankRevolutions >> 8) & 0xFF;

	// Last Crank Event Time (2 bytes, little-endian)
	cscData[offset++] = lastCrankEventTime & 0xFF;
	cscData[offset++] = (lastCrankEventTime >> 8) & 0xFF;

	// Sending the notification
	pMeasurement->setValue(cscData, offset);
	pMeasurement->notify();

	// Logging (uncomment if needed)
	// Serial.print("CSC: ");
	// Serial.print("Wheel: " + String(wheelRevolutions) + " rev, " + String(lastWheelEventTime) + " /1024 s; ");
	// Serial.print("Crank: " + String(crankRevolutions) + " rev, " + String(lastCrankEventTime) + " /1024 s");
	// Serial.println();
}
