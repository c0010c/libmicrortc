#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
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

static int file_contains_required_fields(FILE *file)
{
    char buffer[4096];
    size_t read_len = fread(buffer, 1, sizeof(buffer) - 1, file);

    buffer[read_len] = '\0';
    return strstr(buffer, "\"ice_servers\"") != 0 &&
           strstr(buffer, "\"urls\"") != 0;
}

int main(int argc, char **argv)
{
    const char *config_path = argument_value(argc, argv, "--config");
    FILE *file;

    if (config_path == 0 || config_path[0] == '\0') {
        fprintf(stderr, "phase4 network verify: missing --config path\n");
        return 2;
    }

    file = fopen(config_path, "rb");
    if (file == 0) {
        fprintf(stderr,
                "phase4 network verify: config '%s' is required for host/srflx/relay/dtls/srtp/datachannel checks: %s\n",
                config_path,
                strerror(errno));
        return 3;
    }

    if (!file_contains_required_fields(file)) {
        fclose(file);
        fprintf(stderr, "phase4 network verify: config '%s' must contain ice_servers and urls fields\n", config_path);
        return 4;
    }
    fclose(file);

    printf("host: pending real ICE connectivity\n");
    printf("srflx: pending real STUN candidate\n");
    printf("relay: pending real TURN relay candidate\n");
    printf("dtls: pending OpenSSL handshake\n");
    printf("srtp: pending SRTP session creation\n");
    printf("datachannel: pending SCTP/DCEP path\n");
    return 5;
}
