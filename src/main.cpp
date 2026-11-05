#include <Arduino.h>
#include "blecsc.h"

//==============================================================================
// Config (details in README)
//==============================================================================

// Hall sensor (values depend on your setup)
constexpr int hallHighThreshold = 165;			// reading that starts a magnet pass
constexpr int hallLowThreshold = 130;			// reading that ends it
constexpr bool printHallReadings = false;		// raw readings in serial monitor
constexpr uint32_t sampleRateHz = 60;			// hall sensor reads per second

// Speed and cadence
constexpr float wheelToCrankRatio = 4.1;		// wheel turns per pedal turn, at least 1
constexpr uint32_t stopTimeoutMs = 5000;		// stop after this time without a revolution
constexpr uint32_t minRevolutionMs = 200;		// double pulse rejection

// Bluetooth and board
constexpr char deviceName[] = "ESP Rowerek";	// Bluetooth name, max 18 ASCII characters
constexpr uint8_t ledPin = 2;					// on-board LED pin

//==============================================================================

// Config checks
static_assert(wheelToCrankRatio >= 1, "wheelToCrankRatio must be at least 1");
static_assert(sizeof(deviceName) - 1 <= 18, "deviceName must be at most 18 ASCII characters");

// Time in CSC units (1/1024 s)
static uint16_t toCscTime(uint32_t ms) {
	return (uint16_t)((uint64_t)ms * 1024 / 1000);
}

static TaskHandle_t hallTaskHandle = NULL;

void IRAM_ATTR onTimer() {
	vTaskNotifyGiveFromISR(hallTaskHandle, NULL);
}

void hallTask(void * pvParameters) {
	// Gear ratio × 100, rounded, for integer math
	const uint32_t wheelToCrankRatio100 = wheelToCrankRatio * 100 + 0.5;

	bool hallTriggeredLast = false;
	uint32_t ticks = 0;
	uint32_t count = 0;
	uint32_t lastRevolutionMs = 0;
	uint32_t wheelRevs = 0;
	uint16_t wheelEventTime = 0;
	uint16_t crankEventTime = 0;

	while (1) {
		// Waiting for the timer tick
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

		uint32_t now = millis();
		int hall = hallRead();
		bool hallTriggered = false;

		if (printHallReadings) {
			Serial.println(hall);
		}

		if (hall >= hallHighThreshold || (hall > hallLowThreshold && hallTriggeredLast)) {
			hallTriggered = true;
			digitalWrite(ledPin, HIGH);
		}

		if (hall <= hallLowThreshold) {
			hallTriggered = false;
			digitalWrite(ledPin, LOW);
		}

		// New revolution
		if (!hallTriggeredLast && hallTriggered && now - lastRevolutionMs >= minRevolutionMs) {
			Serial.printf("Hall Triggered: %d\n", hall);

			uint32_t period = now - lastRevolutionMs;
			lastRevolutionMs = now;

			crankEventTime = toCscTime(now);
			wheelEventTime = crankEventTime;

			// First revolution after a stop only sets the reference time
			if (period <= stopTimeoutMs) {
				count++;

				// Wheel revolutions in hundredths (gear ratio without float)
				uint64_t wheel100 = (uint64_t)count * wheelToCrankRatio100;
				wheelRevs = wheel100 / 100;

				// Time of the last full wheel revolution
				uint32_t wheelFraction = wheel100 % 100;
				uint32_t sinceWheelEvent = wheelFraction * period / wheelToCrankRatio100;
				wheelEventTime = toCscTime(now - sinceWheelEvent);
			}
		}

		hallTriggeredLast = hallTriggered;

		// Sending CSC every 1 s
		if (++ticks >= sampleRateHz) {
			ticks = 0;

			// Stopped - event times move on without revolutions, so the app shows 0
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
	BLE_init(deviceName);
	pinMode(ledPin, OUTPUT);
	xTaskCreate(
		hallTask,              /* Task function. */
		"hallTask",            /* name of task. */
		10000,                 /* Stack size of task */
		NULL,                  /* parameter of the task */
		1,                     /* priority of the task */
		&hallTaskHandle);      /* Task handle to keep track of created task */

	hw_timer_t * timer = timerBegin(1, 80, true);
	timerAttachInterrupt(timer, &onTimer, false);
	timerAlarmWrite(timer, 1000000 / sampleRateHz, true);
	timerAlarmEnable(timer);
}

void loop() {
	vTaskDelete(NULL);	// Deleting the Arduino task - the work happens in the timer and hallTask
}
