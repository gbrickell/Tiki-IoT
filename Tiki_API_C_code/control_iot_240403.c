// control_iot_YYMMDD.c custom C code for Tiki IoT functions that can be used with 'main' C code
// as well as with primary Python code that call these routines using the 'ctypes' method
// based on an original development by Geoff Brickell in August/September 2023
// building upon many of the examples from https://curl.se/libcurl/c/libcurl.html
// updated release 240403 for general availability

// *****************
// *** IMPORTANT *** 
// This code, whilst it has undergone significant testing should be considered as early development 'quality' 
// and users should carry out their own testing/quality checks when incorporating it in their own system developments.
// The software is made available on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
// *****************

#define _XOPEN_SOURCE 700
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>   // allows the use of bool, true and false which are otherwise not available in C
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "control_iot_240403.h"

int debug;

char swver[8] = "240403";

struct MemoryStruct {
  char *memory;
  size_t size;
};


// ************************************
// Function to get the size of a file
// ************************************
long int findSize(const char* file_name)
{
    struct stat st; /*declare local stat variable*/

    /*get the size of the passed file name using stat()*/

    if (stat(file_name, &st) == 0)
        return (st.st_size);
    else
        return -1;
}


// ********************************
// Function to copy a string
// ********************************
char* copyString(char s[])
{
    // passed parameter s is the string to be copied
    // returns a copy of the string
    char* s2;
    s2 = (char*)malloc(10000);   // this is made very large just in case some API responses are very long!
 
    strcpy(s2, s);  // s2 is the destination string, s is the source string
    return (char*)s2;
}


// ***********************************************************************************************************
// the function below removes rm_length characters from a string after character index. Taken from :
//  https://codereview.stackexchange.com/questions/116004/remove-specified-number-of-characters-from-a-string
// ***********************************************************************************************************
void removeString (char text[], int index, int rm_length)
{
    // text: is the string that is worked on
    // index: is the pointer in the string from which the removal starts
    // rm_length: is the number of characters that are removed

    int i;
    for ( i = 0; i < index; ++i )
        if ( text[i] == '\0' )
            return;
    for ( ; i < index + rm_length; ++i )
        if ( text[i] == '\0' ) {
            text[index] = '\0';
            return;
        }
    do {
        text[i - rm_length] = text[i];
    } while ( text[i++] != '\0' );
}

// ************************************************************
// this simple 'connection' function included for test purposes
// ************************************************************
void connect_iot()
{
    // this test function just prints out the following message
    printf("\nHello - you are now connected to the %s YYMMDD version of the control IoT C functions...\n", swver);
}



// ***************************************************************
//   download full web page function
// curl code based upon https://curl.se/libcurl/c/getinmemory.html
// ***************************************************************
char* webpage_download(int debug, const char* domain, const char* page, char* access_token)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // page: text for the specific web page part of the URL that must include the leading / and spaces 'filled' with %20 NOT + or -
    // access_token: the API access token that enables specific permissions for the API usage
    // web page content is returned as returnstr from the cURL response

    char *returnstr = "";
	if (debug==1)
    {
       printf ("\n *** debug from webpage_download ...\n");
       printf ("domain name is: %s\n", domain);
       printf ("page name is: %s\n", page);
    }
    // build the full wiki API URL
    char wiki_api[15] = "/api/wiki/page";
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, wiki_api);
	strcat(API_URL, page);
	if (debug==1)
    {
       printf ("\n *** debug from webpage_download ...\n");
       printf ("API URL is: %s\n", API_URL);
	}

    // GET the content of the web page
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;    /* no data at this point */
 
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);
    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers 
    struct curl_slist *headchunk = NULL;
    // Add the accept header */
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    // Add the authorization header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);
 
    /* get it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        returnstr = copyString("curl access to the Tiki site for the web page download function failed");
    } else if (memchunk.size == 0) {
        printf("the curl request may have been processed BUT there was no response from the server API\n");
        returnstr = copyString("no response from the curl request sent to the server API");
    } else {
        /*
        * Now, our memchunk.memory points to a memory block that is memchunk.size
        * bytes big and contains the web page content.
        *
        */
	    if (debug==1)
        {
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full wiki page text is: %s\n", memchunk.memory);
        }
        returnstr = copyString(memchunk.memory);
    }
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    free(memchunk.memory);
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();

	return returnstr;
}


// ***************************************************************
// web page simple/general content check function: looks for 
//  specific content on a page and returns 0 if false or 1 if true
// curl code based upon https://curl.se/libcurl/c/getinmemory.html
// ***************************************************************
_Bool webpage_check(int debug, const char* domain, const char* page, char* access_token, const char* check_text)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // page: text for the specific web page part of the URL that must include the leading / and spaces 'filled' with %20 NOT + or -
    // access_token: the API access token that enables specific permissions for the API usage
    // check_text: text string that is 'looked for' on the web page content
    // check_result is returned as either TRUE or FALSE

    bool check_result = false;
	if (debug==1)
    {
       printf ("\n *** debug from webpage_check ...\n");
       printf ("domain name is: %s\n", domain);
       printf ("page name is: %s\n", page);
       printf ("text to be found is: %s\n", check_text);
    }
    // build the full wiki API URL
    char wiki_api[15] = "/api/wiki/page";
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, wiki_api);
	strcat(API_URL, page);
	if (debug==1)
    {
       printf ("\n *** debug from webpage_check ...\n");
       printf ("API URL is: %s\n", API_URL);
	}

    // GET the content of the web page
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;    /* no data at this point */
 
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);
    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers 
    struct curl_slist *headchunk = NULL;
    // Add the accept header */
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    // Add the authorization header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);
 
    /* get it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        // check_result is already set to false - so no need to update it
    } else if (memchunk.size == 0) {
        printf("the curl request may have been processed BUT there was no response from the server API\n");
        check_result = false;
    } else {
        /*
        * Now, our memchunk.memory points to a memory block that is memchunk.size
        * bytes big and contains the web page content.
        *
        * content check code below does a string check against the 'check_text' string
        */
	    if (debug==1)
        {
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             //printf("the full wiki page text is: %s\n", memchunk.memory);
        }
         char *found;
         // use strstr to check if the check_text is 'in' memchunk.memory
         //  - returns pointer to start if found, or a null pointer if not
         found = strstr(memchunk.memory, check_text);  // found is now the whole string from check_text onwards if found
         // now remove all the characters after the check_text characters
         removeString(found, strlen(check_text), strlen(memchunk.memory));
         if ( found != NULL )  {  // check_text string must have been found!
		     check_result = true;
	         if (debug==1)
             {
                 printf ("\ntext found - cropped found text is: %s\n", found);
             }

         } else {
	         if (debug==1)
             {
                 printf ("\ntext not found\n");
             }
             check_result = false;
         }
    }
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    free(memchunk.memory);
    if (debug==1)
    {
        printf ("\ncurl_handle cleaned up and memchunk.memory freed\n");
    }
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
    if (debug==1)
    {
        printf ("curl_global cleaned up and returning check_result: %s\n", check_result?"true":"false");
    }
	return check_result;
}


// *********************************************************************
// web page content date check function: looks for a specific 
//  date in the content on a page and returns various status text e.g 
//  'true' if the found date is more recent than the passed check date 
//  or 'false' if not and various other error status results.
// this version allows the date character length to be set 
// curl code based upon https://curl.se/libcurl/c/getinmemory.html
// *********************************************************************
char* webpage_datetimecheck(int debug, const char* domain, const char* page, char* access_token, const char* infront_text, int datelen, const char* ref_datetime, const char* datetime_fmt)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // page: text for the specific web page part of the URL that must include the leading / and spaces 'filled' with %20 NOT + or -
    // access_token: the API access token that enables specific permissions for the API usage
    // infront_text: text string that is a marker on the web page that proceeds the date and can be 'looked for' in the web page content
    // datelen: is the character length of the date text
    // ref_datetime: is a string of the integer linux time that is being checked against
    // datetime_fmt: is the expected format of the date text in the web page content e.g. "%a %b %d, %Y %H:%M:%S %Z"
    // check_result_text is returned as a string to indicate the result

    char *check_result_text = "";
    struct tm reftm ;
    memset(&reftm, 0, sizeof(struct tm));     // make sure the struct is initialised otherwise you get crazy numbers
    //strptime(ref_datetime, "%d-%m-%Y %H:%M", &reftm);  // example 27-12-2021 11:05
    strptime(ref_datetime, datetime_fmt, &reftm);        // assumes the ref_datetime uses the format datetime_fmt
    time_t reftime = 0;
    reftime = mktime(&reftm);
    
	if (debug==1)
    {
       printf ("\n *** debug from webpage_datetimecheck ...\n");
       printf ("domain name is: %s\n", domain);
       printf ("page name is: %s\n", page);
       printf ("text in front of date is: %s\n", infront_text);
       printf ("date to be checked is: %s\n", ref_datetime);
       printf("reference date/time structure details ...\n");
       printf("tm_hour:  %d\n",reftm.tm_hour);
       printf("tm_min:  %d\n",reftm.tm_min);
       printf("tm_sec:  %d\n",reftm.tm_sec);
       printf("tm_mday:  %d\n",reftm.tm_mday);
       printf("tm_mon:  %d\n",reftm.tm_mon+1);        // add 1 for display since month is zero based
       printf("tm_year:  %d\n",reftm.tm_year+1900);   // add 1900 as year is the number of years since 1900
       printf("tm_wday:  %d\n",reftm.tm_wday);        // days since Sunday - [0-6]
       printf ("ref_datetime as epoch integer: %ld\n", reftime);
       printf ("datelen parameter: %d\n", datelen);
    }

    // build the full wiki API URL
    char wiki_api[15] = "/api/wiki/page";
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, wiki_api);
	strcat(API_URL, page);
	if (debug==1)
    {
       printf ("\n *** debug from webpage_datetimecheck ...\n");
       printf ("API URL is: %s\n", API_URL);
	}

    // GET the content of the web page
    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;    /* no data at this point */
 
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);
    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers 
    struct curl_slist *headchunk = NULL;
    // Add the accept header */
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    // Add the authorization header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    /* get it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
         fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         check_result_text = copyString("curl access to the Tiki site for web page date-time check failed");
    } else if (memchunk.size == 0) {
        printf("the curl request may have been processed BUT there was no response from the server API\n");
        check_result_text = copyString("no response from the curl request sent to the server API");
    } else {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the web page content.
         *
         * the date check code below gets the date from the web page content and checks it against ref_datetime
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full wiki page text is: %s\n", memchunk.memory);
         }
         char *found;
         found = strstr(memchunk.memory, infront_text);        // found should now be the whole string from infront_text onwards
	     if (debug==1)
         { 
             printf("the full text from infront_text onwards is: %s\n", found);
         }

         if ( found != NULL )  {  // infront_text string found!
             int offset = datelen + strlen(infront_text);
             removeString(found, offset, strlen(memchunk.memory)); // strip away all the back end text after the datelen + infront_text characters
	         if (debug==1)
             {
                 printf ("1st cropped found text is: %s\n", found);
             }
             int infront = strlen(infront_text) + 1;    // add 1 to take away an assumed space in front of the datetime text
             removeString(found, 0, infront);   // strip away the first infront_text + 1 characters for just the date/time string
	         if (debug==1)
             {
                 printf ("2nd cropped found text is: %s\n", found);
             }
             // now convert to epoch time and compare

             struct tm foundtm ;
             memset(&foundtm, 0, sizeof(struct tm));     // make sure the struct is initialised otherwise you get crazy numbers
             strptime(found, datetime_fmt, &foundtm);    // datetime_fmt is passed as a parameter, example "%a %b %d, %Y %H:%M:%S %Z"
             time_t foundtime = 0;
             foundtime = mktime(&foundtm);
	         if (debug==1)
             {
                 printf("found date/time structure details ...\n");
                 printf("tm_hour:  %d\n",foundtm.tm_hour);
                 printf("tm_min:  %d\n",foundtm.tm_min);
                 printf("tm_sec:  %d\n",foundtm.tm_sec);
                 printf("tm_mday:  %d\n",foundtm.tm_mday);
                 printf("tm_mon:  %d\n",foundtm.tm_mon+1);        // add 1 for display since month is zero based
                 printf("tm_year:  %d\n",foundtm.tm_year+1900);   // add 1900 as year is the number of years since 1900
                 printf("tm_wday:  %d\n",foundtm.tm_wday);        // days since Sunday - [0-6]
                 printf ("found date/time as epoch integer: %ld\n", foundtime);
             }
             // now compare found vs ref times
             if (foundtime > reftime) {
                 check_result_text = copyString("true");
                 if (debug==1) {
                    printf("found time is newer, returning: %s\n", check_result_text);
                 }
             } else {
                 check_result_text = copyString("false");
	             if (debug==1) {
                    printf("found time is older, returning: %s\n", check_result_text);
                 }
             }
         } else {
             check_result_text = copyString("not found");
	         if (debug==1)
             {
                 printf ("\n*** infront_text not found!! ***, returning: %s\n", check_result_text);
                 printf ("\n\n");
                 printf ("full original web page text is: %s\n", memchunk.memory);
             }
         }
    }
 
    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    free(memchunk.memory);
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();

	return check_result_text;

}

// ***************************************************************************************
// the function below was taken 'as is' from https://curl.se/libcurl/c/getinmemory.html
//  for use in various other functions above and below
// ***************************************************************************************
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
  char *ptr = realloc(mem->memory, mem->size + realsize + 1);
  if(!ptr) {
    /* out of memory! */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
  mem->memory = ptr;
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;
  return realsize;
}


// ***************************************************************************************
// the function below was taken 'as is' from https://curl.se/libcurl/c/sepheaders.html
//  for use in various File gallery functions below
// ***************************************************************************************
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}


// *********************************************************************
//  new tracker item post function - uses similar curl code as above and 
//  borrows from https://curl.se/libcurl/c/simplepost.html
// *********************************************************************
char* tracker_itempost(int debug, const char* domain, char* access_token, const char* trackerId, const char* post_data)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // access_token: the API access token that enables specific permissions for the API usage
    // trackerId: is a string of the integer Id of the tracker that is being 'posted' to
    // post_data: is a string containing the field data details of the new tracker item e.g
    // returnstr is returned as a string to indicate the result and is the itemId of the new tracker item or an error message

    char *itemId = "";
    char *returnstr = "";
    // build the full tracker API URL
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, "/api/trackers/");
    strcat(API_URL, trackerId);
    strcat(API_URL, "/items");
	if (debug==1)
    {
       printf ("\n *** debug from tracker_itempost ...\n");
       printf ("API URL is: %s\n", API_URL);
	}	

    // POST to the tracker
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;            /* no data at this point */

    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    /* send all returned data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);
    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers 
    struct curl_slist *headchunk = NULL;
    // Add the accept header */
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    // Add the Content-Type header */
    headchunk = curl_slist_append(headchunk, "Content-Type: application/x-www-form-urlencoded");
    // Add the authorization access token header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    // set the post data
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post_data);
    // if we do not provide POSTFIELDSIZE, libcurl will strlen() by itself 
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)strlen(post_data));

    /* post it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
         fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         returnstr = copyString("curl access to the Tiki site for tracker item post failed");
    } else if (memchunk.size == 0) {
         printf("the curl request may have been processed BUT there was no response from the server API\n");
         returnstr = copyString("no response from the curl request sent to the server API");
    } else {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the response text from the API.
         *
         * the code below extracts the new tracker itemId# from the memory block so it can be returned
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full text response is: %s\n", memchunk.memory);
         }
         itemId = strstr(memchunk.memory, "itemId");        // itemId should now be the whole string from the 'itemId' text onwards
         if ( itemId != NULL )  {  // itemId string found!
             // now strip away everything from a comma to the end of the string
             char *ptr;
             ptr = strchr(itemId, ',');
             if (ptr != NULL) {
                 *ptr = '\0';
             }
	         if (debug==1)
             {
                 printf ("1st cropped found text is: %s\n", itemId);
             }

             // the line of code below now removes the first 8 characters i.e. 'itemId":' leaving just the integer value
             //  only 8 cahracters are removed because an itemId is always an integer so is never surrounded in quote characters
             removeString(itemId, 0, 8);
	         if (debug==1)
             {
                 printf ("2nd cropped found text is: %s\n", itemId);
             }
             returnstr = copyString(itemId);
         } else {
             returnstr = copyString("itemId text not found");
	         if (debug==1)
             {
                 printf ("\n*** itemId text not found in response!! ***\n");
                 printf ("full original tracker post API response is: %s\n", memchunk.memory);
                 printf ("returnstr set to                          : %s\n", returnstr);
             }

         }
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
    free(memchunk.memory);
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("return string is: %s\n", returnstr);
    }
	return returnstr;
}	


// ****************************************************************************
//  existing tracker item update function - uses similar curl code as above and 
//  borrows from https://curl.se/libcurl/c/simplepost.html
// ****************************************************************************
char* tracker_itemupdate(int debug, const char* domain, char* access_token, const char* trackerId, const char* itemId, const char* post_data)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // access_token: the API access token that enables specific permissions for the API usage
    // trackerId: is a string of the integer Id of the tracker that is being 'posted' to
    // itemId: is a string of the integer Id of the tracker item that is being updated
    // post_data: is a string containing the updated field data details of the tracker item
    // returnstr is returned as a string to indicate the result and is all the field data in a dictionary-like format or an error message
    char *response = "";
    char *returnstr = "";
    // build the full tracker API URL
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, "/api/trackers/");
    strcat(API_URL, trackerId);
    strcat(API_URL, "/items/");
    strcat(API_URL, itemId);
	if (debug==1)
    {
       printf ("\n *** debug from tracker_itemupdate ...\n");
       printf ("API URL is: %s\n", API_URL);
	}	

    // POST data to the tracker
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;            /* no data at this point */

    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    /* send all returned data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);
    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers 
    struct curl_slist *headchunk = NULL;
    // Add the accept header */
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    // Add the Content-Type header */
    headchunk = curl_slist_append(headchunk, "Content-Type: application/x-www-form-urlencoded");
    // Add the authorization access token header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    // set the post data
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post_data);
    // if we do not provide POSTFIELDSIZE, libcurl will strlen() by itself 
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, (long)strlen(post_data));

    /* post it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
         fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         returnstr = copyString("curl access to the Tiki site for tracker item update failed");
    } else if (memchunk.size == 0) {
         printf("the curl request may have been processed BUT there was no response from the server API\n");
         returnstr = copyString("no response from the curl request sent to the server API");
    } else {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the returned text from the API.
         *
         * multiple steps in the code below extract the returned text after: "mes":[" from the memory block so it can be returned
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full text response is: %s\n", memchunk.memory);
         }

         // first of all check that the update went OK by looking for "Success" in memchunk
         response = strstr(memchunk.memory, "Success");        // response should now be the whole string from "Success" onwards
	     if (debug==1) {
             printf ("length of Success response string is: %d\n", strlen(response) );
         }

         if ( response != NULL )  {  // "Success" string found!
             // now look for the start of the 'mes' text
             response = strstr(memchunk.memory, "mes");        // response should now be the whole string from "mes" onwards
	         if (debug==1) {
                 printf ("length of mes response string is: %d\n", strlen(response) );
             }

             if ( response != NULL )  {  // "mes" string found!

                 removeString(response, 0, 7);     // strip away the first 7 front end characters for just the text and everything after
	             if (debug==1)
                 {
                     printf ("1st cropped found text is: %s\n", response);
                     printf ("length of new mes response string is: %d\n", strlen(response) );
                 }
                 // now strip away everything from the first ] character found
                 char *ptr;
                 ptr = strchr(response, ']');    // ptr is the pointer for the ] in 'response'
                 if (ptr != NULL) {
                     *ptr = '\0';   // this makes the original ] character the 'end of string' character
                 }
	             if (debug==1)
                 {
                     printf ("2nd cropped found text is: %s\n", response);
                     printf ("length of next new mes response string is: %d\n", strlen(response) );
                 }
                 // now remove the final " character to leave just the message text 
                 removeString(response, strlen(response)-1, 1);    
                 returnstr = copyString(response);
	             if (debug==1)
                 {
                     printf ("3rd cropped found text is: %s\n", response);
                     printf ("length of final new mes response string is: %d\n", strlen(response) );
                     printf ("returnstr set to         : %s\n", returnstr);
                 }

             } else {
                 returnstr = copyString("mes text not found");
	             if (debug==1)
                 {
                     printf ("\n*** mes text not found in response!! ***\n");
                     printf ("full original tracker update API response is: %s\n", memchunk.memory);
                     printf ("returnstr set to                            : %s\n", returnstr);
                 }

             }

         } else {
             returnstr = copyString("Success text not found");
	         if (debug==1)
             {
                 printf ("\n*** Success text not found in response!! ***\n");
                 printf ("full original tracker update API response is: %s\n", memchunk.memory);
                 printf ("returnstr set to                            : %s\n", returnstr);
             }

         }
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
	if (debug==1)
    {
        printf ("curl_handle cleaned up\n");
    }
    free(memchunk.memory);
	if (debug==1)
    {
        printf ("memchunk.memory freed\n");
    }
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("curl_global cleaned up, and ...\n");
        printf ("return string is: %s\n", returnstr);
    }
	return returnstr;
}	

// ********************************************************************************
//  existing tracker item download function - much the same as tracker_itemupdate
//   function, but just has an empty 'body' for the cURL and returns the "fields"
//   section of the whole response
// *******************************************************************************
char* tracker_itemget(int debug, const char* domain, char* access_token, const char* trackerId, const char* itemId)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // access_token: the API access token that enables specific permissions for the API usage
    // trackerId: is a string of the integer Id of the tracker that is being 'posted' to
    // itemId: is a string of the integer Id of the tracker item that is being downloaded
    // returnstr is returned as a string to indicate the result and is all the field data in a dictionary-like format or an error message
    char *response = "";
    char *returnstr = "";
    char* post_data = "";   // create an empty body so that POST is used
    // build the full tracker API URL
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, "/api/trackers/");
    strcat(API_URL, trackerId);
    strcat(API_URL, "/items/");
    strcat(API_URL, itemId);
	if (debug==1)
    {
       printf ("\n *** debug from tracker_itemget ...\n");
       printf ("API URL is: %s\n", API_URL);
	}	

    // send data to the tracker API
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;            /* no data at this point */

    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();
    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    /* send all returned data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);
    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers - just needs the 'accept' and the access token
    struct curl_slist *headchunk = NULL;
    // Add the Content-Type header */
    headchunk = curl_slist_append(headchunk, "Content-Type: application/x-www-form-urlencoded");
    // Add the accept header */
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    // Add the authorization access token header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    // set the 'empty' post_data
    curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post_data);

    /* send it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
         fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         returnstr = copyString("curl access to the Tiki site for tracker item download failed");
    } else if (memchunk.size == 0) {
         printf("the curl request may have been processed BUT there was no response from the server API\n");
         returnstr = copyString("no response from the curl request sent to the server API");
    } else {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the returned text from the API.
         *
         * multiple steps in the code below extract the returned text after: "fields": from the memory block so it can be returned
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full text response is: %s\n", memchunk.memory);
         }

         // first of all check that the update went OK by looking for "Success" in memchunk
         response = strstr(memchunk.memory, "Success");        // response should now be the whole string from "Success" onwards
	     if (debug==1) {
             printf ("length of Success response string is: %d\n", strlen(response) );
         }

         if ( response != NULL )  {  // "Success" string found!
             // now look for the start of the 'fields' text
             response = strstr(memchunk.memory, "fields");        // response should now be the whole string from "fields" onwards
	         if (debug==1) {
                 printf ("Success! Length of fields response string is now: %d\n", strlen(response) );
             }

             if ( response != NULL )  {  // "fields" string found!

                 removeString(response, 0, 8);     // strip away the first 8 front end characters for just the fields text and everything after
	             if (debug==1)
                 {
                     printf ("1st cropped found text is: %s\n", response);
                     printf ("length of new fields response string is: %d\n", strlen(response) );
                 }
                 // now strip away everything at the end from 'nextTicket' onwards
                 char *ptr;
                 ptr = strstr(response, "nextTicket");    // ptr is the pointer for nextTicket in 'response'
                 if (ptr != NULL) {
                     *ptr = '\0';   // this makes the original ] character the 'end of string' character
                 }
	             if (debug==1)
                 {
                     printf ("2nd cropped fields text is: %s\n", response);
                     printf ("length of next new fields response string is: %d\n", strlen(response) );
                 }
                 // now remove the final " character to leave just the fields text 
                 removeString(response, strlen(response)-2,2);    
                 returnstr = copyString(response);
	             if (debug==1)
                 {
                     printf ("3rd cropped found text is: %s\n", response);
                     printf ("length of final new fields response string is: %d\n", strlen(response) );
                     printf ("returnstr set to         : %s\n", returnstr);
                 }

             } else {
                 printf ("\n*** fields text not found in response!! ***");
                 printf ("\n\n");
                 returnstr = copyString("fields text not found");
	             if (debug==1)
                 {
                     printf ("full original web page text is: %s\n", memchunk.memory);
                     printf ("returnstr set to              : %s\n", returnstr);
                 }

             }

         } else {
             printf ("\n*** Success text not found in response!! ***");
             printf ("\n\n");
	         if (debug==1)
             returnstr = copyString("Success text not found");
             {
                 printf ("full original web page text is: %s\n", memchunk.memory);
                 printf ("returnstr set to              : %s\n", returnstr);
             }

         }
    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
	if (debug==1)
    {
        printf ("curl_handle cleaned up\n");
    }
    free(memchunk.memory);
	if (debug==1)
    {
        printf ("memchunk.memory freed\n");
    }
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("curl_global cleaned up, and ...\n");
        printf ("return string is: %s\n", returnstr);
    }
	return returnstr;
}	
	


// ********************************************************************************
//  existing Tiki File gallery file download function 
// *******************************************************************************
char* gallery_filedownload(int debug, const char* domain, char* access_token, const char* fileId, const char* filespath, const char* bodyfilename, const char* headerfilename)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // access_token: the API access token that enables specific permissions for the API usage
    // fileId is Id of the file to be downloaded
    // filespath is the folder path on the calling device where the downloaded file and the response header file are 
    //    to be stored and should include both the first and last / character
    /* both bodyfilename and headerfilename are passed parameters for file names in the path defined by filespath and  
       where headerfilename is typically a temporary file on the calling device used for the response headers that would  
       be overwritten with each use, and where bodyfilename can either be 'dictated' by the calling program to be a  
       specific name or if left blank the 'body' i.e. the downloaded file will initially be stored under a temporary name  
       then the actual file name is extracted from the response header file and the downloaded file is renamed */

	if (debug==1)
    {
        printf ("\n\n");
        printf ("***************************************\n");
        printf ("*** debug from gallery_filedownload ...\n");
        printf ("***************************************\n\n");
    }

    char *response = "";
    char *returnstr = "";

    // build the full file gallery API URL
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, "/api/galleries/");
    strcat(API_URL, fileId);
    strcat(API_URL, "/download");

    // build the stored file paths + names
    char download[100] = "";
    strcat(download, filespath);
    if (strlen(bodyfilename) == 0) {
        strcat(download, "tempdownload");
        if (debug==1) { 
            printf ("temporary download file name being used before renaming: tempdownload\n");
        }
    } else {
        strcat(download, bodyfilename);
        if (debug==1) { 
            printf ("dictated download file name being used: %s\n", bodyfilename);
        }
    }
    char respheaders[100] = "";
    strcat(respheaders, filespath);
    strcat(respheaders, headerfilename);

	if (debug==1)
    {
        printf ("API URL is       : %s\n", API_URL);
        printf ("download filename: %s\n", download);
        printf ("headers filename : %s\n", respheaders);
	}	

    // send data to the file gallery API

    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);

    /* no progress meter ! */
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);

    /* send all returned data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);

    /* open the header file */
    FILE *headerfile;
    headerfile = fopen(respheaders, "w");
    if(!headerfile) {     // if the file cannot be opened abort everything
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        returnstr = copyString("response header file location could not be opened");
        return returnstr;
    }
 
    /* open the body file */
    FILE *bodyfile;
    bodyfile = fopen(download, "wb");  // use wb to allow binary
    if(!bodyfile) {    // if the file cannot be opened so abort everything
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        fclose(headerfile);
        returnstr = copyString("download file location could not be opened");
        return returnstr;
    }

    /* we want the headers be written to this file handle */
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, headerfile);
 
    /* we want the body be written to this file handle instead of stdout */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, bodyfile);

    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom request headers - just needs the 'accept' and the access token
    struct curl_slist *headchunk = NULL;
    // Add the Content-Type header */
    headchunk = curl_slist_append(headchunk, "Content-Type: application/x-www-form-urlencoded");
    // Add the accept header */
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    // Add the authorization access token header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    /* get it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
    /* close the header file */
    fclose(headerfile);
    /* close the body file */
    fclose(bodyfile);
	if (debug==1)
    {
        printf ("header and body files written and closed\n");
    }
 
    /* check for errors */
    if(res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        returnstr = copyString("curl access to the Tiki site for File gallery file download failed");
    } else {
        /*
        * Now, check both the header and body files are OK and 
        *  extract the Content-Disposition in the response header file 
        *  if the passed bodyfilename is 'empty' 
        */
        long int headersize = 0;
        headersize = findSize(respheaders);
        if (headersize == -1) {
            returnstr = copyString("header file size is zero or some other error");
	        if (debug==1) { 
                printf("header file size is zero - aborting operation\n");
            }
            curl_easy_cleanup(curl_handle);
            curl_global_cleanup();
            return returnstr;
        }

	    if (debug==1) { 
            printf("header file size: %d\n", headersize);
        }

        long int bodysize = 0;
        bodysize = findSize(download);
        if (bodysize == -1) {
            returnstr = copyString("body file size is zero or some other error");
	        if (debug==1) { 
                printf("body file size is zero or some other error - aborting operation\n");
            }
            curl_easy_cleanup(curl_handle);
            curl_global_cleanup();
            return returnstr;
        }

	    if (debug==1) { 
            printf("body file size: %d\n", bodysize);
        }

        // now check if the download file should be renamed
        if (strlen(bodyfilename) == 0) {    // a temporary file name has been used so it should be renamed

            // now extract the actual file name from the response header file

            // first reopen the header file for read only
            headerfile = fopen(respheaders, "r");
            if(!headerfile) {     // if the file cannot be opened abort the renaming
	            if (debug==1)
                {
                    printf ("couldn't reopen the response header file\n");
                }

                returnstr = copyString(download);
                strcat(returnstr, " downloaded as header file could not be reopened");
                curl_easy_cleanup(curl_handle);
                curl_global_cleanup();
                return returnstr;
               
            } else {
 
                // read the response header file line by line looking for content-disposition
                char *respline = "";
                char resp[200] = "";

                while ( fgets(resp,180,headerfile) != NULL) {
	                if (debug==1)
                    {
                        printf ("line read from file is: %s\n", resp);
                    }                            
                    respline = copyString(resp);
                    respline = strstr(respline, "attachment");  // respline would now be the whole string from 'attachment' onwards if it is found
                    if ( respline != NULL )  {  // not NULL so attachment found in the line so this is the content-disposition line!                    
                        // now strip away some of the front 
                        respline = strstr(respline, "=");  // respline now has ="filename"
	                    if (debug==1)
                        {
                            printf ("1st cropped found text is: %s\n", respline);
                        }

                        // now remove the first two =" characters
                        removeString(respline, 0, 2);
	                    if (debug==1)
                        {
                            printf ("2nd cropped found text is: %s\n", respline);
                        }

                        // now remove everything from the final " character onwards
                        char *ptr;
                        ptr = strstr(respline, "\"");    // ptr is the pointer for " in 'respline'
                        if (ptr != NULL) {
                            *ptr = '\0';   // this makes the original " character the 'end of string' character
                        }
                        if (debug==1)
                        {
                            printf ("3rd cropped found text is: %s\n", respline);
                        }
                        break;  // as we have found the file name we can break the while loop
                    } else {
	                    if (debug==1)
                        {
                            printf ("text not found - reading next line\n");
                        }
                    }
                }  // continue with the while loop reading each line
                fclose(headerfile);

                // once here we should now have the actual file name in respline
                char newdownload[100] = "";
                strcat(newdownload, filespath);
                strcat(newdownload, respline);
	            if (debug==1)
                {
                    printf ("renaming the stored temporary download file\n");
                    printf ("new file name is: %s\n", newdownload);
                }

                // now rename the temp file produced from the main download
                rename(download, newdownload);
	            if (debug==1)
                {
                    printf ("download file renamed\n");
                }
                returnstr = copyString(newdownload);
                strcat(returnstr, " downloaded OK");
            }
        } else {
            returnstr = copyString(download);
            strcat(returnstr, " downloaded OK");
        }

    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
	if (debug==1)
    {
        printf ("curl_handle cleaned up\n");
    }

    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("curl_global cleaned up, and ...\n");
        printf ("return string is: %s\n", returnstr);
    }
	return returnstr;
}	
	

// ************************************************************************************
//  new file gallery file upload function that uses a multipart/form-data Content-type
// ************************************************************************************
char* gallery_fileupload(int debug, const char* domain, char* access_token, const char* filepath, const char* galId, const char* filename, const char* filetitle, const char* filedesc)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // access_token: the API access token that enables specific permissions for the API usage
    // filepath: text string for the path/name of the file on the hub device to be uploaded
    // galId: text string for the integer Id of the Tiki File gallery where the file is to be stored
    // filename: text string of the just the name of the file without its ‘path’ details
    // filetitle: text string of the short text File gallery title to be assigned to the file
    // filedesc: text string of the longer text File gallery description to be assigned to the file

	if (debug==1)
    {
        printf ("\n\n");
        printf ("***************************************\n");
        printf ("*** debug from gallery_fileupload ...\n");
        printf ("***************************************\n\n");
    }

    char *response = "";
    char *returnstr = "";
    // build the full File gallery API URL
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, "/api/galleries/upload");

	if (debug==1)
    {
       printf ("API URL is  : %s\n", API_URL);
       printf ("filepath is : %s\n", filepath);
       printf ("galleryId is: %s\n", galId);
       printf ("filename is : %s\n", filename);
       printf ("filetitle is: %s\n", filetitle);
       printf ("filedesc is : %s\n", filedesc);
	}	

    // send data to the galleries file upload API

    // memory used to store the response text
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;            /* no data at this point */

    CURL *curl_handle;
    CURLcode res;
    curl_mime *form = NULL;
    curl_mimepart *field = NULL;

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* Create the virtual multi part form */
    form = curl_mime_init(curl_handle);

    /* Fill in the file upload field */
    field = curl_mime_addpart(form);
    curl_mime_name(field, "data");
    curl_mime_filedata(field, filepath);

    field = curl_mime_addpart(form);
    curl_mime_name(field, "galleryId");
    curl_mime_data(field, galId, CURL_ZERO_TERMINATED);

    field = curl_mime_addpart(form);
    curl_mime_name(field, "name");
    curl_mime_data(field, filename, CURL_ZERO_TERMINATED);

    field = curl_mime_addpart(form);
    curl_mime_name(field, "title");
    curl_mime_data(field, filetitle, CURL_ZERO_TERMINATED);

    field = curl_mime_addpart(form);
    curl_mime_name(field, "description");
    curl_mime_data(field, filedesc, CURL_ZERO_TERMINATED);

    /* specify URL to POST to */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);

    /* send all returned data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);

    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers - just needs the 'accept' and the access token
    struct curl_slist *headchunk = NULL;
    // Add the Content-Type header */
    headchunk = curl_slist_append(headchunk, "Content-Type: multipart/form-data");
    // Add the authorization access token header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    curl_easy_setopt(curl_handle, CURLOPT_MIMEPOST, form);

    /* POST it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
         fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         returnstr = copyString("curl access to the Tiki site for File gallery file upload failed");
    } else if (memchunk.size == 0) {
         printf("the curl request may have been processed BUT there was no response from the server API\n");
         returnstr = copyString("no response from the curl request sent to the server API");
    } else {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the returned text from the API.
         *
         * multiple steps in the code below extract the returned new fileId from the memory block so it can be returned
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full text response is: %s\n", memchunk.memory);
         }

         // first of all check that the upload went OK by looking for "fileId" in memchunk
         response = strstr(memchunk.memory, "fileId");        // response should now be the whole string from "fileId" onwards

         if ( response != NULL )  {  // "fileId" string found!
	         if (debug==1) {
                 printf ("fileId found! Length of response string is now: %d\n", strlen(response) );
             }

             removeString(response, 0, 9);     // strip away the first 9 front end characters for just the fileId number and everything after
	         if (debug==1)
             {
                 printf ("1st cropped found text is: %s\n", response);
                 printf ("length of new response string is: %d\n", strlen(response) );
             }

             // now strip away everything at the end from 'galleryId' onwards
             char *ptr;
             ptr = strstr(response, "galleryId");    // ptr is the pointer for galleryId in 'response'
             if (ptr != NULL) {
                 *ptr = '\0';   // this makes the original ] character the 'end of string' character
             }
	         if (debug==1)
             {
                 printf ("2nd cropped response text is: %s\n", response);
                 printf ("length of next new response string is: %d\n", strlen(response) );
             }

             // now remove the final "," characters to leave just the fileId number 
             removeString(response, strlen(response)-3,3);    
             returnstr = copyString(response);
	         if (debug==1)
             {
                 printf ("3rd cropped response is: %s\n", response);
                 printf ("length of final response string is: %d\n", strlen(response) );
                 printf ("returnstr set to         : %s\n", returnstr);
             }

         } else {
             printf ("\n*** fileId text not found in response!! ***");
             printf ("\n\n");
             returnstr = copyString("fileId text not found");
	         if (debug==1)
             {
                 printf ("full response text is: %s\n", memchunk.memory);
                 printf ("returnstr set to              : %s\n", returnstr);
             }

         }

    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
	if (debug==1)
    {
        printf ("curl_handle cleaned up\n");
    }
    free(memchunk.memory);
	if (debug==1)
    {
        printf ("memchunk.memory freed\n");
    }
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("curl_global cleaned up, and ...\n");
        printf ("return string is: %s\n", returnstr);
    }
	return returnstr;
}	
	

// ************************************************************************************
//  function to update an existing file gallery file upload function that uses 
//  a multipart/form-data Content-type and can optionally update the file contents 
//  or just update the parameters associated with the file
// ************************************************************************************
char* gallery_fileupdate(int debug, const char* domain, char* access_token, const char* fileId, const char* filepath, const char* filename, const char* filetitle, const char* filedesc)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // access_token: the API access token that enables specific permissions for the API usage
    // fileId: string text for the fileID# integer that is being updated
    // filepath: string text for the full path-name of a file to replace the existing file - if left blank no change is made
    // filename: string text to rename the file that is in Tiki - if left blank no change is made
    // filetitle: string text to change the title of the file that is in Tiki - if left blank no change is made
    // filedesc: string text to change the description of the file that is in Tiki - if left blank no change is made

	if (debug==1)
    {
        printf ("\n\n");
        printf ("***************************************\n");
        printf ("*** debug from gallery_fileupdate ...\n");
        printf ("***************************************\n\n");
    }

    char *response = "";
    char *returnstr = "";
    // build the full File gallery API URL
	char API_URL[100] = "";
    strcat(API_URL, domain);
    strcat(API_URL, "/api/galleries/files/");
    strcat(API_URL, fileId);
    strcat(API_URL, "/update");

	if (debug==1)
    {
       printf ("API URL is  : %s\n", API_URL);
       printf ("fileId is: %s\n", fileId);
       printf ("filepath is : %s\n", filepath);
       printf ("filename is : %s\n", filename);
       printf ("filetitle is: %s\n", filetitle);
       printf ("filedesc is : %s\n", filedesc);
	}	

    // send data to the galleries file update API

    // memory used to store the response text
    struct MemoryStruct memchunk;
    memchunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    memchunk.size = 0;            /* no data at this point */

    CURL *curl_handle;
    CURLcode res;
    curl_mime *form = NULL;
    curl_mimepart *field = NULL;

    curl_global_init(CURL_GLOBAL_ALL);

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* Create the virtual multi part form */
    form = curl_mime_init(curl_handle);

    /* Fill in the file upload field but only if it has been set*/
    if (strlen(filepath) > 0) {
        if (debug==1) { 
            printf("file contents is being updated\n");
        }
        field = curl_mime_addpart(form);
        curl_mime_name(field, "data");
        curl_mime_filedata(field, filepath);
    } else {
        if (debug==1) { 
            printf("file contents are NOT being updated\n");
        }
    }

    /* Fill in the file name field but only if it has been set*/
    //  it should be noted that if the file name is changed but
    //  the file title is not explcitly set below, the title will 
    //  still be changed to text similar to the new name
    if (strlen(filename) > 0) {
        if (debug==1) { 
            printf("file name is being updated\n");
        }
        field = curl_mime_addpart(form);
        curl_mime_name(field, "name");
        curl_mime_data(field, filename, CURL_ZERO_TERMINATED);
    } else {
        if (debug==1) { 
            printf("file name is NOT being updated\n");
        }
    }
    /* Fill in the file title field but only if it has been set*/
    if (strlen(filetitle) > 0) {
        if (debug==1) { 
            printf("file title is being updated\n");
        }
        field = curl_mime_addpart(form);
        curl_mime_name(field, "title");
        curl_mime_data(field, filetitle, CURL_ZERO_TERMINATED);
    } else {
        if (debug==1) { 
            printf("file title is NOT being updated\n");
        }
    }

    /* Fill in the file description field but only if it has been set*/
    if (strlen(filedesc) > 0) {
        if (debug==1) { 
            printf("file description is being updated\n");
        }
        field = curl_mime_addpart(form);
        curl_mime_name(field, "description");
        curl_mime_data(field, filedesc, CURL_ZERO_TERMINATED);
    } else {
        if (debug==1) { 
            printf("file description is NOT being updated\n");
        }
    }

    /* specify URL to POST to */
    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);

    /* send all returned data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'memchunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&memchunk);

    /* some servers do not like requests that are made without a user-agent field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // set the custom headers - just needs the 'accept' and the access token
    struct curl_slist *headchunk = NULL;
    // Add the Content-Type header */
    headchunk = curl_slist_append(headchunk, "Content-Type: multipart/form-data");
    // Add the authorization access token header */
    headchunk = curl_slist_append(headchunk, access_token);
    // set the headers   
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    curl_easy_setopt(curl_handle, CURLOPT_MIMEPOST, form);

    /* POST it! */
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk); /* free the list */
 
    /* check for errors */
    if(res != CURLE_OK) {
         fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
         returnstr = copyString("curl access to the Tiki site for File gallery file update failed");
    } else if (memchunk.size == 0) {
         printf("the curl request may have been processed BUT there was no response from the server API\n");
         returnstr = copyString("no response from the curl request sent to the server API");
    } else {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the returned text from the API, which what is returned
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full text response is: %s\n", memchunk.memory);
         }

         // first of all check that the upload went OK by looking for "fileId" in memchunk
         response = strstr(memchunk.memory, "fileId");        // response should now be the whole string from "fileId" onwards

         if ( response != NULL )  {  // "fileId" string found! so we did a successful update
	         if (debug==1) {
                 printf ("fileId found! So update was successful\n");
             }

             returnstr = copyString(memchunk.memory);
	         if (debug==1)
             {
                 printf ("returnstr set to memchunk.memory\n");
             }

         } else {
             printf ("\n*** fileId text not found in response!! ***");
             printf ("\n\n");
             returnstr = copyString("fileId text not found - so update was not successful");
	         if (debug==1)
             {
                 printf ("full response text is: %s\n", memchunk.memory);
                 printf ("returnstr set to     : %s\n", returnstr);
             }

         }

    }

    /* cleanup curl stuff */
    curl_easy_cleanup(curl_handle);
	if (debug==1)
    {
        printf ("curl_handle cleaned up\n");
    }
    free(memchunk.memory);
	if (debug==1)
    {
        printf ("memchunk.memory freed\n");
    }
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("curl_global cleaned up, and ...\n");
    }
	return returnstr;
}	
	
