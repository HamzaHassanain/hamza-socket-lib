const http = require("http");

// create a simple http server

http
  .createServer((req, res) => {
    res.writeHead(200, { "Content-Type": "text/plain" });
    res.end("Hello World\n");
  })
  .listen(12345, () => {
    console.log("Server running at http://127.0.1:12345/");
  });
// To test, run this file and then access http://
