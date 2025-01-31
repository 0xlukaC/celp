
<p align="center">
  <img src="https://github.com/user-attachments/assets/c51deafd-ddce-47cb-9f04-5cab855ce608" style="width: 60%;" />
</p>

## C - Linux - Webserver

<b>Celp</b> (pronounced "Kelp") is a lightweight, standalone header file that provides a fast, efficient web server framework for C on Linux, perfect for small, localhost projects. It is designed with a high level of abstraction, allowing you to focus on your code, without the awkward hassle of setting up a webserver. Celp allows you to intergrate the webserver into your code, allowing for a streamline experience.


## Installation

Either copy/download [`webserver.h`](https://github.com/0xlukaC/celp/blob/main/webserver.h) into your source tree or download it through [`Releases`](https://github.com/0xlukaC/celp/releases)

> [!NOTE]
> This will not run on any Windows operating systems

## Examples
```c
#include "webserver.h"

void hello(Request *req, Response *res) { res->content.data = "Hello World"; }

int main() {
  celp(8001); // start function w/ port number
  addRoute("/", NULL, hello, GET);

  keepAlive(); // this stops the seperate thread from ending (needs to be at end
               // of stack)
  return 0;
}
```

```c
#include "webserver.h"

void getRoot(Request *req, Response *res) {
  /*
  req->urlRoute // Request path from the browser (same as key) eg "/"
  req->method // The method of the request
  req->param // Parameter of url Route
  req->query // Query of url Route
  req->fileType // The file extenstion of the url Request
  req->bodyFull // All the request headers
  
  // This allows you to select a value out of the request body
  // eg: "Accept: *'/'* "
  char output[1024];
  req->body("Accept", output, req);
  printf("output: %s\n", output); // output = " *'/'* " something like this


  Note: You do not have to assign values for the following properties.
  They are optional

  res->body // Header for response
  res->statusCode // Eg 404
  res->contentType // Can either be a extention or a MIME type eg "html" or
  "plain/html"

  res->content.filePath // Filepath for the response
  OR
  res->content.data // Use a string as a response instead
  */
}

// Fill out these functions if you want
void getPicture(Request *req, Response *res) {}
void postPicture(Request *req, Response *res) {}

void styleRegex(Request *req, Response *res) {
  char buffer[1024];
  snprintf(buffer, sizeof(buffer), "./public/%s", req->path);
  res->content.filePath = buffer;
}

int main() {
  celp(8001); // Start function w/ port number
  addRoute("/", "./index.html", getRoot, GET); // Request, file path, function, method

  addRoute("/public/file.txt", NULL, NULL, GET); // Can take a NULL path (turns / into ./)
  set404("./404page.html"); // Set a 404 page or just do it through an addroute
  addStaticFiles("./public/assets"); // Add all files in the specified directory

  Values *picMethods = malloc(sizeof(Values)); // Do not free this (this has to be allocated on the heap)
  picMethods->GET = getPicture; // Note: You don't have to provide a function for each method
  picMethods->POST = postPicture;
  addRouteM("/picture", "./picture1", picMethods);

  addRoute("/public/assets/styles/*", NULL, styleRegex, GET); // If you want to use a Regex Expression, path has to be NULL
  // Celp will attempt to do an exact string match in the binary tree first
  // before Evaluating Regex Expressions. It will then try to match all routes
  // in a linked list in the order routes are defined in this/the c file.

  if (1) stop(); // stops the webserver and terminates the thread

  inorder();   // Prints the mappings (url requests and their respective paths)
  keepAlive(); // This stops the seperate thread from ending (needs to be at end of stack)
  return 0;
}

```
> [!WARNING]
> You may have to include `-pthread` in your compilation. Eg: `gcc <file> -o <executable> -pthread`
> 
> If your header file has errors, it's likley because your editor (namely neovim) views it as a c++ file. You need to create a compile_flags.txt in the directory and put 
> ```
> compileFlags:
>   Add: [-x, c]
> ``` 

## Contributing
Celp welcomes all constructive contributions. Please feel free to submit pull requests or open issues (preferably open an issue first, as it expedites the process for all parties). It would be helpful to write/update tests if your feature requires it (more so testing end input, ie. curling, see [`/testing`](https://github.com/0xlukaC/celp/tree/main/testing)).

>[!TIP]
>compile `testing/test.c` to run tests

For insights on contributing effectively to open source, consider watching this [Don't contribute to open source](https://www.youtube.com/watch?v=5nY_cy8zcO4) - TLDR: Use open source, notice a bug, investigate the bug, write a bug report and offer to fix it

## License
Celp is licensed under the 'MIT License'. For more details, please refer to the [`LICENSE`](https://github.com/0xlukaC/celp/blob/main/LICENSE) file.
