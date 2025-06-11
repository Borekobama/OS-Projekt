// index.js
const fs = require('fs');
const express = require('express');
const http = require('http');
const { Tail } = require('tail');
const { Server } = require('socket.io');

const LOG_FILE = '/shared/contributor.log';

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
  cors: {
    origin: "*"
  }
});

io.on('connection', (socket) => {
  console.log('Client connected to log stream');
});

const tail = new Tail(LOG_FILE, { useWatchFile: true });

tail.on("line", (data) => {
  io.emit("log_update", data);
});

server.listen(3100, () => {
  console.log("Log forwarder listening on port 3100");
});
