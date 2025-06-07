// worker-client.js

const axios = require('axios');
const { v4: uuidv4 } = require('uuid');

// Backend URL aus Umgebungsvariablen holen (z.B. Docker -e)
const BACKEND_URL = process.env.BACKEND_URL || 'http://backend:3000';
const WORKER_NAME = process.env.WORKER_NAME || 'Remote Worker';

// Eindeutige Worker ID generieren
const workerId = uuidv4();

// Registrierung beim Backend
async function register() {
  try {
    console.log(`Registering worker ${workerId} (${WORKER_NAME}) at ${BACKEND_URL}`);
    const res = await axios.post(`${BACKEND_URL}/register`, {
      id: workerId,
      name: WORKER_NAME,
      status: 'connected'
    });
    console.log('✅ Registered successfully:', res.data);
  } catch (err) {
    console.error('❌ Failed to register:', err.message);
    process.exit(1);
  }
}

// Heartbeat: alle 3 Sekunden "ich bin noch da" senden
async function heartbeat() {
  try {
    await axios.post(`${BACKEND_URL}/register`, {
      id: workerId,
      name: WORKER_NAME,
      status: 'connected'
    });
    console.log('✅ Heartbeat sent');
  } catch (err) {
    console.error('❌ Heartbeat failed:', err.message);
  }
}

(async () => {
  await register();
  setInterval(heartbeat, 3000);
})();
