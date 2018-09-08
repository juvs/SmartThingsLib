#include "WifiManager.h"

//Timer timerCheckWifi(1000, checkWifi);

/**
 * Singleton IoT instance
 * Use getInstance() instead of constructor
 */
WifiManager* WifiManager::getInstance()
{
    if(_instance == NULL)
    {
        _instance = new WifiManager();
    }
    return _instance;
}
WifiManager* WifiManager::_instance = NULL;

WifiManager::WifiManager()
{
    _timerCheckWifi = new Timer(1000, &WifiManager::checkWifi, *this);
}

void WifiManager::log(String msg) {
    Serial.println(String("[WifiManager] " + msg));
}

void WifiManager::begin() {
    log("Connecting wifi...");
    WiFi.connect();
}

void WifiManager::manageWifi() {
    if (WiFi.ready()) {
        if (_connected == 0) {
            log("WiFi is ready!, connected to : " + String(WiFi.SSID()) + ", localIP : " + String(WiFi.localIP()));
            _connected = 1;
            //Connecting to particle
            if (!Particle.connected()) {
                log("Connecting to Particle...");
                Particle.connect();
            }
        }
    } else {
        if (!WiFi.connecting() && _connected == 1) {
            _connected = 0;
            log("Reconnecting WiFi...");
            WiFi.connect();
        }
        if (!_timerCheckWifi->isActive()) {
            _timerCheckWifi->start();
        }
    }
    if (Particle.connected()) {
      Particle.process();
    }
}

void WifiManager::checkWifi() {
    if (WiFi.ready()) {
        _timerCheckWifi->stop();
    } else {
        log("Trying connect to WiFi...");
    }
}
