#include "webserver.h"
#include <pthread.h>
#include <stdio.h>

void get(Request *req, Response *res) {
  printf("currently in function 'get' \n");
  printf("%dcode\n", res->statusCode);
  // res->contentType = "txt";
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "./public/%s", req->path);
  printf("buffer: %s\n", buffer);
  res->content.filePath = strdup(buffer);
  // res->content.filePath = "/daf//adf/";
}

void hawk2(Request *req, Response *res) {}

void test() {
  Values *valuess = malloc(sizeof(Values));
  valuess->GET = get;

  Values *thang = malloc(sizeof(Values));
  thang->GET = hawk2;
  addRoute("/", "./index.html", NULL, GET);
  addStaticFiles("./public/assets");
  // addRouteM("/", "./index.html", thang);
  // addRouteM("/", "./index.html", thang);
  // addRouteM("/public/assets/*", NULL, valuess);
  // addRouteM("./public/assets/picture", "/public/assets/picture", valuess);
}
// if you want to use regex, path has to be NULL

int main() {
  jumpStart();
  printf("Hello, World!\n");
  test();
  inorder(root);
  keepAlive();
  printf("this is never reached\n");
  return 0;
}

/*for (int i = 0; files[i]; i++) {*/
/*  struct Route *temp = checkDuplicates(files[i], values);*/
/*  if (temp) continue;*/
/*  // toHeap(&key, NULL);*/
/*  addRouteWorker(root, key, strdup(files[i]), values);*/
/*}*/

/*
add structs to the key value stuff in the b-tree. route: struct Route

- addStaticFiles()
- lru cache??
- rename Values to Methods/funcs
- add documentation \* *\ to the headerfile
- methods return an int based on weather it was successful or not
- add static things to functions so they cant be used in other header files
- be careful about memory between threads, as stuff can just disappear
- make res.Sendfile() so you dont have to call it yourself :)
- header builder needs better support (eg. content-size);
- pass port/domain name (localhost:8000) into jumpstart()
- can they put addRoutes in the header file w/ pointers to functions in the c?
- check for issues (like chatgpt checks) especially on user input
- check adding normal text as there may be problems since they're still passing
  path param
- does not work if only do addRoute for a thing like /public/assets/*", NULL"
- needs to have cosnt in res I thinks
- is checkDuplicates neccesary?
- I think you can use a switch statement to clean things up on the urlRequest
https://stackoverflow.com/questions/34021194/ideas-for-implementing-wildcard-matching-in-c

*/
