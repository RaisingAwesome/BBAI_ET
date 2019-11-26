//Compile command line:
//    g++ -c -o SSLClient.o wally.cpp
//    g++ -o c SSLClient.o -lssl -lcrypto -

#define DEBUG false			//set to true to receive console output to help troubleshoot

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
#include <time.h>
#include <thread>

#include <sys/stat.h>
#include <fcntl.h>

#include "include/wally.h"
using namespace std;
using namespace rapidjson;

//key definitions that must be changed for one's personal use of this code

#define MONITORING_PORT "5000"
#define WEATHER_KEY "4ba6263cd7a889f74f11d981612dcb22"
#define LOCATION_COORDINATES "38.7732886,-89.9786466"
//end of key definitions

//Function Protocols
void BootUp();
void error(const char *);
int getCurrentHour();
string GetHouseStatus();
string GetBackdoorStatus();
void SetWallyWeather();

Document GetWeatherJSON();
Document GetJSON(string);
void HandleCustom(string, int);
bool LastByteReceived(string);
void ListenForSocketConnection();
string ReadSocket(int);
void UpdateEnvironmentalAwareness();
void saveTemperature();
void saveForecast();
string GetPacket(const char *, const char *, int);
int SendPacket(const char *, SSL *);
bool is_number(const std::string&);
//End Function Protocols

Wally wally;
string forecast="";
int main(int argc, char *argv[]) {	
	if (DEBUG) cout << "Starting..." << endl;
	
	BootUp();
	SetWallyWeather() ;
    time_t timer=time(NULL)+1;

	std::cout<<forecast<<std::endl;
	std::cout<<wally.getTemperatureOutside()<<std::endl;
	saveTemperature();
	saveForecast();

	sleep(0);

	return(0);  //will never get here actually
}

void saveTemperature() {
	char buffer[20];
	int ret = snprintf(buffer, sizeof buffer, "%f", wally.getTemperatureOutside());
	if (DEBUG) printf("About to open for writing...\n");
	int fp = open("/home/debian/ramdisk/bbaibackupcam_temperature", O_WRONLY|O_CREAT,0666);
	if (DEBUG) printf("About to write...%d\n",fp);
	ret=write(fp, buffer, sizeof(buffer));
	if (DEBUG) printf("Written %d\n",ret);
	close(fp);
}

void saveForecast() {
	if (DEBUG) printf("About to open for writing...\n");
	std::ofstream out("/home/debian/ramdisk/bbaibackupcam_forecast");
	
	out << forecast;
    out.close();
	
}
void BootUp() {
	UpdateEnvironmentalAwareness();
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
	my_weather += sprinkler;
	wally.setWeather(my_weather.c_str());
	
	wally.setWindSpeed(weather["currently"]["windGust"].GetDouble());
	if (DEBUG) cout << my_weather << endl;
	wally.setTemperatureOutside(weather["currently"]["temperature"].GetDouble());
	forecast=(weather["hourly"]["summary"].GetString());
	if (DEBUG) cout << "got tempoutside" << wally.getTemperatureOutside() << endl;
}
void UpdateEnvironmentalAwareness() {
	if (DEBUG) cout << "Updating environmental awareness" << endl;
	SetWallyWeather();
	
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
