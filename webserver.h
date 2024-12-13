#ifndef WEBSERVER_H
#define WEBSERVER_H
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#define set404(x) page404 = x
char *page404 = NULL;
const char *errorPage = "<html><body>404 Not Found 2</body></html>";

typedef struct res_ {
  char *contentType;
  char *body;
  union Content {
    char *filePath;
    char *data;
  } content;
  int statusCode;
} Response;

typedef struct req_ {
  char *method;
  char *urlRoute;
  char *fileType;
  char *param;
  char *query;
} Request;

typedef struct Values_ {
  void (*GET)(Request *req, Response *res);  // Function pointer for GET
  void (*POST)(Request *req, Response *res); // Function pointer for POST
  // char *path;
} Values;

typedef enum { GET, POST } Method;

struct Route {
  char *key;
  char *path;
  Values *values; // on heap

  struct Route *left, *right;
};

// WARNING: Do not confuse "root" with "route"
struct Route *root = NULL; // NOTE: this is the head of the b tree

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

void inorder(struct Route *head) {
  if (head != NULL) {
    inorder(head->left);
    printf("%s -> %s \n", head->key, head->path);
    inorder(head->right);
  }
}

struct Route *addRouteWorker(struct Route *head, char *key, char *path,
                             Values *values) {
  if (head == NULL) {
    return initRoute(key, path, values);
  }

  if (strcmp(key, head->key) == 0) {
    printf("Warning!!!\nRoute for %s already exists", key);
  } else if (strcmp(key, head->key) > 0) {
    head->right = addRouteWorker(head->right, key, path, values);
  } else {
    head->left = addRouteWorker(head->left, key, path, values);
  }
  return NULL; // unused
}

struct Route *search(struct Route *head, char *key) {
  if (head == NULL) {
    return NULL;
  }

  if (strcmp(key, head->key) == 0) {
    return head;
  } else if (strcmp(key, head->key) > 0) {
    return search(head->right, key);
  } else if (strcmp(key, head->key) < 0) {
    return search(head->left, key);
  }
  printf("not returning anything");
  return NULL;
}

void toHeap(char **key, char **path) {
  if (!key && !path) perror("\nkey and path are NULL\n");
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

struct Route *checkDuplicates(char *key, Values *values) {
  if (root && values) {
    struct Route *temp = search(root, key);
    if (temp != NULL) {
      free(temp->values);
      temp->values = values;
    }
    return temp;
  }
  return NULL;
}

char **matchFiles(char *path) {
  regex_t regex;
  int allocatedSize = 10; // used to keep track of whether to realloc
  char **charPointerArray = malloc(sizeof(char *) * allocatedSize);
  char *pathPattern = "^(.*/)?[^/]*$";
  regcomp(&regex, pathPattern, REG_EXTENDED);
  regmatch_t matches[2]; // Match group 0 is the entire match, 1 for the first
                         // capture group
  if (regexec(&regex, path, 2, matches, 0) != 0) {
    fprintf(stderr, "No match found\n");
    regfree(&regex);
    return NULL;
  }

  char *dirPath = malloc((matches[1].rm_eo - matches[1].rm_so + 1) *
                         sizeof(char)); // nullter +1
  snprintf(dirPath, matches[1].rm_eo - matches[1].rm_so + 1, "%.*s",
           (int)(matches[1].rm_eo - matches[1].rm_so), path + matches[1].rm_so);

  char *filePath =
      malloc((matches[0].rm_eo - matches[1].rm_eo + 1) * sizeof(char));
  snprintf(filePath, matches[0].rm_eo - matches[1].rm_eo + 1, "%.*s",
           (int)(matches[0].rm_eo - matches[1].rm_eo), path + matches[1].rm_eo);

  if (dirPath == NULL)
    dirPath = "./";
  DIR *dp = opendir(dirPath);
  char *rp = realpath(dirPath, NULL); // absolute path of input dir

  char *fullInputPath = malloc(strlen(rp) + strlen(filePath) + 1);
  strcpy(fullInputPath, dirPath);
  strcat(fullInputPath, filePath);

  struct dirent *ep;
  int i = 0;
  while ((ep = readdir(dp)) != NULL) {
    if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0)
      continue;

    char ab[1024]; // path of current lookup
    snprintf(ab, 1024, "%s%s", dirPath, ep->d_name);
    char *ad = realpath(ab, NULL); // absolute path of current lookup

    //    printf("\n%s - %s - %s - %s - %s - %s\n", rp, ab, ad, dirPath,
    //          fullInputPath, filePath);

    if (strcmp(ad, fullInputPath) == 0) {
      charPointerArray[i] = strdup(ab); // changed from fullIP
      i++;
    } else {

      struct stat path_stat;
      stat(ab, &path_stat);
      if (!S_ISREG(path_stat.st_mode)) {
        char recursive[1024];
        snprintf(recursive, 1024, "%s/%s", ab, filePath);
        char **results = matchFiles(recursive);
        if (results) {
          for (int j = 0; results[j]; j++) {
            if (i >= allocatedSize) {
              allocatedSize *= 2;
              charPointerArray =
                  realloc(charPointerArray, sizeof(char *) * allocatedSize);
            }
            charPointerArray[i++] = results[j];
          }
        }
        free(ad);
        continue;
      }

      regex_t userRegex;
      regcomp(&userRegex, fullInputPath, REG_EXTENDED);
      if (regexec(&userRegex, ad, 0, NULL, 0) == 0) {
        charPointerArray[i] = strdup(ab); // changed from ab
        i++;
      }
      regfree((&userRegex));
    }
    if (i >= allocatedSize) {
      allocatedSize *= 2;
      charPointerArray =
          realloc(charPointerArray, sizeof(char *) * allocatedSize);
    }
  }
  closedir(dp);
  free(filePath);
  free(rp);
  regfree(&regex);
  free(dirPath);
  charPointerArray[i] = NULL; // null terminate the array
  charPointerArray = realloc(charPointerArray, sizeof(char *) * (i + 1));
  return charPointerArray;
}

void intermediateRegex(char *path, char *key, Values *values) {
  char **files = matchFiles(path);
  for (int i = 0; files[i]; i++) {
    struct Route *temp = checkDuplicates(files[i], values);
    if (temp)
      continue;
    // toHeap(&key, NULL);
    addRouteWorker(root, key, strdup(files[i]), values);
  }
  for (int i = 0; files; i++) free(files[i]);
  free(files);
}

/* key is the request from the browser, path is the filepath */
struct Route *addRouteM(char *path, char *key, Values *values) {
  toHeap(&key, &path);

  if (path == NULL) {
    path = strdup(key);
     if (path[0] == '/')
    memmove(path, path + 1, strlen(path));
  }
  struct stat path_stat;
  stat(path, &path_stat);
  if (!S_ISREG(path_stat.st_mode)) {
    intermediateRegex(path, key, values);
  } else {
    struct Route *temp = checkDuplicates(key, values);
    if (temp)
      return temp;
    //toHeap(&key, &path);
    return addRouteWorker(root, key, path, values);
  }
  return NULL;
}

// this was done to exclude having to add the root param every time
struct Route *addRoute(char *path, char *key,
                       void (*func)(Request *, Response *), Method meth) {
  toHeap(&key, &path);

  if (path == NULL) {
    path = strdup(key);
     if (path[0] == '/')
    memmove(path, path + 1, strlen(path));
  }

  Values *values = malloc(sizeof(Values));
  values->GET = NULL;
  values->POST = NULL;
  if (meth == GET) {
    values->GET = func;
  } else if (meth == POST) {
    values->POST = func;
  }

  struct stat path_stat;
  stat(path, &path_stat);
  printf("%s\n", path);
  if (!S_ISREG(path_stat.st_mode)) {
    intermediateRegex(path, key, values);
  } else {
    // toHeap(&key, &path);
    struct Route *temp = checkDuplicates(key, values);
    if (temp)
      return temp;
    return addRouteWorker(root, key, path, values);
  }
  return NULL;
}

void freeRoutes(struct Route *head) {
  if (head != NULL) {
    freeRoutes(head->left);
    freeRoutes(head->right);
    free(head->key);
    free(head->values);
    free(head);
  }
}

char *mimes(char *ext) {
  // clang-format off
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html";
    if (strcmp(ext, "jpeg") == 0 || strcmp(ext, "jpg") == 0) return "image/jpg";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "csv") == 0) return "text/plain";
    if (strcmp(ext, "ttf") == 0) return "font/tts"; 
    if (strcmp(ext, "ico") == 0) return "image/png"; 
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "json") == 0) return "application/json"; 
    if (strcmp(ext, "txt") == 0) return "text/plain"; 
    if (strcmp(ext, "gif") == 0) return "image/gif"; 
    if (strcmp(ext, "png") == 0) return "image/png"; 
  printf("\nmissed mimes"); 
  return "text/plain";
  // clang-format on
}

void staticFiles(const char *dirPath) {
  DIR *dp = opendir(dirPath);
  struct dirent *ep;
  if (dp != NULL) {
    while ((ep = readdir(dp)) != NULL) {
      char *alt = ep->d_name;
      if (strstr(ep->d_name, "index.html") != NULL)
        alt = "/";
      addRouteWorker(root, ep->d_name, alt, NULL); // global
    }
    (void)closedir(dp);
  } else
    perror("Couldn't open the directory");
}

// char *loadFileToString(const char *filePath) {
//   FILE *file = fopen(filePath, "rb");
//   if (!file)
//     return NULL;

//   fseek(file, 0L, SEEK_END);
//   size_t size = ftell(file);
//   fseek(file, 0L, SEEK_SET);

//   char *buffer = malloc(size + 1);
//   if (buffer) {
//     size_t bytesRead = fread(buffer, 1, size, file);
//     if (bytesRead != size) {
//       free(buffer);
//       fclose(file);
//       return NULL;
//     }
//     buffer[size] = '\0'; // Null-terminate the string
//   }

//   fclose(file);
//   return buffer;
// }

char *headerBuilder(char *ext, int b404, char *header, int size) {
  if (b404 == 1) {
    snprintf(header, size,
             "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n");
  } else {
    snprintf(header, size, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n",
             mimes(ext));
  }
  return header;
}

void SendData(int client, char *data, char *contentType, int statusCode,
              char *header, size_t *size) {
  int heap = 0;

  if (header == NULL) {
    header = malloc(sizeof(char) * 128);
    headerBuilder(contentType, statusCode, header, 128);
    heap = 1;
  }
  send(client, header, strlen(header), 0);

  if (data)
    send(client, data, *size, 0);

  if (heap)
    free(header);
}

// dont confuse with sendfile() also its pointed to in Request
void SendFile(int client, const char *filePath, char *fileType, int statusCode,
              char *header) {
  int heap = 0;
  if (header == NULL) {
    header = malloc(sizeof(char) * 128);
    headerBuilder(fileType, statusCode, header, 128);
    heap = 1;
  }
  if (page404 && !filePath)
    filePath = page404;

  if (!filePath) {
    send(client, header, strlen(header), 0);
    send(client, errorPage, strlen(errorPage), 0);
    printf("\n-----404!-----\n");
    close(client);
    return;
  }

  FILE *fp = fopen(filePath, "r");
  fseek(fp, 0L, SEEK_END);
  size_t size = ftell(fp);
  fclose(fp);

  int opened_fd = open(filePath, O_RDONLY);

  int on = 1;
  int off = 0;
  off_t offset = 0;
  ssize_t sent_bytes = 0;
  // setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  setsockopt(client, IPPROTO_TCP, TCP_CORK, &on, sizeof(on));

  // printf("header Size %ld\nFile size, string %ld %d\n", strlen(header),
  //        size, get_file_size(opened_fd));

  send(client, header, strlen(header), 0);

  // send(client, loadFileToString(filePath), size, 0);
  while (offset < size) {
    ssize_t current_bytes = sendfile(client, opened_fd, &offset, size - offset);
    if (current_bytes <= 0) {
      perror("sendfile error");
      exit(EXIT_FAILURE);
      break;
    }
    sent_bytes += current_bytes;
  }
  printf("Offset Sent, Size: %ld, %ld, %ld\n", offset, sent_bytes, size);
  if (sent_bytes < size)
    perror("Incomplete sendfile transmission");
  setsockopt(client, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));
  // setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  //   sendfile(client, opened_fd, 0, size);
  close(opened_fd);
  // close(client);
  if (heap)
    free(header);
}

void *process() {
  // char header[64] = "HTTP/1.1 200 OK\r\n\n";
  // char header404[128] = "HTTP/1.1 404 Not Found\r\n\r\n";
  char header404[256];
  snprintf(header404, sizeof(header404),
           "HTTP/1.1 404 Not Found\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: %zu\r\n" // Length of the errorPage content
           "\r\n",
           strlen(errorPage));
  //  strcat(header, responseData);

  // create a socket
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

  // Values *value = malloc(sizeof(Values));
  // addRouteWorker(root, "/", "./index.html", value); // global

  // define the address
  // struct sockaddr_in addr = {AF_INET, htons(8001), INADDR_ANY};
  struct sockaddr_in addr;
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8001);
  addr.sin_addr.s_addr = INADDR_ANY;
  int bound = bind(server_socket, (struct sockaddr *)&addr, sizeof(addr));
  // variable not used

  listen(server_socket, 15);

  while (1) {
    int client = accept(server_socket, NULL, NULL);
    if (!client)
      continue;

    char buffer[512] = {0};
    recv(client, buffer, 256, 0);
    // printf("\n%s", buffer);

    // GET /idk.html?search_queary=popularity HTTP/1.1
    char *clientHeader = strtok(buffer, "\n"); // automatically adds 0/

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
      // get popularity, whats after the = and before its next space
    }

    if (strcmp(urlRoute, "/") == 0)
      fileType = "html";
    printf("\nMethod + Route + Type + Param + Query:%s.%s.%s.%s.%s.\n", method,
           urlRoute, fileType, param, query);

    // maybe make on heap
    Request req = {method, urlRoute, fileType, param, query};
    Response res = {fileType};

    /* GET */
    if (strcmp(method, "GET") == 0) {
      struct Route *dest = search(root, urlRoute);
      res.statusCode = 200;
      if (dest == NULL)
        res.statusCode = 404;
      // ensuring no memory leaks
      char *filePath = NULL;
      char *data = NULL;
      char *header = NULL;
      res.body = header;
      res.content.filePath = filePath;
      res.content.data = data;

      if (dest && dest->values && dest->values->GET)
        dest->values->GET(&req, &res); // function from other thread
      data = res.content.data;

      if (res.contentType)
        fileType = res.contentType; // declared up
      // try to remove later

      if (res.statusCode != 404 && dest != NULL)
        filePath = dest->path;
      // printf("filePath: %s\n", filePath);

      char template[128];
      header =
          (res.body != NULL)
              ? res.body
              : headerBuilder(fileType, (res.statusCode == 404), template, 128);
      if (!data)
        SendFile(client, filePath, res.contentType, res.statusCode, header);
      else {
        size_t size = strlen(data) * sizeof(char); // check if valid
        SendData(client, data, res.contentType, res.statusCode, header, &size);
      }
      close(client);
    } else if (strcmp(method, "POST") == 0) {
    }
  }
  // untouched code
  freeRoutes(root);
  close(server_socket);
}

/* begin interface */

pthread_cond_t exitCond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t exitMutex = PTHREAD_MUTEX_INITIALIZER;

void keepAlive() {
  // pthread_mutex_lock(&exitMutex);
  pthread_cond_wait(&exitCond, &exitMutex);
  // pthread_mutex_unlock(&exitMutex);
}

void jumpStart() {
  pthread_t webserver;
  int status = pthread_create(&webserver, NULL, process, NULL);
  if (status != 0) {
    perror("Failed to create thread");
    return;
  }
  pthread_detach(webserver);
}

void stop() {
  pthread_cond_signal(&exitCond);
  pthread_mutex_lock(&exitMutex);
  pthread_mutex_unlock(&exitMutex);
  printf("Web server stopped.\n");
}

#endif // WEBSERVER_H
