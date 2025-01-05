#ifndef WEBSERVER_H
#define WEBSERVER_H
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <regex.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <threads.h>
#include <time.h>
#include <unistd.h>
#define set404(x) page404 = x
/* Specifies a 404 Page

  If left undefined a default page will be used. */
char *page404 = NULL;
const char *errorPage = "<html><body>404 Not Found</body></html>";

int port; // Port number eg. 3000

/* Contains the Response data from the webserver. */
typedef struct res_ {
  char *contentType;
  char *body; // header
  struct Content {
    char *filePath;
    char *data;
  } content;
  int statusCode;
} Response;

/* Contains the Request data from the webserver.*/
typedef struct req_ {
  const char *method;

  const char *urlRoute;
  const char *path;
  const char *baseUrl;

  const char *fileType;
  const char *param;
  const char *query;

  const char *body;
} Request;

/* Holds user specified handler functions for each HTTP method.*/
typedef struct Values_ {
  void (*GET)(Request *req, Response *res);  // Function pointer for GET
  void (*POST)(Request *req, Response *res); // Function pointer for POST
} Values;

typedef enum { GET, POST } Method;

/* Node for the binary tree

  Maps a http request (char *key) to a filepath (char *path).
  Key can be a Regex expression, however, path has to be NULL. */
struct Route {
  char *key;
  char *path;
  Values *values;
  struct Route *left, *right;
};

// WARNING: Do not confuse "root" with "route"
struct Route *root = NULL; // This is the head of the binary tree

/* @private
 * Creates a node in the binary tree.
 * This returns a node and therefore does not decide where the node
 * will be in the b-tree, that is left to addRouteWorker */
struct Route *initRoute(char *key, char *path, Values *value) {
  struct Route *newRoute = (struct Route *)malloc(sizeof(struct Route));

  newRoute->key = key;
  newRoute->path = path;
  newRoute->values = value;
  newRoute->left = newRoute->right = NULL;

  if (root == NULL) {
    root = newRoute;
  }
  return newRoute;
}

/* Prints the contents (nodes and their respective data) of the b-tree.*/
void inorderWorker(struct Route *head) {
  if (head != NULL) {
    inorderWorker(head->left);
    printf("%s -> %s \n", head->key, head->path);
    inorderWorker(head->right);
  }
}
void inorder() { inorderWorker(root); }

/* @private Recurses through the b-tree, comparing lexicographically,
   It will either find a duplicate or call initRoute to create a node.*/
struct Route *addRouteWorker(struct Route *head, char *key, char *path,
                             Values *values) {
  if (head == NULL) {
    return initRoute(key, path, values);
  }

  if (strcmp(key, head->key) == 0) {
    printf("Warning!!! Route for %s already exists\n", key);
  } else if (strcmp(key, head->key) > 0) {
    head->right = addRouteWorker(head->right, key, path, values);
  } else {
    head->left = addRouteWorker(head->left, key, path, values);
  }
  return head;
}

/* @private Multiple string replacement.
  Used to replace non POSIX regex symbols.*/
char *sanitse(const char *regexStr) {
  const char *symbols = "*";
  const char *replacer = ".*";
  size_t len = 0;

  for (const char *ptr = regexStr; *ptr; ++ptr) {
    if (*ptr == *symbols)
      len += strlen(replacer);
    else
      len++;
  }
  char *sanitised = malloc(len + 1);

  char *dest = sanitised;
  for (const char *ptr = regexStr; *ptr; ++ptr) {
    if (*ptr == *symbols) {
      strcpy(dest, replacer);
      dest += strlen(replacer);
    } else {
      *dest++ = *ptr;
    }
  }
  *dest = '\0';
  return sanitised;
}

/* @private Searches the b-tree for a filepath or a Regex expression.
   Note: this will return the first match.*/
struct Route *search(struct Route *head, char *key, int modify) {
  if (head == NULL) return NULL;
  char *sanitisedPattern = sanitse(head->key);
  regex_t regex;
  char anchoredKey[strlen(sanitisedPattern) + 3];
  snprintf(anchoredKey, sizeof(anchoredKey), "^%s$", sanitisedPattern);

  int reti = regcomp(&regex, anchoredKey, REG_EXTENDED);
  if (reti) {
    fprintf(stderr, "Could not compile regex\n");
    return NULL;
  }

  free(sanitisedPattern);
  regmatch_t matches[1];
  reti = regexec(&regex, key, 1, matches, 0);
  regfree(&regex);

  if (!reti ||
      (strcmp(key, head->key) == 0)) { // returns 0 on success (both cases)
    return head;
  } else if (reti == REG_NOMATCH) {
    if (strcmp(key, head->key) > 0) {
      return search(head->right, key, modify);
    } else {
      return search(head->left, key, modify);
    }
  } else {
    fprintf(stderr, "Regex match failed\n");
    return NULL;
  }
}

/* @private Places user input on the heap since its from another thread.*/
void toHeap(char **key, char **path) {
  if (!key && !path) fprintf(stderr, "\nkey and path are NULL\n");
  if (key && *key) {
    char *tempKey = malloc(strlen(*key) + 1);
    strcpy(tempKey, *key);
    *key = tempKey;
  }
  if (path && *path) {
    char *tempPath = malloc(strlen(*path) + 1);
    strcpy(tempPath, *path);
    *path = tempPath;
  }
}

/* @private If there is a duplicate, it wil replace its Values with the new
 * Values.*/
struct Route *checkDuplicates(char *key, char *path, Values *values) {
  if (root && values) {
    struct Route *temp = search(root, key, 1);
    if (temp != NULL) {
      free(temp->values);
      free(temp->path);
      temp->values = values;
      temp->path = path;
    }
    return temp;
  }
  return NULL;
}

/* Adds a http request with multiple methods/functions to be called (see
 * examples).
 * Key is the request from the browser, path is the filepath.*/
struct Route *addRouteM(char *key, char *path, Values *uvalues) {
  if (key == NULL) {
    if (path) fprintf(stderr, "No key for path: %s", path);
    return NULL;
  }
  /*Values *values = malloc(sizeof(Values));*/
  /*values->GET = uvalues->GET;*/
  /*values->POST = uvalues->POST;*/

  toHeap(&key, &path);

  if (!path) { // if key (minus "/") is a file, make it = path
    struct stat path_stat;
    if (stat(key + 1, &path_stat) == 0 && !S_ISREG(path_stat.st_mode)) {
      path = strdup(key + 1);
    }
  }

  struct Route *temp = checkDuplicates(key, path, uvalues);
  if (temp) {
    printf("There is a duplicate path!\n");
    return temp;
  }
  return addRouteWorker(root, key, path, uvalues);
}

/* Adds a http request with one method (see examples).*/
struct Route *addRoute(char *key, char *path,
                       void (*func)(Request *, Response *), Method meth) {
  if (key == NULL) {
    if (path) fprintf(stderr, "No key for path: %s", path);
    return NULL;
  }
  toHeap(&key, &path);

  if (path == NULL) { // if key (minus "/") is a file, make it = path
    struct stat path_stat;
    if (stat(key + 1, &path_stat) == 0 && !S_ISREG(path_stat.st_mode)) {
      path = strdup(key + 1);
    }
  }
  Values *values = malloc(sizeof(Values));
  values->GET = NULL;
  values->POST = NULL;
  if (meth == GET) {
    values->GET = func;
  } else if (meth == POST) {
    values->POST = func;
  }
  struct Route *temp = checkDuplicates(key, path, values);
  if (temp) {
    printf("There is a duplicate path!\n");
    return temp;
  }
  return addRouteWorker(root, key, path, values);
}

void staticGet(Request *req, Response *res) {
  if (strcmp(req->urlRoute, "/") == 0) {
    res->content.filePath = "./index.html";
    return;
  }

  char buffer[1024];
  snprintf(buffer, sizeof(buffer), ".%s/%s", req->baseUrl, req->path);
  res->content.filePath = buffer;
}

/* Takes a directory path and adds it to the b-tree using Regex.*/
void addStaticFiles(char *filepath) {
  if (!filepath || strlen(filepath) == 0) {
    fprintf(stderr, "Invalid filepath\n");
    return;
  }
  struct stat pathStat;
  stat(filepath, &pathStat);
  if (!S_ISDIR(pathStat.st_mode)) {
    fprintf(stderr, "Filepath is not a directory\n");
    return;
  }
  // Strip leading './' or '../'
  int length = 0;
  while (filepath[length] == '.' || filepath[length] == '/') length++;

  size_t refinedLen = strlen(filepath + length) + 3; // '/' + '*' '\0'
  char *refinedPath = malloc(refinedLen + 2 - length);

  refinedPath[0] = '/';
  strcpy(refinedPath + 1, filepath + length);
  if (filepath[refinedLen - 3] != '/') strcat(refinedPath, "/");
  strcat(refinedPath, "*");
  addRoute(refinedPath, NULL, staticGet, GET);
  free(refinedPath);
}

/* @private */
void freeRoutes(struct Route *head) {
  if (head != NULL) {
    freeRoutes(head->left);
    freeRoutes(head->right);
    free(head->key);
    if (head->path) free(head->path);
    free(head->values);
    free(head);
  }
}

/* @private Gets the mime type.
 * Input can be "text/plain" or "txt".
 * Note: an input such as text/plain will return text/plain.*/
char *mimes(const char *input) {
  if (input == NULL) return "text/plain";

  static const struct {
    const char *key, *mime;
  } mime_map[] = {
      {"html", "text/html"},
      {"htm", "text/html"},
      {"jpeg", "image/jpeg"},
      {"jpg", "image/jpeg"},
      {"css", "text/css"},
      {"csv", "text/plain"},
      {"ttf", "font/ttf"},
      {"ico", "image/x-icon"},
      {"js", "application/javascript"},
      {"json", "application/json"},
      {"txt", "text/plain"},
      {"gif", "image/gif"},
      {"png", "image/png"},
      {"image/png", "image/png"},
      {"image/jpg", "image/jpeg"},
      {"image/gif", "image/gif"},
      {"image/x-icon", "image/x-icon"},
      {"application/json", "application/json"},
  };
  for (size_t i = 0; i < sizeof(mime_map) / sizeof(mime_map[0]); i++)
    if (strcmp(input, mime_map[i].key) == 0) return (char *)mime_map[i].mime;

  return "text/plain";
}

/* Loads content of a file to a string.*/
char *loadFileToString(const char *filePath) {
  FILE *file = fopen(filePath, "rb");
  if (!file) return NULL;

  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);
  fseek(file, 0L, SEEK_SET);

  char *buffer = malloc(size + 1);
  if (buffer) {
    size_t bytesRead = fread(buffer, 1, size, file);
    if (bytesRead != size) {
      free(buffer);
      fclose(file);
      return NULL;
    }
    buffer[size] = '\0';
  }

  fclose(file);
  return buffer;
}

/* @private Constructs a header.*/
char *headerBuilder(char *ext, int b404, char *header, int size,
                    size_t fileSize) {
  if (b404 == 1) {
    snprintf(header, size,
             "HTTP/1.1 404 Not Found\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %zu\r\n\r\n",
             fileSize);
  } else {
    snprintf(header, size,
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: keep-alive\r\n\r\n",
             mimes(ext), fileSize);
  }
  return header;
}

/* @private Sends a file to the browser.
 * Header may be NULL
 * Do not confuse with sendfile().*/
void SendFile(int client, const char *filePath, char *fileType, int statusCode,
              char *header) {
  size_t size;
  int heap = 0;

  if (!fileType && filePath) {
    char *temp = strdup(filePath);
    char *ext = strrchr(temp, '.');
    if (!ext || *(ext + 1) == '\0')
      fileType = "text/plain";
    else
      fileType = mimes(ext + 1);

    free(temp);
  } else if (!fileType)
    fileType = "text/plain";

  if (header == NULL) {
    if (filePath) {
      FILE *fp = fopen(filePath, "r");
      fseek(fp, 0L, SEEK_END);
      size = ftell(fp);
      fclose(fp);
    } else if (page404) {
      size = strlen(page404);
    } else {
      fileType = "text/html";
      size = strlen(errorPage);
    }
    header = malloc(512);
    heap = 1;
    headerBuilder(fileType, (statusCode == 404), header, 512, size);
  }
  if (page404 && (!filePath || statusCode == 404)) filePath = page404;

  if (!filePath || statusCode == 404) {
    send(client, header, strlen(header), 0);
    send(client, errorPage, strlen(errorPage), 0);
    close(client);
    return;
  }

  int opened_fd = open(filePath, O_RDONLY);

  int on = 1;
  int off = 0;
  off_t offset = 0;
  ssize_t sent_bytes = 0;
  // setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  setsockopt(client, IPPROTO_TCP, TCP_CORK, &on, 4);

  send(client, header, strlen(header), 0);

  // send(client, loadFileToString(filePath), size, 0);

  while (offset < size) {
    ssize_t current_bytes = sendfile(client, opened_fd, &offset, size - offset);
    if (current_bytes <= 0) {

      if (errno == EINVAL || errno == ENOSYS) {
        // Fall back to read/write
        fprintf(stderr,
                "sendfile failed with errno %d, falling back to read/write\n",
                errno);
        char buffer[8192];
        ssize_t read_bytes, write_bytes, written_total;
        while ((read_bytes = read(opened_fd, buffer, sizeof(buffer))) > 0) {
          written_total = 0;
          while (written_total < read_bytes) {
            write_bytes = write(client, buffer + written_total,
                                read_bytes - written_total);
            if (write_bytes < 0) {
              perror("write error");
              return;
            }
            written_total += write_bytes;
          }
        }
        if (read_bytes < 0) {
          perror("read error");
        }
        return;
      }

      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        continue;
      }
      perror("sendfile error");
      exit(EXIT_FAILURE);
      break;
    }
    sent_bytes += current_bytes;
  }

  // printf("Offset Sent, Size: %ld, %ld, %ld\n", offset, sent_bytes, size);
  if (sent_bytes < size) fprintf(stderr, "Incomplete sendfile transmission");
  setsockopt(client, IPPROTO_TCP, TCP_CORK, &off, 4);
  // setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  close(opened_fd);
  if (heap) free(header);
}

/* @private Sends data instead of a file to the browser.*/
void SendData(int client, char *data, char *contentType, int statusCode,
              char *header, size_t *size) {
  if (!contentType) contentType = "text/plain";

  int heap = 0;
  if (header == NULL) {
    header = malloc(sizeof(char) * 128);
    headerBuilder(contentType, (statusCode == 404), header, 128, *size);
    heap = 1;
  }
  if (statusCode == 404)
    SendFile(client, NULL, NULL, 404, NULL);
  else if (data) {
    send(client, header, strlen(header), 0);
    send(client, data, *size, 0);
  }

  if (heap) free(header);
}

/* listens for client connections, processes their requests,
 * and sends appropriate responses based on the request method.
 * Note: Runs the infinite loop.*/
void *process() {
  char header404[256];
  snprintf(header404, sizeof(header404),
           "HTTP/1.1 404 Not Found\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: %zu\r\n" // Length of the errorPage content
           "\r\n",
           strlen(errorPage));

  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  // allows Ctrl+c of the webserver

  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = INADDR_ANY;

  int bound = bind(server_socket, (struct sockaddr *)&addr, sizeof(addr));
  if (bound < 0) {
    perror("Bind failed");
  }

  listen(server_socket, 1);

  while (1) {
    int client = accept(server_socket, NULL, NULL);
    printf("client%d\n", client);
    if (client < 0) continue;

    char buffer[1024] = {0};
    int received = recv(client, buffer, 1024 - 1, 0);
    if (received <= 0) {
      close(client);
      continue; // Skip if no data or error
    }
    buffer[received] = '\0';
    printf("%s", buffer);
    // GET /example.html?search_query=popularity HTTP/1.1
    char *clientHeader = strtok(buffer, "\n");

    char *questionMark = strchr(clientHeader, '?');
    char *method = strtok(clientHeader, " ");
    char *urlRoute = strtok(NULL, questionMark ? "?" : " ");
    char *fileType = NULL;
    char *dot = strrchr(urlRoute, '.');
    if (dot && *(dot + 1) != '\0') {
      fileType = dot + 1;
    }

    char *param = NULL;
    char *query = NULL;
    if (questionMark) {
      param = strtok(NULL, "=");
      query = strtok(NULL, " ");
    }

    char temp[strlen(urlRoute)];
    strcpy(temp, urlRoute);
    char *baseUrl = strtok(temp, "/"); // Gets first part after '/'
    char *path = NULL;
    if (baseUrl) path = strtok(NULL, "");

    if (strcmp(urlRoute, "/") == 0)
      fileType = "html"; // This will be text/plain otherwise
    /*printf("\nMethod + Route + Type + Param + Query:%s.%s.%s.%s.%s.\n",
     * method,*/
    /*       urlRoute, fileType, param, query);*/

    char *body = NULL;
    char *body_start = strstr(buffer, "\r\n\r\n");
    if (body_start) {
      body_start += 4;           // Skip past the \r\n\r\n
      body = strdup(body_start); // Store the body in the variable
    }

    Request req = {method,   urlRoute, path,  baseUrl,
                   fileType, param,    query, body};
    Response res = {};

    /* GET */
    if (strcmp(method, "GET") == 0) {
      struct Route *dest = search(root, urlRoute, 0);
      res.statusCode = 200;
      if (dest == NULL) res.statusCode = 404;

      if (dest && dest->values && dest->values->GET)
        dest->values->GET(&req, &res); // function from other thread

      if (res.statusCode != 404 && dest && !res.content.filePath)
        res.content.filePath = dest->path;

      if (!res.content.data && !res.content.filePath) {
        fprintf(stderr, "No filepath provided anywhere for %s\n", req.urlRoute);
        res.statusCode = 404;
      }

      struct stat file_stat;
      stat(res.content.filePath, &file_stat);
      if (!S_ISREG((file_stat.st_mode))) {
        res.statusCode = 404;
      }

      if (res.content.data && !res.content.filePath) res.statusCode = 200;

      char *header = (res.body != NULL) ? res.body : NULL;
      if (!res.content.data)
        SendFile(client, res.content.filePath, res.contentType, res.statusCode,
                 header);
      else if (res.content.data) {
        size_t size = strlen(res.content.data);
        SendData(client, res.content.data, res.contentType, res.statusCode,
                 header, &size);
      }

      /* POST */
    } else if (strcmp(method, "POST") == 0) {
      printf("in post\n");
      struct Route *dest = search(root, urlRoute, 0);
      res.statusCode = 200;
      if (dest == NULL) res.statusCode = 404;

      if (dest && dest->values && dest->values->POST)
        dest->values->POST(&req, &res); // Call the custom POST handler

      if (res.content.data && !res.content.filePath) res.statusCode = 200;

      if (res.content.data) {
        size_t size = strlen(res.content.data);
        SendData(client, res.content.data, res.contentType, res.statusCode,
                 NULL, &size);
      } else if (res.content.filePath) {
        size_t size = strlen(res.content.filePath);
        SendFile(client, res.content.filePath, res.contentType, res.statusCode,
                 NULL);
      } else {
        send(client, header404, strlen(header404), 0);
        send(client, errorPage, strlen(errorPage), 0);
      }
    }
    if (body) free(body); // since its the only one on heap
    close(client);
  }
  // untouched code
  freeRoutes(root);
  close(server_socket);
}

/* begin interface */

pthread_cond_t exitCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t exitMutex = PTHREAD_MUTEX_INITIALIZER;

/* Allows the thread to not close. This function will block everything after it
 * (recommended to keep at end of stack). */
void keepAlive() {
  // pthread_mutex_lock(&exitMutex);
  pthread_cond_wait(&exitCond, &exitMutex);
  // pthread_mutex_unlock(&exitMutex);
}

/* Starts the Webserver */
void celp(int portNum) {
  port = portNum;
  pthread_t webserver;
  int status = pthread_create(&webserver, NULL, process, NULL);
  if (status != 0) {
    perror("Failed to create thread");
    return;
  }
  pthread_detach(webserver);
}

/* Stops the webserver */
void stop() {
  free(root);
  pthread_cond_signal(&exitCond);
  pthread_mutex_lock(&exitMutex);
  pthread_mutex_unlock(&exitMutex);
  printf("Web server stopped.\n");
}

#endif // WEBSERVER_H
