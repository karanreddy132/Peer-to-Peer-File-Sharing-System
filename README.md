**BIT-TORRENT - PEER TO PEER FILE SHARING SYSTEM**

- In this assignment I have implmented a ```Mini Bit-Torrent``` which is a peer-to-peer file sharing system. 

- It contains two folders ```client``` and ```tracker```.

- I have used one tracker which stores meta data and ```tracker_info.txt``` contains IP address and port number of the tracker 



## Steps to run:
### Tracker

- Run ```make``` in ```tracker``` directory

- Run ```./tracker tracker_info.txt <tracker_number>``` to start the tracker.

### Client

- Run ```make``` in ```client``` directory

- Run ```./client <client_ip>:<client_port_number> tracker_info.txt``` to start_client.

## I have implemented below operations:

- **Run Client** : ```./client <IP>:<PORT> tracker_info.txt```

- **Create User Account** : ```create_user <user_id> <passwd>```

- **Login** : ```login <user_id> <passwd>```
- **Create Group** : ```create_group <group_id>```
- **Join Group** : ```join_group <group_id>```
- **Leave Group** : ```leave_group <group_id>```
- **List pending join** : ```list_requests <group_id>```
- **Accept Group Joining Request** : ```accept_request <group_id> <user_id>```
- **List All Group In Network** : ```list_groups``` 

-  I have used global variables to store the user related data.

- For dowloading chunks from different peers I have used threads to request for chunks in parallel

- While uploading I have calculated hash chynk by chunk and then sent it to the tracker.

- Tracker stores the hash of each chunk by file name. So the number of chunks depends on file size. Here I have used chunks of size ```512KB```.

- When a peer received a chunk from another peer then a response is sent to tracker then the tracker verifies it and if a client has full file it becomes a seeder and added.
