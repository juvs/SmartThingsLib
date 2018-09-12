// Example usage for SmartThingsLib library by Juvenal Guzman.

#include <SmartThingsLib.h>

SYSTEM_THREAD(ENABLED);
SYSTEM_MODE(MANUAL);

// Initialize objects from the lib device id prefix, device name, device class, version
SmartThingsLib smartThingsLib("mydevice", "My Device", "MyDevice", "0.0.1");

int turnOn = 0;
int myVarInt = 0;
long myVarLong = 0;
String myVarStr = "empty";

void setup() {
    Serial.begin(9600);
    delay(1500); // Allow board to settle
    // Call functions on initialized library objects that require hardware
    smartThingsLib.begin();

    //A GET call from ST to this device, normally don't pass parameters like REST request
    smartThingsLib.callbackForAction("action", &callbackAction);

    //If you wanna response something, do it on the callback funcion.
    smartThingsLib.callbackForAction("status", &callbackStatus);

    //To change this value from ST call a GET using this url http://{your_ip}/configSet?myVarInt=1, the value will change to 1
    //can register int, long and String type var
    //you can also retrive the value of this variable on ST using this url http://{your_ip}/configGet?name=myVarInt
    smartThingsLib.monitorVariable("myVarInt", myVarInt);

    smartThingsLib.monitorVariable("myVarStr", myVarStr);

    smartThingsLib.monitorVariable("myVarLong", myVarLong);

    //A callback that notifies when a variable change using configSet call, applies to all variables register using monitorVariable
    smartThingsLib.callbackForVarSet(&callbackVariableSet);

    pinMode(D7, OUTPUT);
}

void loop() {
    // Use the library's initialized objects and functions
    smartThingsLib.process();

    if (System.buttonPushed() > 100) { // just press once SETUP button
        Serial.println("Sending event to ST hub!");
        String json = "{\"event\":\"monitor_start\"}";
        smartThingsLib.notifyHub(json);
    }
    delay(5);
}

String callbackAction() {
    //Do your stuff...
    Serial.println("Call from /action url, do some!");
    if (turnOn == 0) {
        turnOn = 1;
        digitalWrite(D7, HIGH);
    } else {
        turnOn = 0;
        digitalWrite(D7, LOW);
    }
    return "ok";
}

String callbackStatus() {
    String uptime;
    smartThingsLib.getUpTime(uptime);
    String led = turnOn == 0 ? "off" : "on";
    return String("{\"uptime\":\"" + uptime + "\", \"led\":\"" + led + "\" }");
}

void callbackVariableSet(String param) {
    //When the variable change, do some usefull thing on this device...
    Serial.println("Variable " + param + " changed!");
    if (myVarInt > 0) {
        digitalWrite(D7, HIGH);
    } else {
        digitalWrite(D7, LOW);
    }
    if (myVarLong > 100) {
        digitalWrite(D7, HIGH);
    } else {
        digitalWrite(D7, LOW);
    }

    if (param == "myVarStr") {
        Serial.println("Variable myVarStr new value is " + myVarStr);
    }
}
