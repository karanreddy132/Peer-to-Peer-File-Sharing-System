#include "tracker_header.h"

#include<iostream>
#include<vector>
#include<string.h>
#include<cstring>
#include<string>
#include<unistd.h>
#include<fcntl.h>
#include<sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<unordered_set>
#include <sstream>
#include<mutex>
#include <shared_mutex>

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

void createUser(string request,string &response){
    std::lock_guard<std::mutex> lock(user_credentials_mutex);
    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=3){
        response = "Invalid arguments";
        return;
    }

    string user_id = cmds[1];
    string password = cmds[2];

    if(user_credentials.find(user_id)!=user_credentials.end()){
        response = "User already exists!!!";
        return;
    }

    user_credentials[user_id] = password;

    response = "User with user id \"" + user_id + "\" created successfully";
}

void handleLogin(string request,string &response,int client_id){
    std::lock_guard<std::shared_mutex> isLoggedin_lock(isLoggedin_mutex);
    std::lock_guard<std::mutex> user_credentials_lock(user_credentials_mutex);
    std::lock_guard<std::shared_mutex> client_to_user_id_lock(client_to_user_id_mutex);
    std::lock_guard<std::shared_mutex> user_to_client_id_lock(user_to_client_id_mutex);

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=3){
        response = "Invalid arguments";
        return;
    }

    string user_id = cmds[1];
    string password = cmds[2];

    if(isLoggedin.find(user_id)!=isLoggedin.end()){
        response = "User with user id \"" + user_id + "\" already logged in!!!";
    }
    else if(user_credentials.find(user_id)==user_credentials.end()){
        response = "User with user id \"" + user_id + "\" doesn't exist!!!";  //Check if user exists
    }
    else if(checkIsLoggedIn(client_id)){
        response = "User with user id \"" + user_id + "\" already logged in!!!";  //Check if user is already logged in
    }
    else{
        if(user_credentials[user_id]==password){
            response = "User with user id \"" + user_id + "\" logged in successfully!!!";
            client_to_user_id[client_id] = user_id;
            user_to_client_id[user_id] = client_id;
            isLoggedin.insert(user_id);
        }
        else{
            response = "Invalid password";
        }
        
        
    }
}

void handleCreateGroup(string request,string &response,int client_id){
    std::lock_guard<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);
    std::lock_guard<std::shared_mutex> group_users_lock(group_users_mutex);

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }
    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=2){
        response = "Invalid arguments";
        return;
    }
    
    int group_id = stoi(cmds[1]);
    string user_id = client_to_user_id[client_id];

    if(group_to_user_id.find(group_id)!=group_to_user_id.end()){
        response = "Group " + to_string(group_id) + " already exists!!!";
    }
    else{
        group_users[group_id] = {};
        group_to_user_id[group_id] = user_id;
        response = "Group " + to_string(group_id) + " created successfully!!!";
    }
}

bool checkIsLoggedIn(int client_id){
    string user_id = client_to_user_id[client_id];
    return isLoggedin.find(user_id)!=isLoggedin.end();
}

void handleLogout(string request,string &response,int client_id){
    std::lock_guard<std::shared_mutex> isLoggedin_lock(isLoggedin_mutex);
    std::lock_guard<std::shared_mutex> client_to_user_id_lock(client_to_user_id_mutex);

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=1){
        response = "Invalid arguments";
        return;
    }

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    string user_id = client_to_user_id[client_id];

    isLoggedin.erase(user_id);
    

    response = "User with user id \"" + user_id + "\" logged out successfully!!!"; 
}

void handleJoinGroup(string request,string &response,int client_id){
    std::shared_lock<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);
    std::lock_guard<std::shared_mutex> group_users_lock(group_users_mutex);
    std::lock_guard<std::shared_mutex> pending_join_req_lock(pending_join_req_mutex);

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=2){
        response = "Invalid arguments";
        return;
    }

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    int group_id = stoi(cmds[1]);
    string user_id = client_to_user_id[client_id];

    //Check if group exists
    if(group_to_user_id.find(group_id)==group_to_user_id.end()){
        response = "Group id " + to_string(group_id) + " doesn't exist!!!";
    }
    else if(group_to_user_id[group_id]==user_id){
        response = "You are already the owner of group " + to_string(group_id);  //If the current user is the owner of the group he want to join the do nothing
    }
    else if(group_users[group_id].find(user_id)!=group_users[group_id].end()){
        response = "You have already joined group " + to_string(group_id);  //If the user is already in the group do nothing
    }
    else{
        pending_join_req[group_id].insert(user_id);   //Send join request group owner
        response = "Join request sent to group owner \"" + group_to_user_id[group_id] + "\"";
    }
}

void handleLeaveGroup(string request,string &response,int client_id){
    std::lock_guard<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);
    std::lock_guard<std::shared_mutex> group_users_lock(group_users_mutex);
    std::lock_guard<std::shared_mutex> pending_join_req_lock(pending_join_req_mutex);

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=2){
        response = "Invalid arguments";
        return;
    }

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    int group_id = stoi(cmds[1]);
    string user_id = client_to_user_id[client_id];

    unordered_set<string> group_members = group_users[group_id];

    //If the user is owner of the group
    if(group_to_user_id[group_id]==user_id){  

        if(group_members.empty()){
            group_users.erase(group_id);  //If no one is in the group then remove the group
            group_to_user_id.erase(group_id);  //Remove it from that group owner list

            //Remove pending requests as the group 
            if(!pending_join_req[group_id].empty()){
                pending_join_req.erase(group_id);
            }

            response = "You left from group " + to_string(group_id);
        }
        else{
            string new_user;

            for(auto ele:group_members){
                new_user = ele;
                group_to_user_id[group_id] = ele;  //Make a group  member as group owner
                group_users[group_id].erase(ele);  //Remove it from group members list
                break;
            }

            response = "You left from group " + to_string(group_id) + ". New owner is \"" + new_user +"\"";
        }
    }
    else{
        if(group_members.find(user_id)!=group_members.end()){
            group_users[group_id].erase(user_id);
            response = "You left from group " + to_string(group_id);
        }
        else response = "You are not in group " + to_string(group_id);
    }

}

void handleListRequests(string request,string &response,int client_id){
    std::shared_lock<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);
    std::shared_lock<std::shared_mutex> pending_join_req_lock(pending_join_req_mutex);

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=2){
        response = "Invalid arguments";
        return;
    }

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    int group_id = stoi(cmds[1]);

    if(group_to_user_id.find(group_id)==group_to_user_id.end()){  //Check if group exist
        response = "Group " + to_string(group_id) + " doesn't exist";
    }
    else{
        unordered_set<string> pen_req = pending_join_req[group_id];

        if(pen_req.empty()){
            response = "No pending requests for group " + to_string(group_id);   //No pending requests
        } 
        else{
            response = "";

            for(auto pr:pen_req){
                response = pr + " ";
            }
        }
    }
}

void handleAcceptRequest(string request,string &response,int client_id){
    std::lock_guard<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);
    std::lock_guard<std::shared_mutex> group_users_lock(group_users_mutex);
    std::lock_guard<std::shared_mutex> pending_join_req_lock(pending_join_req_mutex);

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=3){
        response = "Invalid arguments";
        return;
    }

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    int group_id = stoi(cmds[1]);
    string requested_user_id = cmds[2];
    string user_id = client_to_user_id[client_id];
    
    if(group_to_user_id.find(group_id)==group_to_user_id.end()){
        response = "Group " + to_string(group_id) + " doesn't exists";  //Check if group exist
    }
    else if(user_credentials.find(requested_user_id)==user_credentials.end()){
        response = "User with user id \"" + requested_user_id + "\" doesn't exist";  //Check if user exist
    }
    else if(group_to_user_id[group_id]!=user_id){
        response = "You are not the owner of group " + to_string(group_id);  //Check if current user is owner of the mentioned group
    }
    else if(pending_join_req[group_id].find(requested_user_id)==pending_join_req[group_id].end()){
        response = "There is no join request of user \"" + requested_user_id + "\" to join in group " + to_string(group_id);
    }
    else{
        group_users[group_id].insert(requested_user_id);
        response = "User with user id \"" + requested_user_id + "\" added to group " + to_string(group_id);  //Add user to the group
        pending_join_req[group_id].erase(requested_user_id);
    }
}

void handleListGroups(string request,string &response,int client_id){
    std::shared_lock<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=1){
        response = "Invalid arguments";
        return;
    }

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    if(group_to_user_id.empty()){
        response = "There are no groups";
    }
    else{
        response = "";

        for(auto ele:group_to_user_id){
            response = response + to_string(ele.first) + " ";
        }
    }
}

void handleInvalidCommand(string &response){
    response = "Invalid command";
}

void writeToLogFile(string response){
    string file_path = "log.txt";

    int fd = open(file_path.c_str(),O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);

    if(fd==-1){
        cerr << "Unable to open file\n";
        return;
    }

    string text = response + "\n";

    ssize_t bytesAppended = write(fd,text.c_str(),strlen(text.c_str()));

    if(bytesAppended==-1){
        cerr << "Error writing to file\n";
        close(fd);
        return;
    }

    close(fd);
}

void handleUploadFile(string request,string &response,int client_id){
    std::lock_guard<std::shared_mutex> group_sharable_files_lock(group_sharable_files_mutex);
    std::lock_guard<std::shared_mutex> seedersList_lock(seedersList_mutex);
    std::lock_guard<std::shared_mutex> user_id_files_chunk_hashes_lock(user_id_files_chunk_hashes_mutex);
    std::lock_guard<std::shared_mutex> file_chunk_hashes_lock(file_chunk_hashes_mutex);
    
    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=4){
        response = "Invalid arguments";
        return;
    }

    string file_path = cmds[1];     //Path of file which needed to be uploaded
    int group_id = stoi(cmds[2]);   //Group id
    string hashes_str =  cmds[3];   //String containing hashes of chunks and file

    vector<string> hashes = tokenize(hashes_str,':');

    int no_of_chunks = hashes.size()-1;

    string file_hash = hashes[no_of_chunks];
    
    string user_id = client_to_user_id[client_id];  //Extract user id and check if the user is in the group
    
    if(group_to_user_id.find(group_id)==group_to_user_id.end()){
        response = "Group " + to_string(group_id) + " doesn't exist";
    }
    else if(group_to_user_id[group_id]==user_id || group_users[group_id].find(user_id)!=group_users[group_id].end()){   //We can add files if the user is in the group
        
        group_sharable_files[group_id].insert(file_path);

        seedersList[group_id][file_path].insert(user_id);

        user_id_files_chunk_hashes[user_id][file_path] = hashes;  //user_id -> files -> hashes

        user_id_files[user_id].insert(file_path);

        file_chunk_hashes[file_path] = hashes;

        vector<string> file_tokens = tokenize(file_path,'/');

        string fp = file_tokens[file_tokens.size()-1];  //Extract only file name

        response = "File \"" + fp + "\" added to group " + to_string(group_id);
    }
    else{
        response = "You are not on this group so you can't add files";
    }
}

void handleListFiles(string request,string &response,int client_id){
    std::shared_lock<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);
    std::shared_lock<std::shared_mutex> group_users_lock(group_users_mutex);
    std::shared_lock<std::shared_mutex> group_sharable_files_lock(group_sharable_files_mutex);

    //Check if the user is logged in
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=2){
        response = "Invalid arguments";
        return;
    }

    int group_id = stoi(cmds[1]);
    string user_id = client_to_user_id[client_id];

    if(group_to_user_id.find(group_id)==group_to_user_id.end()){   //Check if group exist
        response = "Group " + to_string(group_id) + " doesn't exist";
    }
    else if(group_to_user_id[group_id]!=user_id && group_users[group_id].find(user_id)==group_users[group_id].end()){
        response = "You are not in Group " + to_string(group_id);    //Check if the user is in the group
    } 
    else{   //Send all sharable files as response
        unordered_set<string> sharable_files = group_sharable_files[group_id];

        if(sharable_files.empty()){
            response = "No sharable files in group " + to_string(group_id);
        }
        else{
            response = "\nSharable files in Group " + to_string(group_id) + " : \n\n";
            for(auto files:sharable_files){
                vector<string> file_tokens = tokenize(files,'/');

                string fp = file_tokens[file_tokens.size()-1];
        
                response = response + fp + '\n';
            }
        }
    }
}

void handleDownloadfile(string request,string &response,int client_id){
    std::shared_lock<std::shared_mutex> group_to_user_id_lock(group_to_user_id_mutex);
    std::shared_lock<std::shared_mutex> group_users_lock(group_users_mutex);
    std::shared_lock<std::shared_mutex> group_sharable_files_lock(group_sharable_files_mutex);
    std::shared_lock<std::shared_mutex> seedersList_lock(seedersList_mutex);
    std::shared_lock<std::shared_mutex> user_id_files_chunk_hashes_lock(user_id_files_chunk_hashes_mutex);

    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=4){
        response = "Invalid arguments";
        return;
    }

    int group_id = stoi(cmds[1]);
    string file_name = cmds[2];
    string dest_path = cmds[3];
    string user_id = client_to_user_id[client_id];

    if(group_to_user_id.find(group_id)==group_to_user_id.end()){   //Check if group exists
        response = "status_code=404 Group " + to_string(group_id) + " doesn't exist";  //Include a status code 
    }
    else if(group_to_user_id[group_id]!=user_id && group_users[group_id].find(user_id)==group_users[group_id].end()){
        response = "status_code=404 You are not in Group " + to_string(group_id);   //Check if user is in the group
    }
    else{

        unordered_set<string> sharable_files = group_sharable_files[group_id];

        vector<string> file_tokens = tokenize(file_name,'/'); 

        string fp = file_tokens[file_tokens.size()-1];  //Extract only file name

        if(sharable_files.find(file_name)==sharable_files.end()){  //Checks if file is in the group

            response = "status_code=404 File " + fp + " doesn't exist in Group " + to_string(group_id);
        }
        else{
            
            unordered_set<string> file_own = seedersList[group_id][file_name];

            if(file_own.find(user_id)!=file_own.end()){   //If file is already with the user then return
                response = "You already have \"" + fp + "\" file";
                return;
            }

            unordered_map<int,unordered_set<string>> file_locations;   //Stores which chunk is present which peer

            for(auto fo:file_own){
                int cli_id = user_to_client_id[fo];
                if(!checkIsLoggedIn(cli_id)) continue;   //Check if the seeder is logged in currently 

                vector<string> hashes = user_id_files_chunk_hashes[fo][file_name];   //gets all chunks of peers who has file


                for(int i=0;i<hashes.size()-1;i++){
                    if(hashes[i].size()==40) file_locations[i].insert(fo);   //If hash is calculated already then the chunk is present we can add it to our list
                }
            }

            response = "status_code=200 "; 

            for(auto ele:file_locations){
                int i = ele.first;   //Chunk number
                unordered_set<string> chunk_owners = ele.second;

                response = response + to_string(i) + ">";

                //Now we have to get ip port of users having chunks
                for(auto user:chunk_owners){
                    int owner_id = user_to_client_id[user];

                    string ip_port = client_ip_port[owner_id];

                    response = response + ip_port + ",";    //For each chunk append clients ip port seperated by comma and each chunk is seperated by ";"
                }

                response = response + ";";
            }
            
        }
    }
}

void handleCheckHash(string request,string &response,int client_id){
    
    std::lock_guard<std::shared_mutex> user_id_files_chunk_hashes_lock(user_id_files_chunk_hashes_mutex);

    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=5){
        response = "Invalid arguments...";
        return;
    }

    string file_path = cmds[2];
    int chunk_number = stoi(cmds[3]);
    string chunk_hash = cmds[4];

    std::shared_lock<std::shared_mutex> client_to_user_id_lock(client_to_user_id_mutex);
    string user_id = client_to_user_id[client_id];
    int group_id = stoi(cmds[1]);

    std::shared_lock<std::shared_mutex> file_chunk_hashes_lock(file_chunk_hashes_mutex);
    if (file_chunk_hashes.find(file_path) == file_chunk_hashes.end()) {
        response = "File path not found.";
        return;
    }

    if (chunk_number < 0 || chunk_number >= file_chunk_hashes[file_path].size()) {
        response = "Invalid chunk number.";
        return;
    }

    string original_hash = file_chunk_hashes[file_path][chunk_number];


    if(original_hash == chunk_hash){
        response = "TRUE";

        //Update after you receive the chunk
        if(user_id_files_chunk_hashes[user_id][file_path].size()<=chunk_number){
            user_id_files_chunk_hashes[user_id][file_path].resize(chunk_number+1);
        }

        user_id_files_chunk_hashes[user_id][file_path][chunk_number] = chunk_hash;   

        std::lock_guard<std::shared_mutex> seeders_list_lock(seedersList_mutex);
        std::lock_guard<std::shared_mutex> user_id_files_lock(user_id_files_mutex);

        seedersList[group_id][file_path].insert(user_id); 
        user_id_files[user_id].insert(file_path);

        vector<string> file_tokens = tokenize(file_path,'/'); 

        string fp = file_tokens[file_tokens.size()-1];

        group_downloaded_files[group_id].insert(fp);
        
        std::lock_guard<std::shared_mutex> group_sharable_files_lock(group_sharable_files_mutex);
        group_sharable_files[group_id].insert(file_path);
    }
    else response = "FALSE";
}

void handleStopStore(string request,string &response,int client_id){
    
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=3){
        response = "Invalid arguments...";
        return;
    }

    int group_id = stoi(cmds[1]);
    string file_path = cmds[2];
    string user_id = client_to_user_id[client_id];

    vector<string> file_tokens = tokenize(file_path,'/'); 

    string fp = file_tokens[file_tokens.size()-1];  //Extract only file name

    if(group_to_user_id.find(group_id)==group_to_user_id.end()){   //Check if group exists
        response = "Group " + to_string(group_id) + " doesn't exist";  //Include a status code 
    }
    else if(group_to_user_id[group_id]!=user_id && group_users[group_id].find(user_id)==group_users[group_id].end()){
        response = "You are not in Group " + to_string(group_id);   //Check if user is in the group
    }
    else if(seedersList[group_id][file_path].find(user_id)==seedersList[group_id][file_path].end()){   //Check if the user has the given file
        response = "You don't have \"" + fp  + "\" file";
    }
    else{
        seedersList[group_id][file_path].erase(user_id);   //remove user_id from seederList

        if(seedersList[group_id][file_path].empty()){      //If the seeders list is empty for the file remove file from group sharable files
            group_sharable_files[group_id].erase(file_path);
        }

        response = "You stopped sharing \"" + fp + "\" file";
    }
}

void handleShowDownloads(string request,string &response,int client_id){
    
    if(!checkIsLoggedIn(client_id)){
        response = "Please login to perform the operation";
        return;
    }

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(args!=1){
        response = "Invalid arguments...";
        return;
    }

    for(auto ele:group_downloaded_files){
        for(auto files:ele.second){
            response = response + "C " + to_string(ele.first) + " " + files + '\n';
        }
    }
}

void handleOperation(string request,string &response,int client_id){

    vector<string> cmds = tokenize(request,' ');

    int args = cmds.size();

    if(cmds[0]=="create_user"){
        createUser(request,response);
    }
    else if(cmds[0]=="login"){
        handleLogin(request,response,client_id);
    }
    else if(cmds[0]=="create_group"){
        handleCreateGroup(request,response,client_id);
    }
    else if(cmds[0]=="logout"){
        handleLogout(request,response,client_id);
    }
    else if(cmds[0]=="join_group"){
        handleJoinGroup(request,response,client_id);
    }
    else if(cmds[0]=="leave_group"){
        handleLeaveGroup(request,response,client_id);
    }
    else if(cmds[0]=="list_requests"){
        handleListRequests(request,response,client_id);
    }
    else if(cmds[0]=="accept_request"){
        handleAcceptRequest(request,response,client_id);
    }
    else if(cmds[0]=="list_groups"){
        handleListGroups(request,response,client_id);
    }
    else if(cmds[0]=="upload_file"){
        handleUploadFile(request,response,client_id);
    }
    else if(cmds[0]=="list_files"){
        handleListFiles(request,response,client_id);
    }
    else if(cmds[0]=="download_file"){
        handleDownloadfile(request,response,client_id);
    }
    else if(cmds[0]=="check_hash"){
        handleCheckHash(request,response,client_id);
    }
    else if(cmds[0]=="stop_share"){
        handleStopStore(request,response,client_id);
    }
    else if(cmds[0]=="show_downloads"){
        handleShowDownloads(request,response,client_id);
    }
    else{
        handleInvalidCommand(response);
    }

    writeToLogFile(response);
}