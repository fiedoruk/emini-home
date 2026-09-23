#include "home_fetch.h"
#include "home_parse.h"
#include "home_air.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_crc.h"
#include "miniz.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#ifdef ESP_PLATFORM
#include "esp_log.h"
#endif
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define WIRE_MAX (128U * 1024U)
#define TEXT_MAX (128U * 1024U)
#define HEADER_MAX (16U * 1024U)
#define LINE_MAX_BYTES 8192U /* GitHub Atom sends a ~3.6KiB CSP header. */
#define REQUEST_US INT64_C(25000000)
/* Public source repository is a contact pointer, never a user/device identifier. */
#define HOME_UA "emini-home/0.6 (+https://github.com/fiedoruk/emini-home)"
/* Shortest gap between two questions to the same provider, whatever it says about freshness. */
#define HOME_MIN_POLL_S 1800
typedef struct {
    char host[254], path[768], ip[INET_ADDRSTRLEN];
} endpoint_t;
typedef struct {
    char encoding[32], location[768], etag[129], modified[65], expires[65], cache[1025], retry[65],
        date[65];
    int status;
    bool chunked, has_length, has_age;
    size_t length;
    uint64_t age;
    uint32_t seen;
} headers_t;
typedef struct {
    void *connection;
    ssize_t (*read)(void *, void *, size_t);
    int64_t deadline;
    uint8_t buffer[2048];
    size_t at, used, header_bytes, framing_bytes;
    esp_err_t error;
} reader_t;

static bool public_ip(uint32_t net)
{
    uint32_t a = ntohl(net);
    unsigned x = a >> 24, y = (a >> 16) & 255, z = (a >> 8) & 255;
    if (x == 0 || x == 10 || x == 127 || x >= 224 || x == 255)
        return false;
    if ((x == 172 && y >= 16 && y <= 31) || (x == 192 && y == 168) || (x == 169 && y == 254) ||
        (x == 100 && y >= 64 && y <= 127))
        return false;
    if ((x == 192 && y == 0 && (z == 0 || z == 2)) ||
        (x == 198 && (y == 18 || y == 19 || (y == 51 && z == 100))) ||
        (x == 203 && y == 0 && z == 113))
        return false;
    return true;
}
static bool endpoint(const char *url, endpoint_t *e)
{
    if (!url || strncmp(url, "https://", 8) || strlen(url) > 760)
        return false;
    memset(e, 0, sizeof(*e));
    const char *s = url + 8, *p = s;
    while (*p && *p != '/' && *p != '?')
        p++;
    size_t n = (size_t)(p - s);
    if (n == 0 || n >= sizeof(e->host))
        return false;
    for (size_t i = 0; i < n; i++)
        if (!(isalnum((unsigned char)s[i]) || s[i] == '.' || s[i] == '-'))
            return false;
    memcpy(e->host, s, n);
    for (const unsigned char *q = (const unsigned char *)p; *q; q++)
        if (*q < 33 || *q > 126 || *q == '#' || *q == '\\')
            return false;
    if (!*p)
        strcpy(e->path, "/");
    else if (*p == '?')
        snprintf(e->path, sizeof(e->path), "/%s", p);
    else
        snprintf(e->path, sizeof(e->path), "%s", p);
    struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM}, *answers = NULL;
    if (getaddrinfo(e->host, NULL, &hints, &answers) != 0)
        return false;
    bool ok = false;
    for (struct addrinfo *a = answers; a; a = a->ai_next) {
        struct sockaddr_in *in = (struct sockaddr_in *)a->ai_addr;
        if (!public_ip(in->sin_addr.s_addr)) {
            ok = false;
            break;
        }
        if (!ok) {
            inet_ntop(AF_INET, &in->sin_addr, e->ip, sizeof(e->ip));
            ok = true;
        }
    }
    freeaddrinfo(answers);
    return ok;
}
static bool due(reader_t *r)
{
    if (esp_timer_get_time() >= r->deadline) {
        r->error = ESP_ERR_TIMEOUT;
        return true;
    }
    return false;
}
static bool again(ssize_t n)
{
    return n == ESP_TLS_ERR_SSL_WANT_READ || n == ESP_TLS_ERR_SSL_WANT_WRITE;
}
static ssize_t tls_read(void *t, void *b, size_t n)
{
    return esp_tls_conn_read(t, b, n);
}
/* Single-owner nonblocking socket: timeout is checked even while a peer
 * trickles header bytes or every read returns WANT_READ. No timer closes it. */
static int byte(reader_t *r)
{
    if (due(r))
        return -1;
    while (r->at == r->used) {
        ssize_t got = r->read(r->connection, r->buffer, sizeof r->buffer);
        if (due(r))
            return -1;
        if (again(got)) {
            vTaskDelay(1);
            continue;
        }
        if (got < 0) {
            r->error = ESP_FAIL;
            return -1;
        }
        if (!got)
            return -2;
        if ((size_t)got > sizeof r->buffer) {
            r->error = ESP_FAIL;
            return -1;
        }
        r->at = 0;
        r->used = (size_t)got;
    }
    return r->buffer[r->at++];
}
static bool read_line(reader_t *r, char line[LINE_MAX_BYTES + 1], bool framing)
{
    size_t n = 0;
    bool cr = false;
    while (true) {
        int ch = byte(r);
        if (ch < 0) {
            if (ch == -2)
                r->error = ESP_FAIL;
            return false;
        }
        size_t *budget = framing ? &r->framing_bytes : &r->header_bytes;
        if (++*budget > (framing ? 65536U : HEADER_MAX)) {
            r->error = ESP_ERR_INVALID_SIZE;
            return false;
        }
        if (cr) {
            if (ch != '\n') {
                r->error = ESP_FAIL;
                return false;
            }
            line[n] = 0;
            return true;
        }
        if (ch == '\r') {
            cr = true;
            continue;
        }
        if (ch == '\n' || ch == 0 || n == (framing ? 2048U : LINE_MAX_BYTES)) {
            r->error = ESP_ERR_INVALID_SIZE;
            return false;
        }
        line[n++] = (char)ch;
    }
}
static bool token_char(unsigned c)
{
    return isalnum(c) || strchr("!#$%&'*+-.^_`|~", (int)c) != NULL;
}
static bool decimal(const char *s, uint64_t *out)
{
    if (!*s)
        return false;
    uint64_t n = 0;
    for (; *s; s++) {
        if (*s < '0' || *s > '9')
            return false;
        unsigned d = (unsigned)(*s - '0');
        if (n > (UINT64_MAX - d) / 10)
            return false;
        n = n * 10 + d;
    }
    *out = n;
    return true;
}
static bool split_header(char *line, char **value)
{
    char *colon = strchr(line, ':');
    if (!colon || colon == line)
        return false;
    for (char *p = line; p < colon; p++)
        if (!token_char((unsigned char)*p))
            return false;
    *colon = 0;
    char *v = colon + 1;
    while (*v == ' ' || *v == '\t')
        v++;
    for (unsigned char *p = (unsigned char *)v; *p; p++)
        if ((*p < 32 && *p != '\t') || *p == 127)
            return false;
    char *end = v + strlen(v);
    while (end > v && (end[-1] == ' ' || end[-1] == '\t'))
        *--end = 0;
    *value = v;
    return true;
}
static bool header(headers_t *h, char *line, bool trailer)
{
    char *v;
    if (!split_header(line, &v))
        return false;
    static const char *names[] = {
        "content-encoding",  "location",    "etag",         "last-modified",
        "expires",           "retry-after", "date",         "content-length",
        "transfer-encoding", "age",         "cache-control"};
    int index = -1;
    for (int i = 0; i < 11; i++)
        if (!strcasecmp(line, names[i])) {
            index = i;
            break;
        }
    // Trailer fields cannot alter framing, cache policy, representation or route.
    if (trailer)
        return index < 0 && strcasecmp(line, "host") && strcasecmp(line, "authorization");
    if (index < 0)
        return true;
    if (index == 10) {
        size_t used = strlen(h->cache), n = strlen(v);
        if (used + n + (used ? 1 : 0) >= sizeof h->cache)
            return false;
        if (used)
            strcat(h->cache, ",");
        strcat(h->cache, v);
        return true;
    }
    if (h->seen & (1U << index))
        return false;
    h->seen |= 1U << index;
    if (index == 7) {
        uint64_t n;
        if (!decimal(v, &n) || n > WIRE_MAX)
            return false;
        h->has_length = true;
        h->length = (size_t)n;
        return true;
    }
    if (index == 8) {
        if (strcasecmp(v, "chunked"))
            return false;
        h->chunked = true;
        return true;
    }
    if (index == 9) {
        if (!decimal(v, &h->age))
            return false;
        h->has_age = true;
        return true;
    }
    char *out = NULL;
    size_t cap = 0;
    switch (index) {
    case 0:
        out = h->encoding;
        cap = sizeof h->encoding;
        break;
    case 1:
        out = h->location;
        cap = sizeof h->location;
        break;
    case 2:
        out = h->etag;
        cap = sizeof h->etag;
        break;
    case 3:
        out = h->modified;
        cap = sizeof h->modified;
        break;
    case 4:
        out = h->expires;
        cap = sizeof h->expires;
        break;
    case 5:
        out = h->retry;
        cap = sizeof h->retry;
        break;
    case 6:
        out = h->date;
        cap = sizeof h->date;
        break;
    default:
        return false;
    }
    if (strlen(v) >= cap)
        return false;
    strcpy(out, v);
    return true;
}
static bool read_headers(reader_t *r, headers_t *h, char *line)
{
    for (int interim = 0; interim < 4; interim++) {
        memset(h, 0, sizeof(*h));
        if (!read_line(r, line, false))
            return false;
        size_t n = strlen(line);
        if (n < 12 || (strncmp(line, "HTTP/1.1 ", 9) && strncmp(line, "HTTP/1.0 ", 9)) ||
            line[9] < '1' || line[9] > '5' || !isdigit((unsigned char)line[10]) ||
            !isdigit((unsigned char)line[11]) || (n > 12 && line[12] != ' ')) {
            r->error = ESP_FAIL;
            return false;
        }
        h->status = (line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0';
        unsigned fields = 0;
        while (true) {
            if (!read_line(r, line, false))
                return false;
            if (!*line)
                break;
            if (++fields > 128 || !header(h, line, false)) {
                r->error = ESP_ERR_INVALID_SIZE;
                return false;
            }
        }
        if (h->chunked && h->has_length) {
            r->error = ESP_FAIL;
            return false;
        }
        if (h->status >= 200)
            return true;
        if (h->status == 101 || h->chunked || h->has_length) {
            r->error = ESP_FAIL;
            return false;
        }
    }
    r->error = ESP_FAIL;
    return false;
}
static bool chunk_size(char *line, size_t *out)
{
    char *p = line;
    size_t n = 0;
    unsigned digits = 0;
    while (isxdigit((unsigned char)*p)) {
        unsigned d = *p <= '9' ? *p - '0' : tolower((unsigned char)*p) - 'a' + 10;
        if (++digits > 8 || n > (WIRE_MAX - d) / 16)
            return false;
        n = n * 16 + d;
        p++;
    }
    if (!digits)
        return false;
    while (*p) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p++ != ';')
            return false;
        while (*p == ' ' || *p == '\t')
            p++;
        char *begin = p;
        while (*p && token_char((unsigned char)*p))
            p++;
        if (p == begin)
            return false;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '=') {
            p++;
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '"') {
                p++;
                bool closed = false;
                while (*p) {
                    unsigned ch = (unsigned char)*p++;
                    if (ch == '"') {
                        closed = true;
                        break;
                    }
                    if (ch == '\\') {
                        if (!*p)
                            return false;
                        ch = (unsigned char)*p++;
                    }
                    if (ch < 32 || ch == 127)
                        return false;
                }
                if (!closed)
                    return false;
            } else {
                begin = p;
                while (*p && token_char((unsigned char)*p))
                    p++;
                if (p == begin)
                    return false;
            }
        }
    }
    *out = n;
    return true;
}
static bool copy_bytes(reader_t *r, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int b = byte(r);
        if (b < 0) {
            if (b == -2)
                r->error = ESP_FAIL;
            return false;
        }
        out[i] = (uint8_t)b;
    }
    return true;
}
static bool read_body(reader_t *r, const headers_t *h, uint8_t *out, size_t *size, char *line)
{
    size_t n = 0;
    if (h->chunked) {
        unsigned chunks = 0;
        while (true) {
            size_t chunk;
            if (!read_line(r, line, true) || !chunk_size(line, &chunk) || ++chunks > 16384 ||
                chunk > WIRE_MAX - n) {
                if (r->error == ESP_OK)
                    r->error = ESP_ERR_INVALID_SIZE;
                return false;
            }
            if (!chunk) {
                unsigned trailers = 0;
                while (true) {
                    if (!read_line(r, line, false))
                        return false;
                    if (!*line)
                        break;
                    if (++trailers > 32 || !header((headers_t *)h, line, true)) {
                        r->error = ESP_FAIL;
                        return false;
                    }
                }
                break;
            }
            if (!copy_bytes(r, out + n, chunk))
                return false;
            n += chunk;
            int cr = byte(r), lf = byte(r);
            if (cr != '\r' || lf != '\n') {
                if (r->error == ESP_OK)
                    r->error = ESP_FAIL;
                return false;
            }
        }
    } else if (h->has_length) {
        if (!copy_bytes(r, out, h->length))
            return false;
        n = h->length;
    } else {
        while (true) {
            int b = byte(r);
            if (b == -2)
                break;
            if (b < 0)
                return false;
            if (n == WIRE_MAX) {
                r->error = ESP_ERR_INVALID_SIZE;
                return false;
            }
            out[n++] = (uint8_t)b;
        }
    }
    *size = n;
    return true;
}
static uint32_t little(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static char *inflate_body(const uint8_t *b, size_t n, const char *encoding, size_t *outn)
{
    if (!encoding[0] || !strcasecmp(encoding, "identity")) {
        if (n > TEXT_MAX)
            return NULL;
        char *out = malloc(n + 1);
        if (!out)
            return NULL;
        memcpy(out, b, n);
        out[n] = 0;
        *outn = n;
        return out;
    }
    bool gzip = !strcasecmp(encoding, "gzip"), zlib = !strcasecmp(encoding, "deflate");
    if (!gzip && !zlib)
        return NULL;
    size_t start = 0, end = n;
    if (gzip) {
        if (n < 18 || b[0] != 31 || b[1] != 139 || b[2] != 8 || (b[3] & 0xe0))
            return NULL;
        start = 10;
        if (b[3] & 4) {
            if (start + 2 > n - 8)
                return NULL;
            size_t extra = b[start] | ((size_t)b[start + 1] << 8);
            start += 2;
            if (extra > n - 8 - start)
                return NULL;
            start += extra;
        }
        if (b[3] & 8) {
            while (start < n - 8 && b[start])
                start++;
            if (start >= n - 8)
                return NULL;
            start++;
        }
        if (b[3] & 16) {
            while (start < n - 8 && b[start])
                start++;
            if (start >= n - 8)
                return NULL;
            start++;
        }
        if (b[3] & 2) {
            if (start + 2 > n - 8)
                return NULL;
            uint32_t crc = esp_crc32_le(0, b, start);
            if ((crc & 65535) != (unsigned)(b[start] | (b[start + 1] << 8)))
                return NULL;
            start += 2;
        }
        end = n - 8;
        if (start >= end || little(b + n - 4) > TEXT_MAX)
            return NULL;
    }
    char *out = malloc(TEXT_MAX + 1);
    if (!out)
        return NULL;
    size_t length = tinfl_decompress_mem_to_mem(out, TEXT_MAX, b + start, end - start,
                                                zlib ? TINFL_FLAG_PARSE_ZLIB_HEADER : 0);
    if (length == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED || length > TEXT_MAX ||
        (gzip && (little(b + n - 4) != length ||
                  little(b + n - 8) != esp_crc32_le(0, (uint8_t *)out, length)))) {
        free(out);
        return NULL;
    }
    out[length] = 0;
    *outn = length;
    return out;
}
static esp_err_t fetch_scoped(const char *initial, const home_source_meta_t *old, headers_t *h,
                              char **text, size_t *size, const char *host_lock)
{
    char url[768];
    if (snprintf(url, sizeof url, "%s", initial) >= (int)sizeof url)
        return ESP_ERR_INVALID_SIZE;
    *text = NULL;
    *size = 0;
    uint8_t *wire = malloc(WIRE_MAX);
    reader_t *r = calloc(1, sizeof(*r));
    char *line = malloc(LINE_MAX_BYTES + 1), *request = malloc(2048);
    if (!wire || !r || !line || !request) {
        free(wire);
        free(r);
        free(line);
        free(request);
        return ESP_ERR_NO_MEM;
    }
    int64_t started = esp_timer_get_time();
    int64_t deadline = started + REQUEST_US;
    const char *stage = "resolve";
    esp_err_t result = ESP_FAIL;
    for (int hop = 0; hop < 4; hop++) {
        stage = "resolve";
        endpoint_t e;
        if (host_lock) {
            size_t n = strlen(host_lock);
            if (strncmp(url, "https://", 8) || strncmp(url + 8, host_lock, n) ||
                (url[8 + n] && url[8 + n] != '/' && url[8 + n] != '?')) {
                result = ESP_ERR_INVALID_ARG;
                break;
            }
        }
        if (!endpoint(url, &e)) {
            result = ESP_ERR_INVALID_ARG;
            break;
        }
        if (host_lock && strcmp(e.host, host_lock)) {
            result = ESP_ERR_INVALID_ARG;
            break;
        }
        if (esp_timer_get_time() >= deadline) {
            result = ESP_ERR_TIMEOUT;
            break;
        }
        memset(h, 0, sizeof(*h));
        stage = "connect";
        esp_tls_t *tls = esp_tls_init();
        if (!tls) {
            result = ESP_ERR_NO_MEM;
            break;
        }
        // Pinned address, original SNI/certificate hostname, bundled CA, no insecure
        // fallback. The caller owns the bounded connect and HTTP I/O budgets.
        // IDF6.0's TLS1.3 NewSessionTicket reader contains an internal retry loop.
        // TLS1.2 keeps every nonblocking read under our deadline-owning caller.
        esp_tls_cfg_t cfg = {.common_name = e.host,
                             .crt_bundle_attach = esp_crt_bundle_attach,
                             .non_block = true,
                             .tls_version = ESP_TLS_VER_TLS_1_2};
        int64_t connect_deadline = esp_timer_get_time() + INT64_C(8000000);
        if (connect_deadline > deadline)
            connect_deadline = deadline;
        /* IDF6.0 CONNECTING reuses fd_sets after select(); a short timeout clears
         * them. Spend the bounded connect budget in that select instead of
         * retrying an emptied set. TLS reads remain nonblocking. */
        cfg.timeout_ms = (int)((connect_deadline - esp_timer_get_time()) / 1000);
        if (cfg.timeout_ms < 1)
            cfg.timeout_ms = 1;
        int connected = 0;
        while (esp_timer_get_time() < connect_deadline) {
            connected = esp_tls_conn_new_async(e.ip, (int)strlen(e.ip), 443, &cfg, tls);
            if (connected)
                break;
            vTaskDelay(1);
        }
        if (connected != 1 || esp_timer_get_time() >= connect_deadline) {
            result = connected < 0 ? ESP_FAIL : ESP_ERR_TIMEOUT;
            esp_tls_conn_destroy(tls);
            break;
        }
        // Validators only belong to the original resource. Redirects are fetched
        // unconditionally, avoiding accidental validation of a different origin/path.
        bool conditional = hop == 0 && old->valid && !old->no_store;
        const char *etag = conditional ? old->etag : "",
                   *modified = conditional ? old->last_modified : "";
        if (strpbrk(etag, "\r\n") || strpbrk(modified, "\r\n")) {
            result = ESP_ERR_INVALID_ARG;
            esp_tls_conn_destroy(tls);
            break;
        }
        int length = snprintf(
            request, 2048,
            "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: %s\r\nAccept: application/json, "
            "application/atom+xml, application/rss+xml, application/xml, "
            "text/xml\r\nAccept-Encoding: gzip, deflate\r\nConnection: close\r\n%s%s%s%s%s%s\r\n",
            e.path, e.host, HOME_UA, *etag ? "If-None-Match: " : "", etag, *etag ? "\r\n" : "",
            *modified ? "If-Modified-Since: " : "", modified, *modified ? "\r\n" : "");
        if (length < 0 || length >= 2048) {
            result = ESP_ERR_INVALID_SIZE;
            esp_tls_conn_destroy(tls);
            break;
        }
        size_t sent = 0;
        stage = "write";
        result = ESP_OK;
        while (sent < (size_t)length) {
            if (esp_timer_get_time() >= deadline) {
                result = ESP_ERR_TIMEOUT;
                break;
            }
            ssize_t n = esp_tls_conn_write(tls, request + sent, (size_t)length - sent);
            if (again(n)) {
                vTaskDelay(1);
                continue;
            }
            if (n <= 0 || (size_t)n > (size_t)length - sent) {
                result = ESP_FAIL;
                break;
            }
            sent += (size_t)n;
        }
        memset(r, 0, sizeof(*r));
        r->connection = tls;
        r->read = tls_read;
        r->deadline = deadline;
        if (result == ESP_OK) {
            stage = "headers";
            if (!read_headers(r, h, line))
                result = r->error;
        }
        if (result != ESP_OK) {
            esp_tls_conn_destroy(tls);
            break;
        }
        if (h->status == 301 || h->status == 302 || h->status == 303 || h->status == 307 ||
            h->status == 308) {
            esp_tls_conn_destroy(tls);
            if (hop == 3 || !h->location[0]) {
                result = ESP_FAIL;
                break;
            }
            if (h->location[0] == '/' && h->location[1] != '/') {
                if (snprintf(url, sizeof(url), "https://%s%s", e.host, h->location) >=
                    (int)sizeof(url)) {
                    result = ESP_ERR_INVALID_SIZE;
                    break;
                }
            } else
                snprintf(url, sizeof(url), "%s", h->location);
            continue;
        }
        if (h->status == 304) {
            esp_tls_conn_destroy(tls);
            result = conditional && (*etag || *modified) ? ESP_OK : ESP_FAIL;
            break;
        }
        if (h->status != 200) {
            esp_tls_conn_destroy(tls);
            result = ESP_FAIL;
            break;
        }
        size_t total = 0;
        stage = "body";
        if (!read_body(r, h, wire, &total, line))
            result = r->error;
        esp_tls_conn_destroy(tls);
        if (result == ESP_OK) {
            stage = "decode";
            *text = inflate_body(wire, total, h->encoding, size);
            if (!*text)
                result = ESP_ERR_INVALID_SIZE;
        }
        break;
    }
#ifdef ESP_PLATFORM
    if (result != ESP_OK)
        ESP_LOGW("home_fetch",
                 "Request failed stage=%s code=%d http=%d elapsed_ms=%lld header_bytes=%u", stage,
                 (int)result, h->status, (long long)((esp_timer_get_time() - started) / 1000),
                 (unsigned)r->header_bytes);
#else
    (void)stage;
#endif
    free(wire);
    free(r);
    free(line);
    free(request);
    return result;
}
static esp_err_t fetch(const char *initial, const home_source_meta_t *old, headers_t *h,
                       char **text, size_t *size)
{
    return fetch_scoped(initial, old, h, text, size, NULL);
}
esp_err_t home_fetch_location(char **json, size_t *size)
{
    if (!json || !size)
        return ESP_ERR_INVALID_ARG;
    home_source_meta_t no_validators = {0};
    headers_t h = {0};
    esp_err_t e = fetch_scoped("https://free.freeipapi.com/api/json", &no_validators, &h, json,
                               size, "free.freeipapi.com");
    if (e == ESP_OK && (h.status != 200 || !*json || *size > 16384))
        e = ESP_ERR_INVALID_SIZE;
    if (e != ESP_OK && *json) {
        memset(*json, 0, *size);
        free(*json);
        *json = NULL;
        *size = 0;
    }
    return e;
}
typedef struct {
    bool no_store, revalidate, has_max, invalid;
    uint64_t max_age;
} policy_t;
static policy_t policy(const char *cache)
{
    policy_t out = {0};
    const char *p = cache;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',')
            p++;
        if (!*p)
            break;
        const char *start = p;
        while (*p && token_char((unsigned char)*p))
            p++;
        size_t n = (size_t)(p - start);
        if (!n) {
            out.invalid = true;
            break;
        }
        char name[64];
        if (n >= sizeof name) {
            out.invalid = true;
            break;
        }
        memcpy(name, start, n);
        name[n] = 0;
        while (*p == ' ' || *p == '\t')
            p++;
        char value[512] = {0};
        size_t v = 0;
        bool has_value = false;
        if (*p == '=') {
            has_value = true;
            p++;
            while (*p == ' ' || *p == '\t')
                p++;
            if (*p == '"') {
                p++;
                bool closed = false;
                while (*p) {
                    char ch = *p++;
                    if (ch == '"') {
                        closed = true;
                        break;
                    }
                    if (ch == '\\') {
                        if (!*p) {
                            out.invalid = true;
                            break;
                        }
                        ch = *p++;
                    }
                    if (v >= sizeof value - 1) {
                        out.invalid = true;
                        break;
                    }
                    value[v++] = ch;
                }
                if (!closed)
                    out.invalid = true;
            } else {
                while (*p && *p != ',' && *p != ' ' && *p != '\t') {
                    if (v >= sizeof value - 1) {
                        out.invalid = true;
                        break;
                    }
                    value[v++] = *p++;
                }
            }
        }
        value[v] = 0;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p && *p != ',')
            out.invalid = true;
        if (!strcasecmp(name, "no-store"))
            out.no_store = true;
        if (!strcasecmp(name, "no-cache"))
            out.revalidate = true;
        if (!strcasecmp(name, "max-age")) {
            uint64_t seconds;
            if (out.has_max || !has_value || !decimal(value, &seconds))
                out.invalid = true;
            else {
                out.has_max = true;
                out.max_age = seconds;
            }
        }
        if (out.invalid)
            break;
        if (*p == ',')
            p++;
    }
    return out;
}
/* Freshness is never lengthened to satisfy our poll floor. max-age precedes
 * Expires; Date/Age reduce the remaining lifetime. No-cache is immediately
 * stale while next_fetch independently avoids hammering the provider. */
static int64_t expiry(const headers_t *h, int64_t now, int64_t fallback, policy_t p)
{
    if (p.invalid || p.revalidate)
        return now;
    int64_t date = home_parse_time(h->date), expires = home_parse_time(h->expires), life = fallback;
    if (date > now + 300)
        return now;
    if (p.has_max)
        life = p.max_age > 86400 ? 86400 : (int64_t)p.max_age;
    else if (h->expires[0])
        life = expires > 0 ? expires - (date > 0 ? date : now) : 0;
    if (life < 0)
        life = 0;
    if (life > 86400)
        life = 86400;
    uint64_t age = h->has_age ? h->age : 0;
    if (date > 0 && now > date && (uint64_t)(now - date) > age)
        age = (uint64_t)(now - date);
    if (age >= (uint64_t)life)
        return now;
    return now + life - (int64_t)age;
}
static void metadata(home_source_meta_t *m, const headers_t *h, int64_t now, bool newdata)
{
    // A 304 without freshness fields inherits the prior *remaining* lifetime,
    // conservatively, rather than inventing a new hour of validity.
    int64_t fallback = newdata ? 3600 : m->expires_at - m->checked_at;
    if (fallback < 0 || fallback > 86400)
        fallback = 0;
    policy_t p = policy(h->cache);
    bool inherited_no_store = !newdata && m->no_store;
    m->valid = true;
    m->checked_at = now;
    if (newdata)
        m->fetched_at = now;
    m->no_store = p.no_store || p.invalid || inherited_no_store;
    m->expires_at = expiry(h, now, fallback, p);
    m->state = m->expires_at > now ? HOME_READY : HOME_STALE;
    /* Never ask a provider more often than every half hour, whatever it declares. Measured on
     * the tested unit: the default news feed declares two seconds of freshness, so a short
     * floor meant a request every few minutes - about fifteen an hour, each one sending the
     * reader's home IP address to the provider. Half an hour also keeps the radio quiet on
     * battery. Weather (about half an hour) sits at this floor and air quality (an hour) above
     * it. A refresh asked for by hand clears next_fetch outright, so the floor never stands
     * between a press and fresh data. */
    int64_t poll = m->expires_at > now + HOME_MIN_POLL_S ? m->expires_at : now + HOME_MIN_POLL_S;
    m->next_fetch = poll + (esp_random() % 180);
    m->error[0] = 0;
    if (newdata) {
        m->etag[0] = 0;
        m->last_modified[0] = 0;
    }
    if (h->etag[0])
        snprintf(m->etag, sizeof(m->etag), "%s", h->etag);
    if (h->modified[0])
        snprintf(m->last_modified, sizeof(m->last_modified), "%s", h->modified);
}
static void failed(home_source_meta_t *m, const headers_t *h, int64_t now, const char *reason)
{
    m->checked_at = now;
    m->state = m->valid ? HOME_STALE : HOME_ERROR;
    snprintf(m->error, sizeof(m->error), "%s", reason);
    int64_t delay = 900;
    if (h->retry[0]) {
        char *end;
        long n = strtol(h->retry, &end, 10);
        if (*end == 0 && n > 0)
            delay = n;
        else {
            int64_t at = home_parse_time(h->retry);
            if (at > now)
                delay = at - now;
        }
    }
    if (delay < 60)
        delay = 60;
    if (delay > 86400)
        delay = 86400;
    m->next_fetch = now + delay + (esp_random() % 60);
}
esp_err_t home_fetch_weather(const home_config_t *c, home_weather_t *w, int64_t now)
{
    if (now < 1704067200 || now < w->meta.next_fetch)
        return ESP_ERR_INVALID_STATE;
    char url[192];
    double lat = round(c->latitude * 10000.0) / 10000.0,
           lon = round(c->longitude * 10000.0) / 10000.0;
    snprintf(url, sizeof(url),
             "https://api.met.no/weatherapi/locationforecast/2.0/compact?lat=%.4f&lon=%.4f", lat,
             lon);
    home_source_meta_t validators = w->meta;
    /* We cache a selected forecast, not the provider's entire series. A 304
     * cannot re-select a later hour from data we no longer hold. Respect the
     * HTTP expiry above, then request the body when this selection is past. */
    if (w->forecast_at < now) {
        validators.etag[0] = 0;
        validators.last_modified[0] = 0;
    }
    headers_t h = {0};
    char *body = NULL;
    size_t size = 0;
    esp_err_t e = fetch(url, &validators, &h, &body, &size);
    if (e == ESP_OK && h.status == 304) {
        if (w->forecast_at < now) {
            failed(&w->meta, &h, now, "Forecast selection expired");
            return ESP_FAIL;
        }
        metadata(&w->meta, &h, now, false);
        return ESP_OK;
    }
    char error[97] = "Weather connection failed";
    home_weather_t candidate;
    if (e == ESP_OK && !home_parse_weather(body, size, &candidate, now, error))
        e = ESP_FAIL;
    free(body);
    if (e == ESP_OK) {
        int64_t issue = candidate.meta.issued_at;
        candidate.meta = w->meta;
        candidate.meta.issued_at = issue;
        metadata(&candidate.meta, &h, now, true);
        *w = candidate;
    } else {
        if (h.status && h.status != 200)
            snprintf(error, sizeof(error), "Weather HTTP %d", h.status);
        failed(&w->meta, &h, now, error);
    }
    return e;
}
esp_err_t home_fetch_feed(const home_config_t *c, home_feed_t *f, int64_t now)
{
    if (!c->feed_url[0] || now < 1704067200 || now < f->meta.next_fetch)
        return ESP_ERR_INVALID_STATE;
    headers_t h = {0};
    char *body = NULL;
    size_t size = 0;
    esp_err_t e = fetch(c->feed_url, &f->meta, &h, &body, &size);
    if (e == ESP_OK && h.status == 304) {
        metadata(&f->meta, &h, now, false);
        return ESP_OK;
    }
    char error[97] = "Feed connection failed";
    home_feed_t candidate;
    if (e == ESP_OK) {
        /* BBC World is ordered by the publisher. A newer brief must not
         * displace its lead story merely because its timestamp is later. */
        bool editorial = !strcmp(c->feed_url, "https://feeds.bbci.co.uk/news/world/rss.xml");
        bool parsed = editorial ? home_parse_feed_first(body, size, &candidate, now, error)
                                : home_parse_feed(body, size, &candidate, now, error);
        if (!parsed)
            e = ESP_FAIL;
    }
    free(body);
    if (e == ESP_OK) {
        int64_t issue = candidate.meta.issued_at;
        candidate.meta = f->meta;
        candidate.meta.issued_at = issue;
        metadata(&candidate.meta, &h, now, true);
        if (!candidate.url[0])
            snprintf(candidate.url, sizeof(candidate.url), "%s", c->feed_url);
        *f = candidate;
    } else {
        if (h.status && h.status != 200)
            snprintf(error, sizeof(error), "Feed HTTP %d", h.status);
        failed(&f->meta, &h, now, error);
    }
    return e;
}

/* Air quality, UV and pollen. The host is locked: a redirect must not be able to
 * carry the coordinates anywhere but Open-Meteo. The body is bounded by the same
 * 128 KiB wire limit as every other source and the parser keeps 24 hours of it. */
esp_err_t home_fetch_air(const home_config_t *c, home_air_t *a, int64_t now)
{
    if (!c->location_ready || now < 1704067200 || now < a->meta.next_fetch)
        return ESP_ERR_INVALID_STATE;
    char url[288];
    double lat = round(c->latitude * 10000.0) / 10000.0,
           lon = round(c->longitude * 10000.0) / 10000.0;
    int length = snprintf(url, sizeof(url),
                          "https://air-quality-api.open-meteo.com/v1/air-quality?latitude=%.4f&"
                          "longitude=%.4f&hourly=pm2_5,pm10,european_aqi,us_aqi,uv_index,"
                          "alder_pollen,birch_pollen,grass_pollen,mugwort_pollen&forecast_days=2&"
                          "timezone=UTC",
                          lat, lon);
    if (length < 0 || length >= (int)sizeof(url))
        return ESP_ERR_INVALID_SIZE;
    home_source_meta_t validators = a->meta;
    /* We keep a selected window, not the provider's whole series. A 304 cannot
     * move that window to a later hour, so once the selected hour has passed we
     * ask for the body again. */
    if (a->forecast_at + 3600 <= now) {
        validators.etag[0] = 0;
        validators.last_modified[0] = 0;
    }
    headers_t h = {0};
    char *body = NULL;
    size_t size = 0;
    esp_err_t e =
        fetch_scoped(url, &validators, &h, &body, &size, "air-quality-api.open-meteo.com");
    if (e == ESP_OK && h.status == 304) {
        if (a->forecast_at + 3600 <= now) {
            failed(&a->meta, &h, now, "Air selection expired");
            return ESP_FAIL;
        }
        metadata(&a->meta, &h, now, false);
        return ESP_OK;
    }
    char error[97] = "Air connection failed";
    home_air_t candidate;
    if (e == ESP_OK && !home_parse_air(body, size, &candidate, now, error))
        e = ESP_FAIL;
    free(body);
    if (e == ESP_OK) {
        int64_t issue = candidate.meta.issued_at;
        candidate.meta = a->meta;
        candidate.meta.issued_at = issue;
        metadata(&candidate.meta, &h, now, true);
        *a = candidate;
    } else {
        if (h.status && h.status != 200)
            snprintf(error, sizeof(error), "Air HTTP %d", h.status);
        failed(&a->meta, &h, now, error);
    }
    return e;
}
