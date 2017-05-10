# Security Panel
An extension for simple 4 zone Security panel using esp8266

Monitors 4 zones + 1 Alarm trigger signal
Sends IFTTT notification when Alarm trigger changes state
Built in simple web page mainly for testing request and notification
Status of signals on web page ip/status
Recent events on web page ip/recent
Expand sensors beyond 4 zones wirelessly

Config
  Edit IRBlasterWeb.ino
	Wifi set up
      AP_SSID Local network ssid
	  AP_PASSWORD 
	  AP_IP If static IP to be used
	  AP_PORT to access expansion service
	AP_AUTHID Pincode or password to authorise web commands
	update_username user for updating firmware
	update_password
	
Expansion
  Post to ip/request with auth=AP_AUTHID event="zoneSet" value1=0/1 (off/on)
		
Libraries
  ESP8266WiFi
  ESP8266WebServer
  WiFiClientSecure
  ArduinoJson
  ESP8266mDNS
  ESP8266HTTPUpdateServer
  IFTTTMaker
	
Install procedure
	Normal arduino esp8266 compile and upload
