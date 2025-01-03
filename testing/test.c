#include "../webserver.h"
#include <assert.h>

/* put this at end of function */
void check(Request *req, Response *res) {
  // assert(res->contentType != NULL);
  assert(res->content.data || res->content.filePath);
  assert(res->statusCode >= 100 || res->statusCode <= 599);
  assert(req->method != NULL);
}

void getAll(Request *req, Response *res) {}
void garbage(Request *req, Response *res) {
  res->statusCode = 200;
  res->contentType = NULL;
  res->body = NULL;
}

void login(Request *req, Response *res) {
  res->content.data = "Login text";
  check(req, res);
}

void get() {
  // write addRoutes in here
  addRoute("/public/image.jpg", NULL, NULL, GET);

  addRoute("/asdf* ///gArBage/", "", garbage, GET);
  addRoute(NULL, NULL, NULL, GET);
  addRoute("", "/", NULL, GET);

  addRoute("/public/assets/*", NULL, getAll, GET);
  addRoute("/public/assets/*", NULL, getAll, GET);
}

void post() { addRoute("/login", NULL, login, POST); }

int main() {
  celp(8001);
  printf("Hello, World!\n");
  get();
  post();
  inorder(root);
  sleep(1); // wait for the server to start
  system("./test_script.sh");
  keepAlive();
  // stop();
  return 0;
}

/* testing:
  regex
  root (/)
  normal sendfiles
  control paths (res->stuff)
  NULLS
  random junk (including /)
  duplicates

  */

// if you want to use regex, path has to be NULL
