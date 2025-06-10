// connection.js

var socket = null;  // will hold our Socket.io client

//initialize the WebSocket connection and handle server messages
function initializeWebSocket() {
  // connect to server
  socket = io();

  // when we're connected, log our socket ID
  socket.on('connect', function() {
    appendLog('WebSocket connected: ' + socket.id, 'info');
  });

  // server sends initial state (workers + who’s coordinator)
  socket.on('initial_snapshot', function(payload) {
    handleInitialSnapshot(payload);
  });

  // new worker joined
  socket.on('worker_added', function(data) {
    handleWorkerAdded(data);
  });

  // a worker changed status
  socket.on('worker_updated', function(data) {
    handleWorkerUpdated(data);
  });

  // a worker left or got disconnected
  socket.on('worker_removed', function(data) {
    handleWorkerRemoved(data);
  });

  // coordinator has been switched
  socket.on('coordinator_switched', function(data) {
    handleCoordinatorSwitched(data);
  });

  // backend log line
  socket.on('log_update', function(text) {
    handleLogUpdate(text);
  });

  // we finished sorting
  socket.on('sort_complete', function(payload) {
    handleSortComplete(payload);
  });

  // sort error happened
  socket.on('sort_error', function(payload) {
    handleSortError(payload);
  });

  // disconnected from server
  socket.on('disconnect', function() {
    appendLog('WebSocket disconnected', 'error');
  });
}
