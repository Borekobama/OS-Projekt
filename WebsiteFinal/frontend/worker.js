// worker.js

// A new worker shows up
function handleWorkerAdded(data) {
  // check if it’s already in our array
  var already = false;
  for (var i = 0; i < workers.length; i++) {
    if (workers[i].id === data.id) {
      already = true;
      break;
    }
  }
  if (!already) {
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
  appendLog('[Worker ' + data.id + '] joined', 'info');
  populateCoordinatorDropdown();
  positionWorkers();
  drawConnections();
  highlightCoordinatorNode();
}

// A worker changed its status
function handleWorkerUpdated(data) {
  for (var i = 0; i < workers.length; i++) {
    if (workers[i].id === data.id) {
      workers[i].status = data.status;
      break;
    }
  }
  updateWorkerLine(data.id);
  var sel = document.querySelector('#worker-row-' + data.id + ' select');
  if (sel) {
    sel.value = data.status;
  }
  appendLog('[Worker ' + data.id + '] status → ' + data.status, 'info');
}

// A worker left or disconnected
function handleWorkerRemoved(data) {
  currentCoordinatorId = data.coordinatorId;
  appendLog('[Worker ' + data.id + '] left/disconnected', 'warning');
  removeWorker(data.id);
  populateCoordinatorDropdown();
  highlightCoordinatorNode();
}

// Remove from array + DOM
function removeWorker(id) {
  var newArr = [];
  for (var i = 0; i < workers.length; i++) {
    if (workers[i].id !== id) {
      newArr.push(workers[i]);
    }
  }
  workers = newArr;
  // remove node
  var nd = document.querySelector('.worker-node[data-id="' + id + '"]');
  if (nd) {
    nd.parentNode.removeChild(nd);
  }
  // remove control row
  var rw = document.getElementById('worker-row-' + id);
  if (rw) {
    rw.parentNode.removeChild(rw);
  }
  recalcAngles();
  positionWorkers();
  drawConnections();
}

// simple angle recalculation
function recalcAngles() {
  var ring = [];
  for (var i = 0; i < workers.length; i++) {
    if (workers[i].id !== currentCoordinatorId) {
      ring.push(workers[i]);
    }
  }
  var n = ring.length;
  for (var j = 0; j < ring.length; j++) {
    ring[j].angle = (2 * Math.PI * j) / (n || 1);
  }
}
