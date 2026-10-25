#include <Arduino.h>
#include "blecsc.h"

#define LED_BUILTIN 2

static const int hallHighThreshold = 165;
static const int hallLowThreshold = 130;
static const uint32_t wheelToCrankRatio100 = 410;	// przełożenie koło/korba × 100 (4.1)
static const uint32_t minRevolutionMs = 200;		// odrzucanie podwójnych impulsów
static const uint32_t stopTimeoutMs = 5000;			// postój po tym czasie bez obrotu

static TaskHandle_t hallTaskHandle = NULL;

// Czas w jednostkach CSC (1/1024 s)
static uint16_t toCscTime(uint32_t ms) {
	return (uint16_t)((uint64_t)ms * 1024 / 1000);
}

void IRAM_ATTR onTimer() {
	vTaskNotifyGiveFromISR(hallTaskHandle, NULL);
}

void hallTask(void * pvParameters) {
	bool hallTriggeredLast = false;
	uint32_t ticks = 0;
	uint32_t count = 0;
	uint32_t lastRevolutionMs = 0;
	uint32_t wheelRevs = 0;
	uint16_t wheelEventTime = 0;
	uint16_t crankEventTime = 0;

	while (1) {
		// Czekanie na tick timera (60 Hz)
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		uint32_t now = millis();
		int hall = hallRead();
		bool hallTriggered = false;

		if (hall >= hallHighThreshold || (hall > hallLowThreshold && hallTriggeredLast)) {
			hallTriggered = true;
			digitalWrite(LED_BUILTIN, HIGH);
		}

		if (hall <= hallLowThreshold) {
			hallTriggered = false;
			digitalWrite(LED_BUILTIN, LOW);
		}

		// Nowy obrót
		if (!hallTriggeredLast && hallTriggered && now - lastRevolutionMs >= minRevolutionMs) {
			Serial.printf("Hall Triggered: %d\n", hall);

			uint32_t period = now - lastRevolutionMs;
			lastRevolutionMs = now;

			crankEventTime = toCscTime(now);
			wheelEventTime = crankEventTime;

			// Pierwszy obrót po postoju tylko ustawia czas odniesienia
			if (period <= stopTimeoutMs) {
				count++;

				// Obroty koła w setnych częściach obrotu (przełożenie bez float)
				uint64_t wheel100 = (uint64_t)count * wheelToCrankRatio100;
				wheelRevs = wheel100 / 100;

				// Czas ostatniego pełnego obrotu koła
				uint32_t wheelFraction = wheel100 % 100;
				uint32_t sinceWheelEvent = wheelFraction * period / wheelToCrankRatio100;
				wheelEventTime = toCscTime(now - sinceWheelEvent);
			}
		}

		hallTriggeredLast = hallTriggered;

		// Wysyłanie CSC co 1 s
		if (++ticks >= 60) {
			ticks = 0;

			// Postój - przesuwanie czasów bez obrotów, gra pokazuje 0
			if (now - lastRevolutionMs > stopTimeoutMs) {
				crankEventTime = toCscTime(now);
				wheelEventTime = crankEventTime;
			}

			CSC_send(wheelRevs, wheelEventTime, (uint16_t)count, crankEventTime);
		}
	}
}

void setup() {
	Serial.begin(115200);
	BLE_init();
	pinMode(LED_BUILTIN, OUTPUT);
	xTaskCreate(
		hallTask,              /* Task function. */
		"hallTask",            /* name of task. */
		10000,                 /* Stack size of task */
		NULL,                  /* parameter of the task */
		1,                     /* priority of the task */
		&hallTaskHandle);      /* Task handle to keep track of created task */

	hw_timer_t * timer = timerBegin(1, 80, true);
	timerAttachInterrupt(timer, &onTimer, false);
	timerAlarmWrite(timer, 1000000/60, true);
	timerAlarmEnable(timer);
}

void loop() {
	vTaskDelete(NULL);	// Usuwanie taska Arduino - praca odbywa się w timerze i tasku
}
