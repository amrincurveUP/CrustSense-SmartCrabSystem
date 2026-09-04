require('dotenv').config();

const path = require('path');
const cors = require('cors');
const express = require('express');
const { openDatabase } = require('./db');
const { startMqtt } = require('./mqtt');
const { startMdnsAdvertiser, lanIPv4, MDNS_HOST } = require('./mdns');

const PORT = Number(process.env.PORT || 3000);
const MQTT_URL = process.env.MQTT_URL || 'mqtt://localhost:1883';
const MQTT_TOPIC = process.env.MQTT_TOPIC || 'smart-aquaculture/+/readings';
const MQTT_STATUS_TOPIC = process.env.MQTT_STATUS_TOPIC || 'smart-aquaculture/+/status';
const DB_PATH = process.env.DB_PATH || './data/aquaculture.db';
const DASHBOARD_DIR = path.join(__dirname, '../../dashboard');
const MQTT_OPTIONAL = process.env.MQTT_OPTIONAL !== '0';

const db = openDatabase(DB_PATH);
const app = express();

app.use(cors({ origin: process.env.CORS_ORIGIN || '*' }));
app.use(express.json({ limit: '256kb' }));
app.use(express.static(DASHBOARD_DIR));

app.get('/health', (_req, res) => {
  const ip = lanIPv4();
  res.json({
    ok: true,
    service: "crab'IT-backend",
    urls: {
      local: `http://127.0.0.1:${PORT}/`,
      mdns: `http://${MDNS_HOST}:${PORT}/`,
      lan: ip ? `http://${ip}:${PORT}/` : null,
    },
  });
});

app.get('/api/devices', (_req, res) => {
  res.json(db.getAllStatus());
});

app.get('/api/readings', (_req, res) => {
  const latest = db.getLatestAll();
  if (latest.length === 1) {
    const row = latest[0];
    return res.json({
      ...row.data,
      device_id: row.device_id,
      recorded_at: row.recorded_at,
    });
  }
  res.json({ devices: latest });
});

app.get('/api/readings/:deviceId', (req, res) => {
  const row = db.getLatest(req.params.deviceId);
  if (!row) return res.status(404).json({ error: 'No readings for device' });
  res.json({
    ...row.data,
    device_id: row.device_id,
    recorded_at: row.recorded_at,
  });
});

app.get('/api/readings/:deviceId/history', (req, res) => {
  const limit = Math.min(Number(req.query.limit) || 100, 1000);
  res.json(db.getHistory(req.params.deviceId, limit));
});

/* ESP32 HTTP ingest — works when MQTT port is firewalled */
app.post('/api/ingest', (req, res) => {
  const deviceId =
    req.header('x-device-id') ||
    req.body?.device_id ||
    'unknown';
  const payload = req.body?.data && typeof req.body.data === 'object'
    ? req.body.data
    : req.body;

  if (!payload || typeof payload !== 'object') {
    return res.status(400).json({ error: 'JSON body required' });
  }

  try {
    db.saveReading(deviceId, payload);
    db.saveStatus(deviceId, 'online');
    console.log(
      `[http] ingest ${deviceId} connected=${payload.connected} active=${payload.active}`
    );
    res.json({ ok: true, device_id: deviceId });
  } catch (e) {
    console.error('[http] ingest error', e.message);
    res.status(500).json({ error: e.message });
  }
});

app.get('/', (_req, res) => {
  res.sendFile(path.join(DASHBOARD_DIR, 'index.html'));
});

/* ---------- Irrigation (dashboard → ESP polls) ---------- */
const irrigationState = {
  pump: false,
  relay_test: false,
  max_run_ms: 120000,
  updated_at: null,
  updated_by: 'boot',
  device: {
    running: false,
    run_ms: 0,
    device_id: null,
    seen_at: null,
  },
};

app.get('/api/irrigation', (_req, res) => {
  res.json({
    command: {
      pump: irrigationState.pump,
      relay_test: irrigationState.relay_test,
      max_run_ms: irrigationState.max_run_ms,
      updated_at: irrigationState.updated_at,
      updated_by: irrigationState.updated_by,
    },
    device: irrigationState.device,
  });
});

/* ESP polls this every ~2s */
app.get('/api/irrigation/command', (_req, res) => {
  res.json({
    pump: irrigationState.pump,
    relay_test: irrigationState.relay_test,
    max_run_ms: irrigationState.max_run_ms,
  });
});

app.post('/api/irrigation/command', (req, res) => {
  const body = req.body || {};

  if (body.relay_test === true || body.relay_test === '1') {
    irrigationState.relay_test = true;
    irrigationState.pump = false;
  } else if (typeof body.relay_test === 'boolean') {
    irrigationState.relay_test = body.relay_test;
  }

  if (typeof body.pump === 'boolean') {
    irrigationState.pump = body.pump;
    if (body.pump) irrigationState.relay_test = false;
  } else if (body.pump === 'on' || body.pump === 1 || body.pump === '1') {
    irrigationState.pump = true;
    irrigationState.relay_test = false;
  } else if (body.pump === 'off' || body.pump === 0 || body.pump === '0') {
    irrigationState.pump = false;
  } else if (body.relay_test !== true && body.relay_test !== false && body.relay_test !== '1') {
    return res.status(400).json({ error: 'pump boolean or relay_test required' });
  }

  /* Default safety window 2 minutes unless client overrides */
  if (body.max_run_ms != null) {
    const n = Number(body.max_run_ms);
    if (Number.isFinite(n) && n >= 1000 && n <= 600000) {
      irrigationState.max_run_ms = Math.floor(n);
    }
  } else if (irrigationState.pump === true && irrigationState.max_run_ms < 60000) {
    irrigationState.max_run_ms = 120000;
  }

  irrigationState.updated_at = new Date().toISOString();
  irrigationState.updated_by = req.header('x-client') || 'dashboard';
  console.log(
    `[irrigation] pump=${irrigationState.pump} relay_test=${irrigationState.relay_test} max_run_ms=${irrigationState.max_run_ms}`
  );
  res.json({
    ok: true,
    pump: irrigationState.pump,
    relay_test: irrigationState.relay_test,
    max_run_ms: irrigationState.max_run_ms,
    updated_at: irrigationState.updated_at,
  });
});

/* ESP posts actual relay state here */
app.post('/api/irrigation/status', (req, res) => {
  const body = req.body || {};
  irrigationState.device = {
    running: Boolean(body.running),
    run_ms: Number(body.run_ms) || 0,
    device_id: body.device_id || req.header('x-device-id') || null,
    seen_at: new Date().toISOString(),
  };
  if (body.test_done === true) {
    irrigationState.relay_test = false;
  }
  res.json({ ok: true });
});

if (MQTT_OPTIONAL) {
  try {
    startMqtt({
      url: MQTT_URL,
      readingsTopic: MQTT_TOPIC,
      statusTopic: MQTT_STATUS_TOPIC,
      db,
    });
  } catch (e) {
    console.warn('[mqtt] start skipped:', e.message);
  }
}

app.listen(PORT, '0.0.0.0', () => {
  const ip = lanIPv4();
  startMdnsAdvertiser(PORT);
  console.log(`[api]       http://0.0.0.0:${PORT}`);
  console.log(`[dashboard] http://127.0.0.1:${PORT}/`);
  console.log(`[stable]    http://${MDNS_HOST}:${PORT}/  ← prefer this URL`);
  if (ip) console.log(`[lan]       http://${ip}:${PORT}/`);
  console.log(`[ingest]    POST /api/ingest`);
  console.log(`[irrigation] GET/POST /api/irrigation/command`);
  console.log(`[db]  ${DB_PATH}`);
});
