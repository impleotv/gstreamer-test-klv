#include <stdlib.h>
#include <stdio.h>
#include <chrono>
#include <iostream>
#include <time.h>
#include <cstdlib>
#include <cstdint> 
#include "windows.h"
#include "io.h"

using namespace std::chrono;


const char* PathToLibrary = "../bin/MisbCoreNativeLib.dll";
const char* PathToLicenseFile = "D:\\GoogleDrive\\Doc\\Impleo\\Licenses\\Impleo\\MisbCoreNativeLegion.lic";
const char* LicenseKey = "359D6D27-1797FD38-4D9774E4-C0D8CF3A";


#define funcAddr GetProcAddress
#define checkAccess _access


#ifndef F_OK
#define F_OK    0
#endif

// #define MEASURE_EXECUTION_TIME

// Note, we don't provide a mandatory Tag 2 (timestamp). In this case, MisbCore will add a current time automatically.
// If you do need to provide a specific time, you can set it as long (as specified in MISB601 standard) -  "2": 1638730169255332, or as ISO string, for example  - "2": "2021-12-16T13:44:54",)
const char* jsonPckt = R"(
      {
      "3": "MISSION01",
	  "4": "AF-101",
	  "5": 159.974365,
	  "6": -0.431531724,
	  "7": 3.40586566,
	  "8": 147,
	  "9": 159,
	  "10": "MQ1-B",
	  "11": "EO",
	  "12": "WGS-84",
	  "13": 60.176822966978335,
	  "14": 128.42675904204452,
	  "15": 14190.7195,
	  "16": 144.571298,
	  "17": 152.643626,
	  "18": 160.71921143697557,
	  "19": -168.79232483394085,
	  "20": 176.86543764939194,
	  "21": 68590.983298744770,
	  "22": 722.819867,
	  "23": -10.542388633146132,
	  "24": 29.157890122923014,
	  "25": 3216.03723,
	  "26": 0.0136602540,
	  "27": 0.0036602540,
	  "28": -0.0036602540,
	  "29": 0.0136602540,
	  "30": -0.0110621778,
	  "31": -0.0051602540,
	  "32": 0.0010621778,
	  "33": -0.0121602540,
	  "34": 2,
	  "35": 235.924010,
	  "36": 69.8039216,
	  "37": 3725.18502,
	  "38": 14818.6770,
	  "39": 84,
	  "40": -79.163850051892850,
	  "41": 166.40081296041646,
	  "42": 18389.0471,
	  "43": 6,
	  "44": 30,
	  "45": 425.215152,
	  "46": 608.9231,
	  "47": 49,
	  "48": {
		"1": "UNCLASSIFIED//",
		"2": "ISO_3166_TwoLetter",
		"3": "//IL",
		"4": "Impleo test flight",
		"5": "Test data set",
		"6": "Releasing Instructions Test",
		"7": "Classified By Impleo",
		"8": "Derived From test",
		"9": "Classification Test",
		"10": "20201021",
		"11": "Marking System",
		"12": "ISO_3166_TwoLetter",
		"13": "IL",
		"14": "Comments",
		"22": 9,
		"23": "2020-10-21",
		"24": "2020-10-21"
	  },
	  "49": 1191.95850,
	  "50": -8.67030854,
	  "51": -61.8878750,
	  "52": -5.08255257,
	  "53": 2088.96010,
	  "54": 8306.80552,
	  "55": 50.5882353,
	  "56": 140,
	  "57": 3506979.0316063400,
	  "58": 6420.53864,
	  "59": "TOP GUN",
	  "60": 45016,
	  "61": 186,
	  "62": 1743,
	  "63": 2,
	  "64": 311.868162,
	  "65": 13,
	  "67": -86.041207348947040,
	  "68": 0.15552755452484243,
	  "69": 9.44533455,
	  "70": "APACHE",
	  "71": 32.6024262,
	  "72": "1995-04-16T13:44:54"
    })";

const int loop_count = 1000;

struct PcktBuffer
{
	char* buffer;
	int length;
};

typedef char* (*getNodeInfoFunc)();
typedef bool (*activateFunc)(char*, char*);
typedef PcktBuffer(*encodeFunc)(char*);
typedef char* (*decodeFunc)(char*, int len);
typedef char* (*toDetailedFunc)(char*);
typedef void (*cleanUpFunc)();

//  In case you need multiple encoder/decoder instances	
// typedef PcktBuffer (*instanceEncodeFunc)(int id, char*);
// typedef char* (*instanceDecodeFunc)(int id, char*, int len);
// typedef void (*instanceCleanUpFunc)(int id);

int main()
{
	// Check if the library file exists
	if (checkAccess((char*)PathToLibrary, F_OK) == -1)
	{
		puts("Couldn't find library at the specified path");
		return 0;
	}

	// Check if the license file exists
	if (checkAccess((char*)PathToLicenseFile, F_OK) == -1)
		puts("Couldn't find the license file. Will work in demo mode");

	HINSTANCE handle = LoadLibraryA((char*)PathToLibrary);

	if ((intptr_t)handle == 0)
		return 0;

	getNodeInfoFunc GetNodeInfo = (getNodeInfoFunc)funcAddr(handle, (char*)"GetNodeInfo");
	char* nodeInfo = GetNodeInfo();
	printf("The NodeInfo: %s \n", nodeInfo);

	activateFunc Activate = (activateFunc)funcAddr(handle, (char*)"Activate");
	bool fValid = Activate((char*)PathToLicenseFile, (char*)LicenseKey);

	encodeFunc encode601Pckt = (encodeFunc)funcAddr(handle, (char*)"Encode");
	decodeFunc decodeKlvBuffer = (decodeFunc)funcAddr(handle, (char*)"Decode");
	toDetailedFunc toDetailed = (toDetailedFunc)funcAddr(handle, (char*)"ToDetailed");

	//  In case you need multiple encoder/decoder instances	
	//	instanceEncodeFunc encode601Pckt = (instanceEncodeFunc)funcAddr(handle, (char*)"InstanceEncode");
	//	instanceDecodeFunc decodeKlvBuffer = (instanceDecodeFunc)funcAddr(handle, (char*)"InstanceDecode");


#ifdef MEASURE_EXECUTION_TIME
	auto start = high_resolution_clock::now();
#endif

	for (int i = 0; i < loop_count; i++)
	{

		// First, encode the klv buffer from jsonPckt
		PcktBuffer pcktBuf = encode601Pckt((char*)(jsonPckt));
		// In case you need multiple encoder/decoder instances, use this method instead	
		// PcktBuffer pcktBuf = encode601Pckt(1, (char*)(jsonPckt)); 

#ifndef MEASURE_EXECUTION_TIME
		printf("The buff len is %d \n", pcktBuf.length);
#endif

		// Now decode the buffer back to json. 
		char* json = decodeKlvBuffer((char*)(pcktBuf.buffer), pcktBuf.length);
		// In case you need multiple encoder/decoder instances, use this method instead	
		// char* json = decodeKlvBuffer(1, (char*)(pcktBuf.buffer), pcktBuf.length);  

#ifndef MEASURE_EXECUTION_TIME
		printf("Decoded packet:\n %s \n", json);
#endif
	}

#ifdef MEASURE_EXECUTION_TIME
	auto stop = high_resolution_clock::now();
	auto duration = duration_cast<std::chrono::milliseconds>(stop - start);
	printf("Execution time is %I64d ms. (%f) ms per packet\n", duration.count(), ((double)duration.count() / (double)loop_count));
#endif

	// Demonstrate how to convert a compact json format to human readable, detailed json. This may be used for presentation
	char* detailedJsonStr = toDetailed((char*)jsonPckt);
	printf("\n Demonstrate how to convert a compact json format to human readable, detailed json: \n %s", detailedJsonStr);


	cleanUpFunc cleanUp = (cleanUpFunc)funcAddr(handle, (char*)"CleanUp");
	cleanUp();
}
