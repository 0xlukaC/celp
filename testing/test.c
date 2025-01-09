#include "../webserver.h"
#include <assert.h>
#include <stdio.h>
#include <sys/wait.h>

/* put this at end of function */
void check(Request *req, Response *res) {
  assert(res->content.data || res->content.filePath);
  assert(res->statusCode >= 100 || res->statusCode <= 599);
  assert(req->method != NULL);
}

void getAll(Request *req, Response *res) {
  res->content.data = "not file1 :(";
  printf("path: %s baseurl: %s\n", req->path, req->baseUrl);
}
void garbage(Request *req, Response *res) {
  res->statusCode = 200;
  res->contentType = NULL;
  res->body = NULL;
}

void login(Request *req, Response *res) {
  res->content.data = "Login text";
  check(req, res);
}

void normalGet(Request *req, Response *res) { res->contentType = "html"; }

void get() {
  // write addRoutes in here
  addRoute("/public/image.jpg", NULL, NULL, GET); // testing NULL path

  addRoute("/", "./index.txt", normalGet, GET);
  addRoute("/", NULL, NULL, GET); // testing root and replacement of path
  /**/
  addRoute("/asdf* ///gArBage/", "", garbage, GET); // testing junk input
  addRoute(NULL, NULL, NULL, GET);
  addRoute("", "/", NULL, GET);

  // testing rewriting and overlaps
  addRoute("/public/files/*", NULL, getAll, GET);
  addRoute("/public/files/*", NULL, getAll, GET);
  addRoute("/public/files/file1.txt", "./public/files/file1.txt", normalGet,
           GET);
  // txt because I didn't want it showing html as a language on github
}

void post() { addRoute("/login", NULL, login, POST); }

int main() {
  celp(8001);
  printf("Hello, World!\n");
  get();
  post();
  inorder();
  sleep(1); // wait for the server to start (just in case)

  int shres = system("./test_script.sh");
  if (shres == -1) {
    perror("system");
    return 1;
  } else {
    int exit_status = WEXITSTATUS(shres);
    printf("Script exit status: %d\n", exit_status);
    if (exit_status != 0) return 1;
  }

  // keepAlive();
  stop();
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
