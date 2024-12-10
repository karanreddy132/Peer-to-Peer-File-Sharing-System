#include <vector>
#include <string>
#include <unistd.h>
#include <cstring>  
#include <fcntl.h>
#include<iostream>
#include <sstream>

#include "client_header.h"

using namespace std;

vector<string> tokenize(string str, char delimiter) {
    vector<string> tokens;
    stringstream ss(str);
    string token;

    while (getline(ss, token, delimiter)) {
        tokens.push_back(token); 
    }

    return tokens;
}

vector<string> get_trackers_list(string file_path){
    int fd = open(file_path.c_str(), O_RDONLY);

    if(fd==-1){
        cerr << "Failed to open the file\n";
        return {};
    }

    const size_t buffer_size = 256;
    char buffer[buffer_size];

    ssize_t bytes_read;

    string line;

    vector<string> tracker_list;

    while ((bytes_read = read(fd,buffer,sizeof(buffer)-1))>0) {
        buffer[bytes_read] = '\0';

        for (size_t i = 0; i < bytes_read; ++i) {
            if (buffer[i] == '\n') {
                
                tracker_list.push_back(line);
                line.clear();
                
            } else {
                line += buffer[i]; 
            }
        }
    }

    if(!line.empty()) {
        tracker_list.push_back(line);
    }

    return tracker_list;
}