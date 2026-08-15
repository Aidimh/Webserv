#include "Logging.hpp"


set<string> Logging::activeDebugClasses;
set<string> Logging::activeDetailClasses;
bool Logging::globalDebug = false;
bool Logging::globalDetail = false;

Logging *Logging::current = NULL;

Logging::Logging(string filename)
{
	current = this;
	logFile.open(filename.c_str(), ios::app);
}

bool writeInfo = true;

Logging::~Logging()
{
	if (logFile.is_open())
	{
		logFile.close();
	}
}
void Logging::EnableDebug(const string &className)
{
	if (className == "")
		globalDebug = true;
	else
		activeDebugClasses.insert(className);
	
}

void Logging::EnableDetailDebug(const string &className)
{
	if (className == "")
		globalDetail = true;
	else
		activeDetailClasses.insert(className);
}

Logging::LogBase::LogBase() 
{
	ss.str();
}

Logging::Debug::~Debug()
{
	if (isActive && Logging::current)
		Logging::current->WriteDebug(ss.str());
}

Logging::Info::Info()
{
}

Logging::Info::~Info()
{
	if (Logging::current)
		Logging::current->WriteInfo(ss.str());
}

Logging::Warn::Warn()
{
}

Logging::Warn::~Warn()
{
	if (Logging::current)
		Logging::current->WriteWarn(ss.str());
}

Logging::Error::Error()
{
}

Logging::Error::~Error()
{
	if (Logging::current)
		Logging::current->WriteError(ss.str());
}

Logging::Debug::Debug()
{
	isActive = false;
}


Logging::Debug::Debug(string ClsName, bool isDetail)
{
	isActive = false;

	if (isDetail)
	{
		if (Logging::globalDetail || 
			Logging::activeDetailClasses.find(ClsName) != activeDetailClasses.end())
		{
			isActive = true;
		}
	}
	else
	{
		if (Logging::globalDebug || Logging::globalDetail ||
			Logging::activeDebugClasses.find(ClsName) != activeDebugClasses.end() ||
			Logging::activeDetailClasses.find(ClsName) != activeDetailClasses.end())
		{
			isActive = true;
		}
	}
}



Logging::LogBase::~LogBase()
{
	
}

string Logging::getTime()
{
	time_t now = time(0);
	tm *ltm = localtime(&now);
	char buffer[80];
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", ltm);
	return string(buffer);
}

void Logging::WriteDebug(const string &msg)
{
	string timeStr = getTime();
	string prefix = "[DEBUG] ";
	writeInfo = true;
	if (PRINT_LOGGING)
		cout << BLUE << timeStr << " " << prefix << msg << RESET << endl;
	if (logFile.is_open() && WRITE_FILE)
		logFile << timeStr << " " << prefix << msg << endl;
}

void Logging::WriteInfo(const string &msg)
{
	string timeStr = getTime();
	string prefix = "[INFO]  ";
	
	if (PRINT_LOGGING)
		cout << GREEN << timeStr << " " << prefix << msg << RESET << endl;
	if (logFile.is_open() && WRITE_FILE)
		logFile << timeStr << " " << prefix << msg << endl;
}

void Logging::WriteWarn(const string &msg)
{
	string timeStr = getTime();
	string prefix = "[WARN]  ";
	writeInfo = true;
	if (PRINT_LOGGING)
		cout << YELLOW << timeStr << " " << prefix << msg << RESET << endl;
	if (logFile.is_open() && WRITE_FILE)
		logFile << timeStr << " " << prefix << msg << endl;
}

void Logging::WriteError(const string &msg)
{
	string timeStr = getTime();
	string prefix = "[ERROR] ";
	writeInfo = true;
	if (PRINT_LOGGING)
		cout << RED << timeStr << " " << prefix << msg << RESET << endl;
	if (logFile.is_open() && WRITE_FILE)
		logFile << timeStr << " " << prefix << msg << endl;
}
