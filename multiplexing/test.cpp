#include <iostream>
#include <string>

int main() {
    std::string filename = "profile_picture.png";

    // 1. Find where the dot "." is located
    size_t dot_position = filename.find(".");

    // 2. Extract everything starting from that dot position to the end
    std::string extension = filename.substr(dot_position);

    // Print the results
    std::cout << "The dot is at index: " << dot_position << std::endl;
    std::cout << "The extension is: " << extension << std::endl;

    return 0;
}