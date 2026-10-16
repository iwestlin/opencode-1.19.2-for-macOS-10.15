// Exercises the opencode ICU/libSystem shim directly.
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

typedef uint16_t UChar;
typedef int UErrorCode;

#define U_ZERO_ERROR 0
#define UBRK_LINE 2
#define UBT_LINE 2

// Provided by the system libicucore (ICU 64.2).
extern const UChar *ufmtval_getString(const void *ufmtval, int32_t *length, UErrorCode *ec);
extern const UChar *ubrk_first(void *bi);
extern void *ubrk_open(int type, const char *locale, const UChar *text, int32_t textLength, UErrorCode *ec);
extern int32_t ulistfmt_format(const void *listfmt, const UChar *const *strings, const int32_t *stringLengths,
                               int32_t stringCount, UChar *result, int32_t resultCapacity, UErrorCode *ec);
extern void *uplrules_open(const char *locale, UErrorCode *ec);
extern int32_t uplrules_select(const void *uplrules, double number, UChar *keyword, int32_t capacity, UErrorCode *ec);

static void show(const char *label, const UChar *s, int32_t len) {
    printf("%-34s", label);
    for (int32_t i = 0; i < len; i++) putchar(s[i] < 128 ? (char)s[i] : '?');
    printf("  (len=%d)\n", len);
}

static UChar *u16(const char *s) {
    static UChar buf[256];
    int32_t i = 0;
    for (; s[i]; i++) buf[i] = (UChar)(unsigned char)s[i];
    buf[i] = 0;
    return buf;
}

int main(void) {
    void *shim = dlopen("./shim.dylib", RTLD_NOW);
    if (!shim) {
        printf("dlopen failed: %s\n", dlerror());
        return 1;
    }

    UErrorCode ec = U_ZERO_ERROR;
    UChar buf[256];

    // ucal_getHostTimeZone -> host tz id
    int32_t (*hostTz)(UChar *, int32_t, UErrorCode *) = dlsym(shim, "ucal_getHostTimeZone");
    ec = U_ZERO_ERROR;
    int32_t n = hostTz(buf, 256, &ec);
    show("ucal_getHostTimeZone", buf, n);
    if (ec != 0) printf("  ec=%d\n", ec);

    // ubrk_clone -> clone of a line break iterator
    void *(*clone)(const void *, UErrorCode *) = dlsym(shim, "ubrk_clone");
    ec = U_ZERO_ERROR;
    UChar text[32];
    const char *msg = "hello world";
    int32_t tl = (int32_t)strlen(msg);
    for (int32_t i = 0; i <= tl; i++) text[i] = (UChar)(unsigned char)msg[i];
    void *bi = ubrk_open(UBRK_LINE, "en", text, tl, &ec);
    UErrorCode ecClone = U_ZERO_ERROR;
    void *bi2 = clone(bi, &ecClone);
    printf("%-34sbi=%p bi2=%p open_ec=%d clone_ec=%d\n", "ubrk_clone", bi, bi2, ec, ecClone);

    // ulistfmt_openForType -> formatted "a, b, and c"
    void *(*listOpen)(const char *, int32_t, int32_t, UErrorCode *) = dlsym(shim, "ulistfmt_openForType");
    ec = U_ZERO_ERROR;
    void *lf = listOpen("en-US", 0, 0, &ec);  // AND, WIDE
    UChar item0[4] = {'a', 0, 0, 0};
    UChar item1[4] = {'b', 0, 0, 0};
    UChar item2[4] = {'c', 0, 0, 0};
    const UChar *items[3] = {item0, item1, item2};
    int32_t lens[3] = {1, 1, 1};
    ec = U_ZERO_ERROR;
    int32_t ln = ulistfmt_format(lf, items, lens, 3, buf, 256, &ec);
    show("ulistfmt_openForType+format", buf, ln);

    // unumrf_* -> range string, obtained through the system ufmtval_getString
    void *(*rangeOpen)(const UChar *, int32_t, int, int, const char *, void *, UErrorCode *) =
        dlsym(shim, "unumrf_openForSkeletonWithCollapseAndIdentityFallback");
    void *(*rangeResult)(UErrorCode *) = dlsym(shim, "unumrf_openResult");
    void (*rangeDouble)(const void *, double, double, void *, UErrorCode *) = dlsym(shim, "unumrf_formatDoubleRange");
    void *(*rangeAsValue)(const void *, UErrorCode *) = dlsym(shim, "unumrf_resultAsValue");
    void (*rangeClose)(void *) = dlsym(shim, "unumrf_close");
    void (*rangeCloseResult)(void *) = dlsym(shim, "unumrf_closeResult");
    ec = U_ZERO_ERROR;
    void *rnf = rangeOpen(NULL, 0, 0, 2, "en-US", NULL, &ec);
    void *rres = rangeResult(&ec);
    rangeDouble(rnf, 3.0, 5.0, rres, &ec);
    int32_t vlen = 0;
    const UChar *val = rangeAsValue(rres, &ec);
    const UChar *str = ufmtval_getString(val, &vlen, &ec);
    if (str) {
        show("unumrf range 3..5 (en-US)", str, vlen);
    } else {
        printf("unumrf range: NULL (ec=%d)\n", ec);
    }

    // uplrules_selectForRange -> plural category for the same range
    int32_t (*selectRange)(const void *, const void *, UChar *, int32_t, UErrorCode *) =
        dlsym(shim, "uplrules_selectForRange");
    ec = U_ZERO_ERROR;
    void *pr = uplrules_open("en-US", &ec);
    ec = U_ZERO_ERROR;
    int32_t kn = selectRange(pr, rres, buf, 256, &ec);
    show("uplrules_selectForRange 3..5", buf, kn);
    if (ec != 0) printf("  ec=%d\n", ec);

    rangeCloseResult(rres);
    rangeClose(rnf);

    return 0;
}
