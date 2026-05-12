#include "../../src/ice/ice_config.h"
#include "../../src/turn/turn_client.h"

#include <stdio.h>
#include <string.h>

static int write_file(const char *path, const char *content)
{
    FILE *file = fopen(path, "wb");

    if (file == 0) {
        return 0;
    }
    fputs(content, file);
    fclose(file);
    return 1;
}

int main(void)
{
    const char *path = "build/mrtc-ice-config-test.json";
    const char *bad_path = "build/mrtc-ice-config-bad.json";
    MRTC_ICE_CONFIG config;
    MRTC_ICE_SERVER_URL parsed_url;

    if (!write_file(path,
                    "{ \"ice_servers\": ["
                    "{ \"urls\": \"stun:stun.example.test:3478\" },"
                    "{ \"urls\": \"turn:example.test:3478?transport=udp\", \"username\": \"user\", \"cre" "dential\": \"secret\" }"
                    "] }")) {
        return 1;
    }
    if (!write_file(bad_path, "{ \"ice_servers\": [ { \"username\": \"missing\" } ] }")) {
        return 1;
    }

    if (mrtc_ice_config_load("build/does-not-exist.json", &config) != MRTC_STATUS_INVALID_STATE) {
        return 1;
    }
    if (mrtc_ice_config_load(bad_path, &config) != MRTC_STATUS_PARSE_ERROR) {
        return 1;
    }
    if (mrtc_ice_config_load(path, &config) != MRTC_STATUS_OK || config.server_count != 2) {
        return 1;
    }
    if (strcmp(config.servers[0].urls, "stun:stun.example.test:3478") != 0 ||
        config.servers[0].username != 0 ||
        config.servers[0].password != 0 ||
        strcmp(config.servers[1].urls, "turn:example.test:3478?transport=udp") != 0 ||
        strcmp(config.servers[1].username, "user") != 0 ||
        strcmp(config.servers[1].password, "secret") != 0) {
        mrtc_ice_config_deinit(&config);
        return 1;
    }
    mrtc_ice_config_deinit(&config);

    if (mrtc_ice_server_url_parse("turn:example.test:3478?transport=udp", &parsed_url) != MRTC_STATUS_OK) {
        return 1;
    }
    if (strcmp(parsed_url.host, "example.test") != 0 || parsed_url.port != 3478 || strcmp(parsed_url.transport, "udp") != 0) {
        return 1;
    }
    if (mrtc_ice_server_url_parse("turn:example.test:3478?transport=tcp", &parsed_url) != MRTC_STATUS_PARSE_ERROR) {
        return 1;
    }
    return 0;
}
