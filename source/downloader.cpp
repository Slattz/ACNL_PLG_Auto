#include "downloader.h"
#include "jsmn.h"
#include "httpc.h"
#include <vector>
#include <cstdio>
#include <sys/stat.h>

static char     urlDownload[1024];
static int      urlDownloadSize;
static int      g_downloadnum = 0;
HTTPC httpc;

char Sstrncpy(char *dest, const char *src, size_t n) //'Safe' strncpy, always null terminates
{
    strncpy(dest, src, n);
    return dest[n] = '\0';
}

static int jsoneq(const char *json, jsmntok_t *tok, const char *s)
{
    if (tok->type == JSMN_STRING && (int) strlen(s) == tok->end - tok->start &&
            strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

void str_replace(char *target, const char *needle, const char *replacement)
{
    char buffer[1024] = { 0 };
    char *insert_point = &buffer[0];
    const char *tmp = target;
    size_t needle_len = strlen(needle);
    size_t repl_len = strlen(replacement);

    while (1) {
        const char *p = strstr(tmp, needle);

        // walked past last occurrence of needle; copy remaining part
        if (p == NULL) {
            strcpy(insert_point, tmp);
            break;
        }

        // copy part before needle
        memcpy(insert_point, tmp, p - tmp);
        insert_point += p - tmp;

        // copy replacement string
        memcpy(insert_point, replacement, repl_len);
        insert_point += repl_len;

        // adjust pointers, move on
        tmp = p + needle_len;
    }

    // write altered string back to target
    strcpy(target, buffer);
}

bool http_download(const char *src, u8 **output, u32 *outSize)
{
    Result res = 0;
    httpcContext context;
    u32 responseCode = 0;
    u32 size=0;
    u8 *buffer;
    char url[1024] = {0};
    bool resolved = false;
    bool ret = false;

    strncpy(url, src, 1023);
    while(R_SUCCEEDED(res) && !resolved)
    {
        if (R_SUCCEEDED(res = httpc.OpenContext(&context, HTTPC_METHOD_GET, url, 1)))
        {
            httpc.SetSSLOpt(&context, SSLCOPT_DisableVerify);
            httpc.AddRequestHeaderField(&context, (char*)"User-Agent", (char*)"Setup-PLG");
            httpc.AddRequestHeaderField(&context, (char*)"Connection", (char*)"Keep-Alive");
            httpc.BeginRequest(&context); //Crashes here
            res = httpc.GetResponseStatusCode(&context, &responseCode);

            if (responseCode >= 301 && responseCode <= 303)
            {
                //printf("redirecting URL\n");
                memset(url, '\0', strlen(url));
                if (R_SUCCEEDED(res = httpc.GetResponseHeader(&context, (char*)"Location", url, 1024)))
                    httpc.CloseContext(&context);
            }

            else
            {
                resolved = true;

                if (responseCode != 200)
                {
                    printf("URL returned status: %ld\n", responseCode);
                    httpc.CloseContext(&context);
                    return false;       
                }
            }
        }

        else printf("Could not open HTTP context!\n");
    }

    if (R_SUCCEEDED(res = httpc.GetDownloadSizeState(&context, NULL, &size)))
    {
        buffer = (u8*)calloc(1, size);
        if (buffer != NULL)
        {
            if (R_SUCCEEDED(res = httpc.ReceiveData(&context, (u8*)buffer, size)))
            {
                *output = buffer;
                *outSize = size;
            }
            ret = true;
        }

        else
            ret = false;
    }

    else
        ret = false;

    return ret;
}

bool Grabber(const char *url, const char *filename)
{
    char            *json = NULL;
    u32             size = 0;
    Result          res;
    bool            ret = false;
    jsmn_parser     jParser;
    jsmntok_t       tokens[500];
    jsmntok_t       obj_key;
    jsmntok_t       obj_val;
    jsmntok_t       assetName;
    jsmntok_t       assetUrl;
    int             i; //Loop
    int             j; //Loop 2
    int             tok = 0;
    int             tokoffset = 0;
    int             objnum = 0;
    int             objoffset = 0;
    int             assetNameLoopVal = -1;
    int             assetUrlLoopVal = -2; //Start value different to assetNameLoopVal

    if (http_download(url, (u8 **)&json, &size))
    {
        jsmn_init(&jParser);
        res = jsmn_parse(&jParser, json, size, tokens, sizeof(tokens)/sizeof(tokens[0]));

        /* Check if the parse was a success */
        if (res < 0) 
        {
            printf("Failed to parse JSON: %ld\n", res);
            return 0;
        }

        /* Assume the top-level element is an object */
        if (res < 1 || tokens[0].type != JSMN_OBJECT)
        {
            printf("Object expected\n");
            return 0;
        }

        /* Loop over all keys of the root object */
        for (i = 1; i < res; i++)
        {
            jsmntok_t json_key = tokens[i];
            jsmntok_t json_value = tokens[i+1];

            if (jsoneq(json, &json_key, "assets") == 0 && json_value.type == JSMN_ARRAY)
            {
                u32 tempsize = json_value.size; //store size of assets so it can be added to i later
                for (j = 0; j < json_value.size; j++) //Loop for amount of assets found
                {
                    /*
                    i = position in orig loop
                    2 = to bring to start of first object of array
                    j = which array
                    tokoffset = calculated to get to start of next array
                    */
                    tok = i+2+j+tokoffset;
                    jsmntok_t val = tokens[tok]; //Use tok to determine the token in the json
                    if (val.type == JSMN_OBJECT) //If an object is found
                    {
                        /* Increment tokoffset by how big the obj is * 2 to include keys & values */
                        tokoffset += val.size*2;
                        for (int k = 1; k < (val.size*2)+1; k++) //start at 1 to skip original object
                        {
                            objnum = tok+k+objoffset;
                            obj_key = tokens[objnum]; //object keys
                            obj_val = tokens[objnum+1]; //object values

                            /* skip uploader object */
                            if (jsoneq(json, &obj_key, "uploader") == 0 && obj_val.type == JSMN_OBJECT)
                            {
                                objoffset += 1+(obj_val.size*2); //+1 to get to proper end of object
                                tokoffset += (obj_val.size*2);
                            }

                            if (jsoneq(json, &obj_key, "name") == 0 && obj_val.type == JSMN_STRING)
                            {
                                assetName = obj_val;
                                assetNameLoopVal = j; //Ensure name is from same asset as url
                            }

                            else if (jsoneq(json, &obj_key, "browser_download_url") == 0 && obj_val.type == JSMN_STRING)
                            {
                                assetUrl = obj_val;
                                assetUrlLoopVal = j; //Ensure url is from same asset as name
                            }

                            if (jsoneq(json, &assetName, filename) == 0
                                && assetUrl.type == JSMN_STRING
                                && assetNameLoopVal == assetUrlLoopVal)
                            {
                                urlDownloadSize = assetUrl.end - assetUrl.start;
                                Sstrncpy(urlDownload, json + assetUrl.start, urlDownloadSize);
                                ret = true;
                            }
                        }
                        objoffset = 0; //Reset objoffset for the next object
                    }
                }
                i += tempsize; //Add size of assets array to loop to skip it
            }
        }
    }
    return ret;
}

bool Installer(const char *url, const char *filename)
{
    u32 downloadsize = 0;
    u8* downloadbuf;
    u32 bytesWritten;
    Handle ciaHandle;
    Result res;


    if (Grabber(url, filename)) {
        printf("Downloading app...\n");
        if (http_download(urlDownload, &downloadbuf, &downloadsize)) 
        {
            g_downloadnum++;
            printf("[Download %d] - App Download Complete!\n", g_downloadnum);
    
            if(R_SUCCEEDED(res = AM_StartCiaInstall(MEDIATYPE_SD, &ciaHandle)))
            {
                FSFILE_Write(ciaHandle, &bytesWritten, 0, downloadbuf, downloadsize, 0);
                if (R_SUCCEEDED(res = AM_FinishCiaInstall(ciaHandle)))
                {
                    printf("App Installed!\n");
                    return true;
                }

                else
                {
                    AM_CancelCIAInstall(ciaHandle);
                    printf("App Install Failed...\n");
                    return false;
                }
            }

            else
            {
                printf("App install failed (AM_StartCiaInstall): %d\n", (int)res);
                return false;
            }
    
            delete[] downloadbuf;
        }
    }
    return false;
}

bool PLGDownloader(const char *url, const char *filename)
{
    using StringVector = std::vector<std::string>;
    u32 correct = 0;
    u32 downloadsize = 0;
    u8* downloadbuf;
    u32 bytesWritten;

    StringVector path1 = {"luma/plugins", "plugin"};
    StringVector path2 = {"/0004000000086400/", "/0004000000086300/", "/0004000000086200/", 
                    "/0004000000198f00/", "/0004000000198e00/", "/0004000000198d00/"};
    StringVector names = {"plugin.plg", filename};

    if (Grabber(url, filename)) {
        printf("Downloading plugin...\n");
        if (http_download(urlDownload, &downloadbuf, &downloadsize)) 
        {
            g_downloadnum++;
            printf("[Download %d] - Plugin Download Complete!\n", g_downloadnum);
    
            for (int i = 0; i < 2; i++)
            {
                for (int j = 0; j < 6; ++j)
                {
                    std::string path = "sdmc:/" + path1[i];
                    mkdir(path.c_str(), 777);
                    path += path2[j];
                    mkdir(path.c_str(), 777);

                    FILE* File;
                    File = fopen((path+names[i]).c_str(), "w+b");
                    bytesWritten = fwrite(downloadbuf, 1, downloadsize, File);
                    if (downloadsize == bytesWritten)
                        correct++;
                    fclose(File);
                }
            }
            delete[] downloadbuf;
        }
    }

    if (correct == 12)
    {
        printf("Plugin Installed!\n");
        return true;
    }

    return false;
}