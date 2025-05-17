#include <Arduino.h>
#include "blecsc.h"

#define LED_BUILTIN 2

hw_timer_t * timer = NULL;

volatile int count = 0;
// volatile int lastCount = 0;
volatile bool hallTriggeredLast = false;
volatile int ticks = 0;
// volatile int lastRps = 0;

volatile uint16_t lastCrankEventTime = 0;

TaskHandle_t ProcessAVGrpsHandle = NULL;

// int rpsArr[] = {0,0,0};

static int hallHighTreshold = 165;
static int hallLowTreshold = 130;

// float AVGrps() {f
// 	rpsArr[0] = rpsArr[1];
// 	rpsArr[1] = rpsArr[2];
// 	rpsArr[2] = lastRps;
// 	int sum = 0;
// 	for (int i = 0; i < 3; i++) {
// 		sum += rpsArr[i];
// 	}
// 	return (float)sum / 3;
// }


void ProcessAVGrps(void * pvParameters) {
	static float wheelToCrankRatio = 4.1;
	while (1){
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		// float avg = AVGrps();
		// Serial.println("AVGrps: " + String(avg));
    	// FTMS_send(avg);
        uint16_t wheelEventTime = (millis() & 0xFFFF) * 1024 / 1000;
        uint32_t wheelRevs = count * wheelToCrankRatio;
        // Wysyłamy dane z różnymi czasami dla koła i korby
        CSC_send(wheelRevs, wheelEventTime, count, lastCrankEventTime);
	}
}

void IRAM_ATTR hallInterrupt() {
	ticks++;
	int hall = hallRead();
	bool hallTriggered = false;

	if (hall >= hallHighTreshold || (hall > hallLowTreshold && hallTriggeredLast)) {
		hallTriggered = true;
		digitalWrite(LED_BUILTIN, HIGH);
	}

	if (hall <= hallLowTreshold) {
		hallTriggered = false;
		digitalWrite(LED_BUILTIN, LOW);
	}

	if (hallTriggeredLast == false && hallTriggered == true) {
		Serial.println("Hall Triggered: "+ String(hall));
		count++;
		lastCrankEventTime = (millis() & 0xFFFF) * 1024 / 1000;
	}

	hallTriggeredLast = hallTriggered;
	
	if (ticks % 60 == 0) {
		// Serial.println(count);
		// lastRps = count - lastCount;
		// lastCount = count;
		// Serial.println("RPM: " + String(rps*60));
		vTaskNotifyGiveFromISR(ProcessAVGrpsHandle, NULL);
	}  
}

void setup() {
	Serial.begin(115200);
  	BLE_init();
	pinMode(LED_BUILTIN, OUTPUT);
	xTaskCreate(
		ProcessAVGrps,         /* Task function. */
		"ProcessAVGrps",       /* name of task. */
		10000,                 /* Stack size of task */
		NULL,                  /* parameter of the task */
		1,                     /* priority of the task */
		&ProcessAVGrpsHandle); /* Task handle to keep track of created task */

	timer = timerBegin(1, 80, true);
	timerAttachInterrupt(timer, &hallInterrupt, true);
	timerAlarmWrite(timer, 1000000/60, true);
	timerAlarmEnable(timer);
	// Serial.println("Timer started");
}

void loop() {

}