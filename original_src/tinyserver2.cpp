#include <iostream> // cout, cerr
#include <fstream> // ifstream, ofstream
#include <sstream> // istringstream
#include <vector>
#include <sys/socket.h> //// socket(), bind(), accept(), listen()
#include <arpa/inet.h> // struct sockaddr_in, struct sockaddr, inet_ntoa()
#include <cstdlib> // atoi(), exit(), EXIT_FAILURE, EXIT_SUCCESS
#include <string.h> // memset()
#include <unistd.h> // close()
#include <sys/stat.h> // struct stat
#include <libgen.h>
#include <math.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/rsa.h>

#define QUEUELIMIT 5
#define MAX_REQUEST_LEN 2048
#define MAX_PATH_LEN 256
#define RESPONSE_LENGTH 8192
#define READ_BUFFER_LEN (2048 + 7)  // PATCH + \s + .... + \0

#define SERVER_SIGNATURE "tiny-server 1.0.0"

#define HEADER_200 "HTTP/1.1 200 OK\r\nContent-Type: %s; charset=UTF-8\nConnection: keep-alive\nServer: " SERVER_SIGNATURE "\n"
#define BODY_200 "Content-Length: %ld\n\n"

#define TEMPLATE_200 "HTTP/1.1 200 OK\r\nContent-Type: %s; charset=UTF-8\nConnection: keep-alive\nServer: " SERVER_SIGNATURE "\nContent-Length: %ld\n\n%s\n"
#define TEMPLATE_404 "HTTP/1.1 404 Not Found\r\nContent-Type: %s; charset=UTF-8\nConnection: keep-alive\nServer: " SERVER_SIGNATURE "\nContent-Length: %ld\n\n%s\n"
#define TEMPLATE_403 "HTTP/1.1 403 Forbidden\r\nContent-Type: %s; charset=UTF-8\nConnection: keep-alive\nServer: " SERVER_SIGNATURE "\nContent-Length: %ld\n\n%s\n"

#define DEFAULT_PAGE "<html><title>default page</title><body><h1>It Works!</h1></body>\n"

void wsBody(void);
void body(int);
inline void dispatch_get(const int, const char* params);
inline void dispatch_websocket(const int, const char* params, char* key);
inline void dispatch_post(const int, const char* params);
void digestSha1(void* buffer);
bool end_check(const char*, const int);
size_t bytecat(void*, void*, size_t, size_t);
int Base64Encode(char* dest, const char* message);
void printBuffer(void* buffer, size_t len);
std::vector<int> cliSocks;



typedef enum {
    UNKNOWN,
    GET,
    POST,
    PATCH,
    DELETE
} request_type;

int main(int argc, char** argv) {

    int servSock; // server socket descriptor
    int cliSock;  // client socket descripter

    struct sockaddr_in servSockAddr; // server internet socket address
    
    unsigned short servPort; // server port number

    if(argc != 2) {
        // Argument Error
        std::cerr << "Usage: " << argv[0] << " {portnum}" << std::endl;
        exit(EXIT_FAILURE);
    }

    if ((servPort = (unsigned short) atoi(argv[1])) == 0) {
        // Portnumber error
        std::cerr << "Invalid port number.\n" << std::endl;
        exit(EXIT_FAILURE);
    }
 
   
   if ((servSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0 ){
       // open socket
        perror("socket() failed.");
        exit(EXIT_FAILURE);
    }
   
   // setting server sock
   memset(&servSockAddr, 0, sizeof(servSockAddr));
   servSockAddr.sin_family      = AF_INET;
   servSockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
   servSockAddr.sin_port        = htons(servPort);

   // setting for reusable port
   // without this procedure, port will be binded for two minutes after this process is killed
   int yes = 1;
   if (setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes)) < 0) {
       perror("sockopt() failed.");
       exit(EXIT_FAILURE);
   }

   // reflect server setting and bind port
   if (bind(servSock, (struct sockaddr *) &servSockAddr, sizeof(servSockAddr) ) < 0 ) {
        perror("bind() failed.");
        exit(EXIT_FAILURE);
    }

   // listen
   if (listen(servSock, QUEUELIMIT) < 0) {
        perror("listen() failed.");
        exit(EXIT_FAILURE);
    }

    while(1) {
        // freeze by find client sock
        wsBody();
        body(servSock);
    }
    
    return EXIT_SUCCESS;

}

void wsBody() {
    static struct timeval timeout;
    static int rv;
    static fd_set set;
    static char read_buffer[READ_BUFFER_LEN]; // read buffer
    static const unsigned char text[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f}; // hello
    static int acceptLen;
    int i;
    timeout.tv_sec = 0;
    timeout.tv_usec = 900;
    for(auto sock : cliSocks) {
        // readのtimeout
        FD_ZERO(&set); /* clear the set */
        FD_SET(sock, &set); /* add our file descriptor to the set */
        rv = select(sock+1, &set, NULL, NULL, &timeout);
        if(rv <= 0) continue;
        acceptLen = read(sock, read_buffer, READ_BUFFER_LEN);
        if(acceptLen == 0)
            continue;
        read_buffer[acceptLen] = 0;
        std::cout << "accept:" << std::flush;
        printBuffer(read_buffer, acceptLen);
        size_t start = 0;
        while(start < acceptLen) {
            std::cout << start << "/" << acceptLen << std::endl;
            std::cout << (((read_buffer[start] >> 7)&1)? "is fin" : "cont") << std::endl;
            uint8_t opcode = (read_buffer[start] & 0xF);
            uint8_t mask  = (read_buffer[start+1] >> 7) & 0x1;
            unsigned long payloadlen = (read_buffer[start+1] & 0x7F);
            uint8_t payloadstart = start+2;
            if(payloadlen == 126) {
                payloadlen = (read_buffer[start+3] << 8) | read_buffer[start+2];
                payloadstart += 2;
            }
            if(payloadlen == 127) {
                payloadlen = (read_buffer[start+5] << 24) | read_buffer[start+4] << 16 | (read_buffer[start+3] << 8) | read_buffer[start+2];
                payloadstart += 4;
            }
            uint32_t maskkey = 0x00;
            if(mask) {
                maskkey = ((read_buffer[payloadstart+3]&0xFF) << 24) | (read_buffer[payloadstart+2]&0xFF) << 16 | ((read_buffer[payloadstart+1]&0xFF) << 8) | (read_buffer[payloadstart]&0xFF);
                printf("maskkey: 0x%02X 0x%02X 0x%02X 0x%02X\n", read_buffer[payloadstart]&0xFF, read_buffer[payloadstart+1]&0xFF, read_buffer[payloadstart+2]&0xFF, read_buffer[payloadstart+3]&0xFF);
                std::cout << "maskkey:" << maskkey << std::endl;
                payloadstart += 4;
            }
            std::cout << "payloadlen:" << payloadlen << std::endl;
            if(opcode == 0x0) {
                std::cout << "Continuous frame" << std::endl;
            }
            else if(opcode == 0x1) {
                std::cout << "Text frame" << std::endl;
                char* ptr = (char*)(&maskkey);
                if(mask) {
                    for(int i = 0; i < 4; ++i) {
                        printf("0x%02X ", ptr[i]&0xFF);
                    }
                    printf("\n");
                }
                for(unsigned long i = payloadstart; i < payloadstart + payloadlen; ++i) {
                    if(mask) {
                        printf("%c", (read_buffer[i]^((ptr[(i-payloadstart)%4]&0xFF)) & 0xFF));
                    }
                    else {
                        printf("%c", read_buffer[i]);
                    }
                }
                std::cout << std::endl;
                write(sock, text, sizeof(text));

            }
            else if(opcode == 0x2) {
                std::cout << "Binary frame" << std::endl;
            }
            else if(opcode == 0x8) {
                std::cout << "Open frame" << std::endl;
            }
            else if(opcode == 0x9) {
                std::cout << "ping" << std::endl;
            }
            else if(opcode == 0xA) {
                std::cout << "pong" << std::endl;
            }
            start = payloadstart + payloadlen;
        }
    }
}

void body(int servSock) {
    static int cliSock;
    static struct sockaddr_in cliSockAddr;  // client internet socket address
    static char read_buffer[READ_BUFFER_LEN]; // read buffer
    static unsigned int clientLen = sizeof(cliSockAddr);;   // client internet socket address length
    static int acceptLen;
    static request_type request;
    char* params = (char*) malloc(sizeof(char) * MAX_REQUEST_LEN);
    char* sesskey = (char*) malloc(sizeof(char) * 256);

    // timeout
    static struct timeval timeout;
    static int rv;
    static fd_set set;
    int i;
    timeout.tv_sec = 0;
    timeout.tv_usec = 900;

    FD_ZERO(&set); /* clear the set */
    FD_SET(servSock, &set); /* add our file descriptor to the set */
    rv = select(servSock+1, &set, NULL, NULL, &timeout);
    if(rv <= 0) {
        free(sesskey);
        free(params);
        return;
    }
    if ((cliSock = accept(servSock, (struct sockaddr *) &cliSockAddr, &clientLen)) < 0) {
        perror("accept() failed.");
        exit(EXIT_FAILURE);
    }
    request = UNKNOWN;
    std::cout << "connected from " << inet_ntoa(cliSockAddr.sin_addr) << "." << std::endl;
    acceptLen = read(cliSock, read_buffer, READ_BUFFER_LEN);
    int offset = 0;
    if(!strncmp(read_buffer, "GET", 3)) {
        request = GET;
        offset = 4;
    }
    else if(!strncmp(read_buffer, "POST", 4)) {
        request = POST;
        offset = 5;
    }
    else if(!strncmp(read_buffer, "PATCH", 5)) {
        request = PATCH;
        offset = 6;
    }
    else if(!strncmp(read_buffer, "DELETE", 6)) {
        request = DELETE;
        offset = 7;
    }
    for(i = 0; i < MAX_REQUEST_LEN && read_buffer[i+offset] && read_buffer[i+offset] != ' '; ++i) {
        params[i] = read_buffer[i+offset];
    }
    params[i] = 0;
    char* p = strstr(read_buffer, "Upgrade:");
    char* q = strstr(read_buffer, "Sec-WebSocket-Key:");
    if(q) {
        for(i = 0; i < 256 && q[19+i] != '\r' && q[19+i] != '\n'; ++i) {
            sesskey[i] = q[19+i];
        }
        sesskey[i] = 0;
    }
    read_buffer[acceptLen] = 0;
    std::cout << read_buffer << std::endl;
    while(!end_check(read_buffer, acceptLen)) {
        printf("%d:%d:%d:%d\n", read_buffer[acceptLen-4],  read_buffer[acceptLen-3], read_buffer[acceptLen-2], read_buffer[acceptLen-1]);
        // readのtimeout
        FD_ZERO(&set); /* clear the set */
        FD_SET(cliSock, &set); /* add our file descriptor to the set */
        rv = select(cliSock+1, &set, NULL, NULL, &timeout);
        if(rv <= 0) break;
        acceptLen = read(cliSock, read_buffer, READ_BUFFER_LEN);
        read_buffer[acceptLen] = 0;
        if(p == NULL) {
            p = strstr(read_buffer, "Upgrade:");
            q = strstr(read_buffer, "Sec-WebSocket-Key:");
        }
        if(q) {
            for(i = 0; i < 256 && q[19+i] != '\r' && q[19+i] != '\n'; ++i) {
                sesskey[i] = q[19+i];
            }
            sesskey[i] = 0;
        }
        if(read_buffer[acceptLen-1] == read_buffer[acceptLen-2] && read_buffer[acceptLen-1] == '\n')
            break;
    }
    // send message to client
    if(request == GET) {
        if(p == NULL) {
            dispatch_get(cliSock, params);
        }
        else {
            dispatch_websocket(cliSock, params, sesskey);
        }
    }
    else if(request == POST) {
        dispatch_get(cliSock, params);
    }
    else {
        free(params);
        write(cliSock, DEFAULT_PAGE, strlen(DEFAULT_PAGE));
        close(cliSock);
    }
    free(sesskey);
}

inline void dispatch_websocket(int cliSock, const char* params, char* key) {
    char *response = (char*)malloc(sizeof(char) * RESPONSE_LENGTH);
    char *path = (char*)malloc(sizeof(char) * MAX_PATH_LEN);
    char *pathcpy = (char*)malloc(sizeof(char) * MAX_PATH_LEN);
    char *contentType = (char*)malloc(sizeof(char) * 255);
    static char* b64 = (char*)malloc(sizeof(char) * 255);
    std::cout << "websocket:" << std::endl;
    strcat(key, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    std::cout << key << std::endl;
    digestSha1(key);
    std::cout << key << std::endl;
    Base64Encode(b64, key);
    std::cout << b64 << std::endl;
    sprintf(response, "HTTP/1.1 101 Switching Protocol\r\nUpgrade: websocket\r\nConnection: upgrade\r\nSec-WebSocket-Accept: %s\n\n", b64);
    std::cout << response << std::endl;
    std::cout << "End" << std::endl;
    write(cliSock, response, strlen(response));
    cliSocks.push_back(cliSock);
    free(response);
    free(path);
    free(pathcpy);
    free(contentType);
    free((void*)params);
}

inline void dispatch_get(int cliSock, const char* params) {
    int i;
    char *response = (char*)malloc(sizeof(char) * RESPONSE_LENGTH);
    char *path = (char*)malloc(sizeof(char) * MAX_PATH_LEN);
    char *pathcpy = (char*)malloc(sizeof(char) * MAX_PATH_LEN);
    char *contentType = (char*)malloc(sizeof(char) * 255);
    size_t content_size = 0;
    for(i = 0; i <= MAX_PATH_LEN && params[i] && params[i] != '?'; ++i) {
        path[i+1] = params[i];
    }
    path[0] = '.';
    path[i+1] = 0;
    strcpy(pathcpy, path);
    struct stat fstat;

    if(strcmp(path, "./") && !stat(path, &fstat)) {
        char* filename = basename(pathcpy);
        char* extension = strstr(filename, ".");
        if(strcmp(filename, "index.html"))
            strcpy(contentType, "image/x-icon");
        else 
            strcpy(contentType, "text/html");
        sprintf(response, HEADER_200, contentType);
        printf("%s", response);
        FILE *f = fopen(path, "r");
        content_size = fstat.st_size;    
        write(cliSock, response, strlen(response));
        sprintf(response, BODY_200,  content_size);
        printf("%s", response);
        write(cliSock, response, strlen(response));

        while(content_size) {
            size_t read_size = std::min(content_size, (unsigned long)RESPONSE_LENGTH);
            fread(response, 1, read_size, f);            
            write(cliSock, response, read_size);
            content_size -= read_size;
            
        }
        fclose(f);
    }
    else {
        strcpy(contentType, "text/html");
        sprintf(response, TEMPLATE_200, contentType, ((content_size == 0) ? strlen(DEFAULT_PAGE): content_size), DEFAULT_PAGE);
        write(cliSock, response, content_size == 0 ? strlen(response) : RESPONSE_LENGTH);
    }
    std::cerr << content_size << std::endl;
    close(cliSock);
    free(response);
    free(path);
    free(pathcpy);
    free(contentType);
    free((void*)params);
}

inline void dispatch_post(int cliSock, const char* params) {
    close(cliSock);
    free((void*)params);
}

bool end_check(const char* buffer, const int acceptLen) {
    if(acceptLen < READ_BUFFER_LEN) {
        return 1;
    }
    else {
        return (buffer[acceptLen-1] == buffer[acceptLen-2] && (buffer[acceptLen-1] == '\n' || buffer[acceptLen-1] == '\r')) ||
            (buffer[acceptLen-4] == buffer[acceptLen-2] && buffer[acceptLen-3] == buffer[acceptLen-1] && buffer[acceptLen-1] == '\n' && buffer[acceptLen-2] == '\r');
    }
}

size_t bytecat(void* dest, void* src, size_t dst_size, size_t src_size) {
    char* c_dest = (char*)dest;
    char* c_src = (char*)src;
    for(size_t i = 0; i < src_size; ++i) {
        c_dest[dst_size + i] = c_src[i];
    }
    return dst_size + src_size;
}


void digestSha1(void* buffer) {
    SHA_CTX c;
    unsigned char* buff = (unsigned char*) buffer;
    SHA1_Init(&c);
    SHA1_Update(&c, buffer, strlen((char*)buff));
    SHA1_Final(buff, &c);
}

int Base64Encode(char* dest, const char* message) { //Encodes a string to base64
  BIO *bio, *b64;
  FILE* stream;
  int encodedSize = 4*ceil((double)SHA_DIGEST_LENGTH/3);

  stream = fmemopen(dest, encodedSize+1, "w");
  b64 = BIO_new(BIO_f_base64());
  bio = BIO_new_fp(stream, BIO_NOCLOSE);
  bio = BIO_push(b64, bio);
  BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); //Ignore newlines - write everything in one line
  BIO_write(bio, message, SHA_DIGEST_LENGTH);
  BIO_flush(bio);
  BIO_free_all(bio);
  fclose(stream);
  return (0); //success
}

void printBuffer(void* buffer, size_t len) {
    char* p = (char*) buffer;
    for(size_t i = 0; i < len; ++i) {
        printf("0x%02X ", (0xFF&p[i]));
    }
    printf("\n");
}
