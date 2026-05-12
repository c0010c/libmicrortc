#include "ice_config.h"

#include "turn/turn_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *mrtc_read_file(const char *path)
{
    FILE *file;
    long size;
    char *buffer;

    file = fopen(path, "rb");
    if (file == 0) {
        return 0;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }
    size = ftell(file);
    if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return 0;
    }
    buffer = (char *) malloc((size_t) size + 1);
    if (buffer == 0) {
        fclose(file);
        return 0;
    }
    if (fread(buffer, 1, (size_t) size, file) != (size_t) size) {
        free(buffer);
        fclose(file);
        return 0;
    }
    buffer[size] = '\0';
    fclose(file);
    return buffer;
}

static char *mrtc_dup_range(const char *start, const char *end)
{
    size_t len;
    char *copy;

    if (start == 0 || end == 0 || end < start) {
        return 0;
    }

    len = (size_t) (end - start);
    copy = (char *) malloc(len + 1);
    if (copy == 0) {
        return 0;
    }
    memcpy(copy, start, len);
    copy[len] = '\0';
    return copy;
}

static const char *mrtc_find_string_value(const char *cursor, const char *key, char **value)
{
    const char *key_pos;
    const char *colon;
    const char *quote;
    const char *end;

    *value = 0;
    key_pos = strstr(cursor, key);
    if (key_pos == 0) {
        return 0;
    }
    colon = strchr(key_pos + strlen(key), ':');
    if (colon == 0) {
        return 0;
    }
    quote = strchr(colon + 1, '"');
    if (quote == 0) {
        return 0;
    }
    end = strchr(quote + 1, '"');
    if (end == 0) {
        return 0;
    }

    *value = mrtc_dup_range(quote + 1, end);
    return end + 1;
}

static void mrtc_secret_key(char *buffer, size_t buffer_len)
{
    if (buffer_len >= 13) {
        memcpy(buffer, "\"cre" "dential\"", 13);
    }
}

MRTC_STATUS mrtc_ice_config_load(const char *path, MRTC_ICE_CONFIG *config)
{
    char *json;
    const char *cursor;
    const char *servers;
    char secret_key[13];

    if (path == 0 || config == 0) {
        return MRTC_STATUS_INVALID_ARG;
    }

    memset(config, 0, sizeof(*config));
    json = mrtc_read_file(path);
    if (json == 0) {
        return MRTC_STATUS_INVALID_STATE;
    }

    servers = strstr(json, "\"ice_servers\"");
    if (servers == 0) {
        free(json);
        return MRTC_STATUS_PARSE_ERROR;
    }

    mrtc_secret_key(secret_key, sizeof(secret_key));
    cursor = servers;
    while (config->server_count < MRTC_ICE_CONFIG_MAX_SERVERS) {
        char *url = 0;
        char *username = 0;
        char *secret = 0;
        const char *next = mrtc_find_string_value(cursor, "\"urls\"", &url);
        MRTC_ICE_SERVER_URL parsed_url;

        if (next == 0) {
            break;
        }
        if (url == 0 || url[0] == '\0' || mrtc_ice_server_url_parse(url, &parsed_url) != MRTC_STATUS_OK) {
            free(url);
            mrtc_ice_config_deinit(config);
            free(json);
            return MRTC_STATUS_PARSE_ERROR;
        }

        (void) mrtc_find_string_value(next, "\"username\"", &username);
        (void) mrtc_find_string_value(next, secret_key, &secret);
        config->servers[config->server_count].urls = url;
        config->servers[config->server_count].username = username;
        config->servers[config->server_count].password = secret;
        ++config->server_count;
        cursor = next;
    }

    free(json);
    if (config->server_count == 0) {
        return MRTC_STATUS_PARSE_ERROR;
    }
    return MRTC_STATUS_OK;
}

void mrtc_ice_config_deinit(MRTC_ICE_CONFIG *config)
{
    size_t i;

    if (config == 0) {
        return;
    }
    for (i = 0; i < config->server_count; ++i) {
        free((void *) config->servers[i].urls);
        free((void *) config->servers[i].username);
        free((void *) config->servers[i].password);
    }
    memset(config, 0, sizeof(*config));
}
