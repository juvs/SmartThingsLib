#include "SmartThingsLib.h"

/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)

// returns the number of elements in the array
#define SIZE(array) (sizeof(array) / sizeof(*array))

String _persistentUuid;
String _deviceName;
String _deviceClass;
String _version;
String _serialDevice;
String _callbackURLST;
SmartThingsLib _stLib;

int _callbacksCount;
SmartThingsLib::CallbacksMap _callbacks[SMARTTHINGS_LIB_CALLBACKS_COUNT];

int _paramsIntCount;
SmartThingsLib::ParamsIntMap _paramsInt[SMARTTHINGS_LIB_PARAMS_COUNT];

int _paramsLongCount;
SmartThingsLib::ParamsLongMap _paramsLong[SMARTTHINGS_LIB_PARAMS_COUNT];

int _paramsStringCount;
SmartThingsLib::ParamsStringMap _paramsString[SMARTTHINGS_LIB_PARAMS_COUNT];

SmartThingsLib::CallbackVarSet *_callbackVarSet;

unsigned long _uptime = 0;
int _connected = 0;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,

SmartThingsLib::SmartThingsLib(const char *deviceId, const char *deviceName, const char *deviceClass, const char *version) :
    m_deviceId(deviceId),
    m_deviceName(deviceName),
    m_deviceClass(deviceClass),
    m_version(version),
    _udpMulticastAddress(239, 255, 255, 250),
    _webserver(WS_PREFIX, WS_PORT)
{
    _stLib = *this;
    _callbacksCount = 0;
    _paramsIntCount = 0;
}

void SmartThingsLib::prepareIds() {
    _serialDevice = System.deviceID();
    _version = String(m_version);
    _deviceName = String(m_deviceName);
    _deviceClass = String(m_deviceClass);
    _persistentUuid = String(m_deviceName) + "-" + String(m_version) + "-" + _serialDevice;
}

void SmartThingsLib::begin() {
    prepareIds();
    _wfm = WifiManager::getInstance();
    _wfm->begin();

    _webserver.setDefaultCommand(&indexWebCmd);
    _webserver.addCommand("index.html", &indexWebCmd);
    _webserver.addCommand("description.xml", &descriptionWebCmd);
    _webserver.addCommand("subscribe", &subscribeSTWebCmd);
    _webserver.addCommand("configSet", &configSetWebCmd);
    _webserver.addCommand("configGet", &configGetWebCmd);
    _webserver.setFailureCommand(&failureWebCmd);
}

void SmartThingsLib::tryConnect() {
    if (WiFi.ready()) {
        if (_connected == 0) {
            _uptime = millis();
            // start the UDP and the Multicast
            _udp.begin(UDP_LOCAL_PORT);
            _udp.joinMulticast(_udpMulticastAddress);

            /* start the webserver */
            _webserver.begin();
            _connected = 1;

            log("Connected!");
            showInfo();
        }
    } else {
        if (_connected == 1) { //Me desconecto
            log("Lost WiFi connection!");
            log("Shutting down udp and webserver...");
            _uptime = 0;
            _udp.leaveMulticast(_udpMulticastAddress);
            _udp.stop();
            _webserver.reset();
            _connected = 0;
        }
    }
}

void SmartThingsLib::process() {

    char buff[64];
    int len = 64;

    _wfm->manageWifi();
    tryConnect();

    if (WiFi.ready()) {
        /* process incoming connections one at a time forever */
        _webserver.processConnection(buff, &len);
        checkUDPMsg();
    }
}

void SmartThingsLib::showInfo() {
    log("Device info");
    log("  - Device Id    = " + String(m_deviceId));
    log("  - Device Name  = " + String(m_deviceName));
    log("  - Device Class = " + String(m_deviceClass));
    log("  - Version      = " + String(m_version));
    log("WebServer Ready!");
    log("  - localIP      = " + String(WiFi.localIP()));
    log("  - subnetMask   = " + String(WiFi.subnetMask()));
    log("  - gatewayIP    = " + String(WiFi.gatewayIP()));
    // log("- dnsServerIP = " + String(WiFi.dnsServerIP()));
    // log("- dhcpServerIP = " + String(WiFi.dhcpServerIP()));
}

bool SmartThingsLib::isConnected() {
    if (_connected == 0) {
        log("Device is not connected to WiFi");
        return false;
    } else {
        return true;
    }
}

void SmartThingsLib::log(String msg) {
    #if STLIB_SERIAL_DEBUGGING > 1
        Serial.println(String("[SmartThingsLib] " + msg));
    #endif
}

void SmartThingsLib::monitorVariable(const char *name, int &var) {
    if (_paramsIntCount < SIZE(_paramsInt)) {
        checkVariable(name);
        _paramsInt[_paramsIntCount].name = name;
        _paramsInt[_paramsIntCount++].value = &var;
        log("Added int variable with name = " + String(name) + ", value = " + String(var));
    }
}

void SmartThingsLib::monitorVariable(const char *name, long &var) {
    if (_paramsLongCount < SIZE(_paramsLong)) {
        checkVariable(name);
        _paramsLong[_paramsLongCount].name = name;
        _paramsLong[_paramsLongCount++].value = &var;
        log("Added long variable with name = " + String(name) + ", value = " + String(var));
    }
}

void SmartThingsLib::monitorVariable(const char *name, String &var) {
    if (_paramsStringCount < SIZE(_paramsString)) {
        checkVariable(name);
        _paramsString[_paramsStringCount].name = name;
        _paramsString[_paramsStringCount++].value = &var;
        log("Added String variable with name = " + String(name) + ", value = " + String(var));
    }
}

void SmartThingsLib::checkVariable(const char *name) {
    uint8_t i;
    uint8_t found = 0;
    size_t name_len;
    name_len = strlen(name);
    //Review int values
    for (i = 0; i < _paramsIntCount; ++i) {
        if ((name_len == strlen(_paramsInt[i].name)) && (strncmp(name, _paramsInt[i].name, name_len) == 0)) {
            log("Variable " + String(name) + " removed from monitor int values array.");
            _paramsInt[i].name = String("").c_str();
            found = 1;
            break;
        }
    }
    if (found == 1) return;
    for (i = 0; i < _paramsLongCount; ++i) {
        if ((name_len == strlen(_paramsLong[i].name)) && (strncmp(name, _paramsLong[i].name, name_len) == 0)) {
            log("Variable " + String(name) + " removed from monitor long values array.");
            _paramsLong[i].name = String("").c_str();
            found = 1;
            break;
        }
    }
    if (found == 1) return;
    for (i = 0; i < _paramsStringCount; ++i) {
        if ((name_len == strlen(_paramsString[i].name)) && (strncmp(name, _paramsString[i].name, name_len) == 0)) {
            log("Variable " + String(name) + " removed from monitor String values array.");
            _paramsString[i].name = String("").c_str();
            break;
        }
    }
}

void SmartThingsLib::getUpTime(String &uptime) {
    long diff = (millis() - _uptime) / 1000;
    int days = elapsedDays(diff);
    int hours = numberOfHours(diff);
    int minutes = numberOfMinutes(diff);
    int seconds = numberOfSeconds(diff);
    if (days > 0) {
        uptime = String(days) + " days and " + String(hours) + ":" + String(minutes) + ":" + String(seconds);
    } else {
        uptime = String(hours) + ":" + String(minutes) + ":" + String(seconds);
    }
}

void SmartThingsLib::callbackForAction(const char *action, Callback *callback) {
    if (_callbacksCount < SIZE(_callbacks)){
        _callbacks[_callbacksCount].action = action;
        _callbacks[_callbacksCount++].callback = callback;
        log("Added action = " + String(action) + " to callbacks, records = " + String(_callbacksCount));
    }
}

void SmartThingsLib::callbackForVarSet(CallbackVarSet *callback) {
    _callbackVarSet = callback;
}

String SmartThingsLib::dispatchCallback(char *action) {
    log("Checking action = " + String(action) + " to dispatch possible callback in " + String(_callbacksCount) + " records...");
    if (strlen(action) > 0) {
        // if there is no URL, i.e. we have a prefix and it's requested without a
        // trailing slash or if the URL is just the slash
        // if the URL is just a slash followed by a question mark
        // we're looking at the default command with GET parameters passed
        if ((action[0] == 0) || ((action[0] == '/') && (action[1] == 0)) || (action[0] == '/') && (action[1] == '?')) {
            log("Action = " + String(action) + " malformed.");
            return "";
        }

        // We now know that the URL contains at least one character.  And,
        // if the first character is a slash,  there's more after it.
        if (action[0] == '/') {
            uint8_t i;
            char *qm_loc;
            size_t action_len;
            uint8_t qm_offset;
            // Skip over the leading "/",  because it makes the code more
            // efficient and easier to understand.
            action++;
            // Look for a "?" separating the filename part of the URL from the
            // parameters.  If it's not there, compare to the whole URL.
            qm_loc = strchr(action, '?');
            action_len = (qm_loc == NULL) ? strlen(action) : (qm_loc - action);
            qm_offset = (qm_loc == NULL) ? 0 : 1;
            for (i = 0; i < _callbacksCount; ++i) {
                if ((action_len == strlen(_callbacks[i].action)) && (strncmp(action, _callbacks[i].action, action_len) == 0)) {
                    log("Found callback for action = " + String(action) + " dispatching...");
                    return _callbacks[i].callback();
                }

            }
        }
    }
    log("No callback found for action = " + String(action));
    return "";
}

// **** ST ACTIONS **** //

bool SmartThingsLib::notifyHub(String body) {
    if (isConnected()) {
        if (_callbackURLST.trim().length() > 0) {
            HttpClient http;
            http_request_t request;
            http_response_t response;
            http_header_t headersST[] = {
                { "content-type" , "application/json"},
                { "Accept" , "*/*"},
                { "access-control-allow-origin" , "*"},
                { "server" , "Webduino/1.9.1"},
                { NULL, NULL } // NOTE: Always terminate headers will NULL
            };

            String serverIP = _callbackURLST;
            String serverPort;
            String serverPath;

            serverIP = serverIP.substring(7); //remove http://

            int indexColon = serverIP.indexOf(":");

            serverPort = serverIP.substring(indexColon + 1, serverIP.indexOf("/", indexColon + 1));
            serverPath = serverIP.substring(serverIP.indexOf("/", indexColon + 1));
            serverIP = serverIP.substring(0, serverIP.indexOf(":"));

            log("Notify to ST HUB, body: " + body + ", server: " + serverIP + ", port: " + serverPort + ", path: " + serverPath);

            IPAddress server = WiFi.resolve(serverIP);
            request.ip = server;
            request.port = serverPort.toInt();
            request.path = serverPath;

            request.body = body;

            // Get request
            http.post(request, response, headersST);

            if (response.status == 200 || response.status == 202) {
                log("Notify to HUB with response OK!");
                return true;
            } else {
                log("Notify ST ERROR! " + String(response.status) + " " + String(response.body));
                return false;
            }
        } else {
            log("No callbackurl for notify to the HUB!");
        }
    }
}

// **** UDP PROCESSING **** //

void SmartThingsLib::checkUDPMsg() {

  // if there’s data available, read a packet
  int packetSize = _udp.parsePacket();

  // Check if data has been received
  if (packetSize) {

    int len = _udp.read(packetBuffer, 255);
    if (len > 0) {
        packetBuffer[len] = 0;
    }

    String request = packetBuffer;

    if (request.indexOf('M-SEARCH * HTTP/1.1') > 0 && request.indexOf('ST: urn:schemas-upnp-org:device:') > 0) {
        if(request.indexOf("urn:schemas-upnp-org:device:" + _deviceClass + ":1") > 0) {
#if STLIB_SERIAL_DEBUGGING > 1
            log("Received UDP packet from : ");
            IPAddress remote = _udp.remoteIP();
            for (int i =0; i < 4; i++) {
                Serial.print(remote[i], DEC);
                if (i < 3) {
                    Serial.print(".");
                }
            }
            Serial.print(", port = ");
            Serial.println(String(_udp.remotePort()));
            log("Responding to M-SEARCH request...");
#endif
            int r = random(4);
            delay(r);
            respondToSearchUdp();
        }
    } else if (request.indexOf("upnp:event") > 0) {
#if STLIB_SERIAL_DEBUGGING > 1
        log("Received UDP packet of size ");
        Serial.println(String(packetSize));
        Serial.print("From ");
        IPAddress remote = _udp.remoteIP();
        for (int i =0; i < 4; i++) {
            Serial.print(remote[i], DEC);
            if (i < 3) {
                Serial.print(".");
            }
        }
        Serial.print(", port ");
        Serial.println(String(_udp.remotePort()));
        Serial.println("Request:");
        Serial.println(request);
#endif
    }
  }
}

void SmartThingsLib::respondToSearchUdp() {
    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String response =
         "HTTP/1.1 200 OK\r\n"
         "CACHE-CONTROL: max-age=86400\r\n"
         //"DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
         "EXT:\r\n"
         "LOCATION: http://" + String(s) + ":80/description.xml\r\n"
         "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
         //"01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
         "SERVER: FreeRTOS/6.0.5, UPnP/1.0, IpBridge/0.1\r\n"
         "ST: urn:schemas-upnp-org:device:" + _deviceClass + ":1\r\n"
         "USN: uuid:" + _persistentUuid + "::urn:schemas-upnp-org:device:" + _deviceClass + ":1\r\n";
         //"X-User-Agent: redsonic\r\n\r\n";

    _udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
    _udp.write(response.c_str());
    _udp.endPacket();

    log("UPNP Response to M-SEARCH sent!");
}

// **** WEBSERVER COMMANDS **** //

void SmartThingsLib::indexWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
  /* this line sends the standard "we're all OK" headers back to the
     browser */
  server.httpSuccess();

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type != WebServer::HEAD) {
    String indexMsg = "<h1>" + _deviceName + "</h1><br/><h2>v" + _version + "</h2>";

    /* this is a special form of print that outputs from PROGMEM */
    server.printP(indexMsg.c_str());
  }
}

void SmartThingsLib::descriptionWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
    log("Webserver call for description.xml...");
    /* this line sends the standard "we're all OK" headers back to the
        browser */
    server.httpSuccess("text/xml");

  /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
  if (type != WebServer::HEAD)
  {

    /* this defines some HTML text in read-only memory aka PROGMEM.
     * This is needed to avoid having the string copied to our limited
     * amount of RAM. */

    String description_xml = "<?xml version=\"1.0\"?>"
         "<root xmlns=\"urn:schemas-upnp-org:device-1-0\">"
             "<device>"
                "<deviceType>urn:schemas-upnp-org:device:" + _deviceClass + ":1</deviceType>"
                "<friendlyName>"+ _deviceName +"</friendlyName>"
                "<manufacturer>Tenser International Inc.</manufacturer>"
                "<modelName>" + _deviceName + "</modelName>"
                "<modelNumber>" + _version + "</modelNumber>"
                "<UDN>uuid:"+ _persistentUuid +"</UDN>"
                "<serialNumber>" + _serialDevice + "</serialNumber>"
                "<binaryState>0</binaryState>"
                "<serviceList>"
                /*  "<service>"
                      "<serviceType>urn:SmartBell:service:basicevent:1</serviceType>"
                      "<serviceId>urn:SmartBell:serviceId:basicevent1</serviceId>"
                      "<controlURL>/upnp/control/basicevent1</controlURL>"
                      "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                      "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"*/
              "</serviceList>"
              "</device>"
          "</root>"
        "\r\n";

    /* this is a special form of print that outputs from PROGMEM */
    server.printP(description_xml.c_str());
    log("Webserver sending response from description.xml call...");
  }
}

void SmartThingsLib::subscribeSTWebCmd(WebServer &server, WebServer::ConnectionType type, char *, bool)
{
    log("Webserver call for subscribe...");
    /* this line sends the standard "we're all OK" headers back to the
     browser */
    server.httpSuccess();

    /* if we're handling a GET or POST, we can output our data here.
     For a HEAD request, we just stop after outputting headers. */
    if (type == WebServer::SUBSCRIBE)
    {
        char callback[255];

        server.getHeaderCallback(callback);
        _callbackURLST = String(callback);
        _callbackURLST = _callbackURLST.substring(1); //Removemos el <
        _callbackURLST.remove(_callbackURLST.length() - 1); //Removemos el >

        log("Webserver Callback : " + _callbackURLST);
        /* this defines some HTML text in read-only memory aka PROGMEM.
         * This is needed to avoid having the string copied to our limited
         * amount of RAM. */
        String eventservice_xml = "\r\n";
        /* this is a special form of print that outputs from PROGMEM */
        server.printP(eventservice_xml.c_str());
    }
    log("Webserver sending response from subscribe call...");
}

void SmartThingsLib::failureWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    if (strlen(url_tail) > 0) {
        String response = _stLib.dispatchCallback(url_tail);
        if (!response.equals("")) {
            server.httpSuccess("application/json");
            server.printP(response.c_str());
        } else {
            log("Webserver failure! url not mapped!, type = " + String(type) + ", url = " + String(url_tail) + ", tail_complete = " + String(tail_complete) );
        }
    }
}

#define PARAM_NAMELEN 32
#define PARAM_VALUELEN 32

void SmartThingsLib::configSetWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    URLPARAM_RESULT rc;
    char name[PARAM_NAMELEN];
    char value[PARAM_NAMELEN];

    /* this line sends the standard "we're all OK" headers back to the
       browser */
    server.httpSuccess();

    /* if we're handling a GET or POST, we can output our data here.
       For a HEAD request, we just stop after outputting headers. */
    if (type == WebServer::HEAD)
      return;

    if (strlen(url_tail)) {
        while (strlen(url_tail)) {
            rc = server.nextURLparam(&url_tail, name, PARAM_NAMELEN, value, PARAM_VALUELEN);
            if (rc != URLPARAM_EOS) {
                uint8_t i;
                size_t param_len;
                log("Config set for param = " + String(name) + ", with value = " + String(value));
                param_len = strlen(name);
                //Review on int values
                for (i = 0; i < _paramsIntCount; ++i) {
                    if ((param_len == strlen(_paramsInt[i].name)) && (strncmp(name, _paramsInt[i].name, param_len) == 0)) {
                        log("Setting value for int variable = " + String(name) + ", current value " + String(*_paramsInt[i].value) + ", new value = " + String(value));
                        *_paramsInt[i].value = (int)String(value).toInt();
                        if (_callbackVarSet != nullptr) {
                            _callbackVarSet(String(name));
                        }
                        return;
                    }
                }
                //Review on long values
                for (i = 0; i < _paramsLongCount; ++i) {
                    if ((param_len == strlen(_paramsLong[i].name)) && (strncmp(name, _paramsLong[i].name, param_len) == 0)) {
                        log("Setting value for long variable = " + String(name) + ", current value " + String(*_paramsLong[i].value) + ", new value = " + String(value));
                        *_paramsLong[i].value = String(value).toInt();
                        if (_callbackVarSet != nullptr) {
                            _callbackVarSet(String(name));
                        }
                        return;
                    }
                }
                //Review on String values
                for (i = 0; i < _paramsStringCount; ++i) {
                    if ((param_len == strlen(_paramsString[i].name)) && (strncmp(name, _paramsString[i].name, param_len) == 0)) {
                        log("Setting value for String variable = " + String(name) + ", current value " + String(*_paramsString[i].value) + ", new value = " + String(value));
                        // int inputLen = sizeof(value);
                        // char cValue[inputLen];
                        // strcpy(cValue, value.c_str());
                        *_paramsString[i].value = value;
                        if (_callbackVarSet != nullptr) {
                            _callbackVarSet(String(name));
                        }
                        return;
                    }
                }
            }
        }
    }
}

void SmartThingsLib::configGetWebCmd(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
    URLPARAM_RESULT rc;
    char name[PARAM_NAMELEN];
    char value[PARAM_NAMELEN];

    /* this line sends the standard "we're all OK" headers back to the
       browser */
    server.httpSuccess("application/json");

    /* if we're handling a GET or POST, we can output our data here.
       For a HEAD request, we just stop after outputting headers. */
    if (type == WebServer::HEAD)
      return;

    if (strlen(url_tail)) {
        while (strlen(url_tail)) {
            rc = server.nextURLparam(&url_tail, name, PARAM_NAMELEN, value, PARAM_VALUELEN);
            if (rc != URLPARAM_EOS) {
                uint8_t i;
                size_t param_len;
                size_t value_len;
                log("Config get for param = " + String(name));
                param_len = strlen(name);
                if (strncmp(name, "name", param_len) == 0) {
                    value_len = strlen(value);
                    //Review on int values
                    for (i = 0; i < _paramsIntCount; ++i) {
                        if ((value_len == strlen(_paramsInt[i].name)) && (strncmp(value, _paramsInt[i].name, value_len) == 0)) {
                            log("Return value for variable = " + String(value) + ", current int value " + String(*_paramsInt[i].value));
                            server.printP(buildResponseVariable(name, *_paramsInt[i].value));
                            return;
                        }
                    }
                    //Review on long values
                    for (i = 0; i < _paramsLongCount; ++i) {
                        if ((value_len == strlen(_paramsLong[i].name)) && (strncmp(value, _paramsLong[i].name, value_len) == 0)) {
                            log("Return value for variable = " + String(value) + ", current long value " + String(*_paramsLong[i].value));
                            server.printP(buildResponseVariable(name, *_paramsLong[i].value));
                            return;
                        }
                    }
                    //Review on String values
                    for (i = 0; i < _paramsStringCount; ++i) {
                        if ((value_len == strlen(_paramsString[i].name)) && (strncmp(value, _paramsString[i].name, value_len) == 0)) {
                            log("Return value for variable = " + String(value) + ", current String value " + String(*_paramsString[i].value));
                            server.printP(buildResponseVariable(name, *_paramsString[i].value));
                            return;
                        }
                    }
                    log("variable = " + String(value) + " not found!");
                    server.printP(buildJsonResponseVariable(name, "", false));
                }
            }
        }
    }
}

const char* SmartThingsLib::buildResponseVariable(char *name, int value) {
    return buildJsonResponseVariable(name, String(value), true);
}

const char* SmartThingsLib::buildResponseVariable(char *name, long value) {
    return buildJsonResponseVariable(name, String(value), true);
}

const char* SmartThingsLib::buildResponseVariable(char *name, String value) {
    return buildJsonResponseVariable(name, String(value), true);
}

const char* SmartThingsLib::buildJsonResponseVariable(char *name, String value, bool success) {
    String responseValue;
    if (success == true) {
        responseValue = "{"
            "\"name\":\"" + String(name) + "\","
            "\"value\":\"" + value + "\","
            "\"success\":\"true\","
            "\"type\":\"configuration\""
        "}";
    } else {
        responseValue =
            "{"
                "\"success\":\"false\","
                "\"type\":\"configuration\""
            "}";
    }
    return responseValue.c_str();
}
