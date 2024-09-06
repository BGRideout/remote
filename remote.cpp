#include "remote.h"
#include "remotefile.h"

#include <stdio.h>
#include <stdlib.h>
#include <pico/stdlib.h>
#include <pfs.h>
#include <lfs.h>
#include <sys/stat.h>
#include <tiny-json.h>
#include <sstream>

#define ROOT_OFFSET 0x140000
#define ROOT_SIZE   0x060000


static void dump( json_t const* json ) {

    jsonType_t const type = json_getType( json );
    if ( type != JSON_OBJ && type != JSON_ARRAY ) {
        puts("error");
        return;
    }

    sleep_ms(25);
    printf( "%s", type == JSON_OBJ? "\n {": " [" );

    json_t const* child;
    for( child = json_getChild( json ); child != 0; child = json_getSibling( child ) ) {

        jsonType_t propertyType = json_getType( child );
        char const* name = json_getName( child );
        if ( name ) printf(" \"%s\": ", name );

        if ( propertyType == JSON_OBJ || propertyType == JSON_ARRAY )
            dump( child );

        else {
            char const* value = json_getValue( child );
            if ( value ) {
                bool const text = JSON_TEXT == json_getType( child );
                char const* fmt = text? " \"%s\"": " %s";
                printf( fmt, value );
                bool const last = !json_getSibling( child );
                if ( !last ) putchar(',');
            }
        }
    }

    printf( "%s", type == JSON_OBJ? " }\n": " ]" );

}


int main ()
{
    stdio_init_all();
    sleep_ms(3000);

    struct pfs_pfs *pfs;
    struct lfs_config cfg;
    ffs_pico_createcfg (&cfg, ROOT_OFFSET, ROOT_SIZE);
    pfs = pfs_ffs_create (&cfg);
    pfs_mount (pfs, "/");

    printf("Filesystem mounted\n");

    FILE *f = fopen("/test.txt", "r");
    if (f)
    {
        printf("File opened\n");
        char buf[80];
        while (fgets(buf, sizeof(buf), f))
        {
            printf("%s", buf);
        }
        fclose(f);
    }

    f = fopen("/test.txt", "a+");
    if (f)
    {
        fputs("New line\n", f);
        fclose(f);
    }
    else
    {
        printf("Failed to open file\n");
    }

    RemoteFile rfile;
    if (rfile.loadFile("actions.json"))
    {
        printf("%s : %s\n", rfile.filename(), rfile.title());
        const RemoteFile::ButtonMap &btns = rfile.buttons();
        for (auto it = btns.cbegin(); it != btns.cend(); ++it)
        {
            const RemoteFile::Button &btn = it->second;
            printf("%d %s %s %s %d\n", btn.position(), btn.label(), btn.color(), btn.redirect(), btn.repeat());
            const RemoteFile::Button::ActionList &acts = btn.actions();
            for (auto i2 = acts.cbegin(); i2 != acts.cend(); ++i2)
            {
                printf("  %s %d %d %d\n", i2->type(), i2->address(), i2->value(), i2->delay());
                sleep_ms(10);
            }
        }

        
        RemoteFile::Button *btn = rfile.addButton(38, "Cast", "LightMagenta", "", 0);;
        if (btn)
        {
            btn->clearActions();
            btn->addAction("bogus", 1, 2, 3);
        }

        rfile.deleteButton(31);

        std::stringstream out;
        rfile.outputJSON(out);
        printf("%s", out.str().c_str());
    }
    return 0;
}
