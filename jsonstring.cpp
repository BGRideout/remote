//                  *****  JSONString Implementation  *****

#include "jsonstring.h"
#include <string.h>

char JSONString::blank_[] = "";

JSONString::JSONString(const JSONString &other) : string_(nullptr), buflen_(0)
{
    *this = other;
}

JSONString::~JSONString()
{
    clear();
}

JSONString &JSONString::operator = (const JSONString &other)
{
    clear();

    //  If other is allocated, allocate and copy string else copy pointer
    buflen_ = other.buflen_;
    if (buflen_ > 0)
    {
        string_ = new char[buflen_];
        memcpy(string_, other.string_, buflen_);
    }
    else
    {
        string_ = other.string_;
    }

    return *this;
}

void JSONString::operator = (const json_t *json)
{
    clear();

    //  Save JSON string pointer and buffer size
    if (json && json_getType(json) != JSON_ARRAY && json_getType(json) != JSON_OBJ)
    {
        const char *js = json_getValue(json);
        string_ = const_cast<char *>(js);
        buflen_ = -(strlen(js) + 1);
    }
}

void JSONString::operator = (const char *string)
{
    //  Get length of new string
    int ll = strlen(string);

    //  IF pointing at JSON data, copy if it fits
    if (buflen_ < 0)
    {
        if (ll < -buflen_)
        {
            strncpy(string_, string, -buflen_);
        }
        else
        {
            clear();
        }
    }

    //  If pointing at allocated data, copy if it fits
    if (buflen_ > 0)
    {
        if (ll < buflen_)
        {
            strncpy(string_, string, buflen_);
        }
        else
        {
            clear();
        }
    }

    //  IF not assigned, allocate and copy
    if (buflen_ == 0)
    {
        buflen_ = ((ll + 32) / 32) * 32;
        string_ = new char[buflen_];
        memcpy(string_, string, buflen_);
    }
}

void JSONString::clear()
{
    if (buflen_ > 0)
    {
        delete [] string_;
    }
    string_ = nullptr;
    buflen_ = 0;
}