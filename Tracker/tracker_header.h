#include <vector>
#include <string>
#include<unordered_map>
#include<unordered_set>
#include <mutex>
#include <shared_mutex>

extern std::mutex user_credentials_mutex;
extern std::shared_mutex isLoggedin_mutex;
extern std::shared_mutex client_to_user_id_mutex;
extern std::shared_mutex user_to_client_id_mutex;
extern std::shared_mutex group_to_user_id_mutex;
extern std::shared_mutex group_users_mutex;
extern std::shared_mutex pending_join_req_mutex;
extern std::shared_mutex client_ip_port_mutex;
extern std::shared_mutex group_sharable_files_mutex;
extern std::shared_mutex seedersList_mutex;
extern std::shared_mutex file_chunk_hashes_mutex;
extern std::shared_mutex user_id_files_chunk_hashes_mutex;
extern std::shared_mutex user_id_files_mutex;

using namespace std;

extern unordered_map<string,string> user_credentials;  //Stores user credentials
extern unordered_set<string> isLoggedin;               //Keeps track of which users are logged in
extern unordered_map<int,string> client_to_user_id;    //client_id -> user_id
extern unordered_map<string,int> user_to_client_id;    //user_id -> client_id
extern unordered_map<int,string> group_to_user_id;     //Stores user id of group owners
extern unordered_map<int,unordered_set<string>> group_users;  //Stored which user is present in group
extern unordered_map<int,unordered_set<string>> pending_join_req;  //Stores pending requests for each group owner
extern unordered_map<int,string> client_ip_port;        //Stores clients ip and port number
extern unordered_map<int,unordered_set<string>> group_sharable_files;  //Contains group id mapped to all sharable group files
extern unordered_map<int,unordered_map<string,unordered_set<string>>> seedersList;  //Stores users who has the file
extern unordered_map<string,vector<string>> file_chunk_hashes;  //Stores chunk and file hashes last hash is file hash remaining are chunk hashes
extern unordered_map<string,unordered_map<string,vector<string>>> user_id_files_chunk_hashes; //user_id -> files -> chunk hashes
extern unordered_map<string,unordered_set<string>> user_id_files;   //user_id -> files
extern unordered_map<int,unordered_set<string>> group_downloaded_files;  //Group id -> downloaded files

vector<string> tokenize(string str, char delimiter);
vector<string> get_trackers_list(string file_path);
void activateTracker(string req_tracker);
void handleLogin(string request,string &response,int client_id);
void handleOperation(string request,string &response,int client_id);
void createUser(string request,string &response);
void handleCreateGroup(string request,string &response,int client_id);
bool checkIsLoggedIn(int client_id);
void handleLogout(string request,string &response,int client_id);
void handleJoinGroup(string request,string &response,int client_id);
void handleLeaveGroup(string request,string &response,int client_id);
void handleListRequests(string request,string &response,int client_id);
void handleAcceptRequest(string request,string &response,int client_id);
void handleListGroups(string request,string &response,int client_id);
void handleInvalidCommand(string &response);
void writeToLogFile(string response);
void handleUploadFile(string request,string &response,int client_id);
void handleListFiles(string request,string &response,int client_id);
void handleDownloadfile(string request,string &response,int client_id);
void handleCheckHash(string request,string &response,int client_id);
void handleStopStore(string request,string &response,int client_id);
void handleShowDownloads(string request,string &response,int client_id);
