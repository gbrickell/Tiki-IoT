// standalone 'C' program template (IoT_Ctest_template_240807.c) to just demonstrate/test 
// some of Tiki access functions provided in control_iot_240807.c 

// this template provides example code, but needs to be configured for the specific use

// originally developed by Geoff Brickell in September 2023
// using many of the examples from https://curl.se/libcurl/c/libcurl.html

// compiled using gcc on a local 'integrating' hub device using the command:
// gcc -o /your_path_to_compiled_result/Ctest_IoT_240807.exe /your_path_to_this_file/IoT_Ctest_template_240807.c /your_path_to/control_iot_240807.c -I/usr/local/include -L/usr/local/lib -lcurl

// run using the command: /your_path_to/Ctest_IoT_240807.exe

#define _XOPEN_SOURCE 700
#define _GNU_SOURCE /* for tm_gmtoff and tm_zone */
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>   // allows the use of bool, true and false which are otherwise not available in C
#include <string.h>
#include <curl/curl.h>
#include "control_iot_240807.h"

// declaration of functions being used to avoid gcc warnings
long int findSize(const char* file_name);
char* copyString(char s[]);
void removeString (char text[], int index, int rm_length);
void connect_iot();
char* webpage_download(int debug, const char* domain, const char* page, char* access_token);
_Bool webpage_check(int debug, const char* domain, const char* page, char* access_token, const char* check_text);
char* webpage_datetimecheck(int debug, const char* domain, const char* page, char* access_token, const char* infront_text, int datelen, const char* ref_datetime, const char* datetime_fmt);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);
char* tracker_item_post(int debug, const char* domain, const char* page, const char* access_token, const char* post_data);
char* tracker_itemupdate(int debug, const char* domain, char* access_token, const char* trackerId, const char* itemId, const char* post_data);
char* tracker_itemget(int debug, const char* domain, char* access_token, const char* trackerId, const char* itemId);
char* gallery_filedownload(int debug, const char* domain, char* access_token, const char* fileId, const char* filespath, const char* bodyfilename, const char* headerfilename);
char* gallery_fileupload(int debug, const char* domain, char* access_token, const char* filepath, const char* galId, const char* filename, const char* filetitle, const char* filedesc);
char* gallery_fileupdate(int debug, const char* domain, char* access_token, const char* fileId, const char* filepath, const char* filename, const char* filetitle, const char* filedesc);


// ******************************
// *****    main code      ******
// ******************************

int main(){

    connect_iot();  // which display the version of the C functions being used
    printf("Hello: now running various function tests\n\n");
    int debug = 1;

    char strtime[100] = "";
    int numtime = time(NULL);
    sprintf(strtime, "%d", numtime);

    // ************  define the various strings, parameters, etc  *************************
    const char* domain = "https://example_domain.com";   // must include https:// but no trailing /
    // header token data - must include 'Authorization: Bearer' ahead of the token text as shown below
    char* access_token = "Authorization: Bearer your_unique_security_access_token";

    // data for new tracker item post
    const char* trackerId = "1";   // update to your tracker#

    // define tracker item post data with individual field data using their Tiki permanent field names
    // this is just an example where the field names (e.g. IoTtestDeviceName) are all individually given values
    char post_data[500] = "fields[IoTtestDeviceName]=pitikiP48GB&fields[IoTtestUser]=pikitP48GB&fields[IoTtestTextData]=just some text&fields[IoTtestNumericalData]=123.456&fields[IoTtestTextAreaData]=text data line 1\r\ntext data line 2\r\n&fields[IoTtestImage]=2243&fields[IoTtestDescription]=data input using 100% C code";


    // web page data for date check
    const char* page1 = "your%20example%20page";  //must include the leading / and spaces 'filled' with %20 NOT + 
    const char* infront_text = "marker-text";
    const char* ref_datetime = "Tue 28 Dec 2021 12:25:00 GMT"; // update to your reference date-time string
	                                                           // must use the same format as is used on the web page ie as below

    const char* datetime_fmt = "%a %d %b %Y %H:%M:%S %Z";      // update to the format used on the Tiki web site
    int datelen = 29;   // set as +1 of the actual length on the web page as there is usually a space between the 'marker text' and the date


    // web page for simple content check
    const char* page2 = "/IoT%20testing%20homepage";  // must include the leading /
    const char* check_text = "text to be found";

    // do the date check which returns a text string
    char* response = webpage_datetimecheck(debug, domain, page1, access_token, infront_text, datelen, ref_datetime, datetime_fmt);
    printf ("\n************* date check ***********************************\n");
    char* falsestr = "false";
    char* truestr = "true";
    if (strcmp(response, truestr) == 0) {
        printf ("webpage_datetimecheck is TRUE\n");
    } else if (strcmp(response, falsestr) == 0) {
        printf ("webpage_datetimecheck is FALSE\n");
    } else {
        printf ("webpage_datetimecheck did not find the date or some other error occured\n");
    }

    // do the simple content check which returns TRUE/FALSE
    resp = webpage_check(debug, domain, page2, access_token, check_text);
    printf ("\n************* content check ***********************************\n");
    if (resp) {
        printf ("webpage_check is TRUE\n");
    } else {
        printf ("webpage_check is FALSE\n");
    }

    // do the new tracker item post which returns the new itemId#
    char* response = tracker_itempost(debug, domain, access_token, trackerId, post_data);
    printf ("\n************ post tracker item ************************************\n");
    printf ("tracker upload response is: %s\n", response);;

    return 0; 
}	