#include <Wire.h>
#include <OneWire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <Adafruit_BMP280.h>
#include <BLE2902.h>

#include "FS.h"
#include "SPIFFS.h"

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "bbb5483e-36e1-4688-b7f5-ea07361b26a8"


BLEService        *SERVICE;
BLECharacteristic *LUMINOSITY_CHARACTERISTIC;
BLECharacteristic *PRESSURE_CHARACTERISTIC;
BLECharacteristic *TEMPERATURE_CHARACTERISTIC;

Adafruit_BMP280 BAROMETER;

const int LIGHT_SENSOR_PIN = 36;

hw_timer_t *DATA_READ_TIMER = NULL;
bool        TIMER_STATUS    = false;

const int BYTE = 8;

const std::string LUMINOSITY_FILE_NAME  = "/luminosity.txt";
const std::string PRESSURE_FILE_NAME    = "/pressure.txt";
const std::string TEMPERATURE_FILE_NAME = "/temperature.txt";


class LuminosityCharacteristicCallback;
class PressureCharacteristicCallback;
class TemperatureCharacteristicCallback;

void SetupBarometer		();
void SetupDataReadTimer ();
void SetupFile   		();
void SetupBLE			();

void IRAM_ATTR OnDataReadTimer();
void HandleDataReadTimer();

void SetupTemperatureCharacteristic();
void SetupPressureCharacteristic   ();
void SetupLuminosityCharacteristic ();

void HandleLuminosityReadTimer ();
void HandlePressureReadTimer   ();
void HandleTemperatureReadTimer();

void FormatSPIFFS();
void BeginSPIFFS ();

int ReadFile  (const int position, const std::string &file_name);
void WriteFile(const int value,    const std::string &file_name);

void Convert32BitToFour8Bit(const int value, int four_8bit[]);
int  ConvertFour8BitTo32Bit(const int four_8bit[]);

void WriteBLE();


void setup()
{
	Serial.begin(115200);

	while(not Serial)
	{
		delay(100);
	}

	SetupBarometer	  ();
	SetupDataReadTimer();
	SetupFile		  ();
	SetupBLE		  ();
}

void loop()
{
	HandleDataReadTimer();
}

class LuminosityCharacteristicCallback : public BLECharacteristicCallbacks
{
public:
	void onRead(BLECharacteristic *characteristic);
};

void LuminosityCharacteristicCallback::onRead(BLECharacteristic *characteristic)
{
	int static position = 0;

	static int prvious_value = 0;
	int value = ReadFile(position, LUMINOSITY_FILE_NAME);
	if(value < 0)
	{
		LUMINOSITY_CHARACTERISTIC->setValue(std::to_string(prvious_value));
	}
	else
	{
		LUMINOSITY_CHARACTERISTIC->setValue(std::to_string(value));
		prvious_value = value;
		++position;
	}
}

class PressureCharacteristicCallback : public BLECharacteristicCallbacks
{
public:
	void onRead(BLECharacteristic *characteristic) override;
};

void PressureCharacteristicCallback::onRead(BLECharacteristic *characteristic)
{
	static int position = 0;

	static int prvious_value = 0;
	int value = ReadFile(position, PRESSURE_FILE_NAME);
	if(value < 0)
	{
		PRESSURE_CHARACTERISTIC->setValue(std::to_string(prvious_value));
	}
	else
	{
		PRESSURE_CHARACTERISTIC->setValue(std::to_string(value));
		prvious_value = value;
		++position;
	}
}

class TemperatureCharacteristicCallback : public BLECharacteristicCallbacks
{
public:
	void onRead(BLECharacteristic *characteristic) override;
};

void TemperatureCharacteristicCallback::onRead(BLECharacteristic *characteristic)
{
	int static position = 0;

	static int prvious_value = 0;
	int value = ReadFile(position, TEMPERATURE_FILE_NAME);
	if(value < 0)
	{
		TEMPERATURE_CHARACTERISTIC->setValue(std::to_string(prvious_value));
	}
	else
	{
		TEMPERATURE_CHARACTERISTIC->setValue(std::to_string(value));
		prvious_value = value;
		++position;
	}
}

void SetupBarometer()
{
	const unsigned status = BAROMETER.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID);
	if(not status)
	{
		Serial.println("Could not find a valid BMP280 sensor, check wiring or try a different address!");

		while(true)
		{
			Serial.println("No BMP280 sensor found!");
			delay(1000);
		}
	}
	else
	{
		Serial.println("BMP280 sensor found!");
	}

	BAROMETER.setSampling(Adafruit_BMP280::MODE_NORMAL,
					Adafruit_BMP280::SAMPLING_X2,
					Adafruit_BMP280::SAMPLING_X16,
					Adafruit_BMP280::FILTER_X16,
					Adafruit_BMP280::STANDBY_MS_500);
}

void SetupDataReadTimer()
{
	DATA_READ_TIMER = timerBegin(0, 80, true);
	timerAttachInterrupt(DATA_READ_TIMER, &OnDataReadTimer, true);
	timerAlarmWrite(DATA_READ_TIMER, 5000000, true);
	timerAlarmEnable(DATA_READ_TIMER);
}

void SetupFile()
{
	BeginSPIFFS();
	FormatSPIFFS();
}

void SetupBLE()
{
	Serial.begin(115200);
	BLEDevice::init("My Esp32");
	BLEServer *server = BLEDevice::createServer();
	SERVICE = server->createService(SERVICE_UUID);

	SetupLuminosityCharacteristic();
	SetupPressureCharacteristic();
	SetupTemperatureCharacteristic();

	SERVICE->start();

	BLEAdvertising *advertising = BLEDevice::getAdvertising();
	advertising->start();

	Serial.println("Characteristic defined!");
}

void SetupLuminosityCharacteristic()
{
	LUMINOSITY_CHARACTERISTIC = SERVICE->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
	LUMINOSITY_CHARACTERISTIC->setValue("0");
	LUMINOSITY_CHARACTERISTIC->setCallbacks(new LuminosityCharacteristicCallback());
}

void SetupPressureCharacteristic()
{
	PRESSURE_CHARACTERISTIC = SERVICE->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
	PRESSURE_CHARACTERISTIC->setValue("0");
	PRESSURE_CHARACTERISTIC->setCallbacks(new PressureCharacteristicCallback());
}

void SetupTemperatureCharacteristic()
{
	TEMPERATURE_CHARACTERISTIC = SERVICE->createCharacteristic(CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
	TEMPERATURE_CHARACTERISTIC->setValue("0");
	TEMPERATURE_CHARACTERISTIC->setCallbacks(new TemperatureCharacteristicCallback());
}

void BeginSPIFFS()
{
	if(SPIFFS.begin(true))
	{
		Serial.println("Successfully begin SPIFFS");
	}
	else
	{
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
	}
}

void FormatSPIFFS()
{
	bool formatted = SPIFFS.format();
	if(formatted)
	{
		Serial.println("SPIFFS formatted successfully");
	}
	else
	{
		Serial.println("Error formatting");
		return;
	}
}

void IRAM_ATTR OnDataReadTimer()
{
	TIMER_STATUS = true;
}

void HandleDataReadTimer()
{
	if(TIMER_STATUS)
	{
		HandleLuminosityReadTimer ();
		HandlePressureReadTimer   ();
		HandleTemperatureReadTimer();

		TIMER_STATUS = false;
	}
}

void HandleLuminosityReadTimer()
{
	int analog_value = analogRead(LIGHT_SENSOR_PIN);
	WriteFile(analog_value, LUMINOSITY_FILE_NAME);
}

void HandlePressureReadTimer()
{
	int pressure = BAROMETER.readPressure();
	WriteFile(pressure, PRESSURE_FILE_NAME);
}

void HandleTemperatureReadTimer()
{
	int temperature = BAROMETER.readTemperature();
	WriteFile(temperature, TEMPERATURE_FILE_NAME);
}

void WriteFile(const int value, const std::string &file_name)
{
	File file_write = SPIFFS.open(file_name.c_str(), "a");
	if(!file_write)
	{
		Serial.println("Failed to open file_write for reading");
		return;
	}

	int four_8bit[4];
	Convert32BitToFour8Bit(value, four_8bit);

	for(int i = 0; i < 4; ++i)
	{
		file_write.write(four_8bit[i]);
	}

	file_write.close();
}

int ReadFile(const int position, const std::string &file_name)
{
	File file_read = SPIFFS.open(file_name.c_str(), "r");
	if(!file_read)
	{
		Serial.println("Failed to open file_read for reading");
		return -1;
	}

	for(int i = 0; i < (4 * position); ++i)
	{
		file_read.read();
	}

	int four_8bit[4];
	for(int i = 0; i < 4; ++i)
	{
		four_8bit[i] = file_read.read();
	}

	const int value = ConvertFour8BitTo32Bit(four_8bit);

	file_read.close();

	return value;
}

void Convert32BitToFour8Bit(const int value, int four_8bit[])
{
	for (int i = 0; i < 4; i++)
	{
		four_8bit[i] = (value >> (i * BYTE)) & 0xFF;
	}
}

int ConvertFour8BitTo32Bit(const int four_8bit[])
{
	int result = 0;

	for (int i = 0; i < 4; i++)
	{
		result |= (four_8bit[i] & 0xFF) << (i * BYTE);
	}

	return result;
}
