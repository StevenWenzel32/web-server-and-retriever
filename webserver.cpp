#include <iostream>
#include <sys/types.h>    // socket, bind
#include <sys/socket.h>   // socket, bind, listen, inet_ntoa
#include <netdb.h>        // gethostbyname
#include <unistd.h>       // read, write, close, sleep
#include <strings.h>      // bzero
#include <netinet/tcp.h>  // SO_REUSEADDR
#include <errno.h>        // errno
#include <string.h>       // for memset
#include <filesystem>     // for exists
#include <fstream>        // for file stuff
#include <sstream>        // for stringstream stuff
#include <vector>         // for vectors, duh
#include <signal.h>       // for the shutdown signal
#include <fcntl.h>        // for fcntl -- to set non-blocking

using namespace std;

// port number the client will use to connect to server
#define PORT "2087"
// the repetition of sending the data within the buffers
#define REPETITION 20000
// the read buffer size
#define BUFFER_SIZE 2048
// # of connection requests for the server to listen to at a time, used in listen call
#define BACKLOG 10
// HTTP version
#define HTTP "HTTP/1.1"

// flah for shutting the server down -- ends the while loop
volatile sig_atomic_t shutdown_flag = 0;
vector<pthread_t> threads; 

// struct to pass data to threads
struct thread_data {
    int sd;
    int repetition;
};

// all of the error pages -- in a bigger nicer server in the real world these would be in seperate .html files in the web servers directory 
const string error400 = R"(
<html>
  <head>
    <title>400 Bad Request</title>
  </head>
  <body>
    <h1>400 Bad Request</h1>
    <p>The server could not understand your request. Please check the syntax of your request and try again.</p>
  </body>
</html>
)";

const string error401 = R"(
<html>
  <head>
    <title>401 Unauthorized</title>
  </head>
  <body>
    <h1>401 Unauthorized</h1>
    <p>You must be authenticated to access this file.</p>
  </body>
</html>
)";

const string error403 = R"(
<html>
  <head>
    <title>403 Forbidden</title>
  </head>
  <body>
    <h1>403 Forbidden</h1>
    <p>You do not have permission to access this file on the server.</p>
  </body>
</html>
)";

const string error404 = R"(
<html>
  <head>
    <title>404 Not Found</title>
  </head>
  <body>
    <h1>404 Not Found</h1>
    <p>The requested file could not be found on Steven Wenzel's webserver.</p>
    <p>This is my custom Not Found page.</p>
  </body>
</html>
)";

const string error405 = R"(
html>
  <head>
    <title>405 Method Not Allowed</title>
  </head>
  <body>
    <h1>405 Method Not Allowed</h1>
    <p>The method you are using is not allowed for the requested resource. Please check your request method.</p>
  </body>
</html>
)";

// a joke RFC -- currently not being checked for but will be added
const string error418 = R"(
<html>
  <head>
    <title>418 I'm a Teapot</title>
  </head>
  <body>
    <h1>418 I'm a Teapot</h1>
    <p>The server refuses to brew coffee because it is short and fat.</p>
    <p>⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⣤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣘⣿⣿⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⣀⣀⡀⠀⠀⠀⢀⣀⠘⠛⠛⠛⠛⠛⠛⠁⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⢠⡿⠋⠉⠛⠃⣠⣤⣈⣉⡛⠛⠛⠛⠛⠛⠛⢛⣉⣁⣤⣄⠀⠀⣾⣿⡿⠗⠀
⠀⢸⡇⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣆⠀⣿⣿⠀⠀⠀
⠀⢸⣇⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠟⢉⣉⣠⣿⣿⡀⠀⠀
⠀⠀⠙⠷⡆⠘⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠃⢰⣿⣿⣿⣿⣿⡇⠀⠀
⠀⠀⠀⠀⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠸⣿⣿⣿⣿⠟⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠙⠿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠄⠈⠉⠁⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⢄⣉⠉⠛⠛⠛⠛⠛⠋⢉⣉⡠⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⠻⠿⠿⠿⠿⠿⠿⠛⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀</p>
  </body>
</html>
)";

const string unknownCode = R"(
<html>
  <head>
    <title>??? I don't recognise this error</title>
  </head>
  <body>
    <h1>Unknown Error</h1>
    <p>The server does not understand the error code</p>
  </body>
</html>
)";

// handles the shut down of the server
void signalHandler(int signum) {
    cout << "Received signal: " << signum << ". Initiating shutdown..." << std::endl;
    shutdown_flag = 1;
}

// handle making the socket struct for listening 
// can later add in params to change the family and socktype and optional flags and port #
struct addrinfo* makeGetaddrinfo(){
    // for checking the return of getaddrinfo
    int status;
    // holds the info for the server address
    struct addrinfo server_addr;
    // points to the results that are in a linked list - is returned
    struct addrinfo *servinfo; 
    
    // create the struct and address info
    // make sure the struct is empty
    memset(&server_addr, 0, sizeof(server_addr));
    // doesn't matter if its ipv4 or ipv6
    server_addr.ai_family = AF_UNSPEC;
    // tcp stream sockets
    server_addr.ai_socktype = SOCK_STREAM;
    // fill in my IP for me 
    server_addr.ai_flags = AI_PASSIVE;

    // getaddrinfo and error check in one -- doesn't need an IP/host because this is for listening
    if ((status = getaddrinfo(NULL, PORT, &server_addr, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        exit(1);
    }
    
    return servinfo;
}

// make the socket and do error checks, and return the Sd
int makeListeningSocket(struct addrinfo* servinfo){
    // open a stream-oriented socket with the internet address family
    int serverSd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    // check if the socket call had an error
    if (serverSd == -1) {
        cerr << "error making the socket: serverSd - " << serverSd << endl;
    }

    // get the current flags
    int flags = fcntl(serverSd, F_GETFL, 0);
    // turn on the non-blocking flag
    fcntl(serverSd, F_SETFL, flags | O_NONBLOCK); 

    return serverSd;
}

// set the socket resue function to help free up unused sockets and ports
void setSocketReuse(int serverSd){
    // Enable socket reuse without waiting for the OS to recycle it
    // set the so-reuseaddr option
    const int on = 1;
    int success = setsockopt(serverSd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(int));
    // check if the option call failed
    if (success == -1) {
        cerr << "Error setting the socket reuse option: serverSd - " << serverSd << endl;
    }
}

// bind the socket
void bindSocket(int serverSd, struct addrinfo* servinfo){
    // Bind the socket to the port we passed into getaddrinfo
    int binding = bind(serverSd, servinfo->ai_addr, servinfo->ai_addrlen);
    // check if the bind had an error
    if (binding == -1) {
        cerr << "Error binding socket: serverSd - " << serverSd << " to port: " << PORT << endl;
    }
}

// listen on the socket
void listening(int serverSd, int backlog){
    // instruct the OS to Listen to up to N connection requests on the socket
    int listening = listen(serverSd, backlog);
    // check if listen has an error
    if (listening == -1) {
        cerr << "Error listening on socket: serverSd - " << serverSd << endl;
    } else {
        cout << "Server: Waiting for connections..." << endl;
    }
}

// read the request from the client
string readRequest(int sd){
    // string to hold and return the request
    string request;
    // "buffer" for reading in the server response
    char buffer[BUFFER_SIZE];
    while (true){
        int nRead = recv(sd, &buffer, BUFFER_SIZE - 1, 0);
        if (nRead == -1){
            cerr << "Error reading from socket: SD = " << sd << endl; 
            return ""; 
        } else if (nRead == 0) {
            cerr << "Client closed the connection" << endl;
            break;
        } 
        // null terminate th buffer to help other functions work right
        buffer[nRead] = '\0';
        // add what is read to the request
        request.append(buffer);

        // check for the end of the request and exit if found -- means we got the whole message
        if (request.find("\r\n\r\n") != string::npos){
            break;
        }
    }
    return request;
}

// parse the GET request: first = method, second = file
pair<string, string> parseRequest(string& msg){
    // make a stream for the msg
    stringstream ss(msg);
    // make the strings to be stored
    string method;
    string file;
    // needed to put the http string into, but is not used elsewhere
    string http;
    
    // break the status line into seperate vars and check if it worked -- can switch to using the (fail(), etc.) for more helpful error checks for the user 
    if (!(ss >> method >> file >> http)){
        // return the error code so it can be put in the response
        return {"400", "Bad Request"};
    }

    // check if the file has no . ending -- ie no .html -- checks for 400 error
    if(file.find_last_of('.') == string::npos){
        return {"400", "No file extension"};
    }

    return {method, file};
}

// check if the method is supported
int checkMethod(string method){
    if (method == "COFFEE"){
        return 418;
    } else if (method != "GET"){
        return 405;
    }
    return 200;
}

// build arbitrary forbidden and unauthed lists to check
pair<vector<string>, vector<string>> buildFileLists(){
    vector<string> forbidden = {"/MySecret.html", "/forbidden.html", "/nope.html"};
    vector<string> unauthed = {"/YourSecret.html", "/unauthed.html", "/definitelyNot.html"};
    return {forbidden, unauthed};
}

// pass in a list of files to check if the requested file matches
// return status code if applicable, else return 200 = OK
int checkFileLists(string file, vector<string> forbidden, vector<string> unauthed){
    // Check forbidden files
    string adjustedFile = file;
    if (file[0] == '/') {
        adjustedFile = file.substr(1);
    }

    for (const auto &forbiddenFile : forbidden) {
        if (adjustedFile == forbiddenFile) {
            return 403; // Forbidden
        }
    }
    
    // Check unauthed files
    for (const auto &unauthedFile : unauthed) {
        if (adjustedFile == unauthedFile) {
            return 401; // Unauthorized
        }
    }
    return 200;
}

// check if the file exists and return the proper code
// first = code, second = realPath
pair <int, string> checkForFile(const string &file){
    string rootPath = "/home/NETID/stevenw7/CSS-432/HTTP";
    // correct the path given
    string realPath = rootPath + file;

    // check if the file has no . ending -- ie no .html -- checks for 400 error
    if(realPath.find_last_of('.') == string::npos){
        return {400, realPath};
    }

    if (!(std::filesystem::exists(realPath))){
        return {404, realPath};
    }
    return {200, realPath};
}

// takes in a status code and returns the words after the code and the corresponding body if it's an error
// first = codeName, second = body
pair<string, string> checkCode(int code){
    string codeName;
    string body;
    // find what codename and body to return based on code
    if (code == 200){
        codeName = "OK";
        body = "";
    } else if (code == 401){
        codeName = "Unauthorized";
        body = error401;
    }
    // need error page for at least this one *****
    else if (code == 404){
        codeName = "Not Found";
        body = error404;
    } else if (code == 403){
        codeName = "Forbidden";
        body = error403;
    } else if (code == 405){
        codeName = "Method Not Allowed";
        body = error405;
    } else if (code == 400){
        codeName = "Bad Request";
        body = error400;
    }
    // joke RFC
    else if (code == 418){
        codeName = "I'm a teapot";
        // send an ANSCII picture of a teapot
        body = error418;
    }
    // code unknown
    else {
        codeName = "Unknown code";
        body = unknownCode;
    }
    return {codeName, body};
}

// make the reponse to send back to the retriever
// takes in the HTTP version, codeName, body of the message, and filepath
string buildResponse(string http, string code, string codeName, string body, string filepath){
    // check if the body is empty -- should be on a succesful request
    if (body.empty()){
        // open file
        ifstream file(filepath);
        stringstream buffer;
        // read the file and then put in the body
        buffer << file.rdbuf();
        body = buffer.str();
    }
    string msg = http + " " + code + " " + codeName + "\r\n\r\n" + body;
    return msg;
}

// send the msg and make sure that all data is sent
void sendAllData(int sd, string msg){
    int total = 0;
    // send repeatedly to ensure delivery
    while (total < msg.size()){
        int bytes_sent = send(sd, msg.c_str(), msg.size() - total, 0);
        if (bytes_sent == -1){
            cerr << "Problem with send" << endl;
            break;
        }
        total += bytes_sent;
    }
}

// close the socket and check for errors
void closeSocket(int sd){
    int bye = close(sd);
    if (bye == -1){
        cerr << "Error closing socket" << endl;
    }
}

// function that the threads call. reads the request, processes it and returns a response
// might have to change the format to use if checks after each check* method to return the error quicker and to avoid possible errors
void* processRequest(void *ptr) {
    // cast back into data struct
    struct thread_data *data = (thread_data*) ptr;

    string request = readRequest(data->sd);

    // get the method and file from the request -- checks for 400
    pair<string, string> requestParse = parseRequest(request);

    int code = 200;
    // if the parsing fails return 400
    if (requestParse.first == "400"){
        code = 400;
    }

    // check if the server supports the method -- check for 405 (and 418 if I have time)
    int methodCode = checkMethod(requestParse.first);
    // check if the code is 200
    if (code == 200){
        code = methodCode;
    }

    // build arbitrary forbidden and unauthed lists to check: first = forbidden, second = unauthed
    pair<vector<string>, vector<string>> fileLists = buildFileLists();
    // check if the file is in a forbidden or unauthed list -- checks for 401 and 403
    int forbiddenCode = checkFileLists(requestParse.second, fileLists.first, fileLists.second);
    // check if the code is 200
    if (code == 200){
        code = forbiddenCode;
    }

    // check if the server has the file -- checks for 404
    pair<int, string> fileStuff = checkForFile(requestParse.second);
    // check if the code is 200
    if (code == 200){
        code = fileStuff.first;
    }

    // get the codeName and body -- if all is well the code should still be 200
    pair<string, string> codeDetails = checkCode(code);

    // build the response
    string response = buildResponse(HTTP, to_string(code), codeDetails.first, codeDetails.second, fileStuff.second);

    // send response
    sendAllData(data->sd, response);

    // close after sending the response
    closeSocket(data->sd);

    // free thread data
    delete data;
    
    return nullptr;
}

// make a new thread and fill it's data struct
pthread_t makeThread(int sd, int reps){
    // create a new thread
    pthread_t new_thread;
    // create thread data
    struct thread_data *data = new thread_data;
    data->repetition = reps;
    data->sd = sd;

    // start the thread
    int status = pthread_create(&new_thread, NULL, processRequest, (void*) data);
    // check for thread creation error
    if (status != 0) {
        cerr << "Error making thread" << endl; 
        delete data;
    }

    return new_thread;
}

int acceptConnection(int serverSd){
    // connector's address information can be either IPv4 or IPv6
    struct sockaddr_storage their_addr;
    // size of clients address
    socklen_t their_AddrSize = sizeof(their_addr);
    // Accept the connection as a new socket
    int newSd = accept(serverSd, (struct sockaddr *)&their_addr, &their_AddrSize);
    // check if the connection was made properly
    if (newSd == -1) {
        // check if there are no pending connections -- not a real error 
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
        } else {
            // connect fails
            cerr << "Error accepting connection on socket: serverSd - " << serverSd << endl;
        }
    } else {
      //  cerr << "Connection made on socket: newSd - " << newSd << endl;
    }
    return newSd;
}

int main (int argc, char* argv[]) {
    // Set up signal handling for SIGINT and SIGTERM so that the server can shut down properly
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // make the structs needed for the sockets
    struct addrinfo* servinfo = makeGetaddrinfo();

    // Create the listening socket on serverSd
    int serverSd = makeListeningSocket(servinfo);
    
    // set resue of socket
    setSocketReuse(serverSd);

    // bind socket
    bindSocket(serverSd, servinfo);

    // listen on socket for up to BACKLOG connections
    listening(serverSd, BACKLOG);
    
    // free the linked list of addrinfos - done with it after the listening call
    freeaddrinfo(servinfo);
    
    while (!shutdown_flag) {
        // accept new client connection
        int newSd = acceptConnection(serverSd);
        // check if there is a new valid connection
        if (newSd != -1){
            // make thread and pass the process request func
            pthread_t new_thread = makeThread(newSd, REPETITION);
            // put thread in vector
            threads.push_back(new_thread); 
        }
    }

    // once shutting down join all the threads
    for (pthread_t thread : threads) {
        pthread_join(thread, NULL);
    }

    // can close once the shutdown signal is recieved
    closeSocket(serverSd);
    cout << "shutdown" << endl;
    return 0;
}
