//                  ***** Remote class "cleanup" methods  *****

#include "remote.h"
#include "remotefile.h"
#include "menu.h"
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>

void Remote::cleanupFiles()
{
    printf("Files on device:\n");
    DIR *dir = opendir("/");
    if (dir)
    {
        struct dirent *ent = readdir(dir);
        while (ent)
        {
            if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0 && strcmp(ent->d_name, "") != 0)
            {
                struct stat sb = {0};
                if (stat(ent->d_name, &sb) == -1) sb.st_size = 0;
                printf("%-32s %5d", ent->d_name, sb.st_size);

                char buf[41];
                FILE *f = fopen(ent->d_name, "r");
                if (f)
                {
                    if (fgets(buf, sizeof(buf), f))
                    {
                        size_t nn = strlen(buf) - 1;
                        while (nn > 0 && (buf[nn] == '\n' || buf[nn] == '\r'))
                        {
                            buf[nn--] = '\0';
                        }
                        printf("    %s", buf);
                    }
                }
                printf("\n");
            }
            ent = readdir(dir);
        }
    }
    printf("\n");
}

