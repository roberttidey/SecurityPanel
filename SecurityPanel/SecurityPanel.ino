/*
 SecurityPanel
 Monitors alarm and sends notifications
 Allows extension of alarm sensors over wifi web requests
 R.J.Tidey 4 May 2017
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <IFTTTMaker.h>
/*
 Web set up
*/

#define AP_SSID "ssid"
#define AP_PASSWORD "password"
#define AP_MAX_WAIT 10
#define AP_PORT 80

//uncomment next line to use static ip address instead of dhcp
//#define AP_IP 192,168,0,200
#define AP_DNS 192,168,0,1
#define AP_GATEWAY 192,168,0,1
#define AP_SUBNET 255,255,255,0
#define AP_AUTHID "1234"

//IFTT and request key words
#define MAKER_KEY "MakerKey"
#define EVENT_NAME "security" // Name of IFTTT trigger
#define ZONE_SET "zoneSet" // sets zone override

//For update service
String host = "esp8266-security";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "password";

ESP8266WebServer server(AP_PORT);
WiFiClientSecure client;
IFTTTMaker ifttt(MAKER_KEY, client);
ESP8266HTTPUpdateServer httpUpdater;

//History
#define NAME_LEN 16
#define MAX_RECENT 64
int recentPin[MAX_RECENT];
int recentPinValue[MAX_RECENT];
unsigned long recentTimes[MAX_RECENT];
int recentIndex;
int timeInterval = 1000;
unsigned long elapsedTime;
char statString[32];

//IO
#define INPUT_COUNT 5
#define BELL 0
#define ZONE1 1
#define ZONE2 2
#define ZONE3 3
#define ZONE4 4
#define ADC_CAL 1.080
int iPins[INPUT_COUNT]  = {12,5,4,0,14};
int iPinValues[INPUT_COUNT];
float battery_mult = 10.47/0.47*ADC_CAL/1024;//resistor divider, vref, max count
float battery_volts = 12.0;
#define MAX_EXPANDERS 100
int zoneOverridePin = 13;
int zoneOverrideState = 0;
int maxExpander = -1;
int expanderState[MAX_EXPANDERS];

char* mainPage1 = "Main page goes here";
const char mainPage[] =
"<!DOCTYPE HTML>"
"<html>"
"<head>"
"<meta name = \"viewport\" content = \"width = device-width, initial-scale = 1.0, maximum-scale = 1.0, user-scalable=0\">"
"<title>ESP8266 Security Panel</title>"
"<style>"
"\"body { background-color: #808080; font-family: Arial, Helvetica, Sans-Serif; Color: #000000; }\""
"</style>"
"</head>"
"<body>"
"<h1>Security Panel</h1>"
"<FORM action=\"/request\" method=\"post\">"
"<P>"
"Command<br>"
"<INPUT type=\"text\" name=\"auth\" value=\"\">Password<BR>"
"<INPUT type=\"text\" name=\"event\" value=\"\">Event<BR>"
"<INPUT type=\"text\" name=\"value1\" value=\"100\">Value1<BR>"
"<INPUT type=\"text\" name=\"value2\" value=\"200\">Value2<BR>"
"<INPUT type=\"text\" name=\"value3\" value=\"300\">Value3<BR>"
"<INPUT type=\"submit\" value=\"Send\">"
"</P>"
"</FORM>"
"</body>"
"</html>";

/*
 Initialise wifi, message handlers and ir sender
*/
void setup() {
	Serial.begin(115200);
	Serial.println("Set up Security Panel");
	wifiConnect();
	
	//Update service
	Serial.println("Set up Web update service");
	MDNS.begin(host.c_str());
	httpUpdater.setup(&server, update_path, update_username, update_password);
	MDNS.addService("http", "tcp", AP_PORT);

	Serial.println("Set up Web command handlers");
	server.on("/status", checkStatus);
	server.on("/request", request);
	server.on("/recent", recentEvents);

	server.on("/", indexHTML);
	server.begin();
	Serial.println("Set up IO system");
	initIO();
	Serial.println("Set up complete");
}

/*
  Connect to local wifi with retries
*/
int wifiConnect()
{
	int retries = 0;
	Serial.print("Connecting to AP");
#ifdef AP_IP
	IPAddress addr1(AP_IP);
	IPAddress addr2(AP_DNS);
	IPAddress addr3(AP_GATEWAY);
	IPAddress addr4(AP_SUBNET);
	WiFi.config(addr1, addr2, addr3, addr4);
#endif
	WiFi.begin(AP_SSID, AP_PASSWORD);
	while (WiFi.status() != WL_CONNECTED && retries < AP_MAX_WAIT) {
		delay(1000);
		Serial.print(".");
		retries++;
	}
	Serial.println("");
	if(retries < AP_MAX_WAIT) {
		Serial.print("WiFi connected ip ");
		Serial.print(WiFi.localIP());
		Serial.printf(":%d mac %s\r\n", AP_PORT, WiFi.macAddress().c_str());
		return 1;
	} else {
		Serial.println("WiFi connection attempt failed"); 
		return 0;
	} 
}

void initIO() {
	for(int i=0;i< INPUT_COUNT;i++) {
		pinMode(iPins[i], INPUT_PULLUP);
		iPinValues[i] = digitalRead(iPins[i]);
	}
	pinMode(A0, INPUT);
	//Null recent events
	for(int i=0;i< MAX_RECENT;i++) {
		recentPin[i] = -1;
	}
	for(int i=0;i< MAX_EXPANDERS;i++) {
		expanderState[i] = 0;
	}
	digitalWrite(zoneOverridePin, 0);
	pinMode(zoneOverridePin, OUTPUT);
	zoneOverrideState = 0;
}

/*
 Check main page received from web
*/
void indexHTML() {
Serial.println("Main page requested");
    server.send(200, "text/html", mainPage);
}

/*
 Make status string for an event
*/
char* statusString(int i, int iValue) {
	if(i == BELL) {
		strcpy(statString,"Alarm");
	} else if(i<INPUT_COUNT) {
		strcpy(statString,"Zone");
		statString[4] = 48+i;
		statString[5] = 0;
	} else if(i <= (maxExpander + INPUT_COUNT)) {
		strcpy(statString,"Exp ");
		statString[4] = 48+(i-INPUT_COUNT)/10;
		statString[5] = 48+(i-INPUT_COUNT)%10;
		statString[6] = 0;
	} else {
		strcpy(statString,"Unknown");
	}
	if(iValue == 1)
		strcpy(statString+strlen(statString), " = HIGH<BR>");
	else
		strcpy(statString+strlen(statString), " = LOW<BR>");
	return statString;
}

/*
 Check status command received from web
*/
void checkStatus() {
	Serial.println("Check status received");
	if (server.arg("auth") != AP_AUTHID) {
		Serial.println("Unauthorized notify request");
		server.send(401, "text/html", "Unauthorized");
	} else {
		String response = "Security Panel is running<BR>";
		for(int i=0; i < INPUT_COUNT; i++) {
			response += statusString(i, iPinValues[i]);
		}
		if(server.arg("expand") == "true") {
			for(int i=0; i <= maxExpander; i++) {
				response += statusString(i+INPUT_COUNT, expanderState[i]);
			}
		}
		response +="Battery = " + String(battery_volts)+ "V<BR>Expansion Override ";
		if(zoneOverrideState == 0)
			response +="Off";
		else
			response +="On";
		server.send(200, "text/html", response);
	}
}

/*
 log recent events
*/
void addRecentEvent(int iPin, int iPinValue) {
	recentPin[recentIndex] = iPin;
	recentPinValue[recentIndex] = iPinValue;
	recentTimes[recentIndex] = elapsedTime;
	recentIndex++;
	if(recentIndex >= MAX_RECENT) recentIndex = 0;
}

/*
 return recent events
*/
void recentEvents() {
	Serial.println("recent events request received");
	if (server.arg("auth") != AP_AUTHID) {
		Serial.println("Unauthorized notify request");
		server.send(401, "text/html", "Unauthorized");
	} else {
		String response = "Recent events<BR>";
		long minutes;
		int recent = recentIndex - 1;
		if(recent < 0) recent = MAX_RECENT - 1;
		for(int i = 0;i<MAX_RECENT;i++) {
			if((recentPin[recent]) >=0) {
				minutes = (elapsedTime - recentTimes[recent]) * timeInterval / 60000;
				response += String(minutes) + " minutes ago ";
				response += statusString(recentPin[recent], recentPinValue[recent]);
			}
			recent--;
			if(recent < 0) recent = MAX_RECENT - 1;
		}
		server.send(200, "text/html", response);
	}
}

/*
 Test notify command received from web
*/
void request() {
	if (server.arg("auth") != AP_AUTHID) {
		Serial.println("Unauthorized notify request");
		server.send(401, "text/html", "Unauthorized");
	} else if(server.arg("event") == ZONE_SET) {
		zoneSet(server.arg("value1").toInt(),server.arg("value2").toInt());
		server.send(200, "text/html", "zone set " + server.arg("value1") + " " +server.arg("value2"));
	} else {
		Serial.print("Notify test request:");
		Serial.print(server.arg("event"));
		Serial.print(" :");
		Serial.print(server.arg("value1"));
		Serial.print(" :");
		Serial.print(server.arg("value2"));
		Serial.print(" :");
		Serial.println(server.arg("value3"));
		if(ifttt_notify(server.arg("event"), server.arg("value1"), server.arg("value2"), server.arg("value3")))
			server.send(200, "text/html", "notify OK");
		else
			server.send(401, "text/html", "notify failed");
	}
}

/*
 Handle zone set request
*/
void zoneSet(int zoneDevice, int zoneValue) {
    Serial.println("Set Zone expand device " + String(zoneDevice) + " to " + String(zoneValue));
	if(zoneDevice > maxExpander) maxExpander = zoneDevice;
	expanderState[zoneDevice] = zoneValue;
	zoneOverrideState = 0;
	//Or all expanders together to get final state. Any 1 Expander will trigger override.
	for(int i = 0; i <= maxExpander;i++) {
		zoneOverrideState |= expanderState[i];
	}
    Serial.println("Set Zone override to " + String(zoneOverrideState));
	digitalWrite(zoneOverridePin, zoneOverrideState);
	addRecentEvent(INPUT_COUNT+zoneDevice, zoneValue);
}

/*
 Send notify trigger to IFTTT
*/
int ifttt_notify(String eventName, String value1, String value2, String value3) {
  if(ifttt.triggerEvent(eventName, value1, value2, value3)){
    Serial.println("Successfully sent");
	return 1;
  } else {
    Serial.println("Failed!");
	return 0;
  }
}

/*
 Check Inputs
*/
void processIO() {
	int val;
	String response;
	for(int i=INPUT_COUNT-1; i >= 0; i--) {
		val = digitalRead(iPins[i]);
		//Serial.println("Process " + String(i) +":" + String(val));
		if(val != iPinValues[i]) {
			addRecentEvent(i, val);
			response += statusString(i,val);
			//if alarm and notify
			if(i == BELL) {
				ifttt_notify(EVENT_NAME, "alarm", response, "");
			}
		}
		iPinValues[i] = val;
	}
	//simple filter for battery measurement
	battery_volts = (battery_volts * 15 + battery_mult * analogRead(A0)) / 16;
}

void loop() {
	server.handleClient();
	processIO();
	delay(timeInterval);
	elapsedTime++;
}
