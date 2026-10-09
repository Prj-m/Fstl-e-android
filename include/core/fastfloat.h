#ifndef FASTFLOAT_H
#define FASTFLOAT_H

#include <QByteArrayView>
#include <QStringView>
#include <cstdint>

// Fast path for the plain decimal numbers model files contain ("-12.5",
// "1.234560e+01"). A mantissa below 2^53 and a power of ten up to 22 are
// both exact doubles, so one multiplication or division gives a correctly
// rounded double (Clinger's fast path). Anything else -- more digits,
// larger exponents, inf/nan, malformed text -- returns false and callers
// fall back to Qt's full parser, which keeps the same accept/reject rules.
namespace FastFloat {

template <typename Char>
inline bool parse(const Char* p, qsizetype size, float& out)
{
    static constexpr double powers[] = {1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,
                                        1e8,  1e9,  1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
                                        1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22};
    const Char* end = p + size;
    auto digit = [](Char c) { return c >= Char('0') && c <= Char('9'); };
    const bool negative = p < end && *p == Char('-');
    if (p < end && (*p == Char('-') || *p == Char('+')))
        ++p;
    uint64_t mantissa = 0;
    int exponent = 0;
    int significant = 0;
    bool any = false;
    auto accumulate = [&](Char c, bool fraction) {
        any = true;
        if (mantissa == 0 && c == Char('0')) {
            exponent -= fraction ? 1 : 0;
            return true;
        }
        if (++significant > 15) // keep the mantissa below 2^53
            return false;
        mantissa = mantissa * 10 + uint64_t(c - Char('0'));
        exponent -= fraction ? 1 : 0;
        return true;
    };
    for (; p < end && digit(*p); ++p)
        if (!accumulate(*p, false))
            return false;
    if (p < end && *p == Char('.'))
        for (++p; p < end && digit(*p); ++p)
            if (!accumulate(*p, true))
                return false;
    if (!any)
        return false;
    if (p < end && (*p == Char('e') || *p == Char('E'))) {
        ++p;
        const bool negativeExponent = p < end && *p == Char('-');
        if (p < end && (*p == Char('-') || *p == Char('+')))
            ++p;
        if (p == end)
            return false;
        int value = 0;
        for (; p < end && digit(*p); ++p) {
            value = value * 10 + int(*p - Char('0'));
            if (value > 400)
                return false;
        }
        exponent += negativeExponent ? -value : value;
    }
    if (p != end)
        return false;
    if (mantissa == 0) {
        out = negative ? -0.0f : 0.0f;
        return true;
    }
    if (exponent < -22 || exponent > 22)
        return false;
    // At most 1e15 * 1e22, well inside float range. Rounding to double and then
    // to float can differ from direct rounding by one ulp in rare halfway cases.
    double value = double(mantissa);
    value = exponent < 0 ? value / powers[-exponent] : value * powers[exponent];
    out = float(negative ? -value : value);
    return true;
}

inline float toFloat(QByteArrayView text, bool* ok)
{
    float value;
    if (parse(text.data(), text.size(), value)) {
        *ok = true;
        return value;
    }
    return text.toFloat(ok);
}

inline float toFloat(QStringView text, bool* ok)
{
    float value;
    if (parse(text.utf16(), text.size(), value)) {
        *ok = true;
        return value;
    }
    return text.toFloat(ok);
}

} // namespace FastFloat

#endif // FASTFLOAT_H
