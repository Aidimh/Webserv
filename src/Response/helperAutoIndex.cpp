#include "GET.hpp"

void    GET::buildAutoIndexHeader(std::ostringstream& html,const std::string& path) const
{
    html << "<html>\n";
    html << "<head><title>Index of " << path << "</title></head>\n";
    html << "<body>\n";
    html << "<h1>Index of " << path << "</h1>\n";
    html << "<hr>\n";
}


void    GET::buildAutoIndexFooter(std::ostringstream& html) const
{
    html << "<hr>\n";
    html << "</body>\n";
    html << "</html>\n";
}

void    GET::handleDirectoryEntries(DIR* dir,const std::string& currentPath,std::ostringstream& html) const
{
    struct dirent* entry;

    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;

        if (isHiddenEntry(name))
            continue;

        if (isDirectoryEntry(currentPath, name))
            appendDirectoryLink(html, name);
        else
            appendFileLink(html, name);
    }
}

bool    GET::isHiddenEntry(const std::string& name) const
{
    return (name == "." || name == "..");
}

bool    GET::isDirectoryEntry(const std::string& currentPath,const std::string& name) const
{
    std::string fullPath = currentPath + "/" + name;

    return isDirectory(fullPath);
}

void    GET::appendDirectoryLink(std::ostringstream& html,const std::string& name) const
{
    html << "<a href=\""
         << name
         << "/\">"
         << name
         << "/</a><br>\n";
}

void    GET::appendFileLink(std::ostringstream& html,const std::string& name) const
{
    html << "<a href=\"" << name << "\">" << name << "</a><br>\n";
}

