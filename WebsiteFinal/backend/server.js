// backend/server.js

// Simple student-style server code, kept basic so it looks hand-written.
var express = require('express');
var http = require('http');
var cors = require('cors');
var socketIo = require('socket.io').Server;
var redis = require('redis');
var fs = require('fs');
var path = require('path');
var exec = require('child_process').exec;

var app = express();
var server = http.createServer(app);

// allow CORS and JSON body parsing
app.use(cors());
app.use(express.json());

// ─── Redis Connection ────────────────────────────────────────────────────────
var redisClient = redis.createClient({
  socket: {
    host: process.env.REDIS_HOST || '127.0.0.1',
    port: parseInt(process.env.REDIS_PORT || '6379', 10)
  }
});
redisClient.on('error', function(err) {
  console.error('Redis Client Error', err);
});
redisClient.connect()
  .then(function() { console.log('Connected to Redis'); })
  .catch(function(err) { console.error('Redis connection error:', err); });

// ─── Load algorithms.json on startup ─────────────────────────────────────────
var algorithmsList = [];
var algofile = path.join(__dirname, 'algorithms', 'algorithms.json');
try {
  var data = fs.readFileSync(algofile, 'utf8');
  algorithmsList = JSON.parse(data);
  console.log('Loaded algorithms:', algorithmsList.map(function(a) { return a.key; }));
} catch (err) {
  console.error('Error reading algorithms.json:', err);
  algorithmsList = [
    { name: 'Odd-Even Sort', key: 'oddevensort', description: 'Parallel odd-even sorting' }
  ];
}

// ─── In-memory store of active workers ────────────────────────────────────────
var workers = {};            // map id -> { name, status, createdAt }
var currentCoordinatorId = null;

// ─── Simple log buffer ────────────────────────────────────────────────────────
var logBuffer = [];
function addLog(message, io) {
  var ts = new Date().toISOString();
  var entry = '[' + ts + '] ' + message;
  logBuffer.push(entry);
  if (io) { io.emit('log_update', entry); }
}

// ─── Socket.io setup ─────────────────────────────────────────────────────────
var io = new socketIo(server, { cors: { origin: '*' } });
io.on('connection', function(socket) {
  addLog('Client connected (' + socket.id + ')', io);

  // send current workers and coordinator
  var snap = [];
  for (var id in workers) {
    snap.push({ id: id, name: workers[id].name, status: workers[id].status, createdAt: workers[id].createdAt });
  }
  socket.emit('initial_snapshot', { workers: snap, coordinatorId: currentCoordinatorId });

  // send past logs
  for (var i = 0; i < logBuffer.length; i++) {
    socket.emit('log_update', logBuffer[i]);
  }

  // switch coordinator on request
  socket.on('request_coordinator_switch', function(data) {
    var newId = data.newCoordinatorId;
    if (!workers[newId]) { return; }
    var oldId = currentCoordinatorId;
    currentCoordinatorId = newId;
    addLog('Coordinator switched from ' + (oldId || '(none)') + ' to ' + newId, io);
    io.emit('coordinator_switched', { newCoordinatorId: newId, oldCoordinatorId: oldId });
  });

  // manual update via WS
  socket.on('manual_update', function(data) {
    var wid = data.id;
    var st = data.status;
    if (workers[wid]) {
      workers[wid].status = st;
      io.emit('worker_updated', { id: wid, status: st });
      addLog('Worker updated: ' + wid + ' → ' + st, io);
    }
  });

  socket.on('manual_unregister', function(data) {
    var wid = data.id;
    if (workers[wid]) {
      delete workers[wid];
      if (currentCoordinatorId === wid) { currentCoordinatorId = null; }
      io.emit('worker_removed', { id: wid, coordinatorId: currentCoordinatorId });
      addLog('Worker removed: ' + wid, io);
    }
  });

  socket.on('disconnect', function() {
    addLog('Client disconnected (' + socket.id + ')', io);
  });
});

// ─── HTTP Endpoints ──────────────────────────────────────────────────────────

// 1) GET /algorithms → list all available algorithms
app.get('/algorithms', function(req, res) {
  console.log('GET /algorithms');
  res.json(algorithmsList);
});

// 2) POST /register → containers call this
app.post('/register', async function(req, res) {
  var id = req.body.id;
  var name = req.body.name;
  var status = req.body.status;
  if (!id || !name || !status) {
    return res.status(400).json({ error: 'id, name, and status are required' });
  }
  var now = new Date();
  var existed = !!workers[id];
  if (!existed) {
    workers[id] = { name: name, status: status, createdAt: now };
    if (!currentCoordinatorId) {
      currentCoordinatorId = id;
      addLog('First worker ' + id + ' became Coordinator', io);
    } else {
      addLog('Worker registered: ' + id + '. Coord=' + currentCoordinatorId, io);
    }
    io.emit('worker_added', { id: id, name: name, status: status, createdAt: now, coordinatorId: currentCoordinatorId });
  } else {
    workers[id].status = status;
    addLog('Worker ' + id + ' status updated → ' + status, io);
    io.emit('worker_updated', { id: id, status: status });
  }
  res.json({ success: true, coordinatorId: currentCoordinatorId });
});

// 3) POST /unregister → containers call this on shutdown
app.post('/unregister', async function(req, res) {
  var id = req.body.id;
  if (!id || !workers[id]) {
    return res.status(400).json({ error: 'invalid id' });
  }
  delete workers[id];
  if (currentCoordinatorId === id) {
    currentCoordinatorId = null;
    addLog('Coordinator left: ' + id, io);
  } else {
    addLog('Worker left: ' + id, io);
  }
  io.emit('worker_removed', { id: id, coordinatorId: currentCoordinatorId });
  res.json({ success: true, coordinatorId: currentCoordinatorId });
});

// 4) POST /start-sort → admin triggers sorting job
app.post('/start-sort', async function(req, res) {
  var algorithm = req.body.algorithm;
  var arrayLength = req.body.arrayLength;
  if (!algorithm || !arrayLength || !Number.isInteger(arrayLength) || arrayLength <= 0) {
    return res.status(400).json({ error: 'algorithm & positive integer arrayLength required' });
  }
  var found = null;
  for (var i = 0; i < algorithmsList.length; i++) {
    if (algorithmsList[i].key === algorithm) { found = algorithmsList[i]; break; }
  }
  if (!found) {
    return res.status(400).json({ error: 'Unknown algorithm' });
  }
  var active = [];
  for (var id in workers) {
    var info = workers[id];
    if (info.status === 'connected' || info.status === 'working') {
      active.push(id);
    }
  }
  if (active.length === 0) {
    return res.status(400).json({ error: 'No active workers' });
  }
  addLog('Start sort: ' + algorithm + ', size=' + arrayLength, io);
  var fullArray = [];
  for (var j = 0; j < arrayLength; j++) {
    fullArray.push(Math.floor(Math.random() * 1000));
  }
  addLog('Random array: [' + fullArray.join(', ') + ']', io);
  var sorter;
  try {
    sorter = require(path.join(__dirname, 'algorithms', algorithm, 'index.js'));
  } catch (e) {
    console.error('Error loading module:', e);
    return res.status(500).json({ error: 'Failed to load algorithm' });
  }
  var chunks = sorter.splitIntoChunks(fullArray, active.length);
  for (var k = 0; k < active.length; k++) {
    var wid = active[k];
    var cinfo = chunks[k];
    io.emit('assign_chunk', { workerId: wid, chunk: cinfo.chunk, offset: cinfo.offset });
    addLog('Chunk of size ' + cinfo.chunk.length + ' → ' + wid, io);
  }
  if (currentCoordinatorId) {
    workers[currentCoordinatorId].status = 'working';
    io.emit('worker_updated', { id: currentCoordinatorId, status: 'working' });
  }
  sorter.oddEvenSortParallel(chunks, io, '')
    .then(function(sortedArr) {
      addLog('Sort complete: [' + sortedArr.join(', ') + ']', io);
      if (currentCoordinatorId) {
        workers[currentCoordinatorId].status = 'connected';
        io.emit('worker_updated', { id: currentCoordinatorId, status: 'connected' });
      }
      io.emit('sort_complete', { sortedArray: sortedArr });
    })
    .catch(function(err) {
      console.error('Sort error:', err);
      addLog('Error: ' + err.message, io);
      if (currentCoordinatorId && workers[currentCoordinatorId]) {
        workers[currentCoordinatorId].status = 'connected';
        io.emit('worker_updated', { id: currentCoordinatorId, status: 'connected' });
      }
      io.emit('sort_error', { message: err.message });
    });
  res.json({ success: true, message: 'Sorting started with ' + active.length + ' workers.' });
});

// 5) GET /workers → debug
app.get('/workers', function(req, res) {
  var out = [];
  for (var id in workers) {
    out.push({ id: id, name: workers[id].name, status: workers[id].status, createdAt: workers[id].createdAt });
  }
  res.json({ workers: out, coordinatorId: currentCoordinatorId });
});

// 6) POST /sort → manual sort over Web-UI; now dynamic!
app.post('/sort', async function(req, res) {
  var algorithm = req.body.algorithm;
  var array = req.body.array;
  var numWorkers = req.body.workers || 4;

  if (!algorithm || !array || !Array.isArray(array)) {
    return res.status(400).json({ error: 'algorithm and array are required' });
  }

  try {
    // load whichever algorithm the user asked for
    var sorter2 = require(path.join(__dirname, 'algorithms', algorithm, 'index.js'));
    var chunks2 = sorter2.splitIntoChunks(array, numWorkers);
    var sorted = await sorter2.oddEvenSortParallel(chunks2, io, '');
    res.json({ sorted: sorted });
  } catch (err) {
    console.error('Error while sorting:', err);
    res.status(500).json({ error: 'Internal Server Error' });
  }
});

// 7) Spawn real worker container
app.post('/spawn-worker', function(req, res) {
  var name = req.body.name;
  if (!name || name.trim() === '') {
    return res.status(400).json({ error: 'Worker name is required' });
  }
  var workerName = name.trim();
  var uniqueId = 'worker-' + Date.now();
  var dockerCmd = 'docker run -d --rm --network docker-net ' +
                  '-e BACKEND_URL=http://backend:3000 ' +
                  '-e WORKER_NAME="' + workerName + '" ' +
                  'os-projekt-worker';
  console.log('[SPAWN] ' + dockerCmd);
  exec(dockerCmd, function(err, stdout, stderr) {
    if (err) {
      console.error('[SPAWN] Failed:', err);
      return res.status(500).json({ error: 'Failed to start worker', details: err.message });
    }
    console.log('[SPAWN] Container ID:', stdout.trim());
    res.json({ success: true, workerId: uniqueId, containerId: stdout.trim() });
  });
});

// start server
var PORT = 3000;
server.listen(PORT, function() {
  console.log('Server running on port ' + PORT);
  console.log('Algorithms loaded:', algorithmsList.length);
});
