// main.js

// === Global state ===
var socket;
var workers = [];
var currentCoordinatorId = null;
var currentArray = [];
var selectedOperation = null;
var topologyMode = 'star';  // 'star' or 'chain'

// === DOM references ===
var diagramContainer    = document.getElementById('diagram-container');
var centerNode          = document.getElementById('center');
var workersContainer    = document.getElementById('workers-container');
var connectionsSVG      = document.getElementById('connections');
var algorithmSelect     = document.getElementById('algorithm-select');
var addWorkerBtn        = document.getElementById('add-worker-btn');
var workersList         = document.getElementById('workers-list');
var coordinatorSelect   = document.getElementById('coordinator-select');
var toggleLogsCheckbox  = document.getElementById('toggle-logs');
var logContainer        = document.getElementById('log-container');
var logOutput           = document.getElementById('log-output');
var arrayLengthInput    = document.getElementById('array-length');
var generateArrayBtn    = document.getElementById('generate-array-btn');
var customArrayInput    = document.getElementById('custom-array');
var distributeArrayBtn  = document.getElementById('distribute-array-btn');
var startOperationBtn   = document.getElementById('start-operation-btn');
var opButtons           = document.querySelectorAll('.op-btn');

// Worker status options
var STATUS_OPTIONS = [
  { value: 'connected',    label: 'Connected' },
  { value: 'working',      label: 'Working' },
  { value: 'suspended',    label: 'Suspended' },
  { value: 'disconnected', label: 'Disconnected' }
];

var centerX = 0, centerY = 0;

// === Initialize on page load ===
window.addEventListener('load', function() {
  // Position center node
  positionCenter();

  // Initialize WebSocket and handlers
  initializeWebSocket();
  initializeSocketHandlers();

  // Populate algorithm dropdown
  initAlgorithmDropdown();

  // Re-layout on resize
  window.addEventListener('resize', function() {
    positionCenter();
    positionWorkers();
    drawConnections();
  });

  // Operation button clicks
  opButtons.forEach(function(btn) {
    btn.addEventListener('click', function() {
      opButtons.forEach(b => b.classList.remove('active'));
      this.classList.add('active');
      selectedOperation = this.dataset.op;
      appendLog('Selected operation: ' + selectedOperation, 'info');
    });
  });

  // Generate random array
  generateArrayBtn.addEventListener('click', function() {
    var len = parseInt(arrayLengthInput.value, 10);
    if (!len || len <= 0) {
      alert('Please enter a positive array length.');
      return;
    }
    currentArray = [];
    for (var i = 0; i < len; i++) {
      currentArray.push(Math.floor(Math.random() * 1000));
    }
    appendLog('Random array generated: [' + currentArray.join(', ') + ']', 'info');
  });

  // Distribute array (star topology)
  distributeArrayBtn.addEventListener('click', function() {
    if (!selectedOperation) {
      alert('Select an operation first.');
      return;
    }
    var txt = customArrayInput.value.trim();
    if (txt) {
      try {
        var parsed = JSON.parse(txt);
        if (!Array.isArray(parsed)) throw new Error();
        currentArray = parsed;
      } catch (e) {
        alert('Invalid custom array. Use JSON array format, e.g. [1,2,3]');
        return;
      }
    }
    if (!currentArray.length) {
      alert('Generate or input an array first.');
      return;
    }
    topologyMode = 'star';
    drawConnections();

    appendLog('Distributing array (' + currentArray.length + ' elements)…', 'info');
    fetch('/distribute-array', {
      method: 'POST',
      headers: { 'Content-Type':'application/json' },
      body: JSON.stringify({ array: currentArray })
    })
    .then(function(resp) {
      if (!resp.ok) throw new Error('HTTP ' + resp.status);
      appendLog('Array distributed; ready to start.', 'info');
    })
    .catch(function(err) {
      appendLog('Distribute error: ' + err.message, 'error');
    });
  });

  // Start operation (chain for SORT)
  startOperationBtn.addEventListener('click', function() {
    if (!selectedOperation) {
      alert('Select an operation first.');
      return;
    }
    topologyMode = (selectedOperation === 'SORT') ? 'chain' : 'star';
    drawConnections();

    appendLog('Starting operation ' + selectedOperation + '…', 'info');
    fetch('/start-operation', {
      method: 'POST',
      headers: { 'Content-Type':'application/json' },
      body: JSON.stringify({ operation: selectedOperation })
    })
    .then(function(resp) {
      if (!resp.ok) throw new Error('HTTP ' + resp.status);
      return resp.json();
    })
    .then(function(data) {
      if (data.error) {
        appendLog('Error: ' + data.error, 'error');
      } else if (data.result !== undefined) {
        appendLog('Result: ' + JSON.stringify(data.result), 'info');
      } else if (data.message) {
        appendLog(data.message, 'info');
      }
    })
    .catch(function(err) {
      appendLog('Start error: ' + err.message, 'error');
    });
  });

  // Add Worker (mock)
  addWorkerBtn.addEventListener('click', function() {
    var name = prompt('Enter worker name:');
    if (!name || !name.trim()) return;
    name = name.trim();
    var newId = 'mock-' + Date.now();
    var createdAt = new Date();
    workers.push({ id: newId, name: name, status: 'connected', createdAt: createdAt, angle: 0 });

    recalcAngles();
    renderWorkerNode(newId);
    renderControlRow(newId);
    populateCoordinatorDropdown();
    positionWorkers();
    drawConnections();

    fetch('/register', {
      method: 'POST',
      headers: { 'Content-Type':'application/json' },
      body: JSON.stringify({ id: newId, name: name, status: 'connected' })
    }).catch(console.error);
  });

  // Toggle log viewer
  toggleLogsCheckbox.addEventListener('change', function(e) {
    logContainer.style.display = e.target.checked ? 'block' : 'none';
  });

  // Final initial render
  renderAllWorkers();
  drawConnections();
});

// === WebSocket initialization ===
function initializeWebSocket() {
  socket = io();
}

function initializeSocketHandlers() {
  if (!socket) return;
  socket.on('log-update', handleLogUpdate);
  socket.on('sort-complete', handleSortComplete);
  socket.on('sort-error', handleSortError);
  socket.on('operation-update', function(data) {
    appendLog(data.message, 'info');
  });
  socket.on('operation-complete', function(payload) {
    if (selectedOperation === 'SORT') {
      topologyMode = 'star';
      drawConnections();
    }
    appendLog('Operation complete: ' + JSON.stringify(payload), 'info');
  });
}

// === Algorithm dropdown ===
function initAlgorithmDropdown() {
  algorithmSelect.innerHTML = '<option>(loading…)</option>';
  fetch('/algorithms')
    .then(function(resp) {
      if (!resp.ok) throw new Error(resp.status);
      return resp.json();
    })
    .then(function(list) {
      algorithmSelect.innerHTML = '';
      list.forEach(function(alg) {
        var opt = document.createElement('option');
        opt.value = alg.key;
        opt.textContent = alg.name;
        algorithmSelect.appendChild(opt);
      });
      algorithmSelect.value = list[0] ? list[0].key : '';
    })
    .catch(function(err) {
      appendLog('Error loading algorithms: ' + err.message, 'error');
      algorithmSelect.innerHTML = '<option>(error)</option>';
    });
}

// === Logging callbacks ===
function handleLogUpdate(text) {
  var now = new Date().toLocaleTimeString('en-GB',{hour12:false});
  var stripped = text.replace(/^\[.*?\]\s*/, '');
  appendLog('[' + now + '] ' + stripped, 'info');
}

function handleSortComplete(payload) {
  appendLog('Sort complete! Final array: [' + payload.sortedArray.join(', ') + ']', 'info');
  startOperationBtn.disabled = false;
  topologyMode = 'star';
  drawConnections();
}

function handleSortError(payload) {
  appendLog('Sort error: ' + payload.message, 'error');
  startOperationBtn.disabled = false;
}

// === Rendering & Topology ===
function positionCenter() {
  var rect = diagramContainer.getBoundingClientRect();
  centerX = rect.width / 2;
  centerY = rect.height / 2;
  centerNode.style.left = centerX + 'px';
  centerNode.style.top  = centerY + 'px';
}

function recalcAngles() {
  var n = workers.length;
  workers.forEach(function(w, i) {
    w.angle = (2 * Math.PI / n) * i;
  });
}

function positionWorkers() {
  recalcAngles();
  workers.forEach(function(w) {
    var el = document.querySelector('.worker-node[data-id="' + w.id + '"]');
    if (!el) return;
    var circ = el.querySelector('.node-circle');
    var lbl  = el.querySelector('.node-label');
    if (w.id === currentCoordinatorId) {
      el.style.left = centerX + 'px';
      el.style.top  = centerY + 'px';
      circ.style.borderColor = '#28a745';
      lbl.textContent = w.name + ' [Coordinator]';
    } else {
      var rect = diagramContainer.getBoundingClientRect();
      var radius = Math.min(rect.width, rect.height) / 3;
      var x = centerX + radius * Math.cos(w.angle);
      var y = centerY + radius * Math.sin(w.angle);
      el.style.left = x + 'px';
      el.style.top  = y + 'px';
      circ.style.borderColor = '#555555';
      lbl.textContent = w.name;
    }
  });
  centerNode.style.display = currentCoordinatorId ? 'none' : 'block';
}

function renderAllWorkers() {
  workers.forEach(function(w) {
    renderWorkerNode(w.id);
    renderControlRow(w.id);
  });
}

function renderWorkerNode(id) {
  if (document.querySelector('.worker-node[data-id="' + id + '"]')) return;
  var w = workers.find(function(x) { return x.id === id; });
  if (!w) return;
  var div = document.createElement('div');
  div.className = 'node worker-node';
  div.setAttribute('data-id', id);
  var circ = document.createElement('div');
  circ.className = 'node-circle';
  var img = document.createElement('img');
  img.src = 'computer.png';
  img.alt = 'Worker ' + w.name;
  circ.appendChild(img);
  div.appendChild(circ);
  var lbl = document.createElement('div');
  lbl.className = 'node-label';
  lbl.textContent = w.name;
  div.appendChild(lbl);
  workersContainer.appendChild(div);
}

function renderControlRow(id) {
  if (document.getElementById('worker-row-' + id)) return;
  var w = workers.find(function(x) { return x.id === id; });
  if (!w) return;
  var row = document.createElement('div');
  row.className = 'worker-row';
  row.id = 'worker-row-' + id;
  var top = document.createElement('div'); top.className = 'row-top';
  var nameDiv = document.createElement('div'); nameDiv.className = 'worker-name';
  nameDiv.textContent = w.name; top.appendChild(nameDiv);
  var rmBtn = document.createElement('button');
  rmBtn.className = 'btn remove-btn'; rmBtn.textContent = 'Remove';
  rmBtn.addEventListener('click', function() {
    fetch('/unregister', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({ id: id }) }).catch(console.error);
    removeWorker(id);
  }); top.appendChild(rmBtn);
  row.appendChild(top);
  var ctrls = document.createElement('div'); ctrls.className = 'worker-controls';
  var sel = document.createElement('select');
  STATUS_OPTIONS.forEach(function(o) {
    var opt = document.createElement('option'); opt.value = o.value; opt.textContent = o.label; sel.appendChild(opt);
  }); sel.value = w.status;
  sel.addEventListener('change', function(e) {
    w.status = e.target.value; updateWorkerLine(id);
    fetch('/register', { method: 'POST', headers: {'Content-Type':'application/json'}, body: JSON.stringify({ id: id, name: w.name, status: w.status }) }).catch(console.error);
  }); ctrls.appendChild(sel);
  var runSpan = document.createElement('div');
  runSpan.className = 'runtime'; runSpan.id = 'runtime-' + id;
  runSpan.textContent = 'Runtime: 00:00:00'; ctrls.appendChild(runSpan);
  row.appendChild(ctrls);
  workersList.appendChild(row);
}

function removeWorker(id) {
  workers = workers.filter(function(w) { return w.id !== id; });
  var node = document.querySelector('.worker-node[data-id="' + id + '"]'); if (node) node.remove();
  var row = document.getElementById('worker-row-' + id); if (row) row.remove();
  populateCoordinatorDropdown();
  drawConnections();
}

function populateCoordinatorDropdown() {
  coordinatorSelect.innerHTML = '<option value="">(none)</option>';
  workers.forEach(function(w) {
    var opt = document.createElement('option'); opt.value = w.id; opt.textContent = w.name;
    coordinatorSelect.appendChild(opt);
  });
}
coordinatorSelect.addEventListener('change', function(e) {
  currentCoordinatorId = e.target.value || null;
  positionWorkers(); drawConnections();
});

function updateWorkerLine(id) {
  var w = workers.find(function(x) { return x.id === id; });
  if (!w) return;
  var line = connectionsSVG.querySelector('line[data-id="' + id + '"]');
  if (!line) return;
  line.className.baseVal = w.status;
}

function drawConnections() {
  while (connectionsSVG.firstChild) connectionsSVG.removeChild(connectionsSVG.firstChild);
  if (!currentCoordinatorId) return;
  var others = workers.filter(function(w) { return w.id !== currentCoordinatorId; })
                .sort(function(a,b) { return a.angle - b.angle; })
                .map(function(w) { return w.id; });
  if (topologyMode === 'star') {
    others.forEach(function(id) { makeLine(currentCoordinatorId, id); });
  } else {
    var chain = [currentCoordinatorId].concat(others);
    for (var i = 0; i < chain.length - 1; i++) {
      makeLine(chain[i], chain[i+1]);
    }
  }
}

function makeLine(fromId, toId) {
  var fromEl = document.querySelector('.worker-node[data-id="' + fromId + '"]');
  var toEl   = document.querySelector('.worker-node[data-id="' + toId   + '"]');
  if (!fromEl || !toEl) return;
  var pRect = diagramContainer.getBoundingClientRect();
  var fRect = fromEl.getBoundingClientRect();
  var tRect = toEl.getBoundingClientRect();
  var x1 = fRect.left - pRect.left + fRect.width/2;
  var y1 = fRect.top  - pRect.top  + fRect.height/2;
  var x2 = tRect.left - pRect.left + tRect.width/2;
  var y2 = tRect.top  - pRect.top  + tRect.height/2;
  var line = document.createElementNS('http://www.w3.org/2000/svg','line');
  line.setAttribute('x1', x1);
  line.setAttribute('y1', y1);
  line.setAttribute('x2', x2);
  line.setAttribute('y2', y2);
  line.setAttribute('data-id', toId);
  var wData = workers.find(function(w) { return w.id === toId; });
  line.classList.add(wData ? wData.status : 'connected');
  connectionsSVG.appendChild(line);
}

function appendLog(text, severity) {
  severity = severity || 'info';
  var div = document.createElement('div');
  div.className = 'log-' + severity;
  div.textContent = text;
  logOutput.appendChild(div);
  logOutput.scrollTop = logOutput.scrollHeight;
}
