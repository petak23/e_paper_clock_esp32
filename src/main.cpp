#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <GxEPD.h>
#include <GxDEPG0213BN/GxDEPG0213BN.h> // 2.13" b/w 128x250, SSD1680, TTGO T5 V2.4.1, V2.3.1
#include "Tasker.h"
#include "time.h"

#include "definitions.h"

#include GxEPD_BitmapExamples

// FreeFonts from Adafruit_GFX
/*#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeSansBoldOblique24pt7b.h>*/

#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#if defined(ESP32)

// for SPI pin definitions see e.g.:
// C:\Users\xxx\Documents\Arduino\hardware\espressif\esp32\variants\lolin32\pins_arduino.h

GxIO_Class io(SPI, /*CS=5*/ SS, /*DC=*/17, /*RST=*/16); // arbitrary selection of 17, 16
GxEPD_Class display(io, /*RST=*/16, /*BUSY=*/4);				// arbitrary selection of (16), 4

#endif

boolean timeNTPwait = false;
boolean firstRun = true;

// Inicializeácia Tasker-a
Tasker tasker;

int getHour(tm *timeinfo)
{
	char timeHour[3];
	strftime(timeHour, 3, "%H", timeinfo);
	Serial.println(timeHour);
	return atoi(timeHour);
}
int getMin(tm *timeinfo)
{
	char _t[3];
	strftime(_t, 3, "%M", timeinfo);
	Serial.println(_t);
	return atoi(_t);
}
int getSec(tm *timeinfo)
{
	char _t[3];
	strftime(_t, 3, "%S", timeinfo);
	Serial.println(_t);
	return atoi(_t);
}

String getDT(tm *timeinfo)
{
	char _dt[20];
	strftime(_dt, 20, "%d-%m-%Y %H:%M:%S", timeinfo);
	return _dt;
}

void log(String logMessage)
{
#if SERIAL_PORT_ENABLED
	struct tm timeinfo;
	String tmp = "";
	if (!getLocalTime(&timeinfo))
	{
		Serial.println("Failed to obtain time(log)");
		return;
	}
	else
	{
		tmp = getDT(&timeinfo) + " -> ";
	}
	tmp += logMessage;

	Serial.println(tmp);
#endif
}

void setTimezone(String timezone)
{
	Serial.printf("  Setting Timezone to %s\n", timezone.c_str());
	setenv("TZ", timezone.c_str(), 1); //  Now adjust the TZ.  Clock settings are adjusted to show the new local time
	tzset();
}

void initTime(String timezone)
{
	struct tm timeinfo;

	Serial.println("Setting up time");
	configTime(0, 0, NTP_SERVER); // First connect to NTP server, with 0 TZ offset
	if (!getLocalTime(&timeinfo))
	{
		Serial.println("Failed to obtain time(initTime)");
		return;
	}
	Serial.println("-> Got the time from NTP");
	// Now we can set the real timezone
	setTimezone(timezone);
}

void printLocalTime()
{
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo))
	{
		Serial.println("Failed to obtain time(printLocalTime)");
		return;
	}
}

void connectToWifi()
{
	log("Connecting to Wi-Fi...");
	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		log(".");
	}
	String ipaddress = WiFi.localIP().toString();
	log("WiFi connected. IP address: [" + ipaddress + " ]. " + String(WiFi.RSSI()) + "dBm");
}

/*void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
	log("Connected to Wi-Fi.");
	log("IP address: ");
	log(WiFi.localIP());
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
	log("Disconnected from Wi-Fi.");
	tasker.setTimeout(connectToWifi, 2000); // Opakovaný pokus o pripojenie za 2s
}*/

void drawDigit(int digit, int position)
{
	const int y = 20;	 // Začiatok y
	const int w = 40;	 // Šírka
	const int hr = 10; // Hrúbka
	//   |--1--|
	//   4     5
	//   |--2--|
	//   6     7
	//   |--3--|
	// Bity pre čísla: ..... 0     1     2     3     4     5     6     7     8     9     -
	const byte digits[11] = {0x7D, 0x50, 0x37, 0x57, 0x5A, 0x4F, 0x6F, 0x51, 0x7F, 0x5F, 0x02};
	const int xp[4] = {5, 62, 138, 195};
	int x = position > 0 && position < 5 ? xp[position - 1] : 0; // Začiatok x
	digit = digit > 11 && digit < 0 ? 10 : digit;

	if (digits[digit] & 0x01) // 1 - hore
	{
		display.fillRect(x + 5, y, w, hr, GxEPD_BLACK);
	}
	if (digits[digit] & 0x02) // 2 - stred
	{
		display.fillRect(x + 5, y + w + hr / 2, w, hr, GxEPD_BLACK);
	}
	if (digits[digit] & 0x04) // 3 - dole
	{
		display.fillRect(x + 5, y + 2 * w + hr / 2, w, hr, GxEPD_BLACK);
	}
	if (digits[digit] & 0x08) // 4 | hore vľavo
	{
		display.fillRect(x, y + hr / 2, hr, w, GxEPD_BLACK);
	}
	if (digits[digit] & 0x10) // 5 | hore vpravo
	{
		display.fillRect(x + w, y + hr / 2, hr, w, GxEPD_BLACK);
	}
	if (digits[digit] & 0x20) // 6 | dole vľavo
	{
		display.fillRect(x, y + w + hr, hr, w, GxEPD_BLACK);
	}
	if (digits[digit] & 0x40) // 7 | dole vpravo
	{
		display.fillRect(x + w, y + w + hr, hr, w, GxEPD_BLACK);
	}

	display.fillCircle(125, y + w, 6, GxEPD_BLACK);
	display.fillCircle(125, y + w + 2 * hr, 6, GxEPD_BLACK);
}

void doTimeTask()
{
	struct tm timeinfo;
	getLocalTime(&timeinfo);
	int h = getHour(&timeinfo);
	int m = getMin(&timeinfo);
	String my_time = (h < 10 ? "0" : "") + String(h) + (m < 10 ? "0" : "") + String(m);
	int l = my_time.length() + 1;
	char ch_time[l];
	my_time.toCharArray(ch_time, l);
	// log("H:M = " + my_time);

	display.init(115200); // enable diagnostic output on Serial
	display.setRotation(1);
	display.setTextColor(GxEPD_BLACK);

	display.fillScreen(GxEPD_WHITE);
	for (byte i = 0; i < 4; i++)
	{
		drawDigit((int)(ch_time[i] - '0'), i + 1);
	}

	display.update();
	display.powerDown();
}

void firstPublish()
{
	log("Prvé plánované meranie");
	// Publikovanie nových hodnôt od teraz každých PUBLISH_TIME min.
	tasker.setInterval(doTimeTask, (PUBLISH_TIME * 60000));
	doTimeTask();
}

void readNTP()
{
	struct tm timeinfo;
	getLocalTime(&timeinfo);
	int put = PUBLISH_TIME;
	int min = getMin(&timeinfo);
	int sec = 60 - getSec(&timeinfo);								 // Sekundy do celej minúty
	int min_to_publish = put - (min % put) - 1;			 // Celé minúty do najbližšej publikácie
	long sec_to_publish = min_to_publish * 60 + sec; // Sekundy do najbližšej publikácie
	log("put=" + String(put) + " | min=" + String(min) + " | sec=" + String(sec) + " | min_to_publish=" + String(min_to_publish));
	firstRun = false;
	tasker.setTimeout(firstPublish, (sec_to_publish * 1000));
	log("Nastavenie najbližšieho merania o " + String(sec_to_publish) + "sec");
	doTimeTask();
}

/* Funkcia sa spustí pri prvom prechode "loop" slučky */
void firstRunFunction()
{
	if (!timeNTPwait)
	{
		timeNTPwait = true;
		for (byte i = 0; i < 4; i++)
		{
			drawDigit(11, i + 1);
		}
		readNTP();
		// tasker.setTimeout(readNTP, 2000); // Čakanie na serverový čas
	}
}

void setup()
{
	delay(2000);
	Serial.begin(115200);

	connectToWifi();

	initTime(TIME_ZONE);

	log("setup done");
}

void loop()
{
	tasker.loop();

	if (firstRun)
	{
		firstRunFunction();
	}
};