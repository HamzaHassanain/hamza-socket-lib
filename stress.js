// Reads the local 1mb.json once and reuses the payload for all clients.

const fs = require("fs");
const path = require("path");

const TARGETS = [
  "http://127.0.0.1:12346/stress/post",
  "http://127.0.0.1:12346/stress2",
  "http://127.0.0.1:12346/",
  "http://127.0.0.1:12346/stress/3",
  "http://127.0.0.1:12346/stress/4",
  "http://127.0.0.1:12346/stress/xxx/4",
  "http://127.0.0.1:12346/stress/x/4",
  "http://127.0.0.1:12346/stress/../4.css",
  "http://127.0.0.1:12346/style.css",
  "http://127.0.0.1:12346/app.js",
  "http://127.0.0.1:12346/../app.js",
];
const CLIENTS = 100;
const TIMEOUT_MS = 120000; // increased timeout for large uploads

const payloadPath = path.join(__dirname, "5mb.json");
let payload;
try {
  payload = fs.readFileSync(payloadPath, "utf8");
  console.log(
    `Loaded payload from ${payloadPath} (${Buffer.byteLength(
      payload,
      "utf8"
    )} bytes)`
  );
} catch (e) {
  console.error("Failed to read 1mb.json:", e);
  process.exit(1);
}
// node get.js > logs/get.log
function GetRandomIndex() {
  return Math.floor(Math.random() * TARGETS.length);
}
let count_failed = 0;
async function makeClient(id) {
  const idx = GetRandomIndex();
  // const idx = 0;
  const isThePost = TARGETS[idx] === "http://127.0.0.1:12346/stress/post";
  try {
    // const timeout = setTimeout(() => controller.abort(), TIMEOUT_MS);
    const res = await fetch(TARGETS[idx], {
      // method: "GET",
      method: isThePost ? "POST" : "GET",
      headers: isThePost
        ? {
            "Content-Type": "application/json",
            "Content-Length": Buffer.byteLength(payload, "utf8"),
          }
        : {},
      body: isThePost ? payload : undefined,
    });

    const text = await res.text().catch(() => "");
    // console.log(
    //   `client ${id} -> Requested ${TARGETS[idx]}, ${res.status} (${
    //     res.statusText
    //   }) ${text.slice(0, 120).replace(/\n/g, " ")}`
    // );
  } catch (err) {
    count_failed++;
    console.log(
      `client ${id} -> Error ${TARGETS[idx]} error: ${
        err && err.code
          ? err.code
          : err && err.message
          ? err.message
          : String(err)
      }`
    );
    console.error(err);
  }
}

async function main() {
  const promises = [];
  for (let i = 0; i < CLIENTS; ++i) {
    promises.push(makeClient(i));
    // wait for 10 milliseconds
  }

  await Promise.all(promises);
  console.log("All clients finished");
  console.log(`Total failed requests: ${count_failed}`);
}

main().catch((err) => console.log("Fatal error:", err));
