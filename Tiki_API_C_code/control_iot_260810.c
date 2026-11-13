// control_iot_YYMMDD.c custom C code for Tiki IoT functions that can be used with 'main' C code
// as well as with primary Python code that call these routines using the 'ctypes' method
// based on an original development by Geoff Brickell in August/September 2023
// building upon many of the examples from https://curl.se/libcurl/c/libcurl.html
// updated release 240403 for general availability
// further updated release 240807 to tweak:
//  - webpage_datetimecheck to better address timezone offsets for daylight savings changes through the year
// further updated release 250107 to add more error checking
// further updated release 250722 to:
//  - update webpage_datetimecheck to use separate datetime formats for the 'reference' and 'checked' datetimes
//  - add #include <unistd.h> // to use sleep() in various places
//  - start using CURLOPT_ERRORBUFFER to collect more error data for failed curl request
// further update release 250808 to refine and extend the use of CURLOPT_ERRORBUFFER and avoid logging the more generic 'libcurl: (0)' text (which means 'No error') in stderr
// further update release 260701 to continue the multipart cleanup and polishing pass
// further update release 260708 to add additional functions

// *****************
// *** IMPORTANT *** 
// This code, whilst it has undergone significant testing should be considered as early development 'quality' 
// and users should carry out their own testing/quality checks when incorporating it in their own system developments.
// The software is made available on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.
// *****************

#define _XOPEN_SOURCE 700
#define _GNU_SOURCE /* for tm_gmtoff and tm_zone */
#include <time.h>
#include <unistd.h> // to use sleep() in various places
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>   // allows the use of bool, true and false which are otherwise not available in C
#include <string.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include "control_iot_260810.h"

int debug;

char swver[8] = "260810";

struct MemoryStruct {
  char *memory;
  size_t size;
};

static char* safe_copy_string(const char *s);
static int build_api_url(char *buffer, size_t buffer_size, const char *domain, const char *path);
static struct curl_slist* build_json_auth_headers(const char *access_token, bool include_content_type, bool multipart);
static char* curl_error_message(CURLcode res, char errbuf[CURL_ERROR_SIZE], const char *context);
static char* perform_memory_request(int debug, const char *url, struct curl_slist *headers, const char *post_data, long post_fields_size, struct MemoryStruct *memchunk, char errbuf[CURL_ERROR_SIZE]);
static int setup_multipart_curl(CURL **curl_handle, struct MemoryStruct *memchunk, char errbuf[CURL_ERROR_SIZE], const char *url);
static char* handle_curl_result(int debug, CURLcode res, char errbuf[CURL_ERROR_SIZE], const struct MemoryStruct *memchunk, const char *error_message_prefix);
static char* handle_memory_request_result(int debug, char *request_result, const struct MemoryStruct *memchunk, const char *empty_response_message);
static char* extract_and_copy_between(char *text, const char *prefix, const char *suffix);
static char* extract_response_fragment(char *text, const char *start_marker, int start_trim, const char *end_marker, int end_trim);
static char* copy_response_before_marker(const char *text, const char *marker, int keep_trailer_chars);
static int build_tracker_dump_path(char *buffer, size_t buffer_size, const char *filepath, const char *filename);
static long int count_file_lines(const char *path);
struct DumpWriteContext {
    FILE *file;
    int debug;
    int echo;
    int first_line_complete;
    int first_line_flushed;
    int first_line_has_error;
    int linebuf_overflow;
    int aborted;
    char linebuf[16384];
    size_t linebuf_len;
    char errtext[8192];
};
static size_t write_dump_data(void *ptr, size_t size, size_t nmemb, void *stream);
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);


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
static char* safe_copy_string(const char *s)
{
    if (s == NULL) {
        return NULL;
    }
    size_t len = strlen(s);
    char *copy = malloc(len + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, s, len + 1);
    return copy;
}

char* copyString(char s[])
{
    return safe_copy_string(s);
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

static int build_api_url(char *buffer, size_t buffer_size, const char *domain, const char *path)
{
    if (!buffer || !domain || !path) {
        return -1;
    }
    int written = snprintf(buffer, buffer_size, "%s%s", domain, path);
    if (written < 0 || (size_t)written >= buffer_size) {
        if (buffer_size > 0) {
            buffer[0] = '\0';
        }
        return -1;
    }
    return 0;
}

static struct curl_slist* build_json_auth_headers(const char *access_token, bool include_content_type, bool multipart)
{
    struct curl_slist *headchunk = NULL;
    if (include_content_type) {
        headchunk = curl_slist_append(headchunk, multipart ? "Content-Type: multipart/form-data" : "Content-Type: application/x-www-form-urlencoded");
    }
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    headchunk = curl_slist_append(headchunk, access_token);
    return headchunk;
}

static char* curl_error_message(CURLcode res, char errbuf[CURL_ERROR_SIZE], const char *context)
{
    if (errbuf != NULL && errbuf[0] != '\0') {
        size_t buflen = strlen(errbuf);
        if (!(buflen == 4 && strcmp(errbuf, "0") == 0)) {
            char message[512];
            snprintf(message, sizeof(message), "%s failed: %s", context ? context : "curl request", errbuf);
            return safe_copy_string(message);
        }
    }
    return safe_copy_string(context);
}

static char* perform_memory_request(int debug, const char *url, struct curl_slist *headers, const char *post_data, long post_fields_size, struct MemoryStruct *memchunk, char errbuf[CURL_ERROR_SIZE])
{
    CURL *curl_handle;
    CURLcode res;
    char *returnstr = "";

    memchunk->memory = malloc(1);
    if (memchunk->memory == NULL) {
        return safe_copy_string("out of memory allocating curl response buffer");
    }
    memchunk->memory[0] = '\0';
    memchunk->size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        free(memchunk->memory);
        curl_global_cleanup();
        return safe_copy_string("curl_easy_init failed");
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)memchunk);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errbuf);
    if (headers != NULL) {
        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headers);
    }
    if (post_data != NULL) {
        curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDS, post_data);
        if (post_fields_size >= 0) {
            curl_easy_setopt(curl_handle, CURLOPT_POSTFIELDSIZE, post_fields_size);
        }
    }

    errbuf[0] = 0;
    res = curl_easy_perform(curl_handle);
    if (debug == 1) {
        printf("curl_easy_perform() returned %d\n", res);
    }
    if (res != CURLE_OK) {
        returnstr = curl_error_message(res, errbuf, "curl request");
    }

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
    return returnstr;
}

static char* extract_and_copy_between(char *text, const char *prefix, const char *suffix)
{
    char *start = strstr(text, prefix);
    if (!start) {
        return NULL;
    }
    start += strlen(prefix);
    if (suffix) {
        char *end = strstr(start, suffix);
        if (end) {
            *end = '\0';
        }
    }
    return safe_copy_string(start);
}

static char* extract_response_fragment(char *text, const char *start_marker, int start_trim, const char *end_marker, int end_trim)
{
    char *start = strstr(text, start_marker);
    if (start == NULL) {
        return NULL;
    }

    start += strlen(start_marker);
    if (start_trim > 0 && (size_t)start_trim < strlen(start)) {
        start += start_trim;
    }

    if (end_marker != NULL) {
        char *end = strstr(start, end_marker);
        if (end != NULL) {
            if (end_trim > 0 && (size_t)end_trim <= (size_t)(end - start)) {
                end -= end_trim;
            }
            *end = '\0';
        }
    } else if (end_trim > 0) {
        size_t len = strlen(start);
        if ((size_t)end_trim <= len) {
            start[len - (size_t)end_trim] = '\0';
        }
    }

    return safe_copy_string(start);
}

static char* copy_response_before_marker(const char *text, const char *marker, int keep_trailer_chars)
{
    char *copy = safe_copy_string(text);
    if (copy == NULL) {
        return NULL;
    }

    if (marker != NULL) {
        char *cut = strstr(copy, marker);
        if (cut != NULL) {
            if (keep_trailer_chars > 0 && (size_t)keep_trailer_chars <= (size_t)(cut - copy)) {
                cut -= keep_trailer_chars;
            }
            *cut = '\0';
        }
    }

    return copy;
}

static int build_tracker_dump_path(char *buffer, size_t buffer_size, const char *filepath, const char *filename)
{
    if (!buffer || !filepath || !filename) {
        return -1;
    }

    if (snprintf(buffer, buffer_size, "%s%s", filepath, filename) >= (int)buffer_size) {
        if (buffer_size > 0) {
            buffer[0] = '\0';
        }
        return -1;
    }

    size_t len = strlen(buffer);
    if (len < 4 || strcmp(buffer + len - 4, ".csv") != 0) {
        if (snprintf(buffer + len, buffer_size - len, ".csv") >= (int)(buffer_size - len)) {
            if (buffer_size > 0) {
                buffer[0] = '\0';
            }
            return -1;
        }
    }

    return 0;
}

static long int count_file_lines(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }

    long int lines = 0;
    int ch;
    int saw_any = 0;
    int last_was_newline = 0;
    while ((ch = fgetc(fp)) != EOF) {
        saw_any = 1;
        if (ch == '\n') {
            lines++;
            last_was_newline = 1;
        } else {
            last_was_newline = 0;
        }
    }
    fclose(fp);
    if (saw_any && !last_was_newline) {
        lines++;
    }
    return lines;
}

static size_t write_dump_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
    struct DumpWriteContext *ctx = (struct DumpWriteContext *)stream;
    size_t realsize = size * nmemb;
    const char *data = (const char *)ptr;

    if (ctx->aborted) {
        return 0;
    }

    for (size_t i = 0; i < realsize; ++i) {
        char ch = data[i];
        if (!ctx->first_line_complete) {
            if (ctx->linebuf_len + 1 < sizeof(ctx->linebuf)) {
                ctx->linebuf[ctx->linebuf_len++] = ch;
                ctx->linebuf[ctx->linebuf_len] = '\0';
            } else {
                ctx->linebuf_overflow = 1;
            }
            if (ch == '\n') {
                ctx->first_line_complete = 1;
                ctx->first_line_has_error = (strstr(ctx->linebuf, "error") != NULL);
                if (ctx->first_line_has_error) {
                    snprintf(ctx->errtext, sizeof(ctx->errtext), "%s", ctx->linebuf);
                }
            }
        } else {
            if (!ctx->first_line_flushed) {
                if (fwrite(ctx->linebuf, 1, ctx->linebuf_len, ctx->file) != ctx->linebuf_len) {
                    ctx->aborted = 1;
                    return 0;
                }
                if (ctx->debug == 1 || ctx->echo == 1) {
                    fwrite(ctx->linebuf, 1, ctx->linebuf_len, stdout);
                }
                ctx->first_line_flushed = 1;
            }
            if (fwrite(&ch, 1, 1, ctx->file) != 1) {
                ctx->aborted = 1;
                return 0;
            }
            if (ctx->debug == 1 || ctx->echo == 1) {
                fwrite(&ch, 1, 1, stdout);
            }
        }
    }
    return realsize;
}

static char* safe_append_suffix(const char *prefix, const char *suffix)
{
    size_t plen = strlen(prefix);
    size_t slen = strlen(suffix);
    char *result = malloc(plen + slen + 1);
    if (result == NULL) {
        return NULL;
    }
    memcpy(result, prefix, plen);
    memcpy(result + plen, suffix, slen + 1);
    return result;
}

static int setup_multipart_curl(CURL **curl_handle, struct MemoryStruct *memchunk, char errbuf[CURL_ERROR_SIZE], const char *url)
{
    memchunk->memory = malloc(1);
    if (memchunk->memory == NULL) {
        return -1;
    }
    memchunk->memory[0] = '\0';
    memchunk->size = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    *curl_handle = curl_easy_init();
    if (*curl_handle == NULL) {
        free(memchunk->memory);
        curl_global_cleanup();
        return -1;
    }

    curl_easy_setopt(*curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(*curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(*curl_handle, CURLOPT_WRITEDATA, (void *)memchunk);
    curl_easy_setopt(*curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(*curl_handle, CURLOPT_ERRORBUFFER, errbuf);
    errbuf[0] = 0;
    return 0;
}

static char* handle_curl_result(int debug, CURLcode res, char errbuf[CURL_ERROR_SIZE], const struct MemoryStruct *memchunk, const char *error_message_prefix)
{
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        size_t buflen = strlen(errbuf);
        fprintf(stderr, "\nlibcurl: (%d) ", res);
        if (buflen) {
            fprintf(stderr, "%s%s", errbuf, ((errbuf[buflen - 1] != '\n') ? "\n" : ""));
        } else {
            fprintf(stderr, "%s\n", curl_easy_strerror(res));
        }
        return safe_copy_string(error_message_prefix);
    }

    if (memchunk->size == 0) {
        if (debug == 1) {
            printf("the curl request may have been processed BUT there was an empty response from the server API\n");
        }
        size_t buflen = strlen(errbuf);
        if (buflen) {
            char *errresp = strstr(errbuf, "0");
            if (errresp == NULL) {
                fprintf(stderr, "\nlibcurl: (%d) ", res);
                fprintf(stderr, "%s%s", errbuf, ((errbuf[buflen - 1] != '\n') ? "\n" : ""));
                return safe_copy_string("empty response from the curl request sent to the server API - see error log for more detail");
            }
        } else {
            return safe_copy_string("empty response from the curl request sent to the server API - you should check if the request has been processed");
        }
    }

    return NULL;
}

static char* handle_memory_request_result(int debug, char *request_result, const struct MemoryStruct *memchunk, const char *empty_response_message)
{
    if (request_result != NULL && request_result[0] != '\0') {
        return request_result;
    }

    if (memchunk->size == 0) {
        if (debug == 1) {
            printf("the curl request may have been processed BUT there was an empty response from the server API\n");
        }
        return safe_copy_string(empty_response_message);
    }

    return NULL;
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

    char errbuf[CURL_ERROR_SIZE];
    char *returnstr = "";
	if (debug==1)
    {
       printf ("\n *** debug from webpage_download ...\n");
       printf ("domain name is: %s\n", domain);
       printf ("page name is: %s\n", page);
    }
    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/wiki/page") != 0) {
        return safe_copy_string("API URL was too long");
    }
    if (snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s", page) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }
	if (debug==1)
    {
       printf ("\n *** debug from webpage_download ...\n");
       printf ("API URL is: %s\n", API_URL);
	}

    // GET the content of the web page
    struct MemoryStruct memchunk;
    struct curl_slist *headchunk = build_json_auth_headers(access_token, false, false);
    returnstr = perform_memory_request(debug, API_URL, headchunk, NULL, -1, &memchunk, errbuf);
    curl_slist_free_all(headchunk);
    returnstr = handle_memory_request_result(debug, returnstr, &memchunk, "empty response from the curl request sent to the server API - you should check if the request has been processed");
    if (returnstr == NULL) {
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
         // first use strstr to check if 'not found (404)' is 'in' memchunk.memory
         char not_found[16] = "not found (404)";
         char *nopage;
         nopage = strstr(memchunk.memory, not_found);  // nopage is now the whole string from 'not found' onwards if found
         if ( nopage != NULL )  {  // 'not found' string must have been found!
		     returnstr = "page not found";
	         if (debug==1)
             {
                 printf ("\npage not found in Tiki");
             }

         } else {
             returnstr = safe_copy_string(memchunk.memory);
        }
    }
    free(memchunk.memory);
    if (debug==1)
    {
        printf ("\ncurl_handle cleaned up and memchunk.memory freed\n");
        printf ("curl_global cleaned up and returning: %s\n", returnstr);
    }

	return returnstr;

}


// ***************************************************************
// web page simple/general content check function: looks for 
//  specific content on a page and returns a specific result text
// curl code based upon https://curl.se/libcurl/c/getinmemory.html
// ***************************************************************
char* webpage_check(int debug, const char* domain, const char* page, char* access_token, const char* check_text)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // page: text for the specific web page part of the URL that must include the leading / and spaces 'filled' with %20 NOT + or -
    // access_token: the API access token that enables specific permissions for the API usage
    // check_text: text string that is 'looked for' on the web page content
    // check_result is returned as a message string

    char *check_result = "";
	if (debug==1)
    {
       printf ("\n *** debug from webpage_check ...\n");
       printf ("domain name is: %s\n", domain);
       printf ("page name is: %s\n", page);
       printf ("text to be found is: %s\n", check_text);
    }
    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/wiki/page") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s", page) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }
	if (debug==1)
    {
       printf ("\n *** debug from webpage_check ...\n");
       printf ("API URL is: %s\n", API_URL);
	}

    struct MemoryStruct memchunk;
    struct curl_slist *headchunk = build_json_auth_headers(access_token, false, false);
    char errbuf[CURL_ERROR_SIZE];
    check_result = perform_memory_request(debug, API_URL, headchunk, NULL, -1, &memchunk, errbuf);
    curl_slist_free_all(headchunk);
    check_result = handle_memory_request_result(debug, check_result, &memchunk, "empty response from the curl request sent to the server API - you should check if the request has been processed");
    if (check_result == NULL) {
        /*
        * Now, our memchunk.memory points to a memory block that is memchunk.size
        * bytes big and contains the web page content.
        *
        * content check code below first does a check on the page being found 
        *  and then it does a string check against the 'check_text' string
        */
	    if (debug==1)
        {
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             //printf("the full wiki page text is: %s\n", memchunk.memory);
        }
         // use strstr to check if 'not found (404)' is 'in' memchunk.memory
         char not_found[16] = "not found (404)";
         char *nopage;
         nopage = strstr(memchunk.memory, not_found);  // nopage is now the whole string from 'not found' onwards if found
         if ( nopage != NULL )  {  // 'not found' string must have been found!
		     check_result = "page not found";
	         if (debug==1)
             {
                 printf ("\npage not found in Tiki");
             }

         } else {
             // now use strstr to check if the check_text is 'in' memchunk.memory
             //  but only if the page was found
             char *found;
             //  - returns pointer to start of found, or a null pointer if not
             found = strstr(memchunk.memory, check_text);  // found is now the whole string from check_text onwards if found
             // check 'found' and return an error message if the check_text is not found
             if ( found != NULL )  {  // check_text string must have been found!
                 check_result = "check_text found";
                 // now remove all the characters after the check_text characters
                 removeString(found, strlen(check_text), strlen(memchunk.memory));
	             if (debug==1)
                 {
                     printf ("\ntext found - cropped found text is: %s\n", found);
                 }

             } else {
	             if (debug==1) {
                     printf ("\n check_text not found\n");
                 }
                 check_result = "check_text not found";
			 }	 
         }
    }
    free(memchunk.memory);

    if (debug==1)
    {
        printf ("\ncurl_handle cleaned up and memchunk.memory freed\n");
        printf ("curl_global cleaned up and returning check_result: %s\n", check_result);
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
char* webpage_datetimecheck(int debug, const char* domain, const char* page, char* access_token, const char* infront_text, int datelen, const char* ref_datetime, const char* refdatetime_fmt, const char* checkeddatetime_fmt)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // page: text for the specific web page part of the URL that must include the leading / and spaces 'filled' with %20 NOT + or -
    // access_token: the API access token that enables specific permissions for the API usage
    // infront_text: text string that is a marker on the web page that proceeds the date and can be 'looked for' in the web page content
    // datelen: is the character length of the date text
    // ref_datetime: is a string of the integer linux time that is being checked against
    // refdatetime_fmt: is the format of the date/time text provided as the reference e.g. "%Y-%m-%d %H:%M:%S %Z"
    // checkeddatetime_fmt: is the expected format of the date text in the web page content e.g. "%a %d %b %Y %H:%M:%S %Z"
    // check_result_text is returned as a string to indicate the result

    char *check_result_text = "";
    struct tm reftm ;
    memset(&reftm, 0, sizeof(struct tm));     // make sure the struct is initialised otherwise you get crazy numbers
    //strptime(ref_datetime, "%d-%m-%Y %H:%M", &reftm);  // example 27-12-2021 11:05
    strptime(ref_datetime, refdatetime_fmt, &reftm);        // assumes the ref_datetime uses the format refdatetime_fmt
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

    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/wiki/page") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s", page) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }
	if (debug==1)
    {
       printf ("\n *** debug from webpage_datetimecheck ...\n");
       printf ("API URL is: %s\n", API_URL);
	}

    struct MemoryStruct memchunk;
    char errbuf[CURL_ERROR_SIZE];
    struct curl_slist *headchunk = build_json_auth_headers(access_token, false, false);
    check_result_text = perform_memory_request(debug, API_URL, headchunk, NULL, -1, &memchunk, errbuf);
    curl_slist_free_all(headchunk);
    check_result_text = handle_memory_request_result(debug, check_result_text, &memchunk, "empty response from the curl request sent to the server API - you should check if the request has been processed");
    if (check_result_text == NULL) {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the web page content.
         *
         * content check code below first does a check on the page being found 
         *  and then it gets the date from the web page content and checks it against ref_datetime
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             //printf("the full wiki page text is: %s\n", memchunk.memory);
         }

         // use strstr to check if 'not found (404)' is 'in' memchunk.memory
         char not_found[16] = "not found (404)";
         char *nopage;
         nopage = strstr(memchunk.memory, not_found);  // nopage is now the whole string from 'not found' onwards if found
         if ( nopage != NULL )  {  // 'not found' string must have been found!
		     check_result_text = "page not found";
	         if (debug==1)
             {
                 printf ("\npage not found in Tiki");
             }

         } else {
             // now use strstr to check if the infront_text is 'in' memchunk.memory
             //  but only if the page was found

             char *found;
             found = strstr(memchunk.memory, infront_text);        // found should now be the whole string from infront_text onwards
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
                 strptime(found, checkeddatetime_fmt, &foundtm);    // checkeddatetime_fmt is passed as a parameter, example "%a %d %b %Y %H:%M:%S %Z"
                 time_t foundtime = 0;
                 foundtime = mktime(&foundtm);
                 if (foundtm.tm_gmtoff!=0)
                 {
                     foundtime = foundtime - foundtm.tm_gmtoff;
                 }
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
                     printf("tm_gmtoff:  %ld\n",foundtm.tm_gmtoff);    // %Z offset in seconds
                     printf ("found date/time as epoch integer: %ld\n", foundtime);
                 }
                 // now compare found vs ref times
                 if (foundtime > reftime) {
                     check_result_text = copyString("true: found time is after ref time");
                     if (debug==1) {
                        printf("found time is newer, returning: %s\n", check_result_text);
                     }
                 } else {
                     check_result_text = copyString("false: found time is before ref time");
	                 if (debug==1) {
                        printf("found time is older, returning: %s\n", check_result_text);
                     }
                 }
             } else {
                 check_result_text = copyString("infront_text not found");
	             if (debug==1)
                 {
                     printf ("\n*** infront_text not found!! ***, returning: %s\n", check_result_text);
                     printf ("\n\n");
                     //printf ("full original web page text is: %s\n", memchunk.memory);
                 }
             }
         }
    }
 
    free(memchunk.memory);
    if (debug==1)
    {
        printf ("\ncurl_handle cleaned up and memchunk.memory freed\n");
        printf ("curl_global cleaned up and returning: %s\n", check_result_text);
    }

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

    char *returnstr = "";
    // build the full tracker API URL
	char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/trackers/") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s/items", trackerId) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }
	if (debug==1)
    {
       printf ("\n *** debug from tracker_itempost ...\n");
       printf ("API URL is: %s\n", API_URL);
	}	

    struct MemoryStruct memchunk;
    char errbuf[CURL_ERROR_SIZE];
    struct curl_slist *headchunk = build_json_auth_headers(access_token, true, false);
    returnstr = perform_memory_request(debug, API_URL, headchunk, post_data, (long)strlen(post_data), &memchunk, errbuf);
    curl_slist_free_all(headchunk);
    returnstr = handle_memory_request_result(debug, returnstr, &memchunk, "empty response from the curl request sent to the server API - you should check if the request has been processed");
    if (returnstr == NULL) {
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
         returnstr = extract_response_fragment(memchunk.memory, "\"itemId\":", 0, ",", 0);
         if (returnstr != NULL) {
	         if (debug==1)
             {
                 printf ("itemId extracted as: %s\n", returnstr);
             }
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

    free(memchunk.memory);
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

    char *returnstr = "";
    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/trackers/") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s/items/%s", trackerId, itemId) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }
	if (debug==1)
    {
       printf ("\n *** debug from tracker_itemupdate ...\n");
       printf ("API URL is: %s\n", API_URL);
	}	

    struct MemoryStruct memchunk;
    char errbuf[CURL_ERROR_SIZE];
    struct curl_slist *headchunk = build_json_auth_headers(access_token, true, false);
    returnstr = perform_memory_request(debug, API_URL, headchunk, post_data, (long)strlen(post_data), &memchunk, errbuf);
    curl_slist_free_all(headchunk);
    returnstr = handle_memory_request_result(debug, returnstr, &memchunk, "empty response from the curl request sent to the server API - you should check if the request has been processed");
    if (returnstr == NULL) {
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
         char *response = strstr(memchunk.memory, "Success");        // response should now be the whole string from "Success" onwards
         if (debug==1) {
             printf ("check made for the Success text string\n" );
         }

         if ( response != NULL )  {  // "Success" string found!
             if (debug==1) {
                 printf ("length of Success response string is: %ld\n", strlen(response) );
             }

             returnstr = extract_response_fragment(memchunk.memory, "\"mes\":[", 0, "]", 1);
             if (returnstr != NULL) {
	         if (debug==1) {
                     printf ("mes extracted as: %s\n", returnstr);
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

    free(memchunk.memory);

	if (debug==1)
    {
        printf ("curl_handle cleaned up\n");
        printf ("memchunk.memory freed\n");
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

    char *returnstr = "";
    char* post_data = "";   // create an empty body so that POST is used
    char errbuf[CURL_ERROR_SIZE];
    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/trackers/") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s/items/%s", trackerId, itemId) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }
	if (debug==1)
    {
       printf ("\n *** debug from tracker_itemget ...\n");
       printf ("API URL is: %s\n", API_URL);
	}	

    struct MemoryStruct memchunk;
    struct curl_slist *headchunk = build_json_auth_headers(access_token, true, false);
    returnstr = perform_memory_request(debug, API_URL, headchunk, post_data, 0, &memchunk, errbuf);
    curl_slist_free_all(headchunk);
    returnstr = handle_memory_request_result(debug, returnstr, &memchunk, "empty response from the curl request sent to the server API - you should check if the request has been processed");
    if (returnstr == NULL) {
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
         char *response = strstr(memchunk.memory, "Success");        // response should now be the whole string from "Success" onwards

         if ( response != NULL )  {  // "Success" string found!

	         if (debug==1) {
                 printf ("length of Success response string is: %ld\n", strlen(response) );
             }
             returnstr = extract_response_fragment(memchunk.memory, "\"fields\":", 0, "nextTicket", 2);
             if (returnstr != NULL) {
	         if (debug==1) {
                 printf ("fields extracted as: %s\n", returnstr);
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
             returnstr = copyString("Success text not found");
             if (debug==1) {
                 printf ("full original web page text is: %s\n", memchunk.memory);
                 printf ("returnstr set to              : %s\n", returnstr);
             }

         }
    }

    free(memchunk.memory);
	if (debug==1)
    {
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

    char errbuf[CURL_ERROR_SIZE];
    char *errresp = "";
    char *response = "";
    char *returnstr = "";

    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/galleries/") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s/download", fileId) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }

    // build the stored file paths + names
    char download[256] = "";
    if (snprintf(download, sizeof(download), "%s", filespath) >= (int)sizeof(download)) {
        return safe_copy_string("download path was too long");
    }
    if (strlen(bodyfilename) == 0) {
        if (snprintf(download + strlen(download), sizeof(download) - strlen(download), "%s", "tempdownload") >= (int)(sizeof(download) - strlen(download))) {
            return safe_copy_string("download path was too long");
        }
        if (debug==1) { 
            printf ("temporary download file name being used before renaming: tempdownload\n");
        }
    } else {
        if (snprintf(download + strlen(download), sizeof(download) - strlen(download), "%s", bodyfilename) >= (int)(sizeof(download) - strlen(download))) {
            return safe_copy_string("download path was too long");
        }
        if (debug==1) { 
            printf ("dictated download file name being used: %s\n", bodyfilename);
        }
    }
    char respheaders[256] = "";
    if (snprintf(respheaders, sizeof(respheaders), "%s%s", filespath, headerfilename) >= (int)sizeof(respheaders)) {
        return safe_copy_string("response header path was too long");
    }

	if (debug==1)
    {
        printf ("API URL is       : %s\n", API_URL);
        printf ("download filename: %s\n", download);
        printf ("headers filename : %s\n", respheaders);
	}	

    // send data to the file gallery API

    /* open the header file */
    FILE *headerfile;
    headerfile = fopen(respheaders, "w");
    if(!headerfile) {     // if the file cannot be opened abort everything
        returnstr = copyString("response header file location could not be opened");
        return returnstr;
    }
 
    /* open the body file */
    FILE *bodyfile;
    bodyfile = fopen(download, "wb");  // use wb to allow binary
    if(!bodyfile) {    // if the file cannot be opened so abort everything
        fclose(headerfile);
        returnstr = copyString("download file location could not be opened");
        return returnstr;
    }

    CURL *curl_handle;
    CURLcode res;
    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        fclose(headerfile);
        fclose(bodyfile);
        curl_global_cleanup();
        return copyString("curl_easy_init failed");
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, headerfile);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, bodyfile);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, errbuf);

    struct curl_slist *headchunk = NULL;
    headchunk = curl_slist_append(headchunk, "Content-Type: application/x-www-form-urlencoded");
    headchunk = curl_slist_append(headchunk, "accept: application/json");
    headchunk = curl_slist_append(headchunk, access_token);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);

    errbuf[0] = 0;
    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk);

    /* close the header file */
    fclose(headerfile);
    /* close the body file */
    fclose(bodyfile);
	if (debug==1)
    {
        printf ("header and body files written and closed\n");
    }
    if (res != CURLE_OK) {
        fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        size_t buflen = strlen(errbuf);
        fprintf(stderr, "\nlibcurl: (%d) ", res);
        if (buflen) {
            fprintf(stderr, "%s%s", errbuf, ((errbuf[buflen - 1] != '\n') ? "\n" : ""));
        } else {
            fprintf(stderr, "%s\n", curl_easy_strerror(res));
        }
        returnstr = copyString("curl access to the Tiki site for File gallery file download failed - see error log for more detail");
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
            return returnstr;
        }

	    if (debug==1) { 
            printf("header file size: %ld\n", headersize);
        }

        long int bodysize = 0;
        bodysize = findSize(download);
        if (bodysize == -1) {
            returnstr = copyString("body file size is zero or some other error");
	        if (debug==1) { 
                printf("body file size is zero or some other error - aborting operation\n");
            }
            return returnstr;
        } else if (bodysize < 80) {
            returnstr = copyString("body file size is unusually small - fileId may not have been found");
	        if (debug==1) { 
                printf("body file size: %ld\n", bodysize);
                printf("body file size is unusually small which suggests the fileId has not been found - aborting operation\n");
            }
            return returnstr;
        }

	    if (debug==1) { 
            printf("body file size: %ld\n", bodysize);
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

                returnstr = safe_append_suffix(download, " downloaded as header file could not be reopened");
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
                    // first check that a 'good' download has happened
                    respline = copyString(resp);                    
                    respline = strstr(respline, "HTTP/2 404"); 
                    if ( respline != NULL )  {  // not NULL so "HTTP/2 404" found in the line so an error has occured!  
	                    if (debug==1)
                        {
                            printf ("HTTP/2 404 - found in the headerfile, so an error has occurred\n");
                        }
                        returnstr = copyString("HTTP/2 404 - found in the headerfile so the fileId was probably not found");
                        if (debug==1)
                        {
                            printf ("curl_handle cleaned up\n");
                            printf ("curl_global cleaned up, and ...\n");
                            printf ("return string is: %s\n", returnstr);
                        }
	                    return returnstr;
                    }

                    // now look for the 'content-disposition' line
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
                returnstr = safe_append_suffix(newdownload, " downloaded OK");
            }
        } else {
            returnstr = safe_append_suffix(download, " downloaded OK");
        }
    }

    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();

	if (debug==1)
    {
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

    char *errresp = "";
    char *response = "";
    char *returnstr = "";
    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/galleries/upload") != 0) {
        return safe_copy_string("API URL was too long");
    }

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
    char errbuf[CURL_ERROR_SIZE];
    struct MemoryStruct memchunk;
    CURL *curl_handle;
    CURLcode res;
    curl_mime *form = NULL;
    curl_mimepart *field = NULL;
    if (setup_multipart_curl(&curl_handle, &memchunk, errbuf, API_URL) != 0) {
        return copyString("curl_easy_init failed");
    }

    /* Create the virtual multi part form */
    form = curl_mime_init(curl_handle);

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
 
    returnstr = handle_curl_result(debug, res, errbuf, &memchunk, "curl access to the Tiki site for File gallery file upload failed - see error log for more detail");
    if (returnstr == NULL) {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the returned text from the API.
         *
         * multiple steps in the code below check the response and if found extract the 
         *  returned new fileId from the memory block so it can be returned
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full text response is: %s\n", memchunk.memory);
         }

         // first of all check that a valid galId was sent
         response = strstr(memchunk.memory, "gallery does not exist");        // response should now be the whole string from this 'error' onwards
         if ( response != NULL )  {  // "gallery does not exist" string found!
             printf ("\n*** a valid galID was not sent! ***");
             printf ("\n\n");
             returnstr = copyString("invalid galId sent");
	         if (debug==1)
             {
                 printf ("full response text is: %s\n", memchunk.memory);
                 printf ("returnstr set to              : %s\n", returnstr);
             }

         }

         // now check that the upload went OK by looking for "fileId" in memchunk
         returnstr = extract_response_fragment(memchunk.memory, "\"fileId\":", 0, "\"galleryId\"", 3);
         if (returnstr != NULL )  {
	         if (debug==1)
             {
                 printf ("fileId extracted as: %s\n", returnstr);
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
    free(memchunk.memory);
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("return string is: %s\n", returnstr);
    }
	return returnstr;


}	
	

// ************************************************************************************
//  function to update an existing file gallery file upload function that uses 
//  a multipart/form-data Content-type and can optionally update the file contents 
//  or just update the parameters associated with the file
// ************************************************************************************
char* gallery_fileupdate(int debug, const char* domain, char* access_token, const char* fileId, const char* filepath, const char* filename, const char* filetitle, const char* filedesc, const char* metadata)
{
    // debug: if set to 1 this produces (lots!!) of additional output
    // domain: string used for the main URL text that must include https:// but no trailing /
    // access_token: the API access token that enables specific permissions for the API usage
    // fileId: string text for the fileID# integer that is being updated
    // filepath: string text for the full path-name of a file to replace the existing file - if left blank no change is made
    // filename: string text to rename the file that is in Tiki - if left blank no change is made
    // filetitle: string text to change the title of the file that is in Tiki - if left blank no change is made
    // filedesc: string text to change the description of the file that is in Tiki - if left blank no change is made
    // metadata: set to either 'yes' or anything else e.g. 'no'- later versions of Tiki include syntax and lots(!) of metadata in image files and will echo all of it in the API response, which can be difficult to decode by the original 'calling' program. Setting the parameter to 'yes' includes all the syntax + metadata but setting it to (say) 'no' truncates the response text to exclude everything from 'syntax' onwards in the response and makes decoding the response text a lot easier.

	if (debug==1)
    {
        printf ("\n\n");
        printf ("***************************************\n");
        printf ("*** debug from gallery_fileupdate ...\n");
        printf ("***************************************\n\n");
    }

    char errbuf[CURL_ERROR_SIZE];
    char *response = "";
    char *returnstr = "";
    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/galleries/files/") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s/update", fileId) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }

	if (debug==1)
    {
       printf ("API URL is  : %s\n", API_URL);
       printf ("fileId is: %s\n", fileId);
       printf ("filepath is : %s\n", filepath);
       printf ("filename is : %s\n", filename);
       printf ("filetitle is: %s\n", filetitle);
       printf ("filedesc is : %s\n", filedesc);
       printf ("metadata is : %s\n", metadata);
	}	

    // send data to the galleries file update API

    // memory used to store the response text
    struct MemoryStruct memchunk;
    CURL *curl_handle;
    CURLcode res;
    curl_mime *form = NULL;
    curl_mimepart *field = NULL;
    if (setup_multipart_curl(&curl_handle, &memchunk, errbuf, API_URL) != 0) {
        return copyString("curl_easy_init failed");
    }

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
    sleep(2); // pause for 2 seconds to give time for a response
    curl_slist_free_all(headchunk); /* free the list */
 
    returnstr = handle_curl_result(debug, res, errbuf, &memchunk, "curl access to the Tiki site for File gallery file update failed - see error log for more detail");
    if (returnstr == NULL) {
         /*
         * Now, our memchunk.memory points to a memory block that is memchunk.size
         * bytes big and contains the returned text from the API, which what is returned
         */
	     if (debug==1)
         { 
             printf("%lu bytes retrieved\n", (unsigned long)memchunk.size);
             printf("the full text response is: %s\n", memchunk.memory);
         }

         // first of all check that a valid fileId was sent
         response = strstr(memchunk.memory, "file does not exist");        // response should now be the whole string from this 'error' onwards
         if ( response != NULL )  {  // "file does not exist" text string found!
             printf ("\n*** a valid fileId was not sent! ***");
             printf ("\n\n");
             returnstr = copyString("no valid fileId was sent");
	         if (debug==1)
             {
                 printf ("full response text is: %s\n", memchunk.memory);
                 printf ("returnstr set to              : %s\n", returnstr);
             }

         }

         // now check that the upload went OK by looking for "fileId" in memchunk
         response = strstr(memchunk.memory, "fileId");        // response should now be the whole string from "fileId" onwards

         if ( response != NULL )  {  // "fileId" string found! so we did a successful update
	         if (debug==1) {
                 printf ("fileId found! So update was successful\n");
                 printf ("string length from fileId onwards is: %ld\n", strlen(response) );
             }

            if (strcmp (metadata, "yes") == 0) {  
                 returnstr = copyString(memchunk.memory);                
                 if (debug==1) { 
                     printf ("All the syntax and metadata is being included in the response\n");
                     printf ("returnstr set to memchunk.memory\n");
                 }
            } else {
                 returnstr = copy_response_before_marker(memchunk.memory, "\"syntax\"", 0);
                 if (returnstr != NULL) {
                    size_t len = strlen(returnstr);
                    if (len > 0 && returnstr[len - 1] == ',') {
                        returnstr[len - 1] = '\0';
                        len--;
                    }
                    if (len > 0 && returnstr[len - 1] != '}') {
                        char *trimmed = safe_append_suffix(returnstr, "}");
                        if (trimmed != NULL) {
                            free(returnstr);
                            returnstr = trimmed;
                        }
                    }
                    if (debug==1) { 
                     printf ("'syntax' text found in the response\n");
                     printf ("returnstr has been set to a truncated response\n");
                    }
                 } else {
                    returnstr = copyString(memchunk.memory);
                    if (debug==1) { 
                     printf ("'syntax' text NOT found in the response\n");
                     printf ("returnstr set to the full response\n");
                    }
                 }
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
    free(memchunk.memory);
    /* we are done with libcurl, so clean it up */
    curl_global_cleanup();
	if (debug==1)
    {
        printf ("return string is: %s\n", returnstr);
    }
	return returnstr;

}	


// ********************************************************************************
//  dump all the items from an existing tracker in CSV format and write to file
// *******************************************************************************
char* tracker_dump(int debug, const char* domain, char* access_token, const char* trackerId, char* filename, char* filepath, int echo)
{
    if (debug==1)
    {
        printf ("\n\n");
        printf ("***************************************\n");
        printf ("*** debug from tracker_dump ...\n");
        printf ("***************************************\n\n");
    }

    if (domain == NULL || access_token == NULL || trackerId == NULL || filename == NULL || filepath == NULL) {
        return safe_copy_string("invalid tracker_dump arguments");
    }
    if (strlen(filename) == 0) {
        return safe_copy_string("filename must not be empty");
    }
    if (strlen(filepath) == 0) {
        return safe_copy_string("filepath must not be empty");
    }

    char API_URL[256] = "";
    if (build_api_url(API_URL, sizeof(API_URL), domain, "/api/trackers/") != 0 ||
        snprintf(API_URL + strlen(API_URL), sizeof(API_URL) - strlen(API_URL), "%s/dump", trackerId) >= (int)(sizeof(API_URL) - strlen(API_URL))) {
        return safe_copy_string("API URL was too long");
    }

    char output_path[512] = "";
    if (build_tracker_dump_path(output_path, sizeof(output_path), filepath, filename) != 0) {
        return safe_copy_string("dump output path was too long");
    }

    if (debug==1)
    {
        printf ("API URL is       : %s\n", API_URL);
        printf ("dump filename is : %s\n", output_path);
    }

    char temp_output_path[512] = "";
    if (snprintf(temp_output_path, sizeof(temp_output_path), "%s.tmp", output_path) >= (int)sizeof(temp_output_path)) {
        return safe_copy_string("temporary dump output path was too long");
    }

    FILE *dumpfile = fopen(temp_output_path, "wb");
    if (!dumpfile) {
        return safe_copy_string("dump file location could not be opened");
    }

    CURL *curl_handle;
    CURLcode res;
    struct curl_slist *headchunk = NULL;
    struct DumpWriteContext dumpctx;
    char *returnstr = "";

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();
    if (curl_handle == NULL) {
        fclose(dumpfile);
        curl_global_cleanup();
        return safe_copy_string("curl_easy_init failed");
    }

    headchunk = curl_slist_append(headchunk, "accept: text/csv");
    headchunk = curl_slist_append(headchunk, access_token);

    dumpctx.file = dumpfile;
    dumpctx.debug = debug;
    dumpctx.echo = echo;
    dumpctx.first_line_complete = 0;
    dumpctx.first_line_flushed = 0;
    dumpctx.first_line_has_error = 0;
    dumpctx.linebuf_overflow = 0;
    dumpctx.aborted = 0;
    dumpctx.linebuf_len = 0;
    dumpctx.linebuf[0] = '\0';
    dumpctx.errtext[0] = '\0';

    curl_easy_setopt(curl_handle, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, headchunk);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_dump_data);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, &dumpctx);
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    res = curl_easy_perform(curl_handle);
    curl_slist_free_all(headchunk);

    if (res == CURLE_OK && !dumpctx.first_line_complete && dumpctx.linebuf_len > 0) {
        if (strstr(dumpctx.linebuf, "error") != NULL) {
            dumpctx.first_line_has_error = 1;
            snprintf(dumpctx.errtext, sizeof(dumpctx.errtext), "%s", dumpctx.linebuf);
            if (debug==1) {
                printf("single-line error response detected\n");
            }
            if (dumpctx.debug == 1 || dumpctx.echo == 1) {
                printf("%s\n", dumpctx.errtext);
            }
            dumpctx.aborted = 1;
        }
    }

    if (dumpctx.linebuf_overflow && debug==1) {
        printf("tracker dump first-line buffer overflowed while inspecting response header line\n");
    }

    if (res == CURLE_OK && dumpctx.first_line_complete && !dumpctx.first_line_flushed) {
        if (dumpctx.first_line_has_error) {
            if (debug==1) {
                printf("single-line error response detected\n");
            }
            if (dumpctx.debug == 1 || dumpctx.echo == 1) {
                fwrite(dumpctx.linebuf, 1, dumpctx.linebuf_len, stdout);
                fwrite("\n", 1, 1, stdout);
            }
            dumpctx.aborted = 1;
        } else {
            if (fwrite(dumpctx.linebuf, 1, dumpctx.linebuf_len, dumpfile) != dumpctx.linebuf_len) {
                dumpctx.aborted = 1;
            } else if (debug==1 || echo==1) {
                fwrite(dumpctx.linebuf, 1, dumpctx.linebuf_len, stdout);
            }
        }
    }

    if (res != CURLE_OK || dumpctx.aborted) {
        fclose(dumpfile);
        curl_easy_cleanup(curl_handle);
        curl_global_cleanup();
        remove(temp_output_path);
        if (dumpctx.first_line_has_error && dumpctx.errtext[0] != '\0') {
            returnstr = copyString(dumpctx.errtext);
        } else {
            returnstr = safe_copy_string("curl access to the Tiki site for tracker dump failed - see error log for more detail");
            if (debug==1) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }
        }
        return returnstr;
    }

    curl_easy_cleanup(curl_handle);
    fclose(dumpfile);
    curl_global_cleanup();

    if (rename(temp_output_path, output_path) != 0) {
        remove(temp_output_path);
        return safe_copy_string("tracker dump file rename failed");
    }

    if (returnstr == NULL || returnstr[0] == '\0') {
        long int dumpsize = findSize(output_path);
        if (debug==1) {
            printf("dump file size: %ld\n", dumpsize);
        }
        if (dumpsize <= 0) {
            returnstr = safe_copy_string("tracker dump file size is zero or some other error");
        } else if (dumpsize < 80) {
            returnstr = safe_copy_string("tracker dump file size is unusually small - dump may not have completed");
            if (debug==1) {
                printf("tracker dump file size is unusually small - please check the file contents\n");
            }
        } else {
            char success_message[768];
            long int linecount = count_file_lines(output_path);
            long int itemcount = (linecount > 0) ? (linecount - 1) : -1;
            if (itemcount >= 0) {
                snprintf(success_message, sizeof(success_message), "tracker dump written to %s containing %ld items", output_path, itemcount);
            } else {
                snprintf(success_message, sizeof(success_message), "tracker dump written to %s", output_path);
            }
            returnstr = safe_copy_string(success_message);
        }
    }

    if (debug==1)
    {
        printf ("return string is: %s\n", returnstr);
    }
    return returnstr;
}
	
