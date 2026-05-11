#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <openssl/err.h>
#include <openssl/ssl.h>

static unsigned long read_status_kb(const char *key)
{
    FILE *file;
    char line[256];
    size_t key_len;

    file = fopen("/proc/self/status", "r");
    if (file == 0) {
        return 0;
    }

    key_len = strlen(key);
    while (fgets(line, sizeof(line), file) != 0) {
        if (strncmp(line, key, key_len) == 0) {
            char *cursor = line + key_len;
            while (*cursor == ' ' || *cursor == '\t' || *cursor == ':') {
                ++cursor;
            }
            fclose(file);
            return strtoul(cursor, 0, 10);
        }
    }

    fclose(file);
    return 0;
}

static void print_memory(const char *stage)
{
    printf("%s\tVmRSS_kB=%lu\tVmSize_kB=%lu\n", stage,
           read_status_kb("VmRSS"), read_status_kb("VmSize"));
    fflush(stdout);
}

static int parse_hold_seconds(int argc, char **argv)
{
    int i;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--hold-seconds") == 0 && i + 1 < argc) {
            char *end = 0;
            long value;

            errno = 0;
            value = strtol(argv[++i], &end, 10);
            if (errno == 0 && end != argv[i] && *end == '\0' && value >= 0 &&
                value <= 3600) {
                return (int)value;
            }
            return -1;
        }
    }

    return 5;
}

static void hold_process(int seconds)
{
    while (seconds > 0) {
        sleep(1u);
        --seconds;
    }
}

int main(int argc, char **argv)
{
    SSL_CTX *ctx;
    SSL *ssl;
    BIO *read_bio;
    BIO *write_bio;
    int hold_seconds;

    hold_seconds = parse_hold_seconds(argc, argv);
    if (hold_seconds < 0) {
        fprintf(stderr, "usage: %s [--hold-seconds N]\n", argv[0]);
        return 2;
    }

    print_memory("start");

    if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS |
                             OPENSSL_INIT_LOAD_CRYPTO_STRINGS,
                         0) != 1) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    print_memory("openssl_initialized");

    ctx = SSL_CTX_new(DTLS_method());
    if (ctx == 0) {
        ERR_print_errors_fp(stderr);
        return 1;
    }
    SSL_CTX_set_min_proto_version(ctx, DTLS1_2_VERSION);
    SSL_CTX_set_cipher_list(ctx, "ALL");
    print_memory("ctx_created");

    ssl = SSL_new(ctx);
    read_bio = BIO_new(BIO_s_mem());
    write_bio = BIO_new(BIO_s_mem());
    if (ssl == 0 || read_bio == 0 || write_bio == 0) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        BIO_free(read_bio);
        BIO_free(write_bio);
        SSL_CTX_free(ctx);
        return 1;
    }

    SSL_set_bio(ssl, read_bio, write_bio);
    SSL_set_accept_state(ssl);
    print_memory("ssl_session_created");

    hold_process(hold_seconds);
    print_memory("before_cleanup");

    SSL_free(ssl);
    SSL_CTX_free(ctx);
    print_memory("after_cleanup");

    return 0;
}
