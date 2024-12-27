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
char *page404 = NULL;
const char *errorPage = "<html><body>404 Not Found</body></html>";

typedef struct res_ {
  char *contentType;
  char *body;
  struct Content {
    char *filePath;
    char *data;
  } content;
  int statusCode;
} Response;

typedef struct req_ {
  char *method;

  char *urlRoute;
  char *path;
  char *baseUrl;

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
  Values *values;
  struct Route *left, *right;
};

// WARNING: Do not confuse "root" with "route"
struct Route *root = NULL; // NOTE: this is the head of the b tree

struct Route *initRoute(char *key, char *path, Values *value) {
  struct Route *newRoute = (struct Route *)malloc(sizeof(struct Route));

  newRoute->key = key;
  newRoute->path = path;
  newRoute->values = value;
  /*newRoute->overlapLen = 0;*/
  /*newRoute->overlaps = NULL;*/

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
    printf("Warning!!! Route for %s already exists\n", key);
  } else if (strcmp(key, head->key) > 0) {
    head->right = addRouteWorker(head->right, key, path, values);
  } else {
    head->left = addRouteWorker(head->left, key, path, values);
  }
  return head;
}

char **matchFiles(char *path) {
  if (path == NULL) {
    perror("Input (char *path) to matchFiles is NULL");
    return NULL;
  }
  regex_t regex;
  int allocatedSize = 10; // used to keep track of whether to realloc
  char **charPointerArray = malloc(sizeof(char *) * allocatedSize);
  char *pathPattern = "^([^/].*/)?([^/]+)$"; // may be janky
  regcomp(&regex, pathPattern, REG_EXTENDED);
  regmatch_t matches[2]; // Match group 0 is the entire match, 1 for the first
                         // capture group
  if (regexec(&regex, path, 2, matches, 0) != 0) {
    fprintf(stderr, "No file match found\n");
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

  if (dirPath[0] == '\0' || filePath[0] == '\0') {
    fprintf(stderr, "(webserver.h) couldnt match path: %s\n", path);
    return NULL;
  }

  DIR *dp = opendir(dirPath);
  char *rp = realpath(dirPath, NULL); // absolute path of input dir

  char *fullInputPath = malloc(strlen(rp) + strlen(filePath) + 1);
  strcpy(fullInputPath, dirPath);
  strcat(fullInputPath, filePath);

  struct dirent *ep;
  int i = 0;
  while ((ep = readdir(dp)) != NULL) {
    if (strcmp(ep->d_name, ".") == 0 || strcmp(ep->d_name, "..") == 0) continue;

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
        char recursive[2048];
        snprintf(recursive, 2048, "%s/%s", ab, filePath);
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

/*
 * multiple string replacement
 */
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

struct Route *search(struct Route *head, char *key, int modify) {
  if (head == NULL) return NULL;
  char *sanitisedPattern = sanitse(head->key);
  regex_t regex;
  // int reti = regcomp(&regex, head->key, 0);
  char anchoredKey[strlen(sanitisedPattern) + 3];
  snprintf(anchoredKey, sizeof(anchoredKey), "^%s$", sanitisedPattern);

  int reti = regcomp(&regex, anchoredKey, REG_EXTENDED);
  if (reti) {
    fprintf(stderr, "Could not compile regex\n");
    return NULL;
  }
  // printf("patterns %s, %s, %s\n", anchoredKey, key, head->key);
  free(sanitisedPattern);
  regmatch_t matches[1];
  reti = regexec(&regex, key, 1, matches, 0);
  regfree(&regex);

  if (!reti || (strcmp(key, head->key) == 0)) { // returns 0 on success
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
    struct Route *temp = search(root, key, 1);
    if (temp != NULL) {
      free(temp->values);
      temp->values = values;
    }
    return temp;
  }
  return NULL;
}

void intermediateRegex(char *path, char *key, Values *values) {
  char **files = matchFiles(path);
  printf("paths: %s\n", path);
  for (int i = 0; files[i]; i++) {
    struct Route *temp = checkDuplicates(files[i], values);
    if (temp) continue;
    // toHeap(&key, NULL);
    addRouteWorker(root, key, strdup(files[i]), values);
  }
  for (int i = 0; files[i]; i++) free(files[i]);
  free(files);
}

/* key is the request from the browser, path is the filepath */
struct Route *addRouteM(char *key, char *path, Values *values) {
  if (key == NULL) fprintf(stderr, "No key for path: %s", path);
  toHeap(&key, &path);

  if (!path) { // if key (minus "/") is a file, make it = path
    struct stat path_stat;
    if (stat(key + 1, &path_stat) == 0 && !S_ISREG(path_stat.st_mode)) {
      path = strdup(key + 1);
    }
  }

  struct Route *temp = checkDuplicates(key, values);
  if (temp) {
    printf("There is dup!\n");
    return temp;
  }
  return addRouteWorker(root, key, path, values);
}

// this was done to exclude having to add the root param every time
struct Route *addRoute(char *key, char *path,
                       void (*func)(Request *, Response *), Method meth) {
  if (key == NULL) fprintf(stderr, "No key for path: %s", path);
  toHeap(&key, &path);

  if (!path) { // if key (minus "/") is a file, make it = path
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

  struct Route *temp = checkDuplicates(key, values);
  if (temp) {
    printf("There is dup!\n");
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
  printf("refinedPath: %s\n", refinedPath);
  addRoute(refinedPath, NULL, staticGet, GET);
  free(refinedPath);
}

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

  printf("missed mimes: %s\n", input);
  return "text/plain";
}

void staticFiles(const char *dirPath) {
  DIR *dp = opendir(dirPath);
  struct dirent *ep;
  if (dp != NULL) {
    while ((ep = readdir(dp)) != NULL) {
      char *alt = ep->d_name;
      if (strstr(ep->d_name, "index.html") != NULL) alt = "/";
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

// dont confuse with sendfile()
void SendFile(int client, const char *filePath, char *fileType, int statusCode,
              char *header) {
  size_t size;
  int heap = 0;
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
    printf("content: %s\n", fileType);
    header = malloc(512);
    heap = 1;
    headerBuilder(fileType, (statusCode == 404), header, 512, size);
  }
  if (page404 && (!filePath || statusCode == 404)) filePath = page404;

  if (!filePath || statusCode == 404) {
    send(client, header, strlen(header), 0);
    send(client, errorPage, strlen(errorPage), 0);
    printf("-----404!-----\n");
    close(client);
    return;
  }

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

  printf("Offset Sent, Size: %ld, %ld, %ld\n", offset, sent_bytes, size);
  if (sent_bytes < size) fprintf(stderr, "Incomplete sendfile transmission");
  setsockopt(client, IPPROTO_TCP, TCP_CORK, &off, sizeof(off));
  // setsockopt(client, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
  //   sendfile(client, opened_fd, 0, size);
  close(opened_fd);
  // close(client);
  if (heap) free(header);
}

void SendData(int client, char *data, char *contentType, int statusCode,
              char *header, size_t *size) {
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

void *process() {
  char header404[256];
  snprintf(header404, sizeof(header404),
           "HTTP/1.1 404 Not Found\r\n"
           "Content-Type: text/html\r\n"
           "Content-Length: %zu\r\n" // Length of the errorPage content
           "\r\n",
           strlen(errorPage));

  int server_socket = socket(AF_INET, SOCK_STREAM, 0);

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
    if (!client) continue;

    char buffer[512] = {0};
    recv(client, buffer, 256, 0);

    // GET /idk.html?search_query=popularity HTTP/1.1
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
    }

    char temp[strlen(urlRoute)];
    strcpy(temp, urlRoute);
    char *baseUrl = strtok(temp, "/"); // Gets first part after '/'
    char *path = NULL;
    if (baseUrl) path = strtok(NULL, "");

    if (strcmp(urlRoute, "/") == 0) fileType = "html"; //???
    printf("\nMethod + Route + Type + Param + Query:%s.%s.%s.%s.%s.\n", method,
           urlRoute, fileType, param, query);

    Request req = {method, urlRoute, path, baseUrl, fileType, param, query};
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

      // printf("key: %s\n", dest->key);
      if (!res.content.data && !res.content.filePath) {
        fprintf(stderr, "No filepath provided anywhere for %s\n", req.urlRoute);
        // continue;
        res.statusCode = 404;
      }
      if (!res.contentType && fileType) res.contentType = fileType;

      struct stat file_stat;
      stat(res.content.filePath, &file_stat);
      if (!S_ISREG((file_stat.st_mode))) {
        res.statusCode = 404;
        // res.content.filePath = NULL;
      }

      char *header = (res.body != NULL) ? res.body : NULL;
      if (!res.content.data)
        SendFile(client, res.content.filePath, res.contentType, res.statusCode,
                 header);
      else if (res.content.data) {
        size_t size = strlen(res.content.data); // check if valid
        SendData(client, res.content.data, res.contentType, res.statusCode,
                 header, &size);
      }
      usleep(1000);
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
