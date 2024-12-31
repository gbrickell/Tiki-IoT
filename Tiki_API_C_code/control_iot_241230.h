long int findSize(const char* file_name);

char* copyString(char s[]);

void removeString (char text[], int index, int rm_length);

void connect_iot();

char* webpage_download(int debug, const char* domain, const char* page, char* access_token);

_Bool webpage_check(int debug, const char* domain, const char* page, char* access_token, const char* check_text);

char* webpage_datetimecheck(int debug, const char* domain, const char* page, char* access_token, const char* infront_text, int datelen, const char* ref_datetime, const char* datetime_fmt);

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream);

char* tracker_itempost(int debug, const char* domain, char* access_token, const char* trackerId, const char* post_data);

char* tracker_itemupdate(int debug, const char* domain, char* access_token, const char* trackerId, const char* itemId, const char* post_data);

char* tracker_itemget(int debug, const char* domain, char* access_token, const char* trackerId, const char* itemId);

char* gallery_filedownload(int debug, const char* domain, char* access_token, const char* fileId, const char* filespath, const char* bodyfilename, const char* headerfilename);

char* gallery_fileupload(int debug, const char* domain, char* access_token, const char* filepath, const char* galId, const char* filename, const char* filetitle, const char* filedesc);

char* gallery_fileupdate(int debug, const char* domain, char* access_token, const char* fileId, const char* filepath, const char* filename, const char* filetitle, const char* filedesc);
