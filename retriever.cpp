#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <sys/types.h>    // socket, bind
#include <sys/socket.h>   // socket, bind, listen, inet_ntoa
#include <unistd.h>       // read, write, close
#include <strings.h>      // bzero
#include <netinet/tcp.h>  // SO_REUSEADDR
#include <string.h>       // memset
#include <errno.h>        // errno
#include <fstream>        // ofstream for file creation
#include <netdb.h>        // gethostbyname
#include <sstream>        // for stringstream stuff

// the repetition of sending the data within the buffers
#define REPETITION 20000
// the read buffer size
#define BUFFER_SIZE 2048
// HTTP version
#define HTTP "HTTP/1.1"

using namespace std;

// split the domain from the file name
// first = domain, second = file
// might want to add in checks to see if it ends with a / and starts with www.
pair<string, string> parseHost(string& URL){
    // find the start of the file
    int startFile = URL.find('/');
    // If not found make the URL the domain
    if (startFile == string::npos) {
        // make the file /
        return {URL, "/"};
    }
    // assign first part of input
    string domain = URL.substr(0, startFile);
    // assign last part
    string file = URL.substr(startFile);

    return {domain, file};
}

// build the HTTP GET request s

// returns the returns the request msg just in case it's needed, how polite!
string buildRequest(string method, string domain, string file){
      // very basic request
      string msg = method + " /" + file + " " + HTTP + "\r\n" + "Host: " + domain + "\r\n\r\n";
      return msg;
}

// send the msg and make sure that all data is sent
void sendAllData(int clientSd, string msg){
    int total = 0;
    // send repeatedly to ensure delivery
    while (total < msg.size()){
        int bytes_sent = send(clientSd, msg.c_str(), msg.size() - total, 0);
        if (bytes_sent == -1){
            cerr << "Problem with send" << endl;
            break;
        }
        total += bytes_sent;
    }
}

// handle making the socket structs
// can later add in params to change the family and scoktype
struct addrinfo* makeGetaddrinfo(string serverIp, string port){
    // for checking the return of getaddrinfo
    int status;
    // holds the info for the client address
    struct addrinfo client_addr;
    // points to the results that are in a linked list - is returned
    struct addrinfo *servinfo; 
    
    // create the struct and address info
    // make sure the struct is empty
    memset(&client_addr, 0, sizeof client_addr);
    // doesn't matter if its ipv4 or ipv6
    client_addr.ai_family = AF_UNSPEC;
    // tcp stream sockets
    client_addr.ai_socktype = SOCK_STREAM;
    
    // getaddrinfo with error check
    if ((status = getaddrinfo(serverIp.c_str(), port.c_str(), &client_addr, &servinfo)) != 0 ) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    
    return servinfo;
}

// make the socket  and do error checks, and return the Sd
int makeSocket(struct addrinfo* servinfo){
    // open a stream-oriented socket with the internet address family
    int clientSd = socket(servinfo->ai_family, servinfo->ai_socktype, 0);
    // check for error
    if(clientSd == -1){
        cerr << "failed to make socket" << endl;
    }
    
    return clientSd;
}

// connect the socket to the server and do error checks. frees addrinfo list - done with connections
void connectSocket(int clientSd, struct addrinfo* servinfo){
    int connectStatus = connect(clientSd, servinfo->ai_addr, servinfo->ai_addrlen);
    // check for error
    if(connectStatus == -1){
        cerr << "failed to connect to the server" << endl;
    }
    
    // free the linked list -- all done with our connections 
    freeaddrinfo(servinfo);
}

// recieve the responses from the server
string readResponse(int clientSd){
    // string to hold and return the response
    string reply;
    // "buffer" for reading in the server response
    char buffer[BUFFER_SIZE];
    int nRead = 0;
    while ((nRead = recv(clientSd, &buffer, BUFFER_SIZE, 0)) > 0){
        if (nRead == -1){
            cerr << "Error reading from socket: clientSd - " << clientSd << endl;  
            break;
        }
        reply.append(buffer, nRead);
    }
    return reply;
}

// parse the status line in the server response to get the status code and msg body
// first = code, second = body
pair<int, string> parseCode(string &reply){
    // make a stream for the msg
    stringstream ss(reply);
    // needed to put the http string into, but is not used elsewhere
    string http;
    int code;
    string codeName;
    
    // break the status line into seperate vars and check if it worked -- can switch to using the (fail(), etc.) for more helpful error checks for the user 
    if (!(ss >> http >> code >> codeName)){
        cerr << "Error: Response Status Line Format is Wrong." << endl;
        // return the error code so it can be put in the response
        return {400, "Bad Request"};
    }
    
    // find the start of the body
    int startBody = reply.find("\r\n\r\n");
    // grab the msg body -- the 4 moves past the \r\n\r\n
    string body = reply.substr(startBody + 4);
    return {code, body};
}

// check for errors in server response and handle the error codes
// return -1 on error and 1 on success
int checkResponseCode(int code){
    if (code == 200){
        cerr << "Ok. Code: " << code << endl;
        return 1;
    } else if (code == 401){
        cerr << "Error: Unauthorized. Code: " << code << endl;
        return -1;
    } else if (code == 404){
        cerr << "Error: Not found. Code: " << code << endl;
        return -1;
    } else if (code == 403){
        cerr << "Error: Forbidden. Code: " << code << endl;
        return -1;
    } else if (code == 405){
        cerr << "Error: Method not allowed. Code: " << code << endl;
        return -1;
    } else if (code == 400){
        cerr << "Error: Bad request. Code: " << code << endl;
        return -1;
    }
    // joke code
    else if (code == 418){
        cerr << "Error: I'm a teapot. Code: " << code << endl;
        return -1;
    }
    // code unknown
    else {
        cerr << "Error: Unknown response. Code: " << code << endl;
        return -1;
    }
}

// process the response
void processResponse(string &reply){
    pair<int, string> responseParse = parseCode(reply);

    //check the HTTP response code after parsing
    if (checkResponseCode(responseParse.first) == 1){
        // put wanted file into file system -- only HTML right now
        ofstream file("index.html");
        file << responseParse.second;
        file.close();
    } else {
        // print error page to terminal if one was sent -- just 404 right now I think **
        cout << responseParse.second << endl;
    }
}

// close the socket and check for errors
void closeSocket(int sd){
    int bye = close(sd);
    if (bye == -1){
        cerr << "Error closing socket" << endl;
    }
}

// send get request to a web server
int main (int argc, char* argv[]) {
    // check that the command line the right # of params
    if (argc < 4){
        cerr << "Error: Not enough parameters passed in. Usage: " << argv[0] << " <PORT>, <Method>, <Host/URL>\n";
        return 1;
    }

    // params passed in through command line
    string port = argv[1];
    string method = argv[2];
    string url = argv[3];

    // parse the input
    pair<string, string> hostParse = parseHost(url);

    // build
    string msg = buildRequest(method, hostParse.first, hostParse.second);

    // make the socket structs and error check
    // pass in serverIP/domain -- getaddrinfo does the DNS lookup for us, how nice!
    struct addrinfo* servinfo = makeGetaddrinfo(hostParse.first, port);

    // make the socket
    int clientSd = makeSocket(servinfo);

    // connect the socket to the server
    connectSocket(clientSd, servinfo);

    // ensure all the data in the msg is sent
    sendAllData(clientSd, msg);

    // recieve the server response
    string reply = readResponse(clientSd);

    // process server response
    processResponse(reply);
    
    // call that handles error checks in other function
    closeSocket(clientSd);

    return 0;
}