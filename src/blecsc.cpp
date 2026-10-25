#include "blecsc.h"
#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID "1816"  	// UUID dla Cycling Speed and Cadence Service
#define CSC_MEASUREMENT_UUID "2A5B" // UUID dla CSC Measurement
#define CSC_FEATURE_UUID "2A5C"     // UUID dla CSC Feature
#define DEVICE_NAME "ESP Rowerek"
#define APPEARANCE_SPEED_CADENCE 0x0485	// Cycling: Speed and Cadence Sensor

static NimBLECharacteristic *pMeasurement;	// CSC Measurement

class GapCB : public NimBLEServerCallbacks {
	void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
		// Zamykanie starego połączenia tego samego urządzenia
		for (uint16_t handle : pServer->getPeerDevices()) {
			if (handle != connInfo.getConnHandle() &&
			    pServer->getPeerInfoByHandle(handle).getAddress() == connInfo.getAddress()) {
				pServer->disconnect(handle);
			}
		}
		// Wznawianie rozgłaszania (wyłącza się przy połączeniu)
		NimBLEDevice::startAdvertising();
	};
	void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
		NimBLEDevice::startAdvertising();
	};
};

void BLE_init() {
	NimBLEDevice::init(DEVICE_NAME);
	NimBLEDevice::setSecurityAuth(false, false, false);

	NimBLEServer *server = NimBLEDevice::createServer();
	server->setCallbacks(new GapCB());

	NimBLEService *csc = server->createService(CSC_SERVICE_UUID);

	// Charakterystyka dla pomiarów
	pMeasurement = csc->createCharacteristic(
		CSC_MEASUREMENT_UUID,  		// UUID dla CSC Measurement
		NIMBLE_PROPERTY::NOTIFY
	);

	// Charakterystyka dla funkcji
	NimBLECharacteristic *pFeature = csc->createCharacteristic(
		CSC_FEATURE_UUID,  			// UUID dla CSC Feature
		NIMBLE_PROPERTY::READ
	);

	pFeature->setValue<uint16_t>(0x0003);	// koło + korba

	NimBLEAdvertising *adv = server->getAdvertising();
	adv->addServiceUUID(CSC_SERVICE_UUID);
	adv->setName(DEVICE_NAME);
	adv->setAppearance(APPEARANCE_SPEED_CADENCE);
	adv->start();
	// Serial.println("Advertising started!");
}

void CSC_send(uint32_t wheelRevolutions, uint16_t lastWheelEventTime,
		uint16_t crankRevolutions, uint16_t lastCrankEventTime) {
	// Wznawianie rozgłaszania, jeśli stanęło
	if (!NimBLEDevice::getAdvertising()->isAdvertising()) {
		NimBLEDevice::startAdvertising();
	}

	// Bufor na dane CSC - 11 bajtów
	uint8_t cscData[11];
	int offset = 0;

	// Flagi: dane koła i korby
	cscData[offset++] = 0x01 | 0x02;

	// Cumulative Wheel Revolutions (4 bajty, little-endian)
	cscData[offset++] = wheelRevolutions & 0xFF;
	cscData[offset++] = (wheelRevolutions >> 8) & 0xFF;
	cscData[offset++] = (wheelRevolutions >> 16) & 0xFF;
	cscData[offset++] = (wheelRevolutions >> 24) & 0xFF;

	// Last Wheel Event Time (2 bajty, little-endian)
	cscData[offset++] = lastWheelEventTime & 0xFF;
	cscData[offset++] = (lastWheelEventTime >> 8) & 0xFF;

	// Cumulative Crank Revolutions (2 bajty, little-endian)
	cscData[offset++] = crankRevolutions & 0xFF;
	cscData[offset++] = (crankRevolutions >> 8) & 0xFF;

	// Last Crank Event Time (2 bajty, little-endian)
	cscData[offset++] = lastCrankEventTime & 0xFF;
	cscData[offset++] = (lastCrankEventTime >> 8) & 0xFF;

	// Wysyłanie powiadomienia z danymi
	pMeasurement->setValue(cscData, offset);
	pMeasurement->notify();

	// Logowanie (odkomentuj w razie potrzeby)
	// Serial.print("CSC: ");
	// Serial.print("Koło: " + String(wheelRevolutions) + " obr, " + String(lastWheelEventTime) + " /1024 s; ");
	// Serial.print("Korba: " + String(crankRevolutions) + " obr, " + String(lastCrankEventTime) + " /1024 s");
	// Serial.println();
}