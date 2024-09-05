//                  *****  JSONString Class  *****

/**
 *  @details
 * This class is used for strings that can rfrence the document refereenced by the tiny-json
 * library.  If assigned from the tiny-json json_t property, the pointer references the data
 * buffer containing th JSON data.  If assigned from an eplicit string, the string replaces
 * the original JSON buffer string if it fits or is allocated on the heap.
 */

#ifndef JSONSTRING_H
#define JSONSTRING_H

#include <tiny-json.h>

class JSONString
{
private:
    char            *string_;           // String pointer
    int             buflen_;            // String buffer length
                                        // 0 = unchanged
                                        // <0 = JSON data buffer (abs is orignal size)
                                        // >0 = Allocated on heap of indicated size

    static char     blank_[];           // Blank string

public:
    JSONString() : string_(nullptr), buflen_(0) {}
    JSONString(const JSONString &other);
    ~JSONString();

    JSONString &operator = (const JSONString &other);

    /**
     * @brief   Return pointer value
     * 
     * @return  Value of internal pointer
     */
    const char *ptr() const { return string_; }

    /**
     * @brief   Return string value
     * 
     * @return  String referenced or blank string if null
     */
    const char *str() const { return string_ ? string_ : blank_; }

    /**
     * @brief   Assign JSON string
     * 
     * @param   Pointer to JSON property
     */
    void operator = (const json_t *json);

    /**
     * @brief   Assign string
     * 
     * @param   New string value to be assigned
     */
    void operator = (const char *string);

    /**
     * @brief   Clear the string
     */
    void clear();
};

#endif