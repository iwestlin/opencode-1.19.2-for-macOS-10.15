#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef uint16_t UChar;
typedef int UErrorCode;

extern void *unumf_openForSkeletonAndLocale(const UChar *, int32_t, const char *, UErrorCode *);
extern void *unumf_openForSkeletonAndLocaleWithError(const UChar *, int32_t, const char *, void *, UErrorCode *);
extern void *unumf_openResult(UErrorCode *);
extern void unumf_formatDouble(const void *, double, void *, UErrorCode *);
extern int32_t unumf_resultToString(const void *, UChar *, int32_t, UErrorCode *);

static void to16(const char *s, UChar *out, int32_t *len) {
    int32_t i = 0;
    for (; s[i]; i++) out[i] = (UChar)(unsigned char)s[i];
    *len = i;
}

static void run(const char *skeleton, const char *locale) {
    UChar sk[128], buf[128];
    int32_t skl;
    to16(skeleton, sk, &skl);
    UErrorCode ec = 0;
    void *fmt = unumf_openForSkeletonAndLocale(sk, skl, locale, &ec);
    if (!fmt || ec != 0) {
        printf("open skeleton=\"%s\" locale=%s -> NULL or error (ec=%d)\n", skeleton, locale, ec);
        return;
    }
    void *res = unumf_openResult(&ec);
    unumf_formatDouble(fmt, 12345.678, res, &ec);
    int32_t n = unumf_resultToString(res, buf, 128, &ec);
    printf("skeleton=\"%-18s\" locale=%-6s -> ", skeleton, locale);
    for (int32_t i = 0; i < n; i++) putchar(buf[i] < 128 ? (char)buf[i] : '?');
    printf("  (ec=%d)\n", ec);
}

int main(void) {
    const char *skeletons[] = {"", "percent", "currency/USD", "compact-short", "unit/length-meter"};
    for (int i = 0; i < 5; i++) run(skeletons[i], "en-US");
    return 0;
}
