#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<string.h>
#include <netdb.h> 
#include<sys/types.h>
#include<unistd.h>
#include<stdlib.h>
#include <signal.h>
#include <pthread.h>
#include<string>
#include<iostream>
#include<bits/stdc++.h>
using namespace std;


#define bufLen 512
#define bufSize 1024
#define BLOCK 524288
struct downloadstruct{
    string ip;
    string port;
    string fileName;
    string destFilename;
    int part;
};

struct connectionPair
{
string ip;
int port;
};

vector<string> getTokens(string line){

    vector<string> tokens;
    stringstream convert(line); 
      
    string intermediate; 
      
     
    while(getline(convert, intermediate, ' ')) 
    { 
        tokens.push_back(intermediate); 
    } 
    // for(auto x:tokens){
    //     cout<<x<<" "<<endl;
    // }

    return tokens;

}
pair<int,string> blockRead(int partNo,string fileN){

    char buffer[BLOCK];
    FILE *fptr = fopen(fileN.c_str(),"r");
    if(fptr == NULL)
        clog<<"The file could not be opened"<<endl;
    int pos = (partNo - 1)*BLOCK;
    fseek(fptr,pos,SEEK_SET);
    int sze = fread(buffer,sizeof(char),BLOCK,fptr);
    fclose(fptr);
    fptr = NULL;

    return make_pair(sze, string(buffer));
}

void blockWrite(int blockNum,string bufferToString,string fname){
   
    FILE *fptr = fopen(fname.c_str(),"r+");
    if(fptr == NULL)
        clog<<"The file could not be opened"<<endl;
    int off = (blockNum - 1)*BLOCK;
    fseek(fptr,off,SEEK_SET);
    int sze = fwrite(bufferToString.c_str(),sizeof(char),bufferToString.length(),fptr);
    fclose(fptr);
    fptr = NULL;
    //clog<<"Exiting block write\n";
    return ;
}
void* servePeerHandler(void* sockfd)
{   
    //clog<<"Entering serverPeerHandler\n";
    struct  connectionPair client;
    int newsockfd=*((int*)sockfd);
    
    char bufferinit[1024];
    memset(bufferinit,'\0',sizeof(bufferinit));
    free(sockfd);
    //clog<<"peer server read\n";
    ssize_t readst = read(newsockfd, bufferinit, 1024);
    //cout<<buffer<<endl;
    if(readst<0)
        perror("Cannot read for peer-client!");
    
    //clog<<"client server"<<endl;
    vector<string> info = getTokens(string(bufferinit));
    pair<int,string> myp = blockRead(stoi(info[0]),info[1]);
    char buffer[BLOCK];
    strcpy(buffer,myp.second.c_str());
  
    int st = write(newsockfd,buffer,sizeof(buffer));
    if(st<0)
        perror("Couldn't write to client peer");
 
    return NULL;
}
void* servePeer(void* client_server)
{    
    struct connectionPair  connection_pair = *((struct connectionPair *)client_server);
    string port = to_string(connection_pair.port);
    string ip = connection_pair.ip;
    int noOfCon = 10;
    int optn = 1;

    int listenFD, clientFd;

    socklen_t clientLength;

    struct sockaddr_in serverAddress, clientAddress;
    int n;

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
    int cport = stoi(port);
    serverAddress.sin_family = AF_INET;
    int inet_status = inet_pton(AF_INET,ip.c_str(),&serverAddress.sin_addr);
    serverAddress.sin_port = htons(cport);

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
        
        int *sockfdh = (int *)malloc(sizeof(int));
        *sockfdh = clientFd;
        pthread_create(&threadId,NULL,servePeerHandler,sockfdh);
    }
    close(listenFD);
}


int setConnection(string ip,string port)
{    	

    struct sockaddr_in serverAddress;
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    int port_no = stoi(port);
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port_no);
    char buffer[1024];
    strcpy(buffer,ip.c_str());
    int it = inet_pton(AF_INET,buffer,&serverAddress.sin_addr);
    int cs = connect(sockfd,(struct sockaddr *)&serverAddress, sizeof(serverAddress));
    return sockfd;
}

//downloadFileHandlerPeer

void *downloadFileHandlerPeer(void *info)
{

    struct downloadstruct det = *((struct downloadstruct*)info);
    string ip = det.ip;
    string port = det.port;
    int part = det.part;
    string fname = det.fileName;
    string destfilename = det.destFilename; 


    int sockfd = setConnection(ip,port);
   
    char buffer1[1024];
    string message = to_string(part) + " " + fname;
    strcpy(buffer1,message.c_str());
    int status = write(sockfd,buffer1,sizeof(buffer1));
    if(status<0)
    {
        perror("Write error");    
    }
    
    char buffer[BLOCK];
    memset(buffer,'\0',sizeof(buffer));
    status = read(sockfd,buffer,sizeof(buffer));
    if(status<0)
    {
        perror("read chunk error");
    }
    blockWrite(part,string(buffer),destfilename);
    return NULL;
}


void downloadFile(int trackersockfd ,int noParts,string fname, string destfname)
{
 
    for(int i=1;i<=noParts;i++)
    {   

        char buffer[1024];
        memset(buffer,'\0',sizeof(buffer));
        string msgToClient="2 "+ to_string(i) + " F"; 
        int st; 

        st = write(trackersockfd,msgToClient.c_str(), msgToClient.length()) ;
        if(st<0)
        {
            perror("Cannot write to server in download_file ");
        }  
        st = read(trackersockfd,buffer,sizeof(buffer));
        if(st < 0){
            perror("error reading from server");
        }
        vector<string> vec = getTokens(string(buffer));

        struct downloadstruct downloadInfo;
        downloadInfo.ip = vec[0];
        downloadInfo.port= vec[1];
        downloadInfo.part=i;
        downloadInfo.fileName=fname;
        downloadInfo.destFilename=destfname;

        pthread_t d_thread;
        struct downloadstruct *downloadInfo_heap =(struct downloadstruct*) malloc(sizeof(downloadstruct)) ;
        *downloadInfo_heap = downloadInfo;
        
      //  clog<<"Thread created for download"<<endl;

        pthread_create(&d_thread,NULL,downloadFileHandlerPeer,downloadInfo_heap);
        void *retval;
        pthread_join(d_thread,&retval);
     
        if(i == noParts){
            msgToClient = "1 " + to_string(i) + " T";
        }else{
            msgToClient = "1 " + to_string(i) + " F";
        }
        int write_status = write(trackersockfd,msgToClient.c_str(),msgToClient.length());
        if(write_status < 0){
            perror("Cannot write to Server");
        }
        memset(buffer,'\0',sizeof(buffer));

        int read_status = read(trackersockfd,buffer,1024);
        if(read_status < 0){
            perror("Cannot read Server");
        }
   
    }


}

int main(int argc, char *argv[] )
{

    if(argc != 4){
        printf("%s\n","Pass correct arguments" );
        exit(1);
    }

    char msg[bufSize];
    char recvLine[bufSize];
    memset(&recvLine,'\0',bufSize);
    struct hostent *host;

    int sockFD,retVal,port,n;
    // int sockfd, portno, n;
    struct sockaddr_in serverAddress;
    // struct hostent *server;
    sockFD = socket(AF_INET, SOCK_STREAM,0);

    if(sockFD < 0){
        perror("Unable to create a socket");
    }
    port = atoi(argv[3]);
    int portC = atoi(argv[2]); //port number this client ll listen to
 
    host = gethostbyname(argv[1]);
    struct connectionPair  conn;
    conn.port=portC;
    conn.ip=argv[1];
    
    if (host == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
  
    bzero((char *) &serverAddress, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;

    bcopy((char *)host->h_addr, 
        (char *)&serverAddress.sin_addr.s_addr,
        host->h_length);

    serverAddress.sin_port = htons(port);
    //serverAddress.sin_addr.s_addr = INADDR_ANY;

    retVal = connect(sockFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress));  // if connect returns 0, evrything worked fine and connection is established, -1 - not succussful
    if(retVal<0){
        perror("Error !");
    }
     pthread_t peerServerThread;

        struct connectionPair *client = (struct connectionPair*)malloc(sizeof(struct connectionPair));
        *client = conn;
        pthread_create(&peerServerThread,NULL,servePeer,client);

    while(true){
        
        // char buffer[1024];
        string str;
        printf("Enter: ");
    
        memset(&msg,'\0',1024);
        // memset(&recvLine,'\0',1024);

     
        getline(cin,str);
        vector<string> tokens = getTokens(str);
        if(str == "exit_network"){
            strcpy(msg,str.c_str());
            if(write(sockFD,msg,strlen(msg))<0){
                perror("ERROR! ");
                exit(1);
            }

          //  cout<<"leaving network...Bye!"<<endl;
            return 0;
        }
        if(tokens[0] == "login")
        {
            str = str + " " + argv[1] + " " + argv[2];
        }

        strcpy(msg,str.c_str());
        n = write(sockFD,msg,strlen(msg));
        if (n < 0) 
            perror("ERROR! ");
        memset(&msg,'\0',1024);
        n = read(sockFD,msg,1024);
        
        if (n < 0) 
            perror("ERROR! ");

        if(tokens[0].compare("download_file")==0)
        {
            string msgs(msg);
            string fname = tokens[2];
            string destfname=tokens[3];
            int numPiece = stoi(msgs);
            //cout<<"Calling download_file"<<endl;
            downloadFile(sockFD,numPiece,fname,destfname);
            //cout<<"Downloads ended \n";
        }
        printf("%s\n",msg); 
    }
    close(sockFD);
    return 0;
}
