#include "stdarg.h"
#include "change.h"
#include "mklib.h"
#include "olib.h"
#define PR_ASSERT assert
#define PRUint64 unsigned long int
#define PRInt64 long int
#define PRInt32 int
#define PRInt16 short int
#define PRUint16 unsigned short int
#define PRUint32 unsigned int
#define PRIntn int
#define PRUintn unsigned int
#define size_t int
#define block_ void
#define PRStatus int
#define EOF -1
#define PRFloat64 long double  

#define PR_FAILURE -1
#define PR_SUCCESS 1
#define LL_ZERO 0

long int strtol(const char *nptr,char **endptr,int base);
unsigned long int strtoul(const char *nptr,char **endptr,int base);

/*
 * A function that reads a character from 'stream'.
 * Returns the character read, or EOF if end of stream is reached.
 */
typedef int (*_PRGetCharFN)(void *stream);

/*
 * A function that pushes the character 'ch' back to 'stream'.
 */
typedef void (*_PRUngetCharFN)(void *stream, int ch);

/*
 * The size specifier for the integer and floating point number
 * conversions in format control strings.
 */
typedef enum {
    _PR_size_none,  /* No size specifier is given */
    _PR_size_h,     /* The 'h' specifier, suggesting "short" */
    _PR_size_l,     /* The 'l' specifier, suggesting "long" */
    _PR_size_L,     /* The 'L' specifier, meaning a 'long double' */
    _PR_size_ll     /* The 'll' specifier, suggesting "long long" */
} _PRSizeSpec;

/*
 * The collection of data that is passed between the scan function
 * and its subordinate functions.  The fields of this structure
 * serve as the input or output arguments for these functions.
 */
typedef struct {
    _PRGetCharFN get;        /* get a character from input stream */
    _PRUngetCharFN unget;    /* unget (push back) a character */
    void *stream;            /* argument for get and unget */
    va_list ap;              /* the variable argument list */
    int nChar;               /* number of characters read from 'stream' */

    int assign;           /* assign, or suppress assignment? */
    int width;               /* field width */
    _PRSizeSpec sizeSpec;    /* 'h', 'l', 'L', or 'll' */

    int converted;        /* is the value actually converted? */
} ScanfState;

#define GET(state) ((state)->nChar++, (state)->get((state)->stream))
#define UNGET(state, ch) \
        ((state)->nChar--, (state)->unget((state)->stream, ch))

/*
 * The following two macros, GET_IF_WITHIN_WIDTH and WITHIN_WIDTH,
 * are always used together.
 *
 * GET_IF_WITHIN_WIDTH calls the GET macro and assigns its return
 * value to 'ch' only if we have not exceeded the field width of
 * 'state'.  Therefore, after GET_IF_WITHIN_WIDTH, the value of
 * 'ch' is valid only if the macro WITHIN_WIDTH evaluates to true.
 */

#define GET_IF_WITHIN_WIDTH(state, ch) \
        if (--(state)->width >= 0) { \
            (ch) = GET(state); \
        }
#define WITHIN_WIDTH(state) ((state)->width >= 0)

/*
 * _pr_strtoull:
 *     Convert a string to an unsigned 64-bit integer.  The string
 *     'str' is assumed to be a representation of the integer in
 *     base 'base'.
 *
 * Warning:
 *     - Only handle base 8, 10, and 16.
 *     - No overflow checking.
 */

void *memchr (const char *block, int ch, size_t size) 
{
  assert(block != NULL || size == 0);

  for (; size-- > 0; block++)
    if (*block == ch)
      return (void *) block;

  return NULL;
}

static PRUint64
_pr_strtoull(const char *str, char **endptr, int base)
{
    static const int BASE_MAX = 16;
    static const char digits[] = "0123456789abcdef";
    char *digitPtr;
    PRUint64 x = 0;    /* return value */
    //PRInt64 base64;
    const char *cPtr;
    int negative;
    const char *digitStart;

    PR_ASSERT(base == 0 || base == 8 || base == 10 || base == 16);
    if (base < 0 || base == 1 || base > BASE_MAX) {
        if (endptr) {
            *endptr = (char *) str;
            return LL_ZERO;
        }
    }

    cPtr = str;
    while (isspace(*cPtr)) {
        ++cPtr;
    }

    negative = 0;
    if (*cPtr == '-') {
        negative = 1;
        cPtr++;
    } else if (*cPtr == '+') {
        cPtr++;
    }

    if (base == 16) {
        if (*cPtr == '0' && (cPtr[1] == 'x' || cPtr[1] == 'X')) {
            cPtr += 2;
        }
    } else if (base == 0) {
        if (*cPtr != '0') {
            base = 10;
        } else if (cPtr[1] == 'x' || cPtr[1] == 'X') {
            base = 16;
            cPtr += 2;
        } else {
            base = 8;
        }
    }
    PR_ASSERT(base != 0);
    digitStart = cPtr;

    /* Skip leading zeros */
    while (*cPtr == '0') {
        cPtr++;
    }

    while ((digitPtr = (char*)memchr(digits, tolower(*cPtr), base)) != NULL) {
        cPtr++;
    }

    if (cPtr == digitStart) {
        if (endptr) {
            *endptr = (char *) str;
        }
        return LL_ZERO;
    }

    if (negative) {
#ifdef HAVE_LONG_LONG
        /* The cast to a signed type is to avoid a compiler warning */
        x = -(PRInt64)x;
#endif
    }

    if (endptr) {
        *endptr = (char *) cPtr;
    }
    return x;
}

/*
 * The maximum field width (in number of characters) that is enough
 * (may be more than necessary) to represent a 64-bit integer or
 * floating point number.
 */
#define FMAX 31
#define DECIMAL_POINT '.'

static PRStatus
GetInt(ScanfState *state, int code)
{
    char buf[FMAX + 1], *p;
    int ch;
    static const char digits[] = "0123456789abcdefABCDEF";
    int seenDigit = 0;
    int base;
    int dlen;

    switch (code) {
        case 'd': case 'u':
            base = 10;
            break;
        case 'i':
            base = 0;
            break;
        case 'x': case 'X': case 'p':
            base = 16;
            break;
        case 'o':
            base = 8;
            break;
        default:
            return PR_FAILURE;
    }
    if (state->width == 0 || state->width > FMAX) {
        state->width = FMAX;
    }
    p = buf;
    GET_IF_WITHIN_WIDTH(state, ch);
    if (WITHIN_WIDTH(state) && (ch == '+' || ch == '-')) {
        *p++ = ch;
        GET_IF_WITHIN_WIDTH(state, ch);
    }
    if (WITHIN_WIDTH(state) && ch == '0') {
        seenDigit = 1;
        *p++ = ch;
        GET_IF_WITHIN_WIDTH(state, ch);
        if (WITHIN_WIDTH(state)
                && (ch == 'x' || ch == 'X')
                && (base == 0 || base == 16)) {
            base = 16;
            *p++ = ch;
            GET_IF_WITHIN_WIDTH(state, ch);
        } else if (base == 0) {
            base = 8;
        }
    }
    if (base == 0 || base == 10) {
        dlen = 10;
    } else if (base == 8) {
        dlen = 8;
    } else {
        PR_ASSERT(base == 16);
        dlen = 16 + 6; /* 16 digits, plus 6 in uppercase */
    }
    while (WITHIN_WIDTH(state) && memchr(digits, ch, dlen)) {
        *p++ = ch;
        GET_IF_WITHIN_WIDTH(state, ch);
        seenDigit = 1;
    }
    if (WITHIN_WIDTH(state)) {
        UNGET(state, ch);
    }
    if (!seenDigit) {
        return PR_FAILURE;
    }
    *p = '\0';
    if (state->assign) {
        if (code == 'd' || code == 'i') {
            if (state->sizeSpec == _PR_size_ll) {
                PRInt64 llval = _pr_strtoull(buf, NULL, base);
                *va_arg(state->ap, PRInt64 *) = llval;
            } else {
                long lval = strtol(buf, NULL, base);

                if (state->sizeSpec == _PR_size_none) {
                    *va_arg(state->ap, PRIntn *) = lval;
                } else if (state->sizeSpec == _PR_size_h) {
                    *va_arg(state->ap, PRInt16 *) = (PRInt16)lval;
                } else if (state->sizeSpec == _PR_size_l) {
                    *va_arg(state->ap, PRInt32 *) = lval;
                } else {
                    return PR_FAILURE;
                }
            }
        } else {
            if (state->sizeSpec == _PR_size_ll) {
                PRUint64 llval = _pr_strtoull(buf, NULL, base);
                *va_arg(state->ap, PRUint64 *) = llval;
            } else {
                unsigned long lval = strtoul(buf, NULL, base);

                if (state->sizeSpec == _PR_size_none) {
                    *va_arg(state->ap, PRUintn *) = lval;
                } else if (state->sizeSpec == _PR_size_h) {
                    *va_arg(state->ap, PRUint16 *) = (PRUint16)lval;
                } else if (state->sizeSpec == _PR_size_l) {
                    *va_arg(state->ap, PRUint32 *) = lval;
                } else {
                    return PR_FAILURE;
                }
            }
        }
        state->converted = 1;
    }
    return PR_SUCCESS;
}

static PRStatus
GetFloat(ScanfState *state)
{
    char buf[FMAX + 1], *p;
    int ch;
    int seenDigit = 0;

    if (state->width == 0 || state->width > FMAX) {
        state->width = FMAX;
    }
    p = buf;
    GET_IF_WITHIN_WIDTH(state, ch);
    if (WITHIN_WIDTH(state) && (ch == '+' || ch == '-')) {
        *p++ = ch;
        GET_IF_WITHIN_WIDTH(state, ch);
    }
    while (WITHIN_WIDTH(state) && isdigit(ch)) {
        *p++ = ch;
        GET_IF_WITHIN_WIDTH(state, ch);
        seenDigit = 1;
    }
    if (WITHIN_WIDTH(state) && ch == DECIMAL_POINT) {
        *p++ = ch;
        GET_IF_WITHIN_WIDTH(state, ch);
        while (WITHIN_WIDTH(state) && isdigit(ch)) {
            *p++ = ch;
            GET_IF_WITHIN_WIDTH(state, ch);
            seenDigit = 1;
        }
    }

    /*
     * This is not robust.  For example, "1.2e+" would confuse
     * the code below to read 'e' and '+', only to realize that
     * it should have stopped at "1.2".  But we can't push back
     * more than one character, so there is nothing I can do.
     */

    /* Parse exponent */
    if (WITHIN_WIDTH(state) && (ch == 'e' || ch == 'E') && seenDigit) {
        *p++ = ch;
        GET_IF_WITHIN_WIDTH(state, ch);
        if (WITHIN_WIDTH(state) && (ch == '+' || ch == '-')) {
            *p++ = ch;
            GET_IF_WITHIN_WIDTH(state, ch);
        }
        while (WITHIN_WIDTH(state) && isdigit(ch)) {
            *p++ = ch;
            GET_IF_WITHIN_WIDTH(state, ch);
        }
    }
    if (WITHIN_WIDTH(state)) {
        UNGET(state, ch);
    }
    if (!seenDigit) {
        return PR_FAILURE;
    }
    *p = '\0';
    if (state->assign) {
        PRFloat64 dval = atof(buf);

        state->converted = 1;
        if (state->sizeSpec == _PR_size_l) {
            *va_arg(state->ap, PRFloat64 *) = dval;
        } else if (state->sizeSpec == _PR_size_L) {
#if defined(OSF1) || defined(IRIX)
            *va_arg(state->ap, double *) = dval;
#else
            *va_arg(state->ap, long double *) = dval;
#endif
        } else {
            *va_arg(state->ap, float *) = (float) dval;
        }
    }
    return PR_SUCCESS;
}

/*
 * Convert, and return the end of the conversion spec.
 * Return NULL on error.
 */

static const char *
Convert(ScanfState *state, const char *fmt)
{
    const char *cPtr;
    int ch;
    char *cArg = NULL;

    state->converted = 0;
    cPtr = fmt;
    if (*cPtr != 'c' && *cPtr != 'n' && *cPtr != '[') {
        do {
            ch = GET(state);
        } while (isspace(ch));
        UNGET(state, ch);
    }
    switch (*cPtr) {
        case 'c':
            if (state->assign) {
                cArg = va_arg(state->ap, char *);
            }
            if (state->width == 0) {
                state->width = 1;
            }
            for (; state->width > 0; state->width--) {
                ch = GET(state);
                if (ch == EOF) {
                    return NULL;
                } else if (state->assign) {
                    *cArg++ = ch;
                }
            }
            if (state->assign) {
                state->converted = 1;
            }
            break;
        case 'p':
        case 'd': case 'i': case 'o':
        case 'u': case 'x': case 'X':
            if (GetInt(state, *cPtr) == PR_FAILURE) {
                return NULL;
            }
            break;
        case 'e': case 'E': case 'f':
        case 'g': case 'G':
            if (GetFloat(state) == PR_FAILURE) {
                return NULL;
            }
            break;
        case 'n':
            /* do not consume any input */
            if (state->assign) {
                switch (state->sizeSpec) {
                    case _PR_size_none:
                        *va_arg(state->ap, PRIntn *) = state->nChar;
                        break;
                    case _PR_size_h:
                        *va_arg(state->ap, PRInt16 *) = state->nChar;
                        break;
                    case _PR_size_l:
                        *va_arg(state->ap, PRInt32 *) = state->nChar;
                        break;
                    case _PR_size_ll:
                        break;
                    default:
                        PR_ASSERT(0);
                }
            }
            break;
        case 's':
            if (state->width == 0) {
                state->width = INT_MAX;
            }
            if (state->assign) {
                cArg = va_arg(state->ap, char *);
            }
            for (; state->width > 0; state->width--) {
                ch = GET(state);
                if ((ch == EOF) || isspace(ch)) {
                    UNGET(state, ch);
                    break;
                }
                if (state->assign) {
                    *cArg++ = ch;
                }
            }
            if (state->assign) {
                *cArg = '\0';
                state->converted = 1;
            }
            break;
        case '%':
            ch = GET(state);
            if (ch != '%') {
                UNGET(state, ch);
                return NULL;
            }
            break;
        case '[':
            {
                int complement = 0;
                const char *closeBracket;
                size_t n;

                if (*++cPtr == '^') {
                    complement = 1;
                    cPtr++;
                }
                closeBracket = strchr(*cPtr == ']' ? cPtr + 1 : cPtr, ']');
                if (closeBracket == NULL) {
                    return NULL;
                }
                n = closeBracket - cPtr;
                if (state->width == 0) {
                    state->width = INT_MAX;
                }
                if (state->assign) {
                    cArg = va_arg(state->ap, char *);
                }
                for (; state->width > 0; state->width--) {
                    ch = GET(state);
                    if ((ch == EOF)
                            || (!complement && !memchr(cPtr, ch, n))
                            || (complement && memchr(cPtr, ch, n))) {
                        UNGET(state, ch);
                        break;
                    }
                    if (state->assign) {
                        *cArg++ = ch;
                    }
                }
                if (state->assign) {
                    *cArg = '\0';
                    state->converted = 1;
                }
                cPtr = closeBracket;
            }
            break;
        default:
            return NULL;
    }
    return cPtr;
}

static PRInt32
DoScanf(ScanfState *state, const char *fmt)
{
    PRInt32 nConverted = 0;
    const char *cPtr;
    int ch;

    state->nChar = 0;
    cPtr = fmt;
    while (1) {
        if (isspace(*cPtr)) {
            /* white space: skip */
            do {
                cPtr++;
            } while (isspace(*cPtr));
            do {
                ch = GET(state);
            } while (isspace(ch));
            UNGET(state, ch);
        } else if (*cPtr == '%') {
            /* format spec: convert */
            cPtr++;
            state->assign = 1;
            if (*cPtr == '*') {
                cPtr++;
                state->assign = 0;
            }
            for (state->width = 0; isdigit(*cPtr); cPtr++) {
                state->width = state->width * 10 + *cPtr - '0';
            }
            state->sizeSpec = _PR_size_none;
            if (*cPtr == 'h') {
                cPtr++;
                state->sizeSpec = _PR_size_h;
            } else if (*cPtr == 'l') {
                cPtr++;
                if (*cPtr == 'l') {
                    cPtr++;
                    state->sizeSpec = _PR_size_ll;
                } else {
                    state->sizeSpec = _PR_size_l;
                }
            } else if (*cPtr == 'L') {
                cPtr++;
                state->sizeSpec = _PR_size_L;
            }
            cPtr = Convert(state, cPtr);
            if (cPtr == NULL) {
                return (nConverted > 0 ? nConverted : EOF);
            }
            if (state->converted) {
                nConverted++;
            }
            cPtr++;
        } else {
            /* others: must match */
            if (*cPtr == '\0') {
                return nConverted;
            }
            ch = GET(state);
            if (ch != *cPtr) {
                UNGET(state, ch);
                return nConverted;
            }
            cPtr++;
        }
    }
}

static int
StringGetChar(void *stream)
{
    char *cPtr = *((char **) stream);

    if (*cPtr == '\0') {
        return EOF;
    } else {
        *((char **) stream) = cPtr + 1;
        return *cPtr;
    }
}

static void
StringUngetChar(void *stream, int ch)
{
    char *cPtr = *((char **) stream);

    if (ch != EOF) {
        *((char **) stream) = cPtr - 1;
    }
}

int sscanf(const char *buf, const char *fmt, ...)
{
    PRInt32 rv;
    ScanfState state;

    state.get = &StringGetChar;
    state.unget = &StringUngetChar;
    state.stream = (void *) &buf;
    va_start(state.ap, fmt);
    rv = DoScanf(&state, fmt);
    va_end(state.ap);
    return rv;
}

