#include<iostream>
#include<vector>
#include<string>
#include<cstring>
#include<fcntl.h>
#include<unistd.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<csignal>
#include<openssl/sha.h>
#include <iomanip>
#include<sys/stat.h>
#include<pthread.h>
#include<thread>
#include<unordered_map>
#include<mutex>
#include <sys/time.h>

#define BUFFER_SIZE 65536

#include "client_header.h"

using namespace std;

mutex chunk_hash_mutex;
mutex tracker_socket_mutex;
mutex file_read_mutex;

pthread_t tid[1];
pthread_mutex_t mutex_lock;

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

void* handlePeers(void *args){
    pthread_mutex_lock(&mutex_lock);

    arguments *argmts = (arguments*)args;

    int clientSocket = argmts->clientSocket;

    if (clientSocket < 0) {
        cerr << "Connection Failed\n";
        return 0;
    }

    while(true){
        //It connects with other peers
        sockaddr_in peer_addr;
        socklen_t peer_addr_len = sizeof(peer_addr);

        int peerSocket = accept(clientSocket, (struct sockaddr*)&peer_addr, &peer_addr_len);  //Accepts connection from peers

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer)); 

        ssize_t bytes_received = read(peerSocket, buffer, sizeof(buffer));   //Receive bytes from peer
    

        if (bytes_received > 0) {

            string request = string(buffer);

            vector<string> file_path_chunk_no = tokenize(request,' ');

            string file_path = file_path_chunk_no[0];  //Extract file path and chunk number to be sent
            int chunk_number = stoi(file_path_chunk_no[1]);

            int fd = open(file_path.c_str(),O_RDONLY);

            if(fd < 0){
                cerr << "Error opening the file\n";
                return nullptr;
            }

            char buf[512*1024];

            memset(buf,0,sizeof(buf));

            string response = "";

            ssize_t bytesRead;

            int chunk_no = 0;

            std::lock_guard<std::mutex> lock(file_read_mutex);
            while((bytesRead = read(fd,buf,sizeof(buf))) > 0){

                if(chunk_no == chunk_number){
                    send(peerSocket, buf, bytesRead, 0);
                    break;
                }

                chunk_no++;

                memset(buf,0,sizeof(buf));
            }  

            close(fd);       
            close(peerSocket);
        } else {
            cerr << "Failed to receive data!" << '\n';
        }
    }


    pthread_mutex_unlock(&mutex_lock);

    return nullptr;
}

string chunkHash(string chunk_data){

    unsigned char hash[SHA_DIGEST_LENGTH];

    stringstream ss;

    SHA1(reinterpret_cast<const unsigned char*>(chunk_data.c_str()), chunk_data.length(), hash);

    for(int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    string chunk_hash = ss.str();

    return chunk_hash;
}

void getChunk(string file_path,int chunk_number,string peer_ip_addr,int peer_port_number, unordered_map<int,pair<string,string>> &chunk_hash_data){
    int peerSocket = socket(AF_INET,SOCK_STREAM,0); 

    if(peerSocket==0){
        cerr << "Error connecting to peer socket\n";
        return;
    }

    struct sockaddr_in peerAddress;
    
    memset(&peerAddress,0,sizeof(peerAddress));
    
    peerAddress.sin_family = AF_INET;  //IPv4 addressing
    peerAddress.sin_addr.s_addr = inet_addr(peer_ip_addr.c_str());  
    peerAddress.sin_port = htons(peer_port_number);
        
        
    if(connect(peerSocket, (struct sockaddr*)&peerAddress, sizeof(peerAddress))<0){
        cout << "Failed to connect to peer with ip address " << peer_ip_addr << " and port number " << to_string(peer_port_number) << "\n";
        return;
    }

    cout << chunk_number << " -> " << peer_ip_addr << ":" << peer_port_number << "\n";

    string message = file_path + " " + to_string(chunk_number);

    send(peerSocket, message.c_str(), message.size(),0);

    string file_data = "";

    char buffer[512*1024];
    memset(buffer, 0, sizeof(buffer));

    ssize_t bytes_received;

    while((bytes_received = read(peerSocket, buffer, sizeof(buffer))) > 0){
        file_data.append(buffer, bytes_received); 
        memset(buffer, 0, sizeof(buffer));
    }

    string chunk_hash = chunkHash(file_data);

    std::lock_guard<std::mutex> lock(tracker_socket_mutex);
    chunk_hash_data[chunk_number] = {chunk_hash,file_data};

    close(peerSocket);
}


void handleDownloadFile(string res, int clientSocket,int group_id,string file_path,string dest_file_path){
    
    struct timeval start, end;

    gettimeofday(&start, NULL);

    string resp = res.substr(16);  //Extrack IP and port number of clients

    vector<string> chunks = tokenize(resp,';');    //Contains peer list of each chunk

    vector<thread> chunk_threads;   //Stores the threads where each thread fetches a chunk

    unordered_map<int,pair<string,string>> chunk_hash_data;  
    
    int max_threads = 10;  // Limit to 10 concurrent threads
    int active_threads = 0;  

    for(int i=0;i<chunks.size();i++){

        vector<string> chunk_no_ip_port = tokenize(chunks[i],'>');

        int chunk_number = stoi(chunk_no_ip_port[0]);
        
        vector<string> chunk_ip_port = tokenize(chunk_no_ip_port[1],',');    //Contains ip address and port number of each peer
        
        int random_peer_idx = i%chunk_ip_port.size();   //Select a random client
        
        vector<string> peer_ip_port = tokenize(chunk_ip_port[random_peer_idx],':');   //Tokenize ip port

        string peer_ip_addr = peer_ip_port[0];
        int peer_port_number = stoi(peer_ip_port[1]);

        if(active_threads < max_threads) {
            chunk_threads.push_back(thread(getChunk, file_path, chunk_number, peer_ip_addr, peer_port_number, std::ref(chunk_hash_data)));
            active_threads++;
        } else {
            for(auto& thr : chunk_threads) {
                thr.join();
            }
            chunk_threads.clear();
            active_threads = 0;  // Reset thread count
            chunk_threads.push_back(thread(getChunk, file_path, chunk_number, peer_ip_addr, peer_port_number, std::ref(chunk_hash_data)));
        }

        cout << "Percentage download = " << ((float)(i+1)*100.0)/(float)(chunks.size()) << "%" <<'\n';
    }

    for (auto& thr : chunk_threads) {
        thr.join();
    }

    chunk_threads.clear();

    //Now we have got all chunks create a file and append the data

    int dest_fd = open(dest_file_path.c_str(),O_WRONLY | O_APPEND | O_CREAT, 0644);  //Open file in append mode

    if(dest_fd < 0){
        cerr << "Error opening the file\n";
        return;
    }

    for(int i=0;i<chunks.size();i++){

        string chunk_hash = chunk_hash_data[i].first;   //chunk hash

        string message = "check_hash " + to_string(group_id) + " " + file_path + " " + to_string(i) + " " + chunk_hash;

        char buffer[BUFFER_SIZE];
        memset(buffer,0,sizeof(buffer));

        if (tracker_socket <= 0) {
            cerr << "Invalid tracker socket" << endl;
            return;
        }

        if (file_path.empty() || chunk_hash.empty()) {
            cerr << "Invalid file path or chunk hash" << '\n';
            return;
        }

        std::lock_guard<std::mutex> lock(tracker_socket_mutex);
        ssize_t bytes_sent = send(tracker_socket,message.c_str(),message.size(),0);

        if (bytes_sent == -1) {
            cerr << "Error sending message: " << strerror(errno) << endl;
            return;
        }

        ssize_t bytesReceived = read(tracker_socket,buffer,sizeof(buffer));

        if(bytesReceived < 0){
            cerr << "No response from tracker\n";
            return;
        }

        string response(buffer, bytesReceived);  //Response from tracker

        if(response=="TRUE"){

            string chunk_data = chunk_hash_data[i].second;

            int bytes_written = write(dest_fd,chunk_data.c_str(),chunk_data.size());
        }
        else{
            cout << "Chunk number " << i << " hash doen't match\n";
        }

    }

    vector<string> file_tokens = tokenize(file_path,'/'); 

    string fp = file_tokens[file_tokens.size()-1];

    cout << "\nFile " << fp << " download successfully\n";

    gettimeofday(&end, NULL);

    long seconds = end.tv_sec - start.tv_sec;
    long microseconds = end.tv_usec - start.tv_usec;
    double elapsed = seconds + microseconds*1e-6;

    cout << "Time taken to download = " << elapsed << " seconds" << '\n';

    close(dest_fd);

    cout << '\n';
}

//Calculates hash of file and chunks
string calculateFileChunkHashes(string file_path){

    string file_chunk_hash = "";

    int fd = open(file_path.c_str(),O_RDONLY);

    if(fd < 0){
        cerr << "Error opening file\n";
        return "";
    }

    char buffer[512*1024];  //Chunk size = 512KB
    memset(buffer,0,sizeof(buffer));

    ssize_t bytesRead = read(fd, buffer, sizeof(buffer));

    string file_data = "";

    int chunks_count = 0;

    while(bytesRead > 0){

        chunks_count++;

        string chunk_data(buffer, bytesRead);
        file_data+=string(buffer,bytesRead);

        //Chunk wise hash calculation
        unsigned char hash[SHA_DIGEST_LENGTH];

        stringstream ss;

        SHA1(reinterpret_cast<const unsigned char*>(chunk_data.c_str()), chunk_data.length(), hash);

        for(int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
            ss << hex << setw(2) << setfill('0') << (int)hash[i];
        }

        string chunk_hash = ss.str();

        file_chunk_hash = file_chunk_hash + chunk_hash + ":";   //Concatenate hash seperated by ":"

        memset(buffer,0,sizeof(buffer));
        bytesRead = read(fd, buffer, sizeof(buffer));
    }

    //File hash calculation
    unsigned char hash[SHA_DIGEST_LENGTH];

    stringstream ss;

    SHA1(reinterpret_cast<const unsigned char*>(file_data.c_str()), file_data.length(), hash);

    for(int i = 0; i < SHA_DIGEST_LENGTH; ++i) {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }

    string file_hash = ss.str();

    file_chunk_hash = file_chunk_hash + file_hash;

    cout << "\nTotal chunks = " << chunks_count << "\n";

    return file_chunk_hash;
}

int main(int argc, char *argv[]){

    signal(SIGINT,handlerFunction);

    if(argc!=3){
        cerr << "Invalid arguments\n";
        return -1;
    }
    
    string ip_port = argv[1];

    string file_path = "../tracker/" + string(argv[2]);

    vector<string> ip_portno = tokenize(ip_port,':');

    string client_ip_addr = ip_portno[0];
    int clinet_port_number = stoi(ip_portno[1]);

    int clientSocket = socket(AF_INET,SOCK_STREAM,0);  //Create client socket
    client_socket = clientSocket;

    if(clientSocket==0){
        cerr << "Failed to create client socket\n";
        return -1;
    }

    struct sockaddr_in clientAddress;

    memset(&clientAddress,0,sizeof(clientAddress));

    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = inet_addr(client_ip_addr.c_str());
    clientAddress.sin_port = htons(clinet_port_number);

    if (bind(clientSocket, (struct sockaddr*)&clientAddress, sizeof(clientAddress)) < 0) {
        cerr << "Failed to bind\n";
        close(clientSocket);  
        return 0;
    }

    if (listen(clientSocket, 5) < 0) {   //Listens for maxmimum 5 connections 
        cerr << "Listen failed\n";
        close(clientSocket);  
        return 0;
    }

    if(pthread_mutex_init(&mutex_lock, nullptr)!=0) { 
        cout << "Mutex init has failed\n";
        return 1; 
    }

    arguments *argmts = new arguments;
    argmts->clientSocket = clientSocket;

    //Handles request from other peers
    if(pthread_create(&(tid[0]),nullptr,handlePeers,(void*)argmts)!=0){
        cerr << "Error creating thread\n";
        return -1;      
    }

    //Connecting to tracker
    int fd = open(file_path.c_str(), O_RDONLY);

    if(fd==-1){
        cerr << "Failed to open the file\n";
        return -1;
    }

    vector<string> tracker_list = get_trackers_list(file_path);


    int no_of_trackers = tracker_list.size();

    vector<string> tracker_ip_port = tokenize(tracker_list[0],':');

    string tracker_ip_addr = tracker_ip_port[0];
    int tracker_port_number = stoi(tracker_ip_port[1]);

    int trackerSocket = socket(AF_INET,SOCK_STREAM,0);

    if(trackerSocket==0){
        cerr << "Failed to create client socket\n";
        return -1;
    }

    tracker_socket = trackerSocket;

    struct sockaddr_in trackerAddress;
    
    memset(&trackerAddress,0,sizeof(trackerAddress));
    
    trackerAddress.sin_family = AF_INET;  //IPv4 addressing
    trackerAddress.sin_addr.s_addr = inet_addr(tracker_ip_addr.c_str());  
    trackerAddress.sin_port = htons(tracker_port_number);
        
        
    if(connect(trackerSocket, (struct sockaddr*)&trackerAddress, sizeof(trackerAddress))<0){
        cout << "Failed to connect to tracker\n";
        return -1;
    }

    cout << "Server is listening on port number " << tracker_port_number << "\nConnected to tracker!!!\n";

    string ip_port_no = client_ip_addr + ":" + to_string(clinet_port_number);

    send(trackerSocket, ip_port_no.c_str(), ip_port_no.size(),0);  //Send client ip and port number once connection is established

    
    while(true){

        cout << "\n>>";
        
        string message;

        getline(cin, message);

        vector<string> cmds = tokenize(message,' ');

        if(cmds[0]=="upload_file"){

            string file_path = cmds[1];

            string hashes = calculateFileChunkHashes(file_path);

            message = message + " " + hashes;
        }
        
        send(trackerSocket, message.c_str(), message.size(),0);   //Send message to server

        char buffer[BUFFER_SIZE];
        memset(buffer, 0, sizeof(buffer));

        ssize_t bytes_received = read(trackerSocket, buffer, sizeof(buffer));   //Receive response from server
        
        if(cmds[0]=="download_file"){
            string resp = buffer;

            int group_id = stoi(cmds[1]);

            if(resp.substr(0,15)=="status_code=200"){
                handleDownloadFile(resp,clientSocket,group_id,cmds[2],cmds[3]);
            } 

            continue;
        }

        if (bytes_received > 0) {
            cout << buffer << '\n';
        } else {
            cerr << "No data received!!!" << endl;
        }
    }

    pthread_join(tid[0],nullptr);
    pthread_mutex_destroy(&mutex_lock); 
    
    close(trackerSocket);
    close(clientSocket);

    return 0;
}