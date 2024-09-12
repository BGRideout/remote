//                  *****  JSONMap Implementation  *****

#include "jsonmap.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

bool JSONMap::loadFile(const char *filename)
{
    clear();
    bool ret = false;
    FILE *f = fopen(filename, "r");
    if (f)
    {
        struct stat sb;
        stat(filename, &sb);
        data_ = new char[sb.st_size + 1];
        fread(data_, sb.st_size, 1, f);
        data_[sb.st_size] = 0;
        fclose(f);
        ret = load();
    }
    return ret;
}

bool JSONMap::loadString(const char *str)
{
    clear();
    int len = strlen(str);
    data_ = new char[len + 1];
    strncpy(data_, str, len + 1);
    return load();
}

bool JSONMap::load()
{
    int jsz = strlen(data_) / 4;
    json_ = new json_t[jsz];
    json_t const* json = json_create(data_, json_, jsz );
    return json != nullptr;
}

const json_t *JSONMap::findProperty(const char *name) const
{
    return json_getProperty(json_, name);
}

bool JSONMap::hasProperty(const char *name) const
{
    return findProperty(name) != nullptr;
}

const char *JSONMap::strValue(const char *name, const char *defVal) const
{
    const char *ret = defVal;
    const json_t *prop = findProperty(name);
    if (prop)
    {
        ret = json_getValue(prop);
    }
    return ret;
}

int JSONMap::intValue(const char *name, int defVal) const
{
    int ret = defVal;
    const json_t *prop = findProperty(name);
    if (prop)
    {
        ret = json_getInteger(prop);
    }
    return ret;
}

double JSONMap::realValue(const char *name, double defVal) const
{
    double ret = defVal;
    const json_t *prop = findProperty(name);
    if (prop)
    {
        ret = json_getReal(prop);
    }
    return ret;
}

bool JSONMap::fromMap(const JMAP &jmap, std::string &str)
{
    bool ret = false;
    str.clear();
    if (jmap.size() > 0)
    {
        ret = true;
        std::string sep("{\"");
        for (auto it = jmap.cbegin(); it != jmap.cend(); ++it)
        {
            str += sep + it->first + "\":\"" + it->second + "\"";
            sep = ",\"";
        }
        str += "}";
    }
    return ret;
}

void JSONMap::clear()
{
    if (data_) delete [] data_;
    data_ = nullptr;
    if (json_) delete [] json_;
    json_ = nullptr;
}