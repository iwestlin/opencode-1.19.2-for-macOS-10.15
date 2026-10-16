// Compatibility shim that lets opencode's Bun binary (built for macOS 11+)
// run on macOS 10.15 by providing the small set of libSystem/ICU symbols it
// imports that do not exist in the 10.15 system libraries.
//
// ICU forwarders are compiled against the ICU 64.2 headers (same ABI as the
// system libicucore) and link against the system library.

#include <stdint.h>
#include <sys/uio.h>
#include <errno.h>
#include <unistd.h>
#include <utility>
#include <stdarg.h>
#include <stdio.h>
#include <dlfcn.h>

#include "unicode/utypes.h"
#include "unicode/ucal.h"
#include "unicode/ubrk.h"
#include "unicode/brkiter.h"
#include "unicode/udateintervalformat.h"
#include "unicode/ulistformatter.h"
#include "unicode/listformatter.h"
#include "unicode/uformattedvalue.h"
#include "unicode/unistr.h"
#include "unicode/locid.h"
#include "unicode/fmtable.h"
#include "unicode/stringpiece.h"
#include "unicode/numberformatter.h"
#include "unicode/numberrangeformatter.h"
#include "unicode/upluralrules.h"
#include "unicode/uchar.h"
#include "unicode/unum.h"
#include "unicode/unumberformatter.h"
#include "unicode/unumsys.h"
#include "formattedval_impl.h"

U_NAMESPACE_USE
using namespace icu::number;

// Opaque C types introduced with the ICU 68 number range C API; the binary
// only passes them back to us.
struct UNumberRangeFormatter;
struct UFormattedNumberRange;

// The fluent settings setters below are not exported by the 10.15 system
// libicucore, so provide the definitions here. As members of the settings
// class they can reach the private RangeMacroProps constructor, which also
// propagates the range locale to the inner formatters.
template<typename Derived>
Derived NumberRangeFormatterSettings<Derived>::numberFormatterBoth(
        const UnlocalizedNumberFormatter &formatter) const& {
    number::impl::RangeMacroProps macros = this->fMacros;
    macros.formatter1 = formatter;
    macros.formatter2 = formatter;
    macros.singleFormatter = true;
    return Derived(macros, macros.locale);
}

template<typename Derived>
Derived NumberRangeFormatterSettings<Derived>::numberFormatterBoth(
        UnlocalizedNumberFormatter &&formatter) const& {
    number::impl::RangeMacroProps macros = this->fMacros;
    macros.formatter1 = std::move(formatter);
    macros.formatter2 = macros.formatter1;
    macros.singleFormatter = true;
    return Derived(macros, macros.locale);
}

template<typename Derived>
Derived NumberRangeFormatterSettings<Derived>::collapse(UNumberRangeCollapse collapse) const& {
    number::impl::RangeMacroProps macros = this->fMacros;
    macros.collapse = collapse;
    return Derived(macros, macros.locale);
}

template<typename Derived>
Derived NumberRangeFormatterSettings<Derived>::identityFallback(
        UNumberRangeIdentityFallback identityFallback) const& {
    number::impl::RangeMacroProps macros = this->fMacros;
    macros.identityFallback = identityFallback;
    return Derived(macros, macros.locale);
}

extern "C" {

/* ------------------------------------------------------------------ */
/* macOS 11+ libSystem APIs missing from 10.15                         */
/* ------------------------------------------------------------------ */

extern int __ulock_wait(uint32_t operation, void *addr, uint64_t value, uint32_t timeout_us);

// macOS 11+; flags2 is dropped when falling back to __ulock_wait.
__attribute__((visibility("default")))
int __ulock_wait2(uint32_t operation, void *addr, uint64_t value, uint32_t timeout_us, uint64_t flags2) asm("___ulock_wait2");
int __ulock_wait2(uint32_t operation, void *addr, uint64_t value, uint32_t timeout_us, uint64_t flags2) {
    (void)flags2;
    return __ulock_wait(operation, addr, value, timeout_us);
}

// macOS 11+; conservative approximation: threads are never "exiting".
__attribute__((visibility("default")))
int pthread_self_is_exiting_np(void) {
    return 0;
}

// macOS 11+; rebuilt on top of pread/pwrite.
__attribute__((visibility("default")))
ssize_t my_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset) asm("_preadv$NOCANCEL");
ssize_t my_preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0) continue;
        ssize_t r;
        do {
            r = pread(fd, iov[i].iov_base, iov[i].iov_len, offset + total);
        } while (r < 0 && errno == EINTR);
        if (r < 0) return total > 0 ? total : -1;
        total += r;
        if ((size_t)r < iov[i].iov_len) break;
    }
    return total;
}

__attribute__((visibility("default")))
ssize_t my_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset) asm("_pwritev$NOCANCEL");
ssize_t my_pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset) {
    ssize_t total = 0;
    for (int i = 0; i < iovcnt; i++) {
        if (iov[i].iov_len == 0) continue;
        ssize_t w;
        do {
            w = pwrite(fd, iov[i].iov_base, iov[i].iov_len, offset + total);
        } while (w < 0 && errno == EINTR);
        if (w < 0) return total > 0 ? total : -1;
        total += w;
        if ((size_t)w < iov[i].iov_len) break;
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* ICU APIs newer than the 10.15 system libicucore (ICU 64.2)          */
/* ------------------------------------------------------------------ */

// ICU 65: identical semantics to the deprecated ucal_getDefaultTimeZone.
__attribute__((visibility("default")))
int32_t ucal_getHostTimeZone(UChar *result, int32_t resultCapacity, UErrorCode *ec) {
    return ucal_getDefaultTimeZone(result, resultCapacity, ec);
}

// ICU 69: resolve raw/DST offsets for a local wall-clock time. The calendar's
// millis are treated as an instant; good enough for unambiguous local times.
__attribute__((visibility("default")))
void ucal_getTimeZoneOffsetFromLocal(const UCalendar *cal,
                                     int32_t nonExistingTimeOpt,
                                     int32_t duplicatedTimeOpt,
                                     int32_t *rawOffset,
                                     int32_t *dstOffset,
                                     UErrorCode *status) {
    (void)nonExistingTimeOpt;
    (void)duplicatedTimeOpt;
    if (U_FAILURE(*status)) return;
    UErrorCode localStatus = U_ZERO_ERROR;
    int32_t raw = ucal_get(cal, UCAL_ZONE_OFFSET, &localStatus);
    int32_t dst = ucal_get(cal, UCAL_DST_OFFSET, &localStatus);
    if (U_FAILURE(localStatus)) {
        *status = localStatus;
        return;
    }
    if (rawOffset) *rawOffset = raw;
    if (dstOffset) *dstOffset = dst;
}

// ICU 69: identical to ICU's own implementation on top of BreakIterator::clone().
__attribute__((visibility("default")))
UBreakIterator *ubrk_clone(const UBreakIterator *bi, UErrorCode *status) {
    if (U_FAILURE(*status)) return nullptr;
    BreakIterator *newBI = reinterpret_cast<const BreakIterator *>(bi)->clone();
    if (newBI == nullptr) {
        *status = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    return reinterpret_cast<UBreakIterator *>(newBI);
}

// ICU 67: type/width variant of ulistfmt_open(). Map to the style strings that
// ICU 64.2 understands; short/narrow are only distinct for unit lists.
__attribute__((visibility("default")))
UListFormatter *ulistfmt_openForType(const char *locale,
                                     int32_t type,
                                     int32_t width,
                                     UErrorCode *status) {
    if (U_FAILURE(*status)) return nullptr;
    const char *style =
        (type == 2 && width == 1) ? "unit-short" :
        (type == 2 && width == 2) ? "unit-narrow" :
        (type == 1) ? "or" :
        (type == 2) ? "unit" : "standard";
    ListFormatter *lf = ListFormatter::createInstance(Locale(locale), style, *status);
    if (U_FAILURE(*status)) return nullptr;
    return reinterpret_cast<UListFormatter *>(lf);
}

// ICU 67: calendar-based date interval formatting; forward through millis.
__attribute__((visibility("default")))
void udtitvfmt_formatCalendarToResult(const UDateIntervalFormat *uformatter,
                                      UCalendar *fromCalendar,
                                      UCalendar *toCalendar,
                                      UFormattedDateInterval *uresult,
                                      UErrorCode *status) {
    if (U_FAILURE(*status)) return;
    UErrorCode localStatus = U_ZERO_ERROR;
    UDate fromDate = ucal_getMillis(fromCalendar, &localStatus);
    UDate toDate = ucal_getMillis(toCalendar, &localStatus);
    if (U_FAILURE(localStatus)) {
        *status = localStatus;
        return;
    }
    udtitvfmt_formatToResult(uformatter, uresult, fromDate, toDate, status);
}

/* ------------------------------------------------------------------ */
/* ICU 68 number range formatting C API, implemented over the ICU 64.2 */
/* C++ NumberRangeFormatter.                                           */
/* ------------------------------------------------------------------ */

struct RangeFormatterHolder : public UMemory {
    LocalizedNumberRangeFormatter fFormatter;
};

// First base is icu::UFormattedValueImpl so the system ufmtval_getString() can
// validate the magic and dispatch through fFormattedValue.
struct RangeResultHolder : public UFormattedValueImpl {
    FormattedNumberRange *fRange = nullptr;
    ~RangeResultHolder() { delete fRange; }
};

__attribute__((visibility("default")))
UNumberRangeFormatter *unumrf_openForSkeletonWithCollapseAndIdentityFallback(
        const UChar *skeleton,
        int32_t skeletonLen,
        UNumberRangeCollapse collapse,
        UNumberRangeIdentityFallback identityFallback,
        const char *locale,
        UParseError *perror,
        UErrorCode *ec) {
    if (U_FAILURE(*ec)) return nullptr;
    UParseError localParseError;
    if (perror == nullptr) perror = &localParseError;
    UnicodeString skeletonString(skeletonLen == -1, skeleton, skeletonLen);
    UnlocalizedNumberFormatter numberFormatter =
        NumberFormatter::forSkeleton(skeletonString, *perror, *ec);
    if (U_FAILURE(*ec)) return nullptr;
    LocalizedNumberRangeFormatter formatter = NumberRangeFormatter::withLocale(Locale(locale));
    formatter = formatter.numberFormatterBoth(numberFormatter);
    formatter = formatter.collapse(collapse);
    formatter = formatter.identityFallback(identityFallback);
    auto *holder = new RangeFormatterHolder();
    holder->fFormatter = std::move(formatter);
    return reinterpret_cast<UNumberRangeFormatter *>(holder);
}

__attribute__((visibility("default")))
UFormattedNumberRange *unumrf_openResult(UErrorCode *ec) {
    if (U_FAILURE(*ec)) return nullptr;
    auto *holder = new RangeResultHolder();
    if (holder == nullptr) {
        *ec = U_MEMORY_ALLOCATION_ERROR;
        return nullptr;
    }
    return reinterpret_cast<UFormattedNumberRange *>(holder);
}

__attribute__((visibility("default")))
void unumrf_formatDoubleRange(const UNumberRangeFormatter *uformatter,
                              double first,
                              double second,
                              UFormattedNumberRange *uresult,
                              UErrorCode *ec) {
    if (U_FAILURE(*ec)) return;
    auto *formatter = reinterpret_cast<const RangeFormatterHolder *>(uformatter);
    auto *result = reinterpret_cast<RangeResultHolder *>(uresult);
    Formattable firstValue((double)first);
    Formattable secondValue((double)second);
    FormattedNumberRange range = formatter->fFormatter.formatFormattableRange(firstValue, secondValue, *ec);
    if (U_FAILURE(*ec)) return;
    delete result->fRange;
    result->fRange = new FormattedNumberRange(std::move(range));
    result->fFormattedValue = result->fRange;
}

__attribute__((visibility("default")))
void unumrf_formatDecimalRange(const UNumberRangeFormatter *uformatter,
                               const char *first,
                               int32_t firstLen,
                               const char *second,
                               int32_t secondLen,
                               UFormattedNumberRange *uresult,
                               UErrorCode *ec) {
    if (U_FAILURE(*ec)) return;
    auto *formatter = reinterpret_cast<const RangeFormatterHolder *>(uformatter);
    auto *result = reinterpret_cast<RangeResultHolder *>(uresult);
    Formattable firstValue;
    Formattable secondValue;
    firstValue.setDecimalNumber(StringPiece(first, firstLen), *ec);
    secondValue.setDecimalNumber(StringPiece(second, secondLen), *ec);
    if (U_FAILURE(*ec)) return;
    FormattedNumberRange range = formatter->fFormatter.formatFormattableRange(firstValue, secondValue, *ec);
    if (U_FAILURE(*ec)) return;
    delete result->fRange;
    result->fRange = new FormattedNumberRange(std::move(range));
    result->fFormattedValue = result->fRange;
}

__attribute__((visibility("default")))
const UFormattedValue *unumrf_resultAsValue(const UFormattedNumberRange *uresult, UErrorCode *ec) {
    if (U_FAILURE(*ec)) return nullptr;
    auto *holder = reinterpret_cast<const RangeResultHolder *>(uresult);
    return reinterpret_cast<const UFormattedValue *>(static_cast<const UFormattedValueImpl *>(holder));
}

// ICU 68: plural category for a formatted number range. ICU 64.2 has no range
// data, so approximate using the range's first number.
__attribute__((visibility("default")))
int32_t uplrules_selectForRange(const UPluralRules *uplrules,
                                const UFormattedNumberRange *urange,
                                UChar *keyword,
                                int32_t capacity,
                                UErrorCode *status) {
    if (U_FAILURE(*status)) return 0;
    auto *holder = reinterpret_cast<const RangeResultHolder *>(urange);
    if (holder->fRange == nullptr) {
        *status = U_INVALID_STATE_ERROR;
        return 0;
    }
    UErrorCode localStatus = U_ZERO_ERROR;
    UnicodeString first = holder->fRange->getFirstDecimal(localStatus);
    if (U_FAILURE(localStatus)) {
        *status = localStatus;
        return 0;
    }
    char buffer[64];
    int32_t length = first.extract(0, first.length(), buffer, (int32_t)sizeof(buffer) - 1, US_INV);
    buffer[length] = '\0';
    return uplrules_select(uplrules, strtod(buffer, nullptr), keyword, capacity, status);
}

__attribute__((visibility("default")))
void unumrf_close(UNumberRangeFormatter *uformatter) {
    delete reinterpret_cast<RangeFormatterHolder *>(uformatter);
}

__attribute__((visibility("default")))
void unumrf_closeResult(UFormattedNumberRange *uresult) {
    delete reinterpret_cast<RangeResultHolder *>(uresult);
}

} // extern "C"

/* ------------------------------------------------------------------ */
/* Number skeleton compatibility                                        */
/*                                                                    */
/* JSC emits ICU 67+ skeleton syntax (e.g. an "at least" integer width) */
/* the 10.15 ICU 64.2 parser rejects with U_NUMBER_SKELETON_SYNTAX_     */
/* ERROR, making every `new Intl.NumberFormat(...)` throw. Rewrite the  */
/* newer tokens to their 64.2 equivalents and retry on failure.         */
/* ------------------------------------------------------------------ */

template<typename Fn>
static Fn next_sym(const char *name) {
    void *p = dlsym(RTLD_NEXT, name);
    if (p == nullptr) p = dlsym(RTLD_DEFAULT, name);
    return reinterpret_cast<Fn>(p);
}

static bool traceEnabled() {
    static const bool enabled = getenv("OPENCODE_SHIM_TRACE") != nullptr;
    return enabled;
}

static void trace(const char *fmt, ...) {
    if (!traceEnabled()) return;
    va_list ap;
    va_start(ap, fmt);
    FILE *f = fopen("/tmp/shim-trace.log", "a");
    if (f != nullptr) {
        vfprintf(f, fmt, ap);
        fputc('\n', f);
        fclose(f);
    }
    va_end(ap);
}

// ICU 64.2 uses `measure-unit/...` where newer ICU also accepts `unit/...`,
// and has no asterisk ("at least") integer-width form.
static UnicodeString sanitizeSkeleton(const UChar *skeleton, int32_t length) {
    UnicodeString in(skeleton, length);
    UnicodeString out;
    bool first = true;
    int32_t i = 0;
    while (i < in.length()) {
        while (i < in.length() && u_isUWhiteSpace(in[i])) i++;
        int32_t start = i;
        while (i < in.length() && !u_isUWhiteSpace(in[i])) i++;
        if (i <= start) continue;
        UnicodeString token = in.tempSubStringBetween(start, i);
        if (token.startsWith("unit/")) {
            token.replace(0, 5, "measure-unit/");
        } else if (token.startsWith("integer-width/")) {
            int32_t star = token.indexOf('*');
            if (star >= 0) token.remove(star, 1);
        }
        if (!first) out.append(' ');
        out.append(token);
        first = false;
    }
    return out;
}

extern "C" {

UNumberFormatter *unumf_openForSkeletonAndLocale(const UChar *skeleton, int32_t skeletonLen,
                                                 const char *locale, UErrorCode *ec) {
    static auto fn = next_sym<UNumberFormatter *(*)(const UChar *, int32_t, const char *, UErrorCode *)>("unumf_openForSkeletonAndLocale");
    if (ec == nullptr || U_FAILURE(*ec)) return nullptr;

    UErrorCode status = U_ZERO_ERROR;
    UNumberFormatter *result = fn(skeleton, skeletonLen, locale, &status);
    if (result != nullptr && U_SUCCESS(status)) {
        *ec = status;
        return result;
    }

    UnicodeString sanitized = sanitizeSkeleton(skeleton, skeletonLen);
    trace("unumf sanitized: %s", sanitized.isEmpty() ? "(empty)" : "rewrote skeleton");
    UErrorCode retryStatus = U_ZERO_ERROR;
    UNumberFormatter *retry = fn(sanitized.getBuffer(), sanitized.length(), locale, &retryStatus);
    if (retry != nullptr && U_SUCCESS(retryStatus)) {
        *ec = retryStatus;
        return retry;
    }
    *ec = retryStatus;
    return retry;
}

UNumberFormatter *unumf_openForSkeletonAndLocaleWithError(const UChar *skeleton, int32_t skeletonLen,
                                                          const char *locale, UParseError *perror,
                                                          UErrorCode *ec) {
    static auto fn = next_sym<UNumberFormatter *(*)(const UChar *, int32_t, const char *, UParseError *, UErrorCode *)>("unumf_openForSkeletonAndLocaleWithError");
    if (ec == nullptr || U_FAILURE(*ec)) return nullptr;

    UErrorCode status = U_ZERO_ERROR;
    UNumberFormatter *result = fn(skeleton, skeletonLen, locale, perror, &status);
    if (result != nullptr && U_SUCCESS(status)) {
        *ec = status;
        return result;
    }

    UnicodeString sanitized = sanitizeSkeleton(skeleton, skeletonLen);
    UErrorCode retryStatus = U_ZERO_ERROR;
    UNumberFormatter *retry = fn(sanitized.getBuffer(), sanitized.length(), locale, perror, &retryStatus);
    if (retry != nullptr && U_SUCCESS(retryStatus)) {
        *ec = retryStatus;
        return retry;
    }
    *ec = retryStatus;
    return retry;
}

} // extern "C"
