#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/sha.h>
#include <sys/stat.h>
#include <string>
#include <vector>
#include <bits/stdc++.h>
#include <pthread.h>
#include <fstream>

using namespace std;
#define bufSize 1024
#define CHUNK 524288
#define BLOCK 524288
char msg[bufSize];
char rcvLine[bufSize];

struct RequestData
{
    struct UserData *userrequest;
    int socketFd;
};

struct UserData
{

    string userId;
    string password;
    vector<string> groups;
    bool isLoggedin;
    string ipAddress;
    int port;
    UserData()
    {
        isLoggedin = false;
    }
};

struct FileData
{

    string file;
    int fileLength;
    unordered_map<int, string> chunkSha;
    string sha1;

    unordered_map<int, set<pair<int, string>>> pieceDetails;
};

struct GroupData
{

    string admin;
    string id;
    int count;
    unordered_set<string> membersList;
    unordered_map<string, vector<string>> seedersList;
    unordered_set<string> requests;
    unordered_map<string, struct FileData> files;
};

unordered_map<string, UserData> userList;
unordered_map<string, GroupData> groupList;
unordered_map<string, unordered_set<string>> groupFiles; // groups listing files

void writeToClient(string res, int clientFd)
{
   
    memset(&msg, '\0', bufSize);

    strcpy(msg, res.c_str());
  
    int n = write(clientFd, msg, strlen(msg));
    if (n < 0)
        perror("ERROR! ");
}

void fileDetails(struct FileData filename)
{

    cout << filename.file << endl;
    cout << filename.fileLength << endl;
    cout << filename.sha1 << endl;
    for (auto x : filename.chunkSha)
    {
        cout << x.first << " " << x.second << endl;
    }
}

void printFiles(string grpid)
{

    for (auto x : groupList[grpid].files)
    {
        cout << x.second.file << endl;
    }
}

string calSHA(string chnk)
{

    const int len = chnk.length();
    unsigned char buffer[CHUNK + 1];

    strcpy((char *)buffer, chnk.c_str());
    unsigned char hashmsg[SHA_DIGEST_LENGTH];

    char data[50];

    string hash = "";

    unsigned char *temp = SHA1(buffer, len, hashmsg);

    for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
    {

        sprintf(data, "%02x", temp[i]);
        hash += data;
        memset(data, '\0', sizeof(data));
    }
    return hash;
}

int createUserAccount(string usrId, string pass)
{
    cout << "checkUserAccount --- " << usrId << endl;

    if (userList.find(usrId) != userList.end())
    {

        cout << "Username already there server: Can not create user" << endl;
        return 0;
    }
    else
    {
        UserData usr;
        usr.userId = usrId;
        usr.password = pass;
     
        userList[usrId] = usr;
        cout << "User created succesfully at the server" << endl;
        //cout<<"*"<<endl;
        // display_users();
        return 1;
    }
}

void display_users()
{

    clog << "------ Users --------" << endl;

    for (auto it : userList)
    {

        clog << it.first << " " << (it.second).password << " " << (it.second).ipAddress << " " << (it.second).port << " " << (it.second).isLoggedin << endl;
    }
    clog << "--------------------" << endl;
}

vector<string> activeUsers;

int login(string username, string pass, string &user)
{

    if (userList.find(username) == userList.end())
    {
        printf("%s\n", "Invalid user id! ");
        return 0;
    }
    else
    {
        UserData usr = userList[username];
      

        if (!usr.isLoggedin)
        {

            if (usr.password.compare(pass) == 0)
            {

                usr.isLoggedin = true;
             
                userList[username] = usr;
                activeUsers.push_back(username);

                printf("%s\n", "login successful");

                user = username;
          
            }
            else
            {

                printf("%s\n", "Error signing in");
            }
        }
        else
        {
            cout << "User already logged in" << endl;
        }
        display_users();
        return 1;
    }
}

int logout(string &username)
{

    if (username == "")
    {
        display_users();
        return 0;
    }
    else
    {
        userList[username].isLoggedin = false;
        auto it = find(activeUsers.begin(), activeUsers.end(), username);
        activeUsers.erase(it);

        username = "";
        display_users();

        return 1;
    }
}

int createGroup(string username, string grpid)
{

    if (groupList.find(grpid) != groupList.end())
    {
        cout << "Group already exists" << endl;
        return 0;
    }
    else
    {
        userList[username].groups.push_back(grpid);
        groupList[grpid].admin = username;

        return 1;
    }
}

int leaveGroup(string username, string grpid)
{
    // remove user from group
    auto it = find(userList[username].groups.begin(), userList[username].groups.end(), grpid);
    if (it != userList[username].groups.end())
    {
        userList[username].groups.erase(it); //remove group from user list
        if (groupList[grpid].admin == username)
        { // remove group if current user is admin
            groupList.erase(grpid);
            cout << "Group admin deleted group: " << grpid << endl;
        }
        return 1;
    }
    else
    {
        return 0;
    }
}

int findFileSize(string fname)
{

    char file[4096];
    strcpy(file, fname.c_str());
    struct stat st;
    stat(file, &st);
    return st.st_size; // returns size in bytes
}

int uploadFile(string fPath, string grpid, string &user, int clientFd)
{

    struct GroupData gt = groupList[grpid];
    string filename = fPath; // file path

    int n = fPath.length();

    while (n--)
    {

        if (fPath[n] == '/')
        {

            break;
        }
    }
    filename = fPath.substr(0, n); // file name

    if (gt.seedersList.find(filename) != gt.seedersList.end())
    {
        // filename exists in the seeders list
        if (find(gt.seedersList[filename].begin(), gt.seedersList[filename].end(), user) == gt.seedersList[filename].end())
        {
            gt.seedersList[filename].push_back(user);
        }
        return 1;
    }

    FILE *fp = fopen(fPath.c_str(), "rb");
    int fileSize = findFileSize(fPath);
    clog << "filesize:  " << fileSize << endl;
    int totalChunks = ceil((fileSize * 1.0) / CHUNK);
    clog << "totalChunks:  " << totalChunks << endl;
    char buffer[CHUNK];
    string hash = "";
    struct FileData fileinfo;
    int i = 1;
    while (i <= totalChunks)
    {
        fread(buffer, sizeof(char), CHUNK, fp);
        string res(buffer);
        string hash_chunk = calSHA(res);
        hash += hash_chunk;
        fileinfo.chunkSha[i] = hash_chunk;
        fileinfo.pieceDetails[i].insert(make_pair(0, user));
        memset(buffer, '\0', sizeof(buffer));

        i++;
    }

    fileinfo.sha1 = hash;
    fileinfo.file = filename;
    fileinfo.fileLength = fileSize;
    groupList[grpid].files[filename] = fileinfo;
    cout << "check " << fileinfo.fileLength << endl;
    fclose(fp);
    fp = NULL;

    fileDetails(fileinfo);

    printFiles(grpid);

    return 1;
}
void printPiece(FileData fd)
{
    clog << "Inside printPiece" << endl;
    for (auto x : fd.pieceDetails)
    {
        clog << x.first << endl;
        for (auto y : x.second)
            cout << y.first << " " << y.second << endl;
        clog << endl;
    }

    clog << "Outside printPiece" << endl;
}

void getTokens(string line, vector<string> &tokens)
{

    //cout<<"*****"<<endl;
    stringstream convert(line);

    string intermediate;

    // Tokenizing w.r.t. space ' '
    while (getline(convert, intermediate, ' '))
    {
        tokens.push_back(intermediate);
    }
    for (auto x : tokens)
    {
        cout << x << " " << endl;
    }
}
string pieceSelection(int pieceNum, string fname, string grpid)
{
    clog << "Entering pieceSelection" << endl;
    clog << "Selecting peer " + fname + " and chunk " + to_string(pieceNum) << endl;

    struct FileData fileinf = groupList[grpid].files[fname];
    clog << "Check in piece collection 1.2 " << endl;

    printPiece(groupList[grpid].files[fname]);
    auto reqPeer = fileinf.pieceDetails[pieceNum].begin();

    clog << "Check in piece collection 1.3 " << endl;
    pair<int, string> peer = *(reqPeer);

    clog << "Check in piece collection 1.5 " << endl;
    string userid = peer.second;
    int requests = peer.first + 1;

    clog << "Check in piece collection 2  " << endl;

    string ip = userList[peer.second].ipAddress;
    int port = userList[peer.second].port;

    clog << "Check in piece collection 3  " << endl;

    fileinf.pieceDetails[pieceNum].erase(reqPeer);
    fileinf.pieceDetails[pieceNum].insert({requests, userid});
    clog << "Selected Peer with ip and port : " << ip << " " << port << endl;
    clog << "Exit pieceSelection" << endl;
    return ip + " " + to_string(port);
}
void download_file(string grpid, string user, string fname, int sockfd)
{

    clog << "Entering download_file" << endl;
    char buffer[1024];

    while (true)
    {
        int st = read(sockfd, buffer, 1024);
        clog << "Successful read" << endl;

        if (st < 0)
        {
            perror("Could not read from peer server");
        }
        string msg = string(buffer);
        clog << "Message recieved from downloading peer : " << msg << endl;
        vector<string> tokens;
        getTokens(msg, tokens);
        int pieceNum = stoi(tokens[1]);
        clog << "pieceNum from " << pieceNum << endl;
        if (tokens[0] == "1")
        {
            clog << "Update Request " + user << endl;
            groupList[grpid].files[fname].pieceDetails[pieceNum].insert({0, user});
            writeToClient(tokens[1] + " piece info updated", sockfd);
        }
        else if(tokens[0] == "2")
        {
            clog << "Download Request " + to_string(pieceNum) + " of file " + fname << endl;
            string msg_client = pieceSelection(pieceNum, fname, grpid);
            clog << "Sent message to peer for download : " << msg_client << endl;
            int st = write(sockfd, msg_client.c_str(), msg_client.length());
            if (st < 0)
            {
                perror("Cannot write to peer server");
            }
        }else if(tokens[2] == "T"){
            break;
        }
    }
    clog << "Exiting download_file" << endl;
}



int joiningGroup(string gpid, string &user)
{
    if (groupList[gpid].requests.find(user) != groupList[gpid].requests.end())
        return 0;
    else
    {
        groupList[gpid].requests.insert(user);
        return 1;
    }
}

void *processClient(void *sId)
{
    cout << " processClient " << endl;

    memset(rcvLine, '\0', sizeof(rcvLine));
    string user = "";
    // int clientFd = *((int *)sId);
    // int clientFd = 1;
    cout << " ----------- " << endl;
    struct RequestData *peerDetails = (struct RequestData *)sId;
    int clientFd = peerDetails->socketFd;
    int clientPort = peerDetails->userrequest->port;
    cout << clientFd << " " << clientPort << endl;
    cout << " ----------- " << endl;
    // free(sId);
    while (true)
    {

      
        memset(&rcvLine, '\0', sizeof(rcvLine));
        int st = read(clientFd, rcvLine, 1024);
        if (st < 0)
        {
            perror("ERROR! ");
            exit(1);
        }
        printf("%s\n", rcvLine);
        string res(rcvLine);
        vector<string> v;
        getTokens(res, v);

        if (v[0] == "create_user")
        {
            string username = v[1];
            string pass = v[2];
            if (createUserAccount(username, pass))
                writeToClient("User created succesfully!", clientFd);
            v.clear();
        }
        else if (v[0] == "login")
        {

            string username = v[1];
            string pass = v[2];
            if (login(username, pass, user))
            {

                writeToClient("User logged in succesfully", clientFd);
                clog << "check   " << v[3] << " " << v[4] << endl;
                userList[username].ipAddress = v[3];
                clog << "**** " << userList[username].ipAddress << endl;
                userList[username].port = stoi(v[4]);
                clog << "**** " << userList[username].port << endl;
            }
            else
            {
                writeToClient("Error signing in !", clientFd);
            }

            v.clear();
        }
        else if (v[0] == "create_group")
        {

            string grpid = v[1];
            if (user == "")
            {
                writeToClient("you are not logged in! ", clientFd);
            }
            else
            {
                string username = user;
                if (createGroup(username, grpid))
                {
                    groupList[grpid].membersList.insert(username);
                    writeToClient("Group created successfully! ", clientFd);
                }
                else
                {
                    writeToClient("Group already exists! ", clientFd);
                }
            }
            v.clear();
        }
        else if (v[0] == "logout")
        {

            if (logout(user))
            {
                writeToClient("User succesfully logged out!", clientFd);
            }
            else
            {
                writeToClient("User not logged in!", clientFd);
            }
        }
        else if (v[0] == "list_groups")
        {
            if (groupList.empty())
            {
                writeToClient("No groups available! ", clientFd);
            }
            else
            {
                string listOfGroups = "";
                for (auto it = groupList.begin(); it != groupList.end(); it++)
                {
                    listOfGroups += (*it).first + "\n";
                }
                writeToClient(listOfGroups, clientFd);
            }
        }
        else if (v[0] == "list_requests")
        {
            if (groupList[v[1]].requests.empty())
            {
                writeToClient("No groups available! ", clientFd);
            }
            else
            {
                string listOfRequests = "";
                for (auto it = groupList[v[1]].requests.begin(); it != groupList[v[1]].requests.end(); it++)
                {
                    listOfRequests += (*it) + "\n";
                }
                writeToClient(listOfRequests, clientFd);
            }
        }

        else if (v[0] == "list_files")
        {
            if (groupList[v[1]].files.empty())
            {
                writeToClient("No groups available! ", clientFd);
            }
            else
            {
                string listOfRequests = "";
                for (auto it = groupList[v[1]].files.begin(); it != groupList[v[1]].files.end(); it++)
                {
                    listOfRequests += (it->first) + "\n";
                }
                writeToClient(listOfRequests, clientFd);
            }
        }
        else if (v[0] == "leave_group")
        {
            string grpid = v[1];
            if (user == "")
            {
                writeToClient("you are not logged in! ", clientFd);
            }
            else
            {
                string username = user;
                // check if user is admin for the group remove group from the list
                if (leaveGroup(username, grpid))
                {
                    writeToClient("You left the group! ", clientFd);
                }
                else
                {
                    writeToClient("You are not part of this group ", clientFd);
                }
            }
        }
        else if (v[0] == "download_file")
        {

            string grpid = v[1];
            string fname = v[2];
            string destfname = v[3];
            //user not logged in

            int fSize = groupList[grpid].files[fname].fileLength;
            int noPieces = ceil((1.0 * fSize / CHUNK));
            writeToClient(to_string(noPieces), clientFd);

            download_file(grpid, user, fname, clientFd);
        }
        else if (v[0] == "exit_network")
        {
            // remove user from active logins
            // see what else needs to be done
            // if user required to leave group and userList too?
            // get port no and then get corresponding user then remove from  active users list (not required)
            // remove from every list?
            //shareable files
            auto it = find(activeUsers.begin(), activeUsers.end(), user);
            activeUsers.erase(it);
            v.clear();
            return NULL;
        }
        else if (v[0] == "upload_file")
        {

            string filePath = v[1];
            string grpid = v[2];

            // bool flag = true; // if flag is true, then proceed to upload call
            // check if the group exists
            if (groupList.find(grpid) == groupList.end())
            {
                // flag = false;
                writeToClient("No group by this name exists!", clientFd);
                continue;
            }

            // check if user is a apart of this group
            if (groupList[grpid].membersList.find(user) == groupList[grpid].membersList.end())
            {
                writeToClient("You are not part of this group!", clientFd);
                continue;
            }

            // check if user is logged in
            if (find(activeUsers.begin(), activeUsers.end(), user) == activeUsers.end())
            {
                writeToClient("Please login to upload file!", clientFd);
                continue;
            }
            // call upload()
            if (uploadFile(filePath, grpid, user, clientFd))
            {
                writeToClient("File uploaded successfully!", clientFd);
            }
            else
            {
                writeToClient("", clientFd);
            }
        }
        else if (v[0] == "accept_request")
        {
            if (userList.find(v[2]) == userList.end())
            {
                writeToClient("User does not exists!", clientFd);
            }
            else if (groupList.find(v[1]) == groupList.end())
            {
                writeToClient("Group does not exists", clientFd);
            }
            else if (!userList[user].isLoggedin)
                writeToClient("User not logged in", clientFd);
            else if (groupList[v[1]].admin != user)
                writeToClient("You are not authorized to do so", clientFd);
            else
            {

                userList[v[2]].groups.push_back(v[1]);
                groupList[v[1]].requests.erase(v[2]);
                groupList[v[1]].membersList.insert(v[2]);
                writeToClient("Successful", clientFd);
            }
        }
        else if (v[0] == "join_group")
        {
            if (user == "")
                writeToClient("You are not logged in!", clientFd);
            else
            {
                if (joiningGroup(v[1], user))
                    writeToClient("Succesfully sent request", clientFd);
                else
                {
                    writeToClient("Could not send request", clientFd);
                }
            }
        }
    }
    return NULL;
}
int main(int argc, char *argv[])
{

    int noOfCon = 10;
    int optn = 1;

    int listenFD, clientFd, port;

    socklen_t clientLength;

    struct sockaddr_in serverAddress, clientAddress;
    int n;
    if (argc < 3)
    {
        printf("%s\n", "No port provided");
        exit(1);
    }

    listenFD = socket(AF_INET, SOCK_STREAM, 0);

    if (listenFD < 0)
    {
        perror("ERROR! ");
        exit(1);
    }

    if (setsockopt(listenFD, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &optn, sizeof(optn)))
    {

        perror("setsockopt");
        exit(1);
    }

    memset(&serverAddress, '\0', sizeof(serverAddress));
    port = atoi(argv[2]);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port);

    if (bind(listenFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("ERROR! ");
        exit(1);
    }

    int status = listen(listenFD, 10);
    if (status < 0)
    {

        perror("Error! ");
        exit(1);
    }

    clientLength = sizeof(clientAddress);

    while (true)
    {
        int clientPort;
        pthread_t threadId;
        clientFd = accept(listenFD, (struct sockaddr *)&clientAddress, &clientLength);
        if (clientFd < 0)
        {

            perror("ERROR! ");
            exit(1);
        }

        if (getsockname(clientFd, (struct sockaddr *)&clientAddress, &clientLength) == -1)
            perror("getsockname");
        else
            clientPort = clientAddress.sin_port;
        // printf("port number %d\n", ntohs(sin.sin_port));
        cout << " check -- 1 " << endl;
        // (struct RequestData*)
        struct RequestData *wrapper = (struct RequestData *)malloc(sizeof(*wrapper));
        cout << " check -- 2" << endl;
        wrapper->socketFd = clientFd;
        cout << " check -- 3" << endl;
        wrapper->userrequest = (struct UserData *)malloc(sizeof(*wrapper->userrequest));
        wrapper->userrequest->port = clientPort;
        cout << " check -- 5" << endl;
        pthread_create(&threadId, NULL, processClient, (void *)wrapper);
    }
    close(listenFD);
    return 0;
}