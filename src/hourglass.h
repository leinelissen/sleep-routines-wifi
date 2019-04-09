#ifndef HOURGLASS_H   /* Include guard */
#define HOURGLASS_H

#include <Servo.h>
#include <MQTT.h>
#include "sleep_routine_events.h"

class Hourglass {
    public:
        void start(unsigned int);
        void stop(bool timerIsCompleted = false);
        void init(int, MQTTClient*);
        void loop();

    private:
        void moveServo(bool);
        void sendEvent(String);
        int servoPin;
        Servo servo;
        MQTTClient *mqtt_client;
        bool hourglassIsOpen;
        bool timerIsActive;
        unsigned int interval;
        unsigned long startTime;
};

#endif