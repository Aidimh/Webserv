#ifndef REQUESTHELPERS_HPP
#define REQUESTHELPERS_HPP

#include "../../header.hpp"

size_t removeWhitespace(Client& client);
bool ValidLine(std::string line);
void MyToLower(std::string &str);

#endif