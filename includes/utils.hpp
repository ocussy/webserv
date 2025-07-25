#ifndef UTILS_HPP
# define UTILS_HPP

#include "server.hpp"
#include "response.hpp"
#include <sys/stat.h>
#include <ctime>
#include "dirent.h"

std::string toString(int value);
size_t stringToSizeT(const std::string &str);
int stringToInt(const std::string &str);
void	vectorToCStringTab(const std::vector<std::string>& str, std::vector<char *>& cstr);
bool fileExists(const std::string &name);
void split(const std::string &s, const std::string &delimiter, std::vector<std::string> &elements);
time_t parseHttpDate(const std::string &httpDate);
std::string formatHttpDate(time_t timestamp);
std::string generateEtag(const std::string &path);
std::string readFile(const std::string& path);
bool compareLanguages(const std::pair<std::string, double>& a, const std::pair<std::string, double>& b);
bool hasReadPermission(const std::string &path);
bool hasWritePermission(const std::string &path);
bool isOnlyWhiteSpace(const std::string& str);
bool isDirectory(const std::string &path);
bool isValidIP(const std::string& ip);
bool isPathValid(const std::string&	path);
bool isFileValid(const std::string&	filePath);
bool isHttpErrorCodeValid(int code);
bool isNumber(const	std::string& str);
void checkPV(std::string &value);
int countWords(const std::string& str);
std::string ensureRelativeDotPath(const std::string& path);
std::string readPartialFile(const std::string &path, Range &range);
std::string joinPaths(const std::string& a, const std::string& b);
std::string trim(const std::string& str);
std::string generateFileName(const std::string &scriptName, std::string typeOfFile);

#endif
