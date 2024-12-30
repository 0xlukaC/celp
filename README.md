<p align="center">
  <img src="https://github.com/user-attachments/assets/c51deafd-ddce-47cb-9f04-5cab855ce608" style="width: 60%;" />
</p>

<b>Celp</b> (pronounced "Kelp") is a lightweight, standalone header file that provides a fast, efficient web server framework for C on Linux, perfect for small, localhost projects. It is designed with a high level of abstraction, allowing you to focus on your code, without the awkward hassle of setting up a webserver. Celp allows you to intergrate the webserver into your code, allowing for a streamline experience.


> [!NOTE]
> This webserver is still in early development and has not been tested extensively yet.

<sub>wanky introduction out of the way</sub>
# Installation

Either copy/download [`webserver.h`](github.com/0xlukaC/celp/blob/testing/webserver.h) into your source tree or download it through [`Releases`](github.com/0xlukaC/celp/releases)

> [!NOTE]
> This will not run on any windows operating systems

# Examples
```c
#include "webserver.h"

void getRoot(Request *req, Response *res) { res->content.data = "Hello World"; }

int main() {
  celp(8001); // start function w/ port number
  addRoute("/", NULL, getRoot, GET);

  keepAlive(); // this stops the seperate thread from ending (needs to be at end
               // of stack)
  return 0;
}
```
