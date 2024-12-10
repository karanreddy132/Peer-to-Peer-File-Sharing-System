#include<iostream>
#include<sys/socket.h>
#include<cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include<string>
#include<sstream>
#include<vector>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <unordered_map> 
#include<unordered_set>
#include <csignal>
#include <mutex>
#include <shared_mutex>

#define BUFFER_SIZE 65536

#include "tracker_header.h"

using namespace std;

bool stopProgram = false;

std::mutex user_credentials_mutex;
std::shared_mutex isLoggedin_mutex;
std::shared_mutex client_to_user_id_mutex;
std::shared_mutex user_to_client_id_mutex;
std::shared_mutex group_to_user_id_mutex;
std::shared_mutex group_users_mutex;
std::shared_mutex pending_join_req_mutex;
std::shared_mutex client_ip_port_mutex;
std::shared_mutex group_sharable_files_mutex;
std::shared_mutex seedersList_mutex;
std::shared_mutex file_chunk_hashes_mutex;
std::shared_mutex user_id_files_chunk_hashes_mutex;
std::shared_mutex user_id_files_mutex;

//Global variables
unordered_map<string,string> user_credentials;
unordered_set<string> isLoggedin;
unordered_map<int,string> client_to_user_id;
unordered_map<string,int> user_to_client_id;
unordered_map<int,string> group_to_user_id;
unordered_map<int,unordered_set<string>> group_users;
unordered_map<int,unordered_set<string>> pending_join_req;
unordered_map<int,string> client_ip_port;
unordered_map<int,unordered_set<string>> group_sharable_files;
unordered_map<int,unordered_map<string,unordered_set<string>>> seedersList;
unordered_map<string,vector<string>> file_chunk_hashes;
unordered_map<string,unordered_map<string,vector<string>>> user_id_files_chunk_hashes;
unordered_map<string,unordered_set<string>> user_id_files;
unordered_map<int,unordered_set<string>> group_downloaded_files; 

//To handle multiple tasks
pthread_t tid[2];      
pthread_mutex_t pthread_lock;

struct arguments{
    int clientSocket;
};

int client_socket = 0, tracker_socket = 0;

//Close tracker and client port if control c is pressed
void handlerFunction(int signum){
    close(client_socket);
    close(tracker_socket);
    exit(signum);
}

void* handleClients(void *args){

    pthread_mutex_lock(&pthread_lock);   //Create mutex for mutual exclusion

    arguments *argmts = (arguments*)args;

    int clientSocket = argmts->clientSocket;

    if (clientSocket < 0) {
        close(clientSocket);
        cerr << "Connection Failed\n";
        return 0;
    }

    char buf[BUFFER_SIZE];
    memset(buf, 0, sizeof(buf)); 

    ssize_t bytes_rec = recv(clientSocket, buf, sizeof(buf),0);   //IP and port number of client

    if (bytes_rec > 0) {

        string request(buf,bytes_rec);

        vector<string> ip_port = tokenize(request,':');

        string client_ip_add = ip_port[0];
        int client_port_number = stoi(ip_port[1]);

        unique_lock<std::shared_mutex> lock(client_to_user_id_mutex);
        client_ip_port[clientSocket] = request;  //Store clients ip and port number

        cout << "Client with ip address " << client_ip_add << " and port number " << to_string(client_port_number) << " connected\n"; 


    } else {
        cerr << "Failed to receive data!" << '\n';
    }

    while(true){
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer)); 

        string req;

        ssize_t bytes_received = recv(clientSocket, buffer, sizeof(buffer),0);   //Receive bytes from client

        if (bytes_received > 0) {

            string request(buffer,bytes_received);

            string response;

            handleOperation(request,response,clientSocket);   //Handle client requests

            cout << response << '\n';

            send(clientSocket, response.c_str(), response.size(), 0);  //Send response to client
        } else {
            cerr << "Failed to receive data!" << '\n';
        }
    }

    pthread_mutex_unlock(&pthread_lock);

    return nullptr;
}

void* handleQuit(void *args){

    string tmp;
    
    while(!stopProgram){

        cin >> tmp;
        
        if(tmp=="quit"){
            stopProgram = true;
            break;
        }
    }

    pthread_exit(nullptr);
}

int main(int argc, char *argv[]){

    signal(SIGINT,handlerFunction);

    string log_file_path = "log.txt";

    remove(log_file_path.c_str());

    if(argc!=3){
        cerr << "Invalid arguments\n";
        return -1;
    }

    string file_path = argv[1];
    int tracker_no = stoi(argv[2]);

    vector<string> tracker_list = get_trackers_list(file_path);    //Gets tracker list

    string req_tracker = tracker_list[tracker_no-1]; //Required tracker

    vector<string> ip_port = tokenize(req_tracker,':');  //Tokenize to get IP address and port number

    string ip_addr = ip_port[0];
    int port_number = stoi(ip_port[1]);

    int trackerSocket = socket(AF_INET, SOCK_STREAM,0);  //Create a socket

    tracker_socket = trackerSocket;

    if(trackerSocket==0){
        close(trackerSocket);
        cerr << "Failed to create socket\n";
        return 0;
    }

    struct sockaddr_in trackerAddress; 

    memset(&trackerAddress,0,sizeof(trackerAddress));

    trackerAddress.sin_family = AF_INET;            //IPv4
    trackerAddress.sin_addr.s_addr = inet_addr(ip_addr.c_str());     
    trackerAddress.sin_port = htons(port_number); 

    if (bind(trackerSocket, (struct sockaddr*)&trackerAddress, sizeof(trackerAddress)) < 0) {
        cerr << "Failed to bind\n";
        close(trackerSocket);  
        return 0;
    }

    if (listen(trackerSocket, 5) < 0) {   //Listens for maxmimum 5 connections 
        cerr << "Listen failed\n";
        close(trackerSocket);  
        return 0;
    }

    cout << "Tracker is listening on port number " << port_number << "..." << '\n'; 

    //For quit()
    pthread_t quit_thread;
    if(pthread_create(&quit_thread,nullptr,handleQuit,nullptr)!=0){
        cerr << "Error creating thread\n";
        return -1;      
    }


    while(!stopProgram){
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        client_addr.sin_port;
        
        int clientSocket = accept(trackerSocket, (struct sockaddr*)&client_addr, &client_addr_len);  //Accets connection from client

        if(pthread_mutex_init(&pthread_lock, nullptr)!=0) { 
            cout << "Mutex init has failed\n";
            close(clientSocket);
            close(trackerSocket);
            return 1; 
        }

        arguments *argmts = new arguments;
        argmts->clientSocket = clientSocket;

        if(pthread_create(&(tid[0]),nullptr,handleClients,(void*)argmts)!=0){
            cerr << "Error creating thread\n";
            close(clientSocket);
            close(trackerSocket);
            return -1;      
        }
    }
    
    pthread_join(tid[0],nullptr);
    pthread_join(quit_thread,nullptr);
    pthread_mutex_destroy(&pthread_lock);
    close(trackerSocket);

    return 0;
}