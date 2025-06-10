// script.js

// Does this file even load?
console.log('✅ script.js loaded');

// ========== Global State ==========
let workers = [];      // Array of { id, name, angle, status, createdAt }
let socket = null;     // Socket.io connection
let currentCoordinatorId = null; // ID of the current coordinator

// DOM references
const diagramContainer = document.getElementById('diagram-container');
const centerNode = document.getElementById('center');
const workersContainer = document.getElementById('workers-container');
const connectionsSVG = document.getElementById('connections');
const addWorkerBtn = document.getElementById('add-worker-btn');
const workersList = document.getElementById('workers-list');
const coordinatorSelect = document.getElementById('coordinator-select');

// Algorithm UI references
const algorithmSelect = document.getElementById('algorithm-select');
const arrayLengthInput = document.getElementById('array-length');
const startSortBtn = document.getElementById('start-sort-btn');

// Log Viewer references
const toggleLogsCheckbox = document.getElementById('toggle-logs');
const logContainer = document.getElementById('log-container');
const logOutput = document.getElementById('log-output');

// Center slot’s coordinates (recomputed on resize)
let centerX = 0, centerY = 0;

// Possible statuses
const STATUS_OPTIONS = [
  { value: 'connected', label: 'Connected' },
  { value: 'working',   label: 'Working' },
  { value: 'suspended', label: 'Suspended' },
  { value: 'disconnected', label: 'Disconnected' }
];

// ========== Initialization ==========
window.addEventListener('load', () => {
  console.log('🔍 Window loaded — calling init functions');
  positionCenter();
  initializeWebSocket();
  initAlgorithmDropdown();
  window.addEventListener('resize', () => {
    positionCenter();
    updateAllPositions();
    redrawAllLines();
  });
});

// Center the “Center” slot icon
function positionCenter() {
  const rect = diagramContainer.getBoundingClientRect();
  centerX = rect.width / 2;
  centerY = rect.height / 2;
  centerNode.style.left = `${centerX}px`;
  centerNode.style.top = `${centerY}px`;
}

// ========== WebSocket Logic (Socket.io) ==========
function initializeWebSocket() {
  socket = io();

  socket.on('connect', () => {
    appendLog(`WebSocket connected: ${socket.id}`, 'info');
  });

  // 1) initial_snapshot: { workers: [...], coordinatorId }
  socket.on('initial_snapshot', (payload) => {
    currentCoordinatorId = payload.coordinatorId;
    workers = [];
    payload.workers.forEach(wObj => {
      workers.push({
        id: wObj.id,
        name: wObj.name,
        status: wObj.status,
        createdAt: new Date(wObj.createdAt),
        angle: 0
      });
    });
    recalcAngles();
    renderAllWorkers();
    populateCoordinatorDropdown();
    updateAllPositions();
    redrawAllLines();
    highlightCoordinatorNode();
  });

  // 2) worker_added
  socket.on('worker_added', (data) => {
    if (!workers.find(w => w.id === data.id)) {
      workers.push({
        id: data.id,
        name: data.name,
        status: data.status,
        createdAt: new Date(data.createdAt),
        angle: 0
      });
      recalcAngles();
      renderWorkerNode(data.id);
      renderControlRow(data.id);
    }
    currentCoordinatorId = data.coordinatorId;
    instanceLogWithWorker(data.id, `joined`, 'info');
    populateCoordinatorDropdown();
    updateAllPositions();
    redrawAllLines();
    highlightCoordinatorNode();
  });

  // 3) worker_updated
  socket.on('worker_updated', (data) => {
    const w = workers.find(x => x.id === data.id);
    if (!w) return;
    w.status = data.status;
    updateWorkerLine(data.id);
    const select = document.querySelector(`#worker-row-${data.id} select`);
    if (select) select.value = data.status;
    instanceLogWithWorker(data.id, `status → ${data.status}`, 'info');
  });

  // 4) worker_removed
  socket.on('worker_removed', (data) => {
    currentCoordinatorId = data.coordinatorId;
    instanceLogWithWorker(data.id, `left/disconnected`, 'warning');
    removeWorker(data.id);
    populateCoordinatorDropdown();
    highlightCoordinatorNode();
  });

  // 5) coordinator_switched
  socket.on('coordinator_switched', (data) => {
    instanceLogWithWorker(data.newCoordinatorId, `became Coordinator`, 'info');
    currentCoordinatorId = data.newCoordinatorId;
    populateCoordinatorDropdown();
    updateAllPositions();
    redrawAllLines();
    highlightCoordinatorNode();
  });

  // 6) Logs from backend
  socket.on('log_update', (text) => {
    // Always strip any leading ISO‐date prefix (e.g. "[2025-06-04T13:54:36.786Z] ")
    const stripped = text.replace(/^\[\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+Z\]\s*/, '');
    formatAndAppendLog(stripped);
  });

  // 7) Sorting complete
  socket.on('sort_complete', (payload) => {
    appendLog(`Sort complete! Final sorted array: [${payload.sortedArray.join(', ')}]`, 'info');
    startSortBtn.disabled = false;
  });

  // 8) Sort error
  socket.on('sort_error', (payload) => {
    appendLog(`Sort error: ${payload.message}`, 'error');
    startSortBtn.disabled = false;
  });

  socket.on('disconnect', () => {
    appendLog(`WebSocket disconnected`, 'error');
  });
}

// ========== Log‐Appending Helpers ==========

// Format a stripped‐down `text` (no date prefix), then append a <div> to #log-output
function formatAndAppendLog(text) {
  // Extract a “Worker ID” if present: e.g. “Worker abc123 ...”
  let workerTag = '';
  const m = text.match(/Worker\s+([^\s]+)/i);
  if (m && m[1]) {
    workerTag = `[${m[1]}] `;
  }

  // Determine severity from keywords:
  const lower = text.toLowerCase();
  let severity = 'info';
  if (lower.includes('error') || lower.includes('disconnected')) {
    severity = 'error';
  } else if (lower.includes('waiting') || lower.includes('retry') || lower.includes('left')) {
    severity = 'warning';
  }

  // Local time stamp (HH:MM:SS)
  const now = new Date();
  const hhmmss = now.toLocaleTimeString('en-GB', { hour12: false });

  const lineText = `[${hhmmss}] ${workerTag}${text}`;
  appendLog(lineText, severity);
}

// Actually append a single line of text, with a <div class="log‐xxx">
function appendLog(lineText, severity = 'info') {
  const entry = document.createElement('div');
  entry.classList.add(`log-${severity}`);
  entry.textContent = lineText;
  logOutput.appendChild(entry);
  // Scroll to bottom
  logOutput.scrollTop = logOutput.scrollHeight;
}

// Shortcut to log a simple “Worker X did Y” style message
function instanceLogWithWorker(workerId, message, severity = 'info') {
  // Find name from workers array
  const w = workers.find(wk => wk.id === workerId);
  const nameOrId = w ? w.name : workerId;
  appendLog(`[${new Date().toLocaleTimeString('en-GB', { hour12: false })}] [${nameOrId}] ${message}`, severity);
}

// ========== Algorithm Dropdown Initialization ==========

function initAlgorithmDropdown() {
  console.log('🔍 Calling initAlgorithmDropdown()');
  algorithmSelect.innerHTML = '<option value="">(loading…)</option>';

  console.log('🌐 About to fetch /algorithms');
  console.log('🌐 Current location:', window.location.href);
  console.log('🌐 Fetch URL will be:', new URL('/algorithms', window.location.origin).href);

  fetch('/algorithms')
    .then((resp) => {
      console.log('→ Received response for GET /algorithms:', resp);
      console.log('→ Response status:', resp.status);
      console.log('→ Response ok:', resp.ok);
      console.log('→ Response headers:', [...resp.headers.entries()]);
      if (!resp.ok) {
        throw new Error(`HTTP ${resp.status}: ${resp.statusText}`);
      }
      return resp.json();
    })
    .then((list) => {
      console.log('→ Parsed algorithms JSON:', list);
      console.log('→ Type of list:', typeof list);
      console.log('→ Is array:', Array.isArray(list));
      console.log('→ List length:', list?.length);

      algorithmSelect.innerHTML = '';
      if (!Array.isArray(list) || list.length === 0) {
        console.warn('⚠️ No algorithms available or invalid format');
        algorithmSelect.innerHTML = '<option value="">(no algorithms available)</option>';
        return;
      }

      list.forEach((alg, index) => {
        console.log(`→ Processing algorithm ${index}:`, alg);
        const opt = document.createElement('option');
        opt.value = alg.key;
        opt.textContent = alg.name;
        algorithmSelect.appendChild(opt);
      });

      algorithmSelect.value = list[0].key;
      console.log('✅ Algorithm dropdown populated successfully');
    })
    .catch((err) => {
      console.error('❌ Error loading algorithms:', err);
      console.error('❌ Error stack:', err.stack);
      algorithmSelect.innerHTML = '<option value="">(error loading)</option>';
    });
}

// ========== “Start Sorting” Handler ==========

startSortBtn.addEventListener('click', () => {
  const chosenAlg = algorithmSelect.value;
  const lengthVal = parseInt(arrayLengthInput.value);

  if (!chosenAlg) {
    alert('Please select an algorithm.');
    return;
  }
  if (!lengthVal || lengthVal <= 0) {
    alert('Please enter a positive array length.');
    return;
  }

  startSortBtn.disabled = true;
  appendLog(`[${new Date().toLocaleTimeString('en-GB', { hour12: false })}] Requested sort: ${chosenAlg} length=${lengthVal}`, 'info');

  fetch('/start-sort', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      algorithm: chosenAlg,
      arrayLength: lengthVal
    })
  })
    .then((resp) => {
      console.log('→ Response from POST /start-sort:', resp);
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
      return resp.json();
    })
    .then((data) => {
      if (data.error) {
        appendLog(data.error, 'error');
        startSortBtn.disabled = false;
      } else {
        appendLog(data.message, 'info');
      }
    })
    .catch((err) => {
      appendLog(err.message, 'error');
      startSortBtn.disabled = false;
    });
});

// Toggle the logs area
toggleLogsCheckbox.addEventListener('change', (e) => {
  logContainer.style.display = e.target.checked ? 'block' : 'none';
});

// ========== Worker Management (unchanged logic) ==========

addWorkerBtn.addEventListener('click', () => {
  const name = prompt('Enter worker name:');
  if (!name || !name.trim()) return;

  const newId = 'mock-' + Date.now();
  const createdAt = new Date();

  workers.push({
    id: newId,
    name: name.trim(),
    angle: 0,
    status: 'connected',
    createdAt
  });
  recalcAngles();
  renderWorkerNode(newId);
  renderControlRow(newId);
  populateCoordinatorDropdown();
  updateAllPositions();
  redrawAllLines();
  highlightCoordinatorNode();

  fetch('/register', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id: newId, name: name.trim(), status: 'connected' })
  }).catch(err => console.error('Error registering mock worker:', err));
});

function removeWorker(id) {
  workers = workers.filter(w => w.id !== id);

  const wDiv = document.querySelector(`.worker-node[data-id="${id}"]`);
  if (wDiv) wDiv.remove();

  const ctrlRow = document.getElementById(`worker-row-${id}`);
  if (ctrlRow) ctrlRow.remove();

  recalcAngles();
  updateAllPositions();
  redrawAllLines();
}

function recalcAngles() {
  const ringWorkers = workers.filter(w => w.id !== currentCoordinatorId);
  const n = ringWorkers.length;
  ringWorkers.forEach((w, idx) => {
    w.angle = (2 * Math.PI * idx) / n;
  });
}

// ========== Rendering / Positioning (unchanged) ==========

function renderAllWorkers() {
  workers.forEach(w => {
    renderWorkerNode(w.id);
    renderControlRow(w.id);
  });
}

function renderWorkerNode(id) {
  const worker = workers.find(w => w.id === id);
  if (!worker) return;
  if (document.querySelector(`.worker-node[data-id="${id}"]`)) return;

  const div = document.createElement('div');
  div.classList.add('node', 'worker-node');
  div.setAttribute('data-id', id);

  const circle = document.createElement('div');
  circle.classList.add('node-circle');
  const img = document.createElement('img');
  img.src = 'computer.png';
  img.alt = `Worker ${worker.name}`;
  circle.appendChild(img);
  div.appendChild(circle);

  const label = document.createElement('div');
  label.classList.add('node-label');
  label.textContent = worker.name;
  div.appendChild(label);

  workersContainer.appendChild(div);
}

function updateAllPositions() {
  recalcAngles();

  workers.forEach((w) => {
    const nodeEl = document.querySelector(`.worker-node[data-id="${w.id}"]`);
    if (!nodeEl) return;
    const circleEl = nodeEl.querySelector('.node-circle');
    const labelEl = nodeEl.querySelector('.node-label');

    if (w.id === currentCoordinatorId) {
      nodeEl.style.left = `${centerX}px`;
      nodeEl.style.top = `${centerY}px`;
      circleEl.style.borderColor = '#28a745';
      labelEl.textContent = w.name + ' [Coordinator]';
    } else {
      const rect = diagramContainer.getBoundingClientRect();
      const radius = Math.min(rect.width, rect.height) / 3;
      const x = centerX + radius * Math.cos(w.angle);
      const y = centerY + radius * Math.sin(w.angle);
      nodeEl.style.left = `${x}px`;
      nodeEl.style.top = `${y}px`;
      circleEl.style.borderColor = '#555';
      labelEl.textContent = w.name;
    }
  });

  if (!currentCoordinatorId) {
    centerNode.style.display = 'block';
    centerNode.querySelector('.node-circle').classList.remove('coordinator');
    centerNode.querySelector('.node-label').textContent = 'Center';
  } else {
    centerNode.style.display = 'none';
  }
}

function redrawAllLines() {
  while (connectionsSVG.firstChild) {
    connectionsSVG.removeChild(connectionsSVG.firstChild);
  }
  if (!currentCoordinatorId) return;

  workers.forEach((w) => {
    if (w.id === currentCoordinatorId) return;
    const coordEl = document.querySelector(`.worker-node[data-id="${currentCoordinatorId}"]`);
    const nodeEl = document.querySelector(`.worker-node[data-id="${w.id}"]`);
    if (!coordEl || !nodeEl) return;

    const coordRect = coordEl.getBoundingClientRect();
    const wRect = nodeEl.getBoundingClientRect();
    const parentRect = diagramContainer.getBoundingClientRect();

    const x1 = coordRect.left - parentRect.left + coordRect.width / 2;
    const y1 = coordRect.top - parentRect.top + coordRect.height / 2;
    const x2 = wRect.left - parentRect.left + wRect.width / 2;
    const y2 = wRect.top - parentRect.top + wRect.height / 2;

    const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
    line.setAttribute('x1', x1);
    line.setAttribute('y1', y1);
    line.setAttribute('x2', x2);
    line.setAttribute('y2', y2);
    line.setAttribute('data-id', w.id);
    line.classList.add(w.status);
    connectionsSVG.appendChild(line);
  });
}

function updateWorkerLine(id) {
  const w = workers.find(x => x.id === id);
  if (!w) return;
  const line = connectionsSVG.querySelector(`line[data-id="${id}"]`);
  if (!line) return;
  line.classList.remove('connected','working','suspended','disconnected');
  line.classList.add(w.status);
}

function highlightCoordinatorNode() {
  document.querySelectorAll('.worker-node .node-circle').forEach(el => {
    el.style.borderColor = '#555';
  });
  document.querySelectorAll('.worker-node .node-label').forEach(el => {
    el.textContent = el.textContent.replace(' [Coordinator]', '');
  });

  centerNode.querySelector('.node-circle').classList.remove('coordinator');
  centerNode.querySelector('.node-label').textContent = 'Center';

  if (!currentCoordinatorId) {
    centerNode.style.display = 'block';
    return;
  }

  centerNode.style.display = 'none';

  const circleEls = document.querySelectorAll(`.worker-node[data-id="${currentCoordinatorId}"] .node-circle`);
  const labelEls = document.querySelectorAll(`.worker-node[data-id="${currentCoordinatorId}"] .node-label`);
  if (circleEls.length > 0 && labelEls.length > 0) {
    circleEls[0].style.borderColor = '#28a745';
    labelEls[0].textContent = workers.find(w => w.id === currentCoordinatorId).name + ' [Coordinator]';
  }
}

// ========== Control Panel UI ==========

function populateCoordinatorDropdown() {
  while (coordinatorSelect.firstChild) {
    coordinatorSelect.removeChild(coordinatorSelect.firstChild);
  }
  const noneOpt = document.createElement('option');
  noneOpt.value = '';
  noneOpt.textContent = '(none)';
  coordinatorSelect.appendChild(noneOpt);

  workers.forEach(w => {
    const opt = document.createElement('option');
    opt.value = w.id;
    opt.textContent = `${w.name} (${w.id.slice(0,6)})`;
    if (w.id === currentCoordinatorId) opt.selected = true;
    coordinatorSelect.appendChild(opt);
  });
}

coordinatorSelect.addEventListener('change', (e) => {
  const newId = e.target.value || null;
  if (newId === currentCoordinatorId) return;
  socket.emit('request_coordinator_switch', { newCoordinatorId: newId });
});

function renderControlRow(id) {
  const worker = workers.find(w => w.id === id);
  if (!worker) return;
  if (document.getElementById(`worker-row-${id}`)) return;

  const row = document.createElement('div');
  row.classList.add('worker-row');
  row.id = `worker-row-${id}`;

  const rowTop = document.createElement('div');
  rowTop.classList.add('row-top');

  const nameDiv = document.createElement('div');
  nameDiv.classList.add('worker-name');
  nameDiv.textContent = worker.name;
  rowTop.appendChild(nameDiv);

  const removeBtn = document.createElement('button');
  removeBtn.classList.add('btn', 'remove-btn');
  removeBtn.textContent = 'Remove';
  removeBtn.addEventListener('click', () => {
    fetch('/unregister', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id })
    }).catch(err => console.error('Error unregister:', err));
    removeWorker(id);
  });
  rowTop.appendChild(removeBtn);
  row.appendChild(rowTop);

  const controlsDiv = document.createElement('div');
  controlsDiv.classList.add('worker-controls');

  const statusSelect = document.createElement('select');
  STATUS_OPTIONS.forEach(o => {
    const opt = document.createElement('option');
    opt.value = o.value;
    opt.textContent = o.label;
    statusSelect.appendChild(opt);
  });
  statusSelect.value = worker.status;
  statusSelect.addEventListener('change', (e) => {
    const newStatus = e.target.value;
    worker.status = newStatus;
    updateWorkerLine(id);

    fetch('/register', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ id, name: worker.name, status: newStatus })
    }).catch(err => console.error('Error updating status:', err));
  });
  controlsDiv.appendChild(statusSelect);

  const runtimeSpan = document.createElement('div');
  runtimeSpan.classList.add('runtime');
  runtimeSpan.id = `runtime-${id}`;
  runtimeSpan.textContent = 'Runtime: 00:00:00';
  controlsDiv.appendChild(runtimeSpan);

  row.appendChild(controlsDiv);
  workersList.appendChild(row);
}

setInterval(() => {
  const now = Date.now();
  workers.forEach(w => {
    const diff = now - w.createdAt.getTime();
    const hours = Math.floor(diff / (1000 * 60 * 60));
    const minutes = Math.floor((diff % (1000 * 60 * 60)) / (1000 * 60));
    const seconds = Math.floor((diff % (1000 * 60)) / 1000);
    const hh = String(hours).padStart(2, '0');
    const mm = String(minutes).padStart(2, '0');
    const ss = String(seconds).padStart(2, '0');
    const span = document.getElementById(`runtime-${w.id}`);
    if (span) span.textContent = `Runtime: ${hh}:${mm}:${ss}`;
  });
}, 1000);
