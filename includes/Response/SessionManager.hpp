#ifndef SESSIONMANAGER_HPP
#define SESSIONMANAGER_HPP

#include <map>
#include <string>
#include <ctime>

class ClientRequest;

/*
 * Minimal session tracking: the server hands every visitor an id in a cookie
 * and remembers the ids it issued, so it can tell a returning visitor from a
 * client that made one up.
 */
class SessionManager
{
    public:
        static const std::string&   cookieName();
        static std::string          readCookie(const ClientRequest& request);
        static bool                 isKnown(const std::string& id);
        static std::string          create();
        static std::string          buildSetCookieHeader(const std::string& id);

    private:
        static std::map<std::string, time_t> _sessions;
        static std::string          generateId();
        static void                 forgetExpired();
};

#endif