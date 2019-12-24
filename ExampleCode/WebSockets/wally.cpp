//Compile command line:
//    g++ -c -o SSLClient.o wally.cpp
//    g++ -o c SSLClient.o -lssl -lcrypto -

#define DEBUG true			//set to true to receive console output to help troubleshoot

#include <openssl/ssl.h>				//get this package using this command line: sudo apt-get install libssl-dev
#include "include/rapidjson/document.h"	//Get these two includes at the following link and place with
#include "include/rapidjson/error/en.h"	//at the main code location: https://github.com/Tencent/rapidjson/tree/master/include/rapidjson

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <locale>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/err.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>
#include <fstream>
#include <ctime>
#include "include/wally.h"
using namespace std;
using namespace rapidjson;

//key definitions that must be changed for one's personal use of this code

#define MONITORING_PORT "5000"
#define ALEXA_SKILL_ID "amzn1.ask.skill.42c087a6-ac2a-41c0-9ddb-c65104fee8ba"
#define NEST_URL "https://firebase-apiserver17-tah01-iad01.dapi.production.nest.com"
#define NEST_IP "52.4.203.41"
#define NEST_PORT 9553
#define NEST_AUTHORIZATION_TOKEN "c.0yp2ihOmsSynxCJDV1hc02yG2mnTkOvpi6PlsjywaGbrosiypzXY5CBEo4bNmTF6ZnnPj1KTbKugYqJ4CAsSf03P3KWgmL3ahaKiZPPGU4PBZEWGSSSSfRm6TtfTKVSvGTA7mbvt7g5UgKuW"
#define NEST_UPSTAIRS_THERMOSTAT_KEY "Po0QJ0k3KeInLLL505Rl0rfLweLZHUJf"
#define NEST_FAMILY_ROOM_THERMOSTAT_KEY "Po0QJ0k3KeLErmowXCIxMbfLweLZHUJf"
#define NEST_STRUCTURE_KEY "gp4LsLoCx-C8yB_rn6skTKMP92Cxio_rw1CSNi1Tc3l3lmXa2rku9w"
#define WEATHER_KEY "4ba6263cd7a889f74f11d981612dcb22"
#define LOCATION_COORDINATES "38.7732886,-89.9786466"
#define IFTTT_KEY "-oGdgnjVyMGITYPtqYJm3"
//end of key definitions

//Function Protocols
string AlexaResponseJSON(string, string);
void BootUp();
void error(const char *);
int getCurrentHour();
string GetHouseStatus();
string GetBackdoorStatus();
void SetWallyWeather();
void SetWallyNest();
Document GetWeatherJSON();
Document GetJSON(string);
void HandleAlexa(string, int);
void HandleCustom(string, int);
bool LastByteReceived(string);
void ListenForSocketConnection();
string ReadSocket(int);
void UpdateEnvironmentalAwareness();
string GetPacket(const char *, const char *, int);
int SendPacket(const char *, SSL *);
void SetNestThermostat(string, string);
bool is_number(const std::string&);
//End Function Protocols

Wally wally;
int main(int argc, char *argv[]) {	
	if (DEBUG) cout << "Starting..." << endl;
	
	BootUp();
	ListenForSocketConnection();
	return(0);  //will never get here actually
}
void BootUp() {
	UpdateEnvironmentalAwareness();
}
void ListenForSocketConnection() {
	int sockfd, newsockfd, portno, pid;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	portno = atoi(MONITORING_PORT);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0)
		error("ERROR opening socket");

	bzero((char *)&serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
		sizeof(serv_addr)) < 0)
		error("ERROR on binding");

	listen(sockfd, 5);

	clilen = sizeof(cli_addr);
	if (DEBUG) cout<<"About to monitor port"<<endl;
	while (1) {
		newsockfd = accept(sockfd,
			(struct sockaddr *) &cli_addr, &clilen);
		if (DEBUG) cout<<"Got a hit"<<endl;
		if (newsockfd < 0)
			error("ERROR on accept");
		pid = fork();
		if (pid < 0)
			error("ERROR on fork");
		if (pid == 0) {
			close(sockfd);

			string request = ReadSocket(newsockfd);
			//cout << request << endl;
			if (request.find(ALEXA_SKILL_ID) != string::npos) {
				if (DEBUG) cout << endl << endl << "About to handle alexa" << endl << endl;
				HandleAlexa(request, newsockfd);
			}
			else
				HandleCustom(request, newsockfd);
			system("sudo systemctl restart pagekite");
			exit(0);
		}
		
		else {
			close(newsockfd);
			wait(&pid);
		}
	} /* end of while */
	close(sockfd);
}
string ReadSocket(int sock) {
	int n; 
	std::clock_t start;
	string all_bytes_received;
	char buffer[1000];
	start = std::clock();
	while (((std::clock() - start) / (double)CLOCKS_PER_SEC)<10) { //this is to prevent a hang if lastbytereceived doesn't ever return true.  It is a 10 second timeout.
		bzero(buffer, 1000);
		n = read(sock, buffer, 999);
		if (n < 0)
			error("ERROR reading from socket");
		if (DEBUG) cout << "reading in" << endl;
		std::string str(buffer);
		if (DEBUG) cout << "read buffer" << endl;
		all_bytes_received += str;
		if (DEBUG) cout << "added string" << endl;
		if (LastByteReceived(all_bytes_received)) break;
	}
	if (DEBUG) cout << "Readsocket finished" << endl;
	return(all_bytes_received);
}
string GetHouseStatus() {
	return (wally.getHouseSummary());
}
string GetBackDoorStatus() {
	return (wally.getBackDoorStatus());
}
void HandleAlexa(string str, int sock) {
	string request = str.substr(str.find("\r\n\r\n") + 4);
	if (DEBUG) cout << endl << endl << "Entered HandleAlexa and about to GETJSON" << endl;
	Document AlexaJSON = GetJSON(request);
	if (DEBUG) cout << "Getting intent" << endl;
	string intent = AlexaJSON["request"]["intent"]["name"].GetString();
	if (DEBUG) cout << intent << endl;
	UpdateEnvironmentalAwareness();
	str = AlexaResponseJSON("House Status", GetHouseStatus());

	if (intent.compare("CheckStatus") == 0) {
		UpdateEnvironmentalAwareness();
		//str = AlexaResponseJSON("House Status", GetHouseStatus());
		write(sock, str.c_str(), str.length());
	}
	else if (intent.compare("CheckOnWally") == 0) {
		UpdateEnvironmentalAwareness();
		str = AlexaResponseJSON("Wally Status", "Wally said he is doing great and thank you for asking!  He also just updated his environmental awareness variables.");
		write(sock, str.c_str(), str.length());
	}
	else if (intent.compare("DoorStatus") == 0) {
		str = AlexaResponseJSON("Door Status", GetBackDoorStatus());
		write(sock, str.c_str(), str.length());
	}
	else if (intent.compare("GarageDoor") == 0) {
		string response = wally.getGarageDoorStatus();
		response += "  I'll trigger it now.";
		str = AlexaResponseJSON("Garage Door", response.c_str());
		write(sock, str.c_str(), str.length());
	}
	else if (intent.compare("Shutdown") == 0) {
		str = AlexaResponseJSON("Shutting Down", "Okay, Wally is shutting down.  Wait for the little green light to stop blinking.");
		write(sock, str.c_str(), str.length());
	}
	else if (intent.compare("StartFans") == 0) {
		string response = "Wally has turned on the fans on both floors.  You'll hear it kick on in a few seconds.  They will stay on for two hours.";
		str = AlexaResponseJSON("Raise Temp", response.c_str());
		write(sock, str.c_str(), str.length());

		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"fan_timer_active\": true}");
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"fan_timer_active\": true}");

		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"fan_timer_duration\": 120}");
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"fan_timer_duration\": 120}");

	}
	else if (intent.compare("TempUp")==0) {
		int dt=(int)wally.getTemperatureDownstairs();
		int ut = (int)wally.getTemperatureUpstairs();
		int ot = (int)wally.getTemperatureOutside();

		string the_temp;
		string response;
		string hvac_mode = wally.getHVACModeUpstairs();
		if (dt <= ut) {
			response = "The downstairs is the colder of the two.  It is currently " + to_string(dt) + " degrees.  He raised it to " + to_string(dt + 2) + " and set the upstairs the same.";
			dt += 2;
			the_temp = to_string(dt);
			if (ot < dt) { response += "  Since its colder outside than inside, he made sure the HVAC mode is set to heat. "; }
		}
		else {			
			response = "The upstairs is the colder of the two.  It is currently " + to_string(ut) + " degrees.  He raised it to " + to_string(ut+2) +" and set the downstairs the same.";
			ut += 2;
			the_temp = to_string(ut);
			if (ot < ut) { response += "  Since its colder outside than inside, he made sure the HVAC mode is set to heat. "; }
		}		

		str = AlexaResponseJSON("Raise Temp", response.c_str());
		write(sock, str.c_str(), str.length());

		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"target_temperature_f\": " + the_temp + "}");
		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"hvac_mode\": \"" + hvac_mode + "\"}");

		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"target_temperature_f\": " + the_temp + "}");
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"hvac_mode\": \"" + hvac_mode + "\"}");
	}
	else if (intent.compare("TempDown") == 0) {
		int dt = (int)wally.getTemperatureDownstairs();
		int ut = (int)wally.getTemperatureUpstairs();
		int ot = (int)wally.getTemperatureOutside();

		string the_temp;
		string response;
		string hvac_mode = wally.getHVACModeUpstairs();
		if (dt >= ut) {			
			response = "The downstairs is the warmer of the two.  It is currently " + to_string(dt) + " degrees.  He lowered it to " + to_string(dt - 2) + " and set the upstairs the same.";
			dt -= 2;
			the_temp = to_string(dt);
			if (ot>dt) { 
				response += "  Since its warmer outside than inside, he made sure the HVAC mode is set to cool. ";
				hvac_mode = "heat";
			}
		}
		else {			
			response = "The upstairs is the warmer of the two.  It is currently " + to_string(ut) + " degrees.  He lowered it to " + to_string(ut + 2) + " and set the downstairs the same.";
			ut -= 2;
			the_temp = to_string(ut);
			if (ot>ut) { 
				response += "  Since its warmer outside than inside, he made sure the HVAC mode is set to cool. ";
			}
		}

		str = AlexaResponseJSON("Lower Temp", response.c_str());
		write(sock, str.c_str(), str.length());

		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"target_temperature_f\": " + the_temp + "}");
		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"hvac_mode\": \"" + hvac_mode + "\"}");

		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"target_temperature_f\": " + the_temp + "}");
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"hvac_mode\": \"" + hvac_mode + "\"}");
	}
	else if (intent.compare("Heat") == 0) {
		double the_temp;
		if (wally.getTemperatureUpstairs() > wally.getTemperatureDownstairs()) the_temp = wally.getTemperatureUpstairs(); else the_temp = wally.getTemperatureDownstairs();

		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"hvac_mode\": \"heat\"}");
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"target_temperature_f\": " + to_string(the_temp) + "}");
		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"hvac_mode\": \"heat\"}");
		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"target_temperature_f\": " + to_string(the_temp) + "}");
		string response = "Wally has set the mode to heat at " + (to_string((int)the_temp)) + " degrees.";
		str = AlexaResponseJSON("Heat", response.c_str());
		write(sock, str.c_str(), str.length());

	}
	else if (intent.compare("Cool") == 0) {
		double the_temp;
		if (wally.getTemperatureUpstairs() < wally.getTemperatureDownstairs()) the_temp = wally.getTemperatureUpstairs(); else the_temp = wally.getTemperatureDownstairs();

		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"hvac_mode\": \"cool\"}");
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"target_temperature_f\": " + to_string(the_temp) + "}");
		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"hvac_mode\": \"cool\"}");
		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"target_temperature_f\": " + to_string(the_temp) + "}");
		string response = "Wally has set the mode to cool at " + (to_string((int)the_temp)) + " degrees.";
		str = AlexaResponseJSON("Heat", response.c_str());
		write(sock, str.c_str(), str.length());
	}
	else if (intent.compare("HouseTemperature") == 0) {

		int ot=(int)wally.getTemperatureOutside();
		string hvac_mode="heat";
		if (DEBUG) cout << endl<<endl<< "Alexa House Temperature Intent Receved" << endl<<endl;
		int the_hour=getCurrentHour();
		if (the_hour < 7||the_hour>=21) {
			if (ot > 67) hvac_mode = "cool";
		}
		else if (the_hour >= 7 && the_hour<21) {
			if (ot > 63) hvac_mode = "cool";
		}

		string the_temp = AlexaJSON["request"]["intent"]["slots"]["temperature"]["value"].GetString();
		int ut = (int)wally.getTemperatureUpstairs();
		if (!is_number(the_temp)) the_temp = "70";
		int temp_delta = ut - atoi(the_temp.c_str());

		if (temp_delta > 1) {
			hvac_mode = "cool";
		}
		else if (temp_delta < -1) hvac_mode = "heat";
		string big_change = "";
		if (temp_delta > 5 || temp_delta < -5) big_change = "  This is somewhat of a big change.  The Nest may not have accepted it.";
		
		string response = "The house temperature upstairs is currently " + to_string(ut) + " and the outside temp is " + to_string(ot) + ".  Based on the time of day and conditions, Wally, who is better than me, set the HVAC mode to " + hvac_mode + " to your requested " + (the_temp)+" degrees." + big_change;
		str = AlexaResponseJSON("Set Temp", response.c_str());
		write(sock, str.c_str(), str.length());

		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"hvac_mode\": \"" + hvac_mode + "\"}");
		sleep(2);
		SetNestThermostat(NEST_UPSTAIRS_THERMOSTAT_KEY, "{\"target_temperature_f\": " + the_temp + "}");
		sleep(2);
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"hvac_mode\": \"" + hvac_mode + "\"}");
		sleep(2);
		SetNestThermostat(NEST_FAMILY_ROOM_THERMOSTAT_KEY, "{\"target_temperature_f\": " + the_temp + "}");
	}
	else if (intent.compare("SetHouseAlarm") == 0) {
		str = AlexaResponseJSON("Set Alarm", "The house alarm is now set.");
		write(sock, str.c_str(), str.length());
	}
	else if (intent.compare("DisableHouseAlarm") == 0) {
		str = AlexaResponseJSON("Disable Alarm", "The house alarm is now disabled.");
		write(sock, str.c_str(), str.length());
	}
	else write(sock, str.c_str(), str.length());
	
}
bool is_number(const std::string& s)
{
	std::string::const_iterator it = s.begin();
	while (it != s.end() && std::isdigit(*it)) ++it;
	return !s.empty() && it == s.end();
}
int getCurrentHour() {
	time_t now = time(0);
	string the_date(ctime(&now));
	string the_time(ctime(&now));
	int the_hour = atoi((the_time.substr(11, 2)).c_str());
	return the_hour;	
}

Document GetJSON(string str) {
	Document d;
	if (DEBUG) cout << endl << endl << str << endl;
	ParseResult result = d.Parse(str.c_str());
	if (DEBUG) cout << "parsed the result" << endl;
	if (!result)
		error("JSON parse error");
	if (DEBUG) cout << "Processed GetJSON" << endl;
	return(d);
}
void HandleCustom(string request, int sock) {
	string response = request.substr(request.find("\r\n\r\n") + 4);
	if (response.find("backdoor") != string::npos) {
		string str = "espeak \"The backoor was opened.\"";
		system(str.c_str());
		time_t now = time(0);
		string the_date(ctime(&now));
		the_date = the_date.substr(0, 10);
		string the_time( ctime(&now));
		the_time = the_time.substr(11,5);
		str = "The back door was last opened on " + the_date + " at " + the_time + ".  ";
		wally.setBackDoor(str.c_str());
		wally.save();
	}
	if (response.find("motion") != string::npos) {
		string str = "espeak \"Motion was detected at the front door.\"";
		system(str.c_str());
		time_t now = time(0);
		string the_date(ctime(&now));
		the_date = the_date.substr(0, 10);
		string the_time(ctime(&now));
		the_time = the_time.substr(11, 5);
		str = "Motion was detected at the front door on " + the_date + " at " + the_time + ".  ";
		wally.setFrontDoorMotion(str.c_str());
		wally.save();
	}

	if (response.find("garagedoor") != string::npos) {
		cout<<"Hit!!!!Garage Door"<<endl;
	}
}
string AlexaResponseJSON(string title, string response)
{
	string body = "{\r\n"
		"  \"version\": \"1.0\",\r\n"
		"  \"response\" : {\r\n"
		"    \"outputSpeech\": {\r\n"
		"      \"type\": \"PlainText\",\r\n"
		"      \"text\" : \"" + response + "\",\r\n"
		"      \"playBehavior\" : \"REPLACE_ENQUEUED\"\r\n"
		"    }, \r\n"
		"    \"card\" : {\r\n"
		"      \"type\": \"Standard\",\r\n"
		"      \"title\" : \"" + title + "\",\r\n"
		"      \"text\" : \"" + response + "\"\r\n"
		"    }\r\n"
		"  }, \r\n"
		"    \"shouldEndSession\": true\r\n"
		"  }\r\n"
		"}\r\n";

	string header = "HTTP/1.1 200 OK\r\n"
		"Content-Type: application/json; charset = UTF-8\r\n"
		"Content-Length:" + to_string(body.length()) + "\r\n\r\n";
	if (DEBUG) cout << endl << endl << endl << header + body << endl << endl;
	return (header + body);
}
bool LastByteReceived(string str)
{   //Check for the content-length in the header once it is received.  Then, start counting bytes to see if we got them all.
	static int content_length = 0;
	if (DEBUG) cout << "Entering LastByteREceived" << endl;
	if (content_length == 0) {
		if (DEBUG) cout << "Checking for Content-length" << endl;
		if (str.find("Content-Length: ") != string::npos) {
			string temp = str.substr(str.find("Content-Length: ") + 16);
			if (temp.find("\r\n")) {
				if (DEBUG) cout << "Setting Content-length" << endl;
				content_length = atoi((temp.substr(0, temp.find("\r\n")).c_str()));
			}
			else
				return(false);
		}
		else
			return(false);
	}

	if (content_length > 0)
	{
		if (str.find("\r\n\r\n") != string::npos) {
			string temp = str.substr(str.find("\r\n\r\n"));
			if (DEBUG) cout << "Checking if content length has been reached" << endl;
			if (temp.length() - 4 >= content_length) {
				if (DEBUG) cout << "Content length reached" << endl;
				//imagine I need to clear this for future first
				//calls to LastByteReceived, but maybe not because 
				//we exit in the main after handling the command.
				content_length = 0;
				return(true);
			}
			else
				return (false);
		}
	}
	else
		return (false);
	if (DEBUG) cout << "Exiting LastByteREceived" << endl;
}
void SetWallyWeather() {
	Document weather = GetWeatherJSON();
	if (DEBUG) cout << "About to set hourly summary" << endl;
	string my_weather(weather["hourly"]["summary"].GetString());
	
	string sprinkler = "  Since it is going to rain, I've turned off the sprinklers.  ";
	string new_weather; locale loc;
	for (std::string::size_type i = 0; i<my_weather.length(); ++i)
		new_weather+=std::toupper(my_weather[i], loc);
	if (DEBUG) cout << new_weather << endl;
	if (new_weather.find("RAIN") == string::npos) {
		sprinkler = "  Since there is no rain in the forecast, I've turned on the sprinklers.  ";
	}
	//my_weather += sprinkler;
	wally.setWeather(my_weather.c_str());
	
	wally.setWindSpeed(weather["currently"]["windGust"].GetDouble());
	if (DEBUG) cout << my_weather << endl;
	wally.setTemperatureOutside(weather["currently"]["temperature"].GetDouble());
	if (DEBUG) cout << "got tempoutside" << wally.getTemperatureOutside() << endl;
}
void SetWallyNest(){
	string request = "GET /" 
		" HTTP/1.1\r\nHost: "
		NEST_URL
		"\r\n"
		"Content-Type: application/json\r\n"
		"Authorization: Bearer "
		NEST_AUTHORIZATION_TOKEN
		"\r\nConnection: keep - alive"
		"\r\n\r\n";
	if (DEBUG) cout << request << endl;
	string str = GetPacket(request.c_str(), NEST_IP, (NEST_PORT));
	str = str.substr(str.find("\r\n\r\n") + 4);
	Document d = GetJSON(str);
	if (DEBUG) cout << "Got weather JSON and about to return" << endl;
	if (DEBUG) cout << endl << endl << "Thermostat:" << d["devices"]["thermostats"][NEST_UPSTAIRS_THERMOSTAT_KEY]["hvac_mode"].GetString() << endl;
	wally.setDownstairsHVACMode(d["devices"]["thermostats"][NEST_FAMILY_ROOM_THERMOSTAT_KEY]["hvac_mode"].GetString());
	wally.setUpstairsHVACMode(d["devices"]["thermostats"][NEST_UPSTAIRS_THERMOSTAT_KEY]["hvac_mode"].GetString());
	wally.setHumidityDownstairs(d["devices"]["thermostats"][NEST_FAMILY_ROOM_THERMOSTAT_KEY]["humidity"].GetDouble());
	wally.setHumidityUpstairs(d["devices"]["thermostats"][NEST_UPSTAIRS_THERMOSTAT_KEY]["humidity"].GetDouble());	
	wally.setTemperatureDownstairs(d["devices"]["thermostats"][NEST_FAMILY_ROOM_THERMOSTAT_KEY]["ambient_temperature_f"].GetDouble());
	wally.setTemperatureUpstairs(d["devices"]["thermostats"][NEST_UPSTAIRS_THERMOSTAT_KEY]["ambient_temperature_f"].GetDouble());
}
void SetNestThermostat(string thermostat, string body) {
	
	string request =
		"PUT /devices/thermostats/" + thermostat + " HTTP/1.1\r\n"
		"Host: " NEST_URL ":" + to_string(NEST_PORT) + "\r\n"
		"Content-Type: application/json\r\n"
		"Authorization: Bearer " NEST_AUTHORIZATION_TOKEN "\r\n"
		"Content-Length: " + to_string(body.length()) + "\r\n"
		"Connection: keep - alive\r\n\r\n" + body;

	string str = GetPacket(request.c_str(), NEST_IP , NEST_PORT);
	//cout << str << endl;
}
void UpdateEnvironmentalAwareness() {
	if (DEBUG) cout << "Updating environmental awareness" << endl;
	SetWallyWeather();
	//SetWallyNest();
	wally.save();
}
void error(const char *msg) {
	perror(msg);
	exit(0);
}
string RecvPacket(SSL *ssl)
{	int n;
	string all_bytes_received;
	std::clock_t start;
	char buffer[1000];
	int ii = 0; int abort_counter = 0;
	start = std::clock();
	while (((std::clock() - start) / (double)CLOCKS_PER_SEC)<10) //this prevents a hang if LastByteReceived never returns true.  It is a 10 second timeout.
	{
		if (ii++ > 40) break;
		bzero(buffer, 1000);
		n = SSL_read(ssl, buffer, 999);
		if (n < 0) {
			int err = SSL_get_error(ssl, n);
			if (err == SSL_ERROR_WANT_READ)
				exit(0);
			if (err == SSL_ERROR_WANT_WRITE)
				exit(0);
			if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL || err == SSL_ERROR_SSL)
				exit(0);
		}
		if (DEBUG) cout << "reading in" << endl;
		std::string str(buffer);
		if (DEBUG) cout << "read buffer" << endl;
		all_bytes_received += str;
		if (DEBUG) cout << "added string" << endl;
		//if (DEBUG) cout << all_bytes_received << endl;
		if (LastByteReceived(all_bytes_received)) break;
	}
	if (DEBUG) cout << "finished recvbytes "<< ii << endl;
	return(all_bytes_received);
}
int SendPacket(const char *buf, SSL *ssl)
{
	int len = SSL_write(ssl, buf, strlen(buf));
	if (len < 0) {
		int err = SSL_get_error(ssl, len);
		switch (err) {
		case SSL_ERROR_WANT_WRITE:
			return 0;
		case SSL_ERROR_WANT_READ:
			return 0;
		case SSL_ERROR_ZERO_RETURN:
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_SSL:
		default:
			return -1;
		}
	}
}
void log_ssl()
{
	int err;
	while (err = ERR_get_error()) {
		char *str = ERR_error_string(err, 0);
		if (!str)
			return;
		printf(str);
		printf("\n");
		fflush(stdout);
	}
}
string GetPacket(const char *the_request, const char *the_address, int the_port)
{
	SSL *ssl;
	int s, sock;
	s = socket(AF_INET, SOCK_STREAM, 0);
	if (!s) {
		printf("Error creating socket.\n");
		return "-1";
	}
	struct sockaddr_in sa;
	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = inet_addr(the_address);
	sa.sin_port = htons(the_port);
	socklen_t socklen = sizeof(sa);
	if (connect(s, (struct sockaddr *)&sa, socklen)) {
		printf("Error connecting to server.\n");
		return "-1";
	}
	SSL_library_init();
	SSLeay_add_ssl_algorithms();
	SSL_load_error_strings();
	const SSL_METHOD *meth = TLSv1_2_client_method();
	SSL_CTX *ctx = SSL_CTX_new(meth);
	ssl = SSL_new(ctx);
	if (!ssl) {
		printf("Error creating SSL.\n");
		log_ssl();
		return "-1";
	}
	sock = SSL_get_fd(ssl);
	SSL_set_fd(ssl, s);
	int err = SSL_connect(ssl);
	if (err <= 0) {
		printf("Error creating SSL connection.  err=%x\n", err);
		log_ssl();
		fflush(stdout);
		return "-1";
	}
	//printf ("SSL connection using %s\n", SSL_get_cipher (ssl));
	if (DEBUG) cout << "Sending request." << endl << endl;
	SendPacket(the_request, ssl);
	return RecvPacket(ssl);
}
Document GetWeatherJSON()
{   
	string request = "GET /forecast/" 
		WEATHER_KEY
		"/" 
		LOCATION_COORDINATES
		" HTTP/1.1\r\nHost: api.darksky.net\r\nConnection: keep-alive\r\nCache-Control: no-cache\r\n\r\n";
	if (DEBUG) cout << request << endl;
	string str = GetPacket(request.c_str(), "52.3.137.27", 443);
	str = str.substr(str.find("\r\n\r\n")+4);
	Document d = GetJSON(str);
	if (DEBUG) cout << "Got weather JSON and about to return" << endl;
	return (d);
}

