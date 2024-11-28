#ifndef WEBSERVER_H
#define WEBSERVER_H
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

typedef struct res_ {
  int statusCode;
  char *contentType;
  char *body;
  int *fd; // file descriptor
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

struct Route {
  char *key;
  char *path;
  Values *values;

  struct Route *left, *right;
};

// WARNING: Dont confuse "root" with "route"
struct Route *root = NULL; // NOTE: this is the head of the b tree

struct Route *initRoute(char *key, char *path, Values *value) {
  root = (struct Route *)malloc(sizeof(struct Route));

  root->key = key;
  root->path = path;
  root->values = value;

  root->left = root->right = NULL;
  return root;
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

// this was done to exclude having to add the root param every time
struct Route *addRoute(char *key, char *path, Values *values) {
  if (path == NULL)
    path = key;
  return addRouteWorker(root, key, path, values);
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

void freeRoutes(struct Route *head) {
  if (head != NULL) {
    freeRoutes(head->left);
    freeRoutes(head->right);
    // free(head->key);
    // free(head->values);
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
  printf("\n missed mimes"); 
  return "text/plain";
  // clang-format on
}

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

void *process() {
  // char header[64] = "HTTP/1.1 200 OK\r\n\n";
  // char header404[128] = "HTTP/1.1 404 Not Found\r\n\r\n";
  const char *errorPage = "<html><body>404 Not Found 2</body></html>";
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
    Response res;

    /* GET */
    if (strcmp(method, "GET") == 0) {
      struct Route *dest = search(root, urlRoute);
      if (dest == NULL)
        res.statusCode = 404;

      char *filePath = dest->path;
      printf("filePath: %s\n", filePath);

      FILE *fp = fopen(filePath, "r");
      fseek(fp, 0L, SEEK_END);
      size_t size = ftell(fp);
      fclose(fp);
      int opened_fd = open(filePath, O_RDONLY);

      char *header;
      res.fd = &opened_fd;
      res.body = header;
      res.contentType = fileType;
      dest->values->GET(&req, &res);
      if (res.contentType)
        fileType = res.contentType;

      if (res.statusCode == 404) {
        send(client, header404, strlen(header404), 0);
        send(client, errorPage, strlen(errorPage), 0);
        printf("\n-----404!-----\n");
        close(client);
        continue;
      }

      char template[128];
      header =
          (res.body != NULL)
              ? res.body
              : headerBuilder(fileType, (res.statusCode == 404), template, 128);
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
        ssize_t current_bytes =
            sendfile(client, opened_fd, &offset, size - offset);
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
