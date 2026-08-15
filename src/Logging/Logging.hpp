#pragma once

#include <map>
#include <vector>
#include <set>
#include <algorithm>
#include <stdlib.h>
#include <climits>
#include <limits>
#include <string>
#include <cstring>
#include <fstream>
#include <cstdlib>
#include <unistd.h>
#include <sstream>
#include <cctype>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <linux/errqueue.h>
#include <signal.h>
#include <sys/types.h> 
#include <dirent.h>
#include <iomanip>
#include <ctime>
#include <queue>
#include <sys/ioctl.h>
#include <sys/resource.h>
using namespace std;

// #include "Headers.hpp"

#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"

class Logging
{
	ofstream logFile;
	static set<string> activeDebugClasses;
	static set<string> activeDetailClasses;
	static bool globalDebug;
    static bool globalDetail;

	string getTime();
	void WriteDebug(const string &msg);
	void WriteInfo(const string &msg);
	void WriteWarn(const string &msg);
	void WriteError(const string &msg);

public:

	static void EnableDebug(const string &filename);
	static void EnableDetailDebug(const string &filename);
	static Logging *current;
	Logging(string filename);
	~Logging();

	class LogBase
	{
	protected:
		stringstream ss;

	public:
		LogBase();
		virtual ~LogBase();
		template <typename T>
		LogBase &operator<<(const T &value)
		{
			ss << value;
			return *this;
		}
	};

	class Debug : public LogBase
	{
		bool isActive;
	public:
		Debug();
		Debug(string ClsName, bool isDetail = false);
		~Debug();
	};

	class Info : public LogBase
	{
	public:
		Info();
		~Info();
	};

	class Warn : public LogBase
	{
	public:
		Warn();
		~Warn();
	};

	class Error : public LogBase
	{
	public:
		Error();
		~Error();
	};
};


// #define DISABLE_LOGGING
#define PRINT_LOGGING true
#define WRITE_FILE true



#ifdef DISABLE_LOGGING

// Helper class to safely swallow the stream expression
class LogMessageVoidifier {
public:
    // This catches the final LogBase reference and returns void
    void operator&(const Logging::LogBase&) {}
};

// If disabled, this evaluates to (void)0. 
// Because of short-circuiting, the right side is NEVER evaluated at runtime.
#define LOGGING_VOIDIFY true ? (void)0 : LogMessageVoidifier() &

#define DEBUG LOGGING_VOIDIFY Logging::Debug
#define INFO  LOGGING_VOIDIFY Logging::Info
#define WARN  LOGGING_VOIDIFY Logging::Warn
#define ERR   LOGGING_VOIDIFY Logging::Error

// Explicitly handle macros that take arguments
#define DDEBUG(className) LOGGING_VOIDIFY Logging::Debug(className, true)

#else

// Original active macros
#define DEBUG Logging::Debug
#define INFO  Logging::Info
#define WARN  Logging::Warn
#define ERR   Logging::Error
#define DDEBUG(className) Logging::Debug(className, true)

#endif