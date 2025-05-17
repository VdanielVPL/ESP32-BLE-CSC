#include "blecsc.h"
#include <NimBLEDevice.h>

#define CSC_SERVICE_UUID "1816"  	// UUID dla Cycling Speed and Cadence Service
#define CSC_MEASUREMENT_UUID "2A5B" // UUID dla CSC Measurement
#define CSC_FEATURE_UUID "2A5C"     // UUID dla CSC Feature

BLECharacteristic *pMeasurement;  	// CSC Measurement
BLECharacteristic *pFeature;      	// CSC Feature

static bool notify = false;			// Flaga do powiadamiania

static uint32_t lastWheelRevsValue = 0;
static uint16_t lastWheelTimeValue = 0;
static uint16_t lastCrankRevsValue = 0;
static uint16_t lastCrankTimeValue = 0;

class GapCB : public NimBLEServerCallbacks {
	void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
		// Serial.println("Connected");
		pServer->getAdvertising()->stop();
	};
	void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
		// Serial.println("Disconnected");
		notify = false;
		NimBLEAdvertising *adv = pServer->getAdvertising();
		adv->reset();
		adv->addServiceUUID(CSC_SERVICE_UUID);
		adv->setAppearance(0x0485);  // Appearance: Cycling
		adv->setName("ESP Rowerek");
		adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);
		adv->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
		adv->start();
		// Serial.println("Advertising started again!");
	};
};


class MeasurementCB : public NimBLECharacteristicCallbacks {
	void onSubscribe(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo, uint16_t subValue) override {
		notify = (subValue & 1) != 0;
		// if (notify) {
		// 	Serial.println("Subscribed to notifications");
		// } else {
		// 	Serial.println("Unsubscribed from notifications");
		// }
	};
};

void BLE_init() {
	NimBLEDevice::setSecurityAuth(false, false, false);
	NimBLEDevice::init("ESP Rowerek");

	BLEServer *server = NimBLEDevice::createServer();
	server->setCallbacks(new GapCB());

	BLEService *csc = server->createService(CSC_SERVICE_UUID);

	// Charakterystyka dla pomiarów
	pMeasurement = csc->createCharacteristic(
		CSC_MEASUREMENT_UUID,  		// UUID dla CSC Measurement
		NIMBLE_PROPERTY::NOTIFY
	);
	pMeasurement->setCallbacks(new MeasurementCB());

	// Charakterystyka dla funkcji
	pFeature = csc->createCharacteristic(
		CSC_FEATURE_UUID,  			// UUID dla CSC Feature
		NIMBLE_PROPERTY::READ
	);

	uint16_t featFlags = 0x0003;
	pFeature->setValue((uint8_t*)&featFlags, 2);

	csc->start();

	NimBLEAdvertising *adv = server->getAdvertising();
    adv->addServiceUUID(CSC_SERVICE_UUID);
    adv->setName("ESP Rowerek");
    adv->setAppearance(0x0485);     // Appearance: Cycling
    adv->setConnectableMode(BLE_GAP_CONN_MODE_UND);
    adv->setDiscoverableMode(BLE_GAP_DISC_MODE_GEN);
    adv->start();
    // Serial.println("Advertising started!");
}

void CSC_send(uint32_t wheelRevolutions, uint16_t lastWheelEventTime, 
              uint16_t crankRevolutions, uint16_t lastCrankEventTime) {
    // Nie wysyłaj danych, jeśli nikt nie jest zasubskrybowany
    if (!notify) {
        // Serial.println("CSC: Brak subskrypcji, dane nie wysłane");
        return;
    }
    
    // Obsługa duplikatów - jeśli wszystkie wartości są takie same jak poprzednio
    if (wheelRevolutions == lastWheelRevsValue && lastWheelEventTime == lastWheelTimeValue &&
        crankRevolutions == lastCrankRevsValue && lastCrankEventTime == lastCrankTimeValue) {
        // Jeśli wartości są takie same jak ostatnio, ale nie zerowe - zwiększ czas o 1
        if (lastWheelEventTime > 0) {
            lastWheelEventTime++;
        }
        if (lastCrankEventTime > 0) {
            lastCrankEventTime++;
        }
    }
    
    // Zapisz aktualne wartości do porównania przy następnym wywołaniu
    lastWheelRevsValue = wheelRevolutions;
    lastWheelTimeValue = lastWheelEventTime;
    lastCrankRevsValue = crankRevolutions;
    lastCrankTimeValue = lastCrankEventTime;
    
    // Bufor na dane CSC - maksymalnie 11 bajtów
    uint8_t cscData[11] = {0};
    int offset = 0;
    
    // Flagi określające, które dane są dostępne
    uint8_t flags = 0;
    
    // Bit 0: Wheel Revolution Data Present (dane koła)
    if (wheelRevolutions > 0) {
        flags |= 0x01;
    }
    
    // Bit 1: Crank Revolution Data Present (dane korby)
    if (crankRevolutions > 0) {
        flags |= 0x02;
    }
    
    // Pierwszy bajt to zawsze flagi
    cscData[offset++] = flags;
    
    // Dodaj dane obrotu koła, jeśli są dostępne
    if (flags & 0x01) {
        // Cumulative Wheel Revolutions (4 bajty, little-endian)
        cscData[offset++] = wheelRevolutions & 0xFF;
        cscData[offset++] = (wheelRevolutions >> 8) & 0xFF;
        cscData[offset++] = (wheelRevolutions >> 16) & 0xFF;
        cscData[offset++] = (wheelRevolutions >> 24) & 0xFF;
        
        // Last Wheel Event Time (2 bajty, little-endian)
        cscData[offset++] = lastWheelEventTime & 0xFF;
        cscData[offset++] = (lastWheelEventTime >> 8) & 0xFF;
    }
    
    // Dodaj dane obrotu korby, jeśli są dostępne
    if (flags & 0x02) {
        // Cumulative Crank Revolutions (2 bajty, little-endian)
        cscData[offset++] = crankRevolutions & 0xFF;
        cscData[offset++] = (crankRevolutions >> 8) & 0xFF;
        
        // Last Crank Event Time (2 bajty, little-endian)
        cscData[offset++] = lastCrankEventTime & 0xFF;
        cscData[offset++] = (lastCrankEventTime >> 8) & 0xFF;
    }
    
    // Wyślij powiadomienie z danymi
    pMeasurement->setValue(cscData, offset);
    pMeasurement->notify();
    
    // Logowanie (odkomentuj w razie potrzeby)
    // Serial.print("CSC: ");
    // if (flags & 0x01) {
    //     Serial.print("Koło: " + String(wheelRevolutions) + " obr, " + String(lastWheelEventTime) + " ms; ");
    // }
    // if (flags & 0x02) {
    //     Serial.print("Korba: " + String(crankRevolutions) + " obr, " + String(lastCrankEventTime) + " ms");
    // }
    // Serial.println();
}