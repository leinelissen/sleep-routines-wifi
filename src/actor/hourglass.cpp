#include "hourglass.h"

void Hourglass::init(int pin, MQTTClient* client) {
    servoPin = pin;
    mqtt_client = client;

    // Set hourglass to default position
    moveServo(false);
}

void Hourglass::moveServo(bool open) {
    // Attach and init servo
    servo.attach(servoPin);

    // Set position according to start/stop
    servo.write(open ? 100 : 120);

    // The servo will operate at about 0.14 - 0.20 s / 60 degrees, so a full 180 degree turn should take at most 0.6s
    delay(750);

    // Detach the servo so that we don't supply any stall current and overheat the servo
    servo.detach();

    // Switch flag
    hourglassIsOpen = open;
}

void Hourglass::start(unsigned int intv) {
    // Store interval and start time of the timer
    interval = intv;
    startTime = millis();
    timerIsActive = true;

    // Publish event
    sendEvent(SR_EVENT_STARTED_TIMER);

    if (!hourglassIsOpen) {
        moveServo(true);
    }
}

void Hourglass::stop(bool timerIsCompleted) {
    timerIsActive = false;

    // Publish event
    sendEvent(timerIsCompleted ? SR_EVENT_TIMER_COMPLETED : SR_EVENT_TIMER_STOPPED);

    if (hourglassIsOpen) {
        moveServo(false);
    }
}

void Hourglass::loop() {
    if (timerIsActive) {
        if (millis() > (startTime + interval)) {
            stop(true);
        }
    }
}

void Hourglass::sendEvent(String event) {
    mqtt_client->publish("/sleep-routines", "{ \"event\":\"" + event + "\" }");
}