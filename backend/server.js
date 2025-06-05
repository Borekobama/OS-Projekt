// backend/server.js

const express = require('express');
const http = require('http');
const cors = require('cors');
const { Server } = require('socket.io');
const { createClient } = require('redis');
const fs = require('fs');
const path = require('path');

const app = express();
const server = http.createServer(app);

// Enable CORS and JSON body parsing
app.use(cors());
app.use(express.json());

// ─── Debug: Serve static files from the “frontend” directory ─────────────────
// In your setup, NGINX already handles serving all front-end assets (index.html,
// style.css, script.js, etc.) from the “frontend” container. So we do not need
// express.static here for those. However, if you ever want to test things
// without NGINX, you could uncomment the following line and adjust the path:
//
// app.use(express.static(path.join(__dirname, '../frontend')));
  
// ─── Redis Connection ────────────────────────────────────────────────────────
const redisClient = createClient({
  socket: {
    host: process.env.REDIS_HOST || '127.0.0.1',
    port: parseInt(process.env.REDIS_PORT || '6379'),
  }
});
redisClient.on('error', err => console.error('Redis Client Error', err));
redisClient.connect()
  .then(() => console.log('🟢 Connected to Redis'))
  .catch(err => console.error('Redis connection error:', err));

// ─── Load algorithms.json on startup ─────────────────────────────────────────
let algorithmsList = [];
const algofile = path.join(__dirname, 'algorithms', 'algorithms.json');
try {
  const data = fs.readFileSync(algofile, 'utf8');
  algorithmsList = JSON.parse(data);
  console.log('✅ Loaded algorithms.json:', algorithmsList.map(a => a.key));
} catch (err) {
  console.error('❌ Error reading algorithms.json:', err);
  console.error('❌ Tried to read from:', algofile);
  // Fallback: create a default algorithm list so /algorithms never breaks
  algorithmsList = [
    { name: "Odd-Even Sort", key: "oddevensort", description: "Parallel odd-even sorting" }
  ];
}

// ─── In-memory store of active workers ────────────────────────────────────────
let workers = new Map();            // Map<id, { name, status, createdAt }>
let currentCoordinatorId = null;    // ID of the current Coordinator

// ─── In-memory “logs” for sorting runs ───────────────────────────────────────
let logBuffer = [];
function addLog(message, io) {
  const timestamp = new Date().toISOString();
  const entry = `[${timestamp}] ${message}`;
  logBuffer.push(entry);
  if (io) io.emit('log_update', entry);
}

// ─── Socket.io setup ─────────────────────────────────────────────────────────
const io = new Server(server, { cors: { origin: '*' } });

io.on('connection', (socket) => {
  addLog(`🔌 Frontend client connected (${socket.id})`, io);

  // Send snapshot of workers + Coordinator ID
  const snapshot = [];
  for (let [id, info] of workers.entries()) {
    snapshot.push({
      id,
      name: info.name,
      status: info.status,
      createdAt: info.createdAt
    });
  }
  socket.emit('initial_snapshot', {
    workers: snapshot,
    coordinatorId: currentCoordinatorId
  });

  // Send all existing logs so the new client “catches up”
  logBuffer.forEach(line => {
    socket.emit('log_update', line);
  });

  // Handle Coordinator switch request
  socket.on('request_coordinator_switch', ({ newCoordinatorId }) => {
    if (!workers.has(newCoordinatorId)) return;
    const old = currentCoordinatorId;
    currentCoordinatorId = newCoordinatorId;
    addLog(`🔄 Coordinator switched from ${old || '(none)'} to ${newCoordinatorId}`, io);
    io.emit('coordinator_switched', { newCoordinatorId, oldCoordinatorId: old });
  });

  // Manual status update from front-end (if ever used)
  socket.on('manual_update', ({ id, status }) => {
    if (workers.has(id)) {
      const info = workers.get(id);
      info.status = status;
      io.emit('worker_updated', { id, status });
      addLog(`↻ Worker updated (via WS): ${id} → status=${status}`, io);
    }
  });

  // Manual unregister from front-end (if ever used)
  socket.on('manual_unregister', ({ id }) => {
    if (workers.has(id)) {
      workers.delete(id);
      if (currentCoordinatorId === id) currentCoordinatorId = null;
      io.emit('worker_removed', { id, coordinatorId: currentCoordinatorId });
      addLog(`↘ Worker removed (via WS): ${id}`, io);
    }
  });

  socket.on('disconnect', () => {
    addLog(`🔌 Frontend client disconnected (${socket.id})`, io);
  });
});

// ─── HTTP Endpoints ──────────────────────────────────────────────────────────

// 1) GET /algorithms → list all available algorithms
app.get('/algorithms', (req, res) => {
  console.log('📡 GET /algorithms called');
  console.log('📡 Returning algorithms:', algorithmsList);
  res.json(algorithmsList);
});

// 2) POST /register → containers call this to join/update status
//    Body: { id: string, name: string, status: string }
app.post('/register', async (req, res) => {
  const { id, name, status } = req.body;
  if (!id || !name || !status) {
    return res.status(400).json({ error: 'id, name, and status are required' });
  }

  const now = new Date();
  const existed = workers.has(id);

  if (!existed) {
    workers.set(id, { name, status, createdAt: now });
    if (!currentCoordinatorId) {
      currentCoordinatorId = id;
      addLog(`🟢 First worker ${id} joined and became Coordinator`, io);
    } else {
      addLog(`↗ Worker registered: ${id} (${name}). Coordinator=${currentCoordinatorId}`, io);
    }
    io.emit('worker_added', {
      id, name, status, createdAt: now, coordinatorId: currentCoordinatorId
    });
  } else {
    const info = workers.get(id);
    info.status = status;
    addLog(`↻ Worker ${id} updated status → ${status}`, io);
    io.emit('worker_updated', { id, status });
  }

  res.json({ success: true, coordinatorId: currentCoordinatorId });
});

// 3) POST /unregister → containers call this on shutdown
//    Body: { id: string }
app.post('/unregister', async (req, res) => {
  const { id } = req.body;
  if (!id || !workers.has(id)) {
    return res.status(400).json({ error: 'invalid id' });
  }
  workers.delete(id);
  if (currentCoordinatorId === id) {
    currentCoordinatorId = null;
    addLog(`↘ Coordinator ${id} left. No Coordinator now.`, io);
  } else {
    addLog(`↘ Worker ${id} left. Coordinator still ${currentCoordinatorId}`, io);
  }
  io.emit('worker_removed', { id, coordinatorId: currentCoordinatorId });
  return res.json({ success: true, coordinatorId: currentCoordinatorId });
});

// 4) POST /start-sort → admin triggers sorting job
//    Body: { algorithm: string, arrayLength: number }
app.post('/start-sort', async (req, res) => {
  const { algorithm, arrayLength } = req.body;
  if (!algorithm || !arrayLength || !Number.isInteger(arrayLength) || arrayLength <= 0) {
    return res.status(400).json({ error: 'algorithm & positive integer arrayLength required' });
  }

  const alg = algorithmsList.find(a => a.key === algorithm);
  if (!alg) {
    return res.status(400).json({ error: 'Unknown algorithm' });
  }

  // Find all “active” workers (status = connected or working, excluding the Coordinator)
  const activeWorkers = Array.from(workers.entries())
    .filter(([id, info]) => info.status === 'connected' || info.status === 'working')
    .map(([id, info]) => id);

  if (activeWorkers.length === 0) {
    return res.status(400).json({ error: 'No active workers available for sorting' });
  }

  addLog(`🚀 Starting sort: algorithm=${algorithm}, arrayLength=${arrayLength}`, io);

  // Generate the random array
  const fullArray = Array.from({ length: arrayLength }, () => Math.floor(Math.random() * 1000));
  addLog(`Generated random array: [${fullArray.join(', ')}]`, io);

  // Dynamically load the algorithm module
  let sorter;
  try {
    sorter = require(`./algorithms/${algorithm}/index.js`);
  } catch (e) {
    console.error('❌ Error loading algorithm module:', e);
    return res.status(500).json({ error: 'Failed to load algorithm module' });
  }

  // Split into N chunks (N = activeWorkers.length)
  const numWorkers = activeWorkers.length;
  const chunks = sorter.splitIntoChunks(fullArray, numWorkers);

  // Emit “assign_chunk” to each worker via Socket.io
  for (let i = 0; i < numWorkers; i++) {
    const workerId = activeWorkers[i];
    const { chunk, offset } = chunks[i];
    io.emit('assign_chunk', { workerId, chunk, offset });
    addLog(`Assigned chunk of size ${chunk.length} to Worker ${workerId}`, io);
  }

  // Mark Coordinator as “working” for the duration of this sort
  if (currentCoordinatorId) {
    const cinfo = workers.get(currentCoordinatorId);
    cinfo.status = 'working';
    io.emit('worker_updated', { id: currentCoordinatorId, status: 'working' });
  }

  // Run the parallel odd-even sort
  sorter.oddEvenSortParallel(chunks, io, '')
    .then((sortedFullArr) => {
      addLog(`Sorting complete. Final array: [${sortedFullArr.join(', ')}]`, io);

      // Un‐mark Coordinator as “working”
      if (currentCoordinatorId) {
        const cinfo2 = workers.get(currentCoordinatorId);
        cinfo2.status = 'connected';
        io.emit('worker_updated', { id: currentCoordinatorId, status: 'connected' });
      }

      // Emit final sorted array to front-end
      io.emit('sort_complete', { sortedArray: sortedFullArr });
    })
    .catch((err) => {
      console.error('❌ Error during parallel sort:', err);
      addLog(`❌ Error during sorting: ${err.message}`, io);

      if (currentCoordinatorId && workers.has(currentCoordinatorId)) {
        const cinfo2 = workers.get(currentCoordinatorId);
        cinfo2.status = 'connected';
        io.emit('worker_updated', { id: currentCoordinatorId, status: 'connected' });
      }
      io.emit('sort_error', { message: err.message });
    });

  // Reply immediately; final result is sent over WebSocket
  res.json({ success: true, message: `Sorting started with ${numWorkers} workers.` });
});

// 5) GET /workers → debug endpoint
app.get('/workers', (req, res) => {
  const arr = [];
  for (let [id, info] of workers.entries()) {
    arr.push({
      id,
      name: info.name,
      status: info.status,
      createdAt: info.createdAt
    });
  }
  res.json({ workers: arr, coordinatorId: currentCoordinatorId });
});

// ─── Start HTTP + WebSocket server on port 3000 ───────────────────────────────
const PORT = 3000;
server.listen(PORT, () => {
  console.log(`🚀 Backend listening on http://0.0.0.0:${PORT}`);
  console.log(`📁 (Static files are served by NGINX from the frontend container)`);
  console.log(`📊 Algorithms loaded: ${algorithmsList.length}`);
});
