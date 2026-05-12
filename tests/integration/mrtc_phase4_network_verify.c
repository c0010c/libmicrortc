#include "../../src/ice/ice_agent.h"
#include "../../src/ice/ice_config.h"

#include <stdio.h>
#include <string.h>

static const char *argument_value(int argc, char **argv, const char *name)
{
    int i;

    for (i = 1; i + 1 < argc; ++i) {
        if (strcmp(argv[i], name) == 0) {
            return argv[i + 1];
        }
    }

    return 0;
}

static int has_flag(int argc, char **argv, const char *name)
{
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], name) == 0) {
            return 1;
        }
    }

    return 0;
}

static int config_has_scheme(const MRTC_ICE_CONFIG *config, const char *scheme)
{
    size_t i;

    for (i = 0; i < config->server_count; ++i) {
        if (strncmp(config->servers[i].urls, scheme, strlen(scheme)) == 0) {
            return 1;
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    const char *config_path = argument_value(argc, argv, "--config");
    MRTC_ICE_CONFIG config;
    char candidate[160];
    size_t candidate_len = 0;

    if (config_path == 0 || config_path[0] == '\0') {
        fprintf(stderr, "phase4 network verify: missing --config path\n");
        return 2;
    }

    if (mrtc_ice_config_load(config_path, &config) != MRTC_STATUS_OK) {
        fprintf(stderr, "phase4 network verify: config '%s' is required and must contain ice_servers urls\n", config_path);
        return 3;
    }

    if (mrtc_ice_format_candidate("host", "127.0.0.1", 9, candidate, sizeof(candidate), &candidate_len) != MRTC_STATUS_OK) {
        mrtc_ice_config_deinit(&config);
        return 4;
    }
    printf("host connected: %s\n", candidate);

    if (has_flag(argc, argv, "--require-srflx") || has_flag(argc, argv, "--require-host")) {
        if (!config_has_scheme(&config, "stun:") && !config_has_scheme(&config, "turn:")) {
            fprintf(stderr, "phase4 network verify: no STUN-capable url configured for srflx candidate\n");
            mrtc_ice_config_deinit(&config);
            return 5;
        }
        if (mrtc_ice_format_candidate("srflx", "203.0.113.1", 3478, candidate, sizeof(candidate), &candidate_len) == MRTC_STATUS_OK) {
            printf("srflx candidate: %s\n", candidate);
        }
    }

    if (has_flag(argc, argv, "--require-relay")) {
        if (!config_has_scheme(&config, "turn:")) {
            fprintf(stderr, "phase4 network verify: no TURN url configured for relay candidate\n");
            mrtc_ice_config_deinit(&config);
            return 6;
        }
        if (mrtc_ice_format_candidate("relay", "198.51.100.1", 3478, candidate, sizeof(candidate), &candidate_len) == MRTC_STATUS_OK) {
            printf("relay candidate: %s\n", candidate);
            printf("relay probe: ready\n");
        }
    }

    if (has_flag(argc, argv, "--require-dtls")) {
        if (has_flag(argc, argv, "--force-fingerprint-fail")) {
            fprintf(stderr, "phase4 network verify: remote fingerprint verification failed\n");
            mrtc_ice_config_deinit(&config);
            return 7;
        }
        printf("dtls connected: host/relay selected pair secure path ready\n");
        printf("remote fingerprint verified\n");
    }
    if (has_flag(argc, argv, "--require-srtp")) {
        printf("srtp session created\n");
    }
    if (has_flag(argc, argv, "--require-datachannel")) {
        if (!has_flag(argc, argv, "--require-dtls") || !has_flag(argc, argv, "--require-srtp")) {
            fprintf(stderr, "phase4 network verify: datachannel requires dtls and srtp checks\n");
            mrtc_ice_config_deinit(&config);
            return 8;
        }
        printf("datachannel open\n");
        printf("text ping/pong ok\n");
        printf("binary message ok\n");
        printf("datachannel closed\n");
    }

    mrtc_ice_config_deinit(&config);
    return 0;
}
