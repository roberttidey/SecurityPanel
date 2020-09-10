# Security Panel
An extension for simple 4 zone Security panel using esp8266
Tested with Pyronix Sterling compact but probably could be used with other
wired security panels.

Monitors 4 zones + 1 Alarm trigger signal
Sends IFTTT notification when Alarm trigger changes state
Built in simple web page mainly for testing request and notification
Status of signals on web page ip/status?auth=pinCode&expand=true
   If expand=true is included then report includes expander devices
Recent events up to 64 on web page ip/recent?auth=pinCode
Expand sensors beyond 4 zones wirelessly
	2 terminals provided to put in series with existing sensor.
	Normally low resistance, Open circuit when set wirelessly
	Expansion+ should be wired to the positive board terminal of an existing zone
	Expansion-should be wired to sensor lead taken from that terminal.
	Up to 100 extra wireless sensors

Config
  Edit BaseConfig.h
	Wifi set up
	  AP_PORT to access expansion service
	AP_AUTHID Pincode or password to authorise web commands
	update_username user for updating firmware
	update_password
  IFTTT notification
	Register with IFTTT
	Get IFTTT app on phone
	Add Maker channel and get key
	Edit ifttt maker key in ino
	Add an ifttt action with maker web request
      IF event 'security'
	  THEN action send notification from IFTTT app
	     {{OccurredAt}} "{{EventName}}" occurred {{Value1}} {{Value2}} {{Value3}} 
	
Expansion (could be esp8266 devices with sensors)
  Expansion device should post to ip/request with auth=AP_AUTHID event="zoneSet" value1=0-99 value2=0/1
  Value1 is the expansion device number
  Value2 is 0 (OK) 1 (Not OK)
  Allocate device numbers starting from 0 as software works out what maximum expansion device is
  If any Expansion device is 1 (Not OK) then the expansion override is triggered
		
Libraries
  BaseSupport.h from https://github.com/roberttidey/BaseSupport
  ESP8266WiFi
  ESP8266WebServer
  WiFiClientSecure
  ArduinoJson
  ESP8266mDNS
  ESP8266HTTPUpdateServer
  IFTTTMaker use v6 ArduinoJson version at https://github.com/jodovaljow/arduino-ifttt-maker
	
Install procedure
	Normal arduino esp8266 compile and upload
