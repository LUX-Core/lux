// Copyright (c) 2012-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The Luxcore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilstrencodings.h"

#include "tinyformat.h"

#include "support/cleanse.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <limits>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

using namespace std;

string SanitizeString(const string& str)
{
    /**
     * safeChars chosen to allow simple messages/URLs/email addresses, but avoid anything
     * even possibly remotely dangerous like & or >
     */
    static string safeChars("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890 .,;_/:?@()");
    string strResult;
    for (std::string::size_type i = 0; i < str.size(); i++) {
        if (safeChars.find(str[i]) != std::string::npos)
            strResult.push_back(str[i]);
    }
    return strResult;
}

const signed char p_util_hexdigit[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
        -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

signed char HexDigit(char c)
{
    return p_util_hexdigit[(unsigned char)c];
}

bool IsHex(const string& str)
{
    for (std::string::const_iterator it(str.begin()); it != str.end(); ++it) {
        if (HexDigit(*it) < 0)
            return false;
    }
    return (str.size() > 0) && (str.size() % 2 == 0);
}

vector<unsigned char> ParseHex(const char* psz)
{
    // convert hex dump to vector
    vector<unsigned char> vch;
    while (true) {
        while (isspace(*psz))
            psz++;
        signed char c = HexDigit(*psz++);
        if (c == (signed char)-1)
            break;
        unsigned char n = (c << 4);
        c = HexDigit(*psz++);
        if (c == (signed char)-1)
            break;
        n |= c;
        vch.push_back(n);
    }
    return vch;
}

vector<unsigned char> ParseHex(const string& str)
{
    return ParseHex(str.c_str());
}

string EncodeBase64(const unsigned char* pch, size_t len)
{
    static const char* pbase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    string strRet = "";
    strRet.reserve((len + 2) / 3 * 4);

    int mode = 0, left = 0;
    const unsigned char* pchEnd = pch + len;

    while (pch < pchEnd) {
        int enc = *(pch++);
        switch (mode) {
        case 0: // we have no bits
            strRet += pbase64[enc >> 2];
            left = (enc & 3) << 4;
            mode = 1;
            break;

        case 1: // we have two bits
            strRet += pbase64[left | (enc >> 4)];
            left = (enc & 15) << 2;
            mode = 2;
            break;

        case 2: // we have four bits
            strRet += pbase64[left | (enc >> 6)];
            strRet += pbase64[enc & 63];
            mode = 0;
            break;
        }
    }

    if (mode) {
        strRet += pbase64[left];
        strRet += '=';
        if (mode == 1)
            strRet += '=';
    }

    return strRet;
}

string EncodeBase64(const string& str)
{
    return EncodeBase64((const unsigned char*)str.c_str(), str.size());
}

vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid)
{
    static const int decode64_table[256] =
        {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1,
            -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
            29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
            49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    if (pfInvalid)
        *pfInvalid = false;

    vector<unsigned char> vchRet;
    vchRet.reserve(strlen(p) * 3 / 4);

    int mode = 0;
    int left = 0;

    while (1) {
        int dec = decode64_table[(unsigned char)*p];
        if (dec == -1) break;
        p++;
        switch (mode) {
        case 0: // we have no bits and get 6
            left = dec;
            mode = 1;
            break;

        case 1: // we have 6 bits and keep 4
            vchRet.push_back((left << 2) | (dec >> 4));
            left = dec & 15;
            mode = 2;
            break;

        case 2: // we have 4 bits and get 6, we keep 2
            vchRet.push_back((left << 4) | (dec >> 2));
            left = dec & 3;
            mode = 3;
            break;

        case 3: // we have 2 bits and get 6
            vchRet.push_back((left << 6) | dec);
            mode = 0;
            break;
        }
    }

    if (pfInvalid)
        switch (mode) {
        case 0: // 4n base64 characters processed: ok
            break;

        case 1: // 4n+1 base64 character processed: impossible
            *pfInvalid = true;
            break;

        case 2: // 4n+2 base64 characters processed: require '=='
            if (left || p[0] != '=' || p[1] != '=' || decode64_table[(unsigned char)p[2]] != -1)
                *pfInvalid = true;
            break;

        case 3: // 4n+3 base64 characters processed: require '='
            if (left || p[0] != '=' || decode64_table[(unsigned char)p[1]] != -1)
                *pfInvalid = true;
            break;
        }

    return vchRet;
}

string DecodeBase64(const string& str)
{
    vector<unsigned char> vchRet = DecodeBase64(str.c_str());
    return (vchRet.size() == 0) ? string() : string((const char*)&vchRet[0], vchRet.size());
}

// Base64 decoding with secure memory allocation
SecureString DecodeBase64Secure(const SecureString& input)
{
    SecureString output;

    // Init openssl BIO with base64 filter and memory input
    BIO *b64, *mem;
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); //Do not use newlines to flush buffer
    mem = BIO_new_mem_buf((void*)&input[0], input.size());
    BIO_push(b64, mem);

    // Prepare buffer to receive decoded data
    if (input.size() % 4 != 0) {
        throw runtime_error("Input length should be a multiple of 4");
    }
    size_t nMaxLen = input.size() / 4 * 3; // upper bound, guaranteed divisible by 4
    output.resize(nMaxLen);

    // Decode the string
    size_t nLen;
    nLen = BIO_read(b64, (void*)&output[0], input.size());
    output.resize(nLen);

    // Free memory
    BIO_free_all(b64);
    return output;
}

// Base64 encoding with secure memory allocation
SecureString EncodeBase64Secure(const SecureString& input)
{
    // Init openssl BIO with base64 filter and memory output
    BIO *b64, *mem;
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // No newlines in output
    mem = BIO_new(BIO_s_mem());
    BIO_push(b64, mem);

    // Decode the string
    BIO_write(b64, &input[0], input.size());
    (void)BIO_flush(b64);

    // Create output variable from buffer mem ptr
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    SecureString output(bptr->data, bptr->length);

    // Cleanse secure data buffer from memory
    memory_cleanse((void*)bptr->data, bptr->length);

    // Free memory
    BIO_free_all(b64);
    return output;
}

string EncodeBase32(const unsigned char* pch, size_t len)
{
    static const char* pbase32 = "abcdefghijklmnopqrstuvwxyz234567";

    string strRet = "";
    strRet.reserve((len + 4) / 5 * 8);

    int mode = 0, left = 0;
    const unsigned char* pchEnd = pch + len;

    while (pch < pchEnd) {
        int enc = *(pch++);
        switch (mode) {
        case 0: // we have no bits
            strRet += pbase32[enc >> 3];
            left = (enc & 7) << 2;
            mode = 1;
            break;

        case 1: // we have three bits
            strRet += pbase32[left | (enc >> 6)];
            strRet += pbase32[(enc >> 1) & 31];
            left = (enc & 1) << 4;
            mode = 2;
            break;

        case 2: // we have one bit
            strRet += pbase32[left | (enc >> 4)];
            left = (enc & 15) << 1;
            mode = 3;
            break;

        case 3: // we have four bits
            strRet += pbase32[left | (enc >> 7)];
            strRet += pbase32[(enc >> 2) & 31];
            left = (enc & 3) << 3;
            mode = 4;
            break;

        case 4: // we have two bits
            strRet += pbase32[left | (enc >> 5)];
            strRet += pbase32[enc & 31];
            mode = 0;
        }
    }

    static const int nPadding[5] = {0, 6, 4, 3, 1};
    if (mode) {
        strRet += pbase32[left];
        for (int n = 0; n < nPadding[mode]; n++)
            strRet += '=';
    }

    return strRet;
}

string EncodeBase32(const string& str)
{
    return EncodeBase32((const unsigned char*)str.c_str(), str.size());
}

vector<unsigned char> DecodeBase32(const char* p, bool* pfInvalid)
{
    static const int decode32_table[256] =
        {
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 0, 1, 2,
            3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
            23, 24, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

    if (pfInvalid)
        *pfInvalid = false;

    vector<unsigned char> vchRet;
    vchRet.reserve((strlen(p)) * 5 / 8);

    int mode = 0;
    int left = 0;

    while (1) {
        int dec = decode32_table[(unsigned char)*p];
        if (dec == -1) break;
        p++;
        switch (mode) {
        case 0: // we have no bits and get 5
            left = dec;
            mode = 1;
            break;

        case 1: // we have 5 bits and keep 2
            vchRet.push_back((left << 3) | (dec >> 2));
            left = dec & 3;
            mode = 2;
            break;

        case 2: // we have 2 bits and keep 7
            left = left << 5 | dec;
            mode = 3;
            break;

        case 3: // we have 7 bits and keep 4
            vchRet.push_back((left << 1) | (dec >> 4));
            left = dec & 15;
            mode = 4;
            break;

        case 4: // we have 4 bits, and keep 1
            vchRet.push_back((left << 4) | (dec >> 1));
            left = dec & 1;
            mode = 5;
            break;

        case 5: // we have 1 bit, and keep 6
            left = left << 5 | dec;
            mode = 6;
            break;

        case 6: // we have 6 bits, and keep 3
            vchRet.push_back((left << 2) | (dec >> 3));
            left = dec & 7;
            mode = 7;
            break;

        case 7: // we have 3 bits, and keep 0
            vchRet.push_back((left << 5) | dec);
            mode = 0;
            break;
        }
    }

    if (pfInvalid)
        switch (mode) {
        case 0: // 8n base32 characters processed: ok
            break;

        case 1: // 8n+1 base32 characters processed: impossible
        case 3: //   +3
        case 6: //   +6
            *pfInvalid = true;
            break;

        case 2: // 8n+2 base32 characters processed: require '======'
            if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || p[3] != '=' || p[4] != '=' || p[5] != '=' || decode32_table[(unsigned char)p[6]] != -1)
                *pfInvalid = true;
            break;

        case 4: // 8n+4 base32 characters processed: require '===='
            if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || p[3] != '=' || decode32_table[(unsigned char)p[4]] != -1)
                *pfInvalid = true;
            break;

        case 5: // 8n+5 base32 characters processed: require '==='
            if (left || p[0] != '=' || p[1] != '=' || p[2] != '=' || decode32_table[(unsigned char)p[3]] != -1)
                *pfInvalid = true;
            break;

        case 7: // 8n+7 base32 characters processed: require '='
            if (left || p[0] != '=' || decode32_table[(unsigned char)p[1]] != -1)
                *pfInvalid = true;
            break;
        }

    return vchRet;
}

string DecodeBase32(const string& str)
{
    vector<unsigned char> vchRet = DecodeBase32(str.c_str());
    return (vchRet.size() == 0) ? string() : string((const char*)&vchRet[0], vchRet.size());
}

static bool ParsePrechecks(const std::string& str)
{
    if (str.empty()) // No empty string allowed
        return false;
    if (str.size() >= 1 && (isspace(str[0]) || isspace(str[str.size()-1]))) // No padding allowed
        return false;
    if (str.size() != strlen(str.c_str())) // No embedded NUL characters allowed
        return false;
    return true;
}

bool ParseInt32(const std::string& str, int32_t *out)
{
	if (!ParsePrechecks(str))
		return false;
	char *endp = NULL;

    errno = 0; // strtol will not set errno if valid
    long int n = strtol(str.c_str(), &endp, 10);
    if(out) *out = (int32_t)n;
    // Note that strtol returns a *long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int32_t*. On 64-bit
    // platforms the size of these types may be different.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int32_t>::min() &&
        n <= std::numeric_limits<int32_t>::max();
}

bool ParseInt64(const std::string& str, int64_t *out)
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = NULL;
    errno = 0; // strtoll will not set errno if valid
    long long int n = strtoll(str.c_str(), &endp, 10);
    if(out) *out = (int64_t)n;
    // Note that strtoll returns a *long long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int64_t*.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int64_t>::min() &&
        n <= std::numeric_limits<int64_t>::max();
}

bool ParseUInt32(const std::string& str, uint32_t *out)
{
    if (!ParsePrechecks(str))
        return false;
    if (str.size() >= 1 && str[0] == '-') // Reject negative values, unfortunately strtoul accepts these by default if they fit in the range
        return false;
    char *endp = NULL;
    errno = 0; // strtoul will not set errno if valid
    unsigned long int n = strtoul(str.c_str(), &endp, 10);
    if(out) *out = (uint32_t)n;
    // Note that strtoul returns a *unsigned long int*, so even if it doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *uint32_t*. On 64-bit
    // platforms the size of these types may be different.
    return endp && *endp == 0 && !errno &&
        n <= std::numeric_limits<uint32_t>::max();
}

bool ParseUInt64(const std::string& str, uint64_t *out)
{
    if (!ParsePrechecks(str))
        return false;
    if (str.size() >= 1 && str[0] == '-') // Reject negative values, unfortunately strtoull accepts these by default if they fit in the range
        return false;
    char *endp = NULL;
    errno = 0; // strtoull will not set errno if valid
    unsigned long long int n = strtoull(str.c_str(), &endp, 10);
    if(out) *out = (uint64_t)n;
    // Note that strtoull returns a *unsigned long long int*, so even if it doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *uint64_t*.
    return endp && *endp == 0 && !errno &&
        n <= std::numeric_limits<uint64_t>::max();
}


bool ParseDouble(const std::string& str, double *out)
{
    if (!ParsePrechecks(str))
        return false;
    if (str.size() >= 2 && str[0] == '0' && str[1] == 'x') // No hexadecimal floats allowed
        return false;
    std::istringstream text(str);
    text.imbue(std::locale::classic());
    double result;
    text >> result;
    if(out) *out = result;
    return text.eof() && !text.fail();
}

std::string FormatParagraph(const std::string in, size_t width, size_t indent)
{
    std::stringstream out;
    size_t ptr = 0;
    size_t indented = 0;
    while (ptr < in.size())
    {
        size_t lineend = in.find_first_of('\n', ptr);
        if (lineend == std::string::npos) {
            lineend = in.size();
        }
        const size_t linelen = lineend - ptr;
        const size_t rem_width = width - indented;
        if (linelen <= rem_width) {
            out << in.substr(ptr, linelen + 1);
            ptr = lineend + 1;
            indented = 0;
        } else {
            size_t finalspace = in.find_last_of(" \n", ptr + rem_width);
            if (finalspace == std::string::npos || finalspace < ptr) {
                // No place to break; just include the entire word and move on
                finalspace = in.find_first_of("\n ", ptr);
                if (finalspace == std::string::npos) {
                    // End of the string, just add it and break
                    out << in.substr(ptr);
                    break;
                }
            }
            out << in.substr(ptr, finalspace - ptr) << "\n";
            if (in[finalspace] == '\n') {
                indented = 0;
            } else if (indent) {
                out << std::string(indent, ' ');
                indented = indent;
            }
            ptr = finalspace + 1;
        }
    }
    return out.str();
}

std::string i64tostr(int64_t n)
{
    return strprintf("%d", n);
}

std::string itostr(int n)
{
    return strprintf("%d", n);
}

int64_t atoi64(const char* psz)
{
#ifdef _MSC_VER
    return _atoi64(psz);
#else
    return strtoll(psz, NULL, 10);
#endif
}

int64_t atoi64(const std::string& str)
{
#ifdef _MSC_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), NULL, 10);
#endif
}

int atoi(const std::string& str)
{
    return atoi(str.c_str());
}

/** Upper bound for mantissa.
 * 10^18-1 is the largest arbitrary decimal that will fit in a signed 64-bit integer.
 * Larger integers cannot consist of arbitrary combinations of 0-9:
 *
 *   999999999999999999  1^18-1
 *  9223372036854775807  (1<<63)-1  (max int64_t)
 *  9999999999999999999  1^19-1     (would overflow)
 */
static const int64_t UPPER_BOUND = 1000000000000000000LL - 1LL;

/** Helper function for ParseFixedPoint */
static inline bool ProcessMantissaDigit(char ch, int64_t &mantissa, int &mantissa_tzeros)
{
    if(ch == '0')
        ++mantissa_tzeros;
    else {
        for (int i=0; i<=mantissa_tzeros; ++i) {
            if (mantissa > (UPPER_BOUND / 10LL))
                return false; /* overflow */
            mantissa *= 10;
        }
        mantissa += ch - '0';
        mantissa_tzeros = 0;
    }
    return true;
}

bool ParseFixedPoint(const std::string &val, int decimals, int64_t *amount_out)
{
    int64_t mantissa = 0;
    int64_t exponent = 0;
    int mantissa_tzeros = 0;
    bool mantissa_sign = false;
    bool exponent_sign = false;
    int ptr = 0;
    int end = val.size();
    int point_ofs = 0;

    if (ptr < end && val[ptr] == '-') {
        mantissa_sign = true;
        ++ptr;
    }
    if (ptr < end)
    {
        if (val[ptr] == '0') {
            /* pass single 0 */
            ++ptr;
        } else if (val[ptr] >= '1' && val[ptr] <= '9') {
            while (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
                if (!ProcessMantissaDigit(val[ptr], mantissa, mantissa_tzeros))
                    return false; /* overflow */
                ++ptr;
            }
        } else return false; /* missing expected digit */
    } else return false; /* empty string or loose '-' */
    if (ptr < end && val[ptr] == '.')
    {
        ++ptr;
        if (ptr < end && val[ptr] >= '0' && val[ptr] <= '9')
        {
            while (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
                if (!ProcessMantissaDigit(val[ptr], mantissa, mantissa_tzeros))
                    return false; /* overflow */
                ++ptr;
                ++point_ofs;
            }
        } else return false; /* missing expected digit */
    }
    if (ptr < end && (val[ptr] == 'e' || val[ptr] == 'E'))
    {
        ++ptr;
        if (ptr < end && val[ptr] == '+')
            ++ptr;
        else if (ptr < end && val[ptr] == '-') {
            exponent_sign = true;
            ++ptr;
        }
        if (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
            while (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
                if (exponent > (UPPER_BOUND / 10LL))
                    return false; /* overflow */
                exponent = exponent * 10 + val[ptr] - '0';
                ++ptr;
            }
        } else return false; /* missing expected digit */
    }
    if (ptr != end)
        return false; /* trailing garbage */

    /* finalize exponent */
    if (exponent_sign)
        exponent = -exponent;
    exponent = exponent - point_ofs + mantissa_tzeros;

    /* finalize mantissa */
    if (mantissa_sign)
        mantissa = -mantissa;

    /* convert to one 64-bit fixed-point value */
    exponent += decimals;
    if (exponent < 0)
        return false; /* cannot represent values smaller than 10^-decimals */
    if (exponent >= 18)
        return false; /* cannot represent values larger than or equal to 10^(18-decimals) */

    for (int i=0; i < exponent; ++i) {
        if (mantissa > (UPPER_BOUND / 10LL) || mantissa < -(UPPER_BOUND / 10LL))
            return false; /* overflow */
        mantissa *= 10;
    }
    if (mantissa > UPPER_BOUND || mantissa < -UPPER_BOUND)
        return false; /* overflow */

    if (amount_out)
        *amount_out = mantissa;

    return true;
}
