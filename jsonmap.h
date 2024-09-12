//                  *****  JSONMap Class  *****

#ifndef JSONMAP_H
#define JSONMAP_H

#include <tiny-json.h>
#include <map>
#include <string>

class JSONMap
{
private:
    json_t          *json_;             // JSON properties
    char            *data_;             // Data buffer

    JSONMap(const JSONMap &other);
    const JSONMap &operator = (const JSONMap &other);

    bool load();
    const json_t *findProperty(const char *name) const;

public:
    JSONMap() : json_(nullptr), data_(nullptr) {}
    JSONMap(const char *json) : json_(nullptr), data_(nullptr) { loadString(json); }
    ~JSONMap() { clear(); }

    bool loadFile(const char *filename);
    bool loadString(const char *str);

    bool hasProperty(const char *name) const;
    const char *strValue(const char *name, const char *defVal=nullptr) const;
    int intValue(const char *name, int defVal=0) const;
    double realValue(const char *name, double defVal=0.0) const;

    typedef std::map<std::string, std::string> JMAP;
    static bool fromMap(const JMAP &jmap, std::string &str);

    void clear();
};

#endif
