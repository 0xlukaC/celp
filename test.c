#include "webserver.h"
#include <pthread.h>

void get(Request *req, Response *res) {
  printf("%dcode\n", res->statusCode);
  res->contentType = "txt";
}

void test() {
  Values *valuess = malloc(sizeof(Values));
  valuess->GET = get;
  addRoute("/index.html", "./index.html", get, GET);
  // addRouteM("/", "./index.html", valuess);
}

int main() {
  jumpStart();
  printf("Hello, World!\n");
  test();
  inorder(root);
  keepAlive();
  printf("this is never reached\n");
  return 0;
}
/*
add structs to the key value stuff in the b-tree. route: struct Route

- addStaticFiles()
- lru cache??
- rename Values to Methods/funcs
- add default get function to addstatic
- Why are we still passing "/" "index.html"???
- add wildcards * to addroute
- add documentation \* *\ to the headerfile
- methods return an int based on weather it was successful or not
- add static things to functions so they cant be used in other header files
- be careful about memory between threads, as stuff can just disappear
- make res.Sendfile() so you dont have to call it yourself :)
- what if they dont want to send a file
- header builder needs better support (eg. content-size);
- pass port into jumpstart()
- can they put addRoutes in the header file w/ pointers to functions in the c?
*/
