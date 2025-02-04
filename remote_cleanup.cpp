//                  ***** Remote class "cleanup" methods  *****

#include "remote.h"
#include "remotefile.h"
#include "menu.h"
#include "command.h"
#include <dirent.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

void Remote::cleanupFiles()
{
    int nfa = add_missing_actions();
    int nfr = remove_excess_actions();
    if (nfa > 0 || nfr > 0)
    {
        log_->print("Added %d files, removed %d files\n\n", nfa, nfr);
        list_files();
    }
}

void Remote::list_files()
{
    log_->print("Files on device:\n");
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
                log_->print("%-32s %5d\n", ent->d_name, sb.st_size);
            }

            ent = readdir(dir);
        }
	closedir(dir);
    }
    log_->print("\n");
}

void Remote::get_references(std::set<std::string> &files, std::set<std::string> &references)
{
    files.clear();
    references.clear();
    RemoteFile::actionFiles(files);
    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        std::string url = RemoteFile::actionToURL(*it);
        if (get_rfile(url))
        {
            for (auto bi = rfile_.buttons().cbegin(); bi != rfile_.buttons().cend(); ++bi)
            {
                if (strlen(bi->redirect()) > 0)
                {
                    if (strcmp(bi->redirect(), "..") != 0 &&
                        strncmp(bi->redirect(), "http://", 7) != 0 &&
                        strncmp(bi->redirect(), "https://", 8) != 0)
                    {
                        std::string rurl = Command::make_redirect(url, bi->redirect());
                        references.insert(RemoteFile::urlToAction(rurl));
                    }
                }
            }
        }
        else
        {
            log_->print("Failed to load '%s' for url '%s'\n", it->c_str(), url.c_str());
        }
    }
    rfile_.clear();
}

int Remote::add_missing_actions()
{
    std::set<std::string> files;
    std::set<std::string> references;
    get_references(files, references);

    references.insert("actions.json");
    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        references.erase(*it);
    }

    for (auto it = references.cbegin(); it != references.cend(); ++it)
    {
        log_->print("File %s is referenced but does not exist\n", it->c_str());
        std::string title("New Remote");
        std::size_t i1 = it->rfind('_');
        if (i1 != std::string::npos)
        {
            title = it->substr(i1 + 1, it->length() - i1 - 1 - 5);
            title[0] = std::toupper(title.at(0));
        }
        std::string json("{\"title\": \"");
        json += title + "\", \"buttons\": []}";
        if (rfile_.loadString(json, it->c_str()))
        {
            rfile_.saveFile();
            rfile_.clear();
        }
    }

    return references.size();
}

int Remote::remove_excess_actions()
{
    std::set<std::string> files;
    std::set<std::string> references;
    get_references(files, references);

    files.erase("actions.json");
    for (auto it = references.cbegin(); it != references.cend(); ++it)
    {
        files.erase(*it);
    }

    for (auto it = files.cbegin(); it != files.cend(); ++it)
    {
        log_->print("File %s exists but is not referenced\n", it->c_str());
        unlink(it->c_str());
    }

    return files.size();
}

