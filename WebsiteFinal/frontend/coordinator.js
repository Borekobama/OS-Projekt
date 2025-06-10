// coordinator.js

// Handle the first snapshot: set up workers array + coordinator
function handleInitialSnapshot(payload) {
  currentCoordinatorId = payload.coordinatorId;
  workers = [];  // reset

  for (var i = 0; i < payload.workers.length; i++) {
    var wObj = payload.workers[i];
    workers.push({
      id: wObj.id,
      name: wObj.name,
      status: wObj.status,
      createdAt: new Date(wObj.createdAt),
      angle: 0
    });
  }

  recalcAngles();
  renderAllWorkers();
  populateCoordinatorDropdown();
  positionWorkers();
  drawConnections();
  highlightCoordinatorNode();
}

// When someone new becomes coordinator
function handleCoordinatorSwitched(data) {
  appendLog('Coordinator switched to: ' + data.newCoordinatorId, 'info');
  currentCoordinatorId = data.newCoordinatorId;
  populateCoordinatorDropdown();
  positionWorkers();
  drawConnections();
  highlightCoordinatorNode();
}

// Add a green border to the coordinator’s node
function highlightCoordinatorNode() {
  // first, reset everyone
  var circles = document.querySelectorAll('.worker-node .node-circle');
  for (var i = 0; i < circles.length; i++) {
    circles[i].style.borderColor = '#555';
  }
  var labels = document.querySelectorAll('.worker-node .node-label');
  for (var j = 0; j < labels.length; j++) {
    labels[j].textContent = labels[j].textContent.replace(' [Coordinator]', '');
  }

  // if no coord, show the center
  if (!currentCoordinatorId) {
    document.getElementById('center').style.display = 'block';
    return;
  }
  document.getElementById('center').style.display = 'none';

  // now highlight the chosen one
  var circleEl = document.querySelector('.worker-node[data-id="' + currentCoordinatorId + '"] .node-circle');
  var labelEl = document.querySelector('.worker-node[data-id="' + currentCoordinatorId + '"] .node-label');
  if (circleEl && labelEl) {
    circleEl.style.borderColor = '#28a745';
    labelEl.textContent = labelEl.textContent + ' [Coordinator]';
  }
}

// Fill the coordinator <select> with current workers
function populateCoordinatorDropdown() {
  var select = document.getElementById('coordinator-select');
  // clear old options
  while (select.firstChild) {
    select.removeChild(select.firstChild);
  }
  // add a "(none)" option
  var noneOpt = document.createElement('option');
  noneOpt.value = '';
  noneOpt.textContent = '(none)';
  select.appendChild(noneOpt);

  // loop and add each worker
  for (var i = 0; i < workers.length; i++) {
    var w = workers[i];
    var opt = document.createElement('option');
    opt.value = w.id;
    opt.textContent = w.name + ' (' + w.id.slice(0,6) + ')';
    if (w.id === currentCoordinatorId) {
      opt.selected = true;
    }
    select.appendChild(opt);
  }
}

// when user picks a new coordinator in the UI
document.getElementById('coordinator-select').addEventListener('change', function(e) {
  var newId = e.target.value || null;
  if (newId !== currentCoordinatorId) {
    socket.emit('request_coordinator_switch', { newCoordinatorId: newId });
  }
});
