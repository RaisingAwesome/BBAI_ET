#include "rapidjson/document.h"	//Get these two includes at the following link and place with
#include "rapidjson/error/en.h"	//at the main code location: https://github.com/Tencent/rapidjson/tree/master/include/rapidjson
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/err.h>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>

using namespace std;
using namespace rapidjson;

class Wally {
	Document d;
	void writeStatusToFile();
	string readWallyFile();

public:
	Wally();
	void setTemperatureOutside(double);
	void setBackDoor(const char*);
	void setFrontDoorMotion(const char*);
	string getHouseSummary();
	string getBackDoorStatus();
	string getFrontDoorStatus();
	string getGarageDoorStatus();
	string getHVACModeUpstairs();
	string getHVACModeDownstairs();
	void setGarageDoorStatus(const char*);
	double getTemperatureUpstairs();
	double getTemperatureDownstairs();
	double getTemperatureOutside();
	void setWindSpeed(double);
	double getWindSpeed();
	void setWeather(const char*);

	void setHumidityUpstairs(double);
	void setHumidityDownstairs(double);

	void setUpstairsHVACMode(const char*);
	void setDownstairsHVACMode(const char*);

	void setTemperatureUpstairs(double);
	void setTemperatureDownstairs(double);

	void save();
};

double Wally::getTemperatureOutside() {
	return (d["temperatureOutside"].GetDouble());
}
double Wally::getTemperatureUpstairs() {
	return (d["temperatureUpstairs"].GetDouble());
}
double Wally::getTemperatureDownstairs() {
	return (d["temperatureDownstairs"].GetDouble());
}

double Wally::getWindSpeed() {
	return (d["windSpeed"].GetDouble());
}

void Wally::setTemperatureOutside(double temp) {
	d["temperatureOutside"] = temp;
}
string Wally::getGarageDoorStatus() {
	string response(d["garageDoor"].GetString());
	return(response);
}
string Wally::getBackDoorStatus() {
	string response(d["backDoor"].GetString());
	return(response);
}
string Wally::getFrontDoorStatus() {
	string response(d["frontDoor"].GetString());
	return(response);
}
string Wally::getHVACModeUpstairs() {
	string response(d["upstairsHVACMode"].GetString());
	return(response);
}

string Wally::getHVACModeDownstairs() {
	string response(d["downstairsHVACMode"].GetString());
	return(response);
}

void Wally::setGarageDoorStatus(const char* status) {
	d["garageDoor"].SetString(status, d.GetAllocator());
}
void Wally::setTemperatureUpstairs(double temp) {
	d["temperatureUpstairs"] = temp;
}
void Wally::setTemperatureDownstairs(double temp) {
	d["temperatureDownstairs"] = temp;
}
void Wally::setWeather(const char* weather) {
	d["weather"].SetString(weather, d.GetAllocator());
}
void Wally::setWindSpeed(double ws) {
	if (DEBUG) cout << "setting wind speed" << endl;
	d["windSpeed"] = ws;
}
void Wally::writeStatusToFile() {
	ofstream myfile;
	StringBuffer sb;

	PrettyWriter<StringBuffer> writer(sb);
	d.Accept(writer);

	myfile.open("/home/pi/wally.json");
	myfile << sb.GetString();
	myfile.close();
	if (DEBUG) cout << "Wrote file" << endl;
}
string Wally::readWallyFile() {
	ifstream myfile;
	string output = "";
	myfile.open("/home/pi/wally.json");
	if (myfile.is_open())
	{
		string line;
		while (getline(myfile, line))
		{
			output += line;
		}
		myfile.close();
	}
	return(output);
}
void Wally::setBackDoor(const char* bd) {
	d["backDoor"].SetString(bd, d.GetAllocator());
}
void Wally::setFrontDoorMotion(const char* fd) {
	d["frontDoor"].SetString(fd, d.GetAllocator());
}
void Wally::setUpstairsHVACMode(const char* mode) {
	d["upstairsHVACMode"].SetString(mode, d.GetAllocator());
}
void Wally::setDownstairsHVACMode(const char* mode) {
	d["downstairsHVACMode"].SetString(mode, d.GetAllocator());
}
void Wally::setHumidityUpstairs(double hm) {
	d["humidityUpstairs"] = hm;
}
void Wally::setHumidityDownstairs(double hm) {
	d["humidityDownstairs"] = hm;
}
void Wally::save()
{
	writeStatusToFile();
}
string Wally::getHouseSummary()
{
	string response = "Here is Wallys house status report.  "
		"The outside temperature from dark sky is " + to_string((int)(d["temperatureOutside"].GetDouble() + .5)) + " degrees Fahrenheit.  "

		"The upstairs temperature is " + to_string((int)(d["temperatureUpstairs"].GetDouble()+.5)) +
		"  .  "

		"The upstairs humidity is " + to_string((int)(d["humidityUpstairs"].GetDouble()+.5)) + ".  "

		"The HVAC mode upstairs is set to " + d["upstairsHVACMode"].GetString() + ".  "

		"The downstairs temperature is " + to_string((int)(d["temperatureDownstairs"].GetDouble()+.5)) +
		" degrees Fahrenheit.  "

		"The downstairs humidity is " + to_string((int)(d["humidityDownstairs"].GetDouble()+.5)) + ". "

		"The HVAC mode downstairs is set to " + d["downstairsHVACMode"].GetString() + ".  " +

		d["backDoor"].GetString() +

		d["frontDoor"].GetString() +

		d["garageDoor"].GetString() +

		d["humidifier"].GetString() +

		d["sprinklers"].GetString() +

		"Here is the forecast: " + d["weather"].GetString();
	return(response);
}
Wally::Wally() {
	const char* json = "{\"windSpeed\":-100,"
		"\"humidityUpstairs\":-100,"
		"\"humidityDownstairs\":-100,"
		"\"temperatureUpstairs\":-100,"
		"\"temperatureDownstairs\":-100,"
		"\"temperatureOutside\":-100,"
		"\"upstairsHVACMode\":\"unknown\","
		"\"downstairsHVACMode\":\"unknown\","
		"\"backDoor\":\"The backdoor was never opened.  \","
		"\"frontDoor\":\"The front door camera has not detected motion.  \","
		"\"garageDoor\":\"The state of the garage door is unknown.  \","
		"\"humidifier\":\"The whole house humidifier never ran.  \","
		"\"sprinklers\":\"The sprinklers never have ran.  \","
		"\"away\":\"home\","
		"\"alarm\":\"set\","
		"\"weather\":\"The weather is not yet set.  \"}";

	string name = "/home/pi/wally.json";

	ifstream f(name.c_str());
	if (!f.good()) {
		d.Parse(json);
		writeStatusToFile();
	}
	else {
		string the_file = readWallyFile();

		if (d.Parse(the_file.c_str()).HasParseError()) {
			d.Parse(json);
			writeStatusToFile();
		}
	}
}
