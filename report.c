/*
 * =====================================================================================
 *
 *       Filename:  report.c
 *
 *    Description:  汇报服务器信息
 *
 *        Version:  1.0
 *        Created:  03/01/2013 01:59:11 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  聂汉子 (niehanzi), niehanzi@qiyi.com
 *        Company:  奇艺世纪
 *
 * =====================================================================================
 */
#include "report.h"

typedef struct _bw_t {
    time_t t;
    unsigned long long out_bw;
    unsigned long long in_bw;
} bw_t;

extern config_t *_config;

static hashmap_t *merge_idc_bw(sw_scaner_t *sw_scaner);
static int fill_post_buffer(topology_t *topology, hashmap_t *hashmap, char *post_buffer, int size);
static int save_post_buffer(char *post_buffer, int size);

int report_do(topology_t *topology, sw_scaner_t *sw_scaner, int timeout)
{
    CURL           *curl                  = NULL;
    CURLcode        result                = -1;
    char           *post_buffer           = NULL;
    hashmap_t      *hashmap               = NULL;
    int             length;

    curl = curl_easy_init();
    if (NULL == curl) {
        log_error("get curl handle failed.");
        return 0;
    }

    post_buffer = (char *)malloc(1024 * 1024);
    if (NULL == post_buffer) {
        log_error("malloc for post_buffer failed.");
        curl_easy_cleanup(curl);
        return 0;
    }
    bzero(post_buffer, 1024 * 1024);

    hashmap = merge_idc_bw(sw_scaner);
    if (NULL == hashmap) {
        log_error("merge idc bw failed.");
        curl_easy_cleanup(curl);
        free(post_buffer);
        return 0;
    }

    length = fill_post_buffer(topology, hashmap, post_buffer, 1024 * 1024);
    if (length < 0) {
        log_error("fill post buffer failed.");
        curl_easy_cleanup(curl);
        free(post_buffer);
        hashmap_delete(hashmap);
        return 0;
    }

    save_post_buffer(post_buffer, length);
    curl_easy_setopt(curl, CURLOPT_URL, _config->status_report_url);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout/1000);
    curl_easy_setopt(curl, CURLOPT_POST, 8);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_buffer);

    result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        log_error("report to: %s failed, %s.", _config->status_report_url, curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        free(post_buffer);
        hashmap_delete(hashmap);
        return 0;
    }

    free(post_buffer);
    hashmap_delete(hashmap);
    curl_easy_cleanup(curl);
    return 1;
}

static hashmap_t *merge_idc_bw(sw_scaner_t *sw_scaner)
{
    int i = 0;
    hashmap_t *hashmap = NULL;
    scan_session_t *session = NULL;
    
    hashmap = hashmap_new(1024, 0, 1, free);
    if (NULL == hashmap) {
        log_error("hashmap_new faied.");
        return NULL;
    }
    
    for (i = 0; i < sw_scaner->session_count; i++) {
        bw_t *bw       = NULL;
        char *idc_name = NULL;
        
        session = &sw_scaner->sessions[i];
        idc_name = session->sw_if->idc->name;
        bw = hashmap_find(hashmap, idc_name);
        if (NULL == bw) {
            bw = (bw_t *)malloc(sizeof(bw_t));
            if (NULL == bw) {
                log_error("malloc for bw failed.");
                return NULL;
            }
            bzero(bw, sizeof(bw_t));
            hashmap_add(hashmap, idc_name, bw);
        }
        if (session->update_time.tv_sec > bw->t) {
            bw->t = session->update_time.tv_sec;
        }
        bw->out_bw += session->if_out_bw;
        bw->in_bw  += session->if_in_bw;
    }

    return hashmap;
}

static int fill_post_buffer(topology_t *topology, hashmap_t *hashmap, char *post_buffer, int size)
{
    int    i         = 0;
    char  *idc_name  = NULL;
    int    offset    = 0;
    int    length    = 0;
    size_t else_size = size - 1;

    for (i = 0; i < topology->idc_count; i++) {
        char buffer[256] = {0}; 
        char *fmt = NULL;
        bw_t *bw  = NULL;
        
        if (1 == topology->idc_count) {
            fmt = "idc=[{\"name\":\"%s\",\"t\":%u,\"out_bw\":%llu,\"in_bw\":%llu}]";
        }
        else if (0 == i) {
            fmt = "idc=[{\"name\":\"%s\",\"t\":%u,\"out_bw\":%llu,\"in_bw\":%llu}";
        }
        else if (i == topology->idc_count - 1) {
            fmt = ",{\"name\":\"%s\",\"t\":%u,\"out_bw\":%llu,\"in_bw\":%llu}]";
        }
        else {
            fmt = ",{\"name\":\"%s\",\"t\":%u,\"out_bw\":%llu,\"in_bw\":%llu}";
        }

        idc_name = topology->idcs[i].name;
        bw = hashmap_find(hashmap, idc_name);

        if (bw) {
            length = snprintf(buffer, sizeof(buffer), fmt, idc_name, bw->t, bw->out_bw, bw->in_bw);
        }
        else {
            length = snprintf(buffer, sizeof(buffer), fmt, idc_name, 0, 0, 0);
        }
        if (length >= (int)sizeof(buffer)) {
            log_error("buffer is too small for bw report buffer, %s.", strerror(errno));
            return -1;
        }
        
        if (length > (int)else_size) {
            log_error("post buffer is too small.");
            return -1;
        }

        memcpy(post_buffer + offset, buffer, length);
        offset    += length;
        else_size -= length;
    }

    //printf("%s\n", post_buffer);
    return offset;
}

static FILE *post_file = NULL;
static int save_post_buffer(char *post_buffer, int size)
{
    if (NULL == _config->post_file) {
        return 1;
    }

    if (NULL == post_file) {
        post_file = fopen(_config->post_file, "a");
        if (post_file == NULL) {
            log_error("fopen %s failed.", _config->post_file);
            return 0;
        }
    }

    fwrite(post_buffer, 1, size, post_file); 
    fputs("\n\n", post_file);
    fflush(post_file);
    return 1;
}
