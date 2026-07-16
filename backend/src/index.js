require('dotenv').config();

const cors = require('cors');
const express = require('express');
const { openDatabase } = require('./db');
const { startMqtt } = require('./mqtt');

const PORT = Number(process.env.PORT || 3000);
const MQTT_URL = process.env.MQTT_URL || 'mqtt://localhost:1883';
const MQTT_TOPIC = process.env.MQTT_TOPIC || 'smart-aquaculture/+/readings';
const MQTT_STATUS_TOPIC = process.env.MQTT_STATUS_TOPIC || 'smart-aquaculture/+/status';
const DB_PATH = process.env.DB_PATH || './data/aquaculture.db';

const db = openDatabase(DB_PATH);
const app = express();

app.use(cors({ origin: process.env.CORS_ORIGIN || '*' }));
app.use(express.json());

app.get('/health', (_req, res) => {
  res.json({ ok: true, service: 'smart-aquaculture-backend' });
});

app.get('/api/devices', (_req, res) => {
  res.json(db.getAllStatus());
});

app.get('/api/readings', (_req, res) => {
  const latest = db.getLatestAll();
  if (latest.length === 1) {
    return res.json(latest[0].data);
  }
  res.json({ devices: latest });
});

app.get('/api/readings/:deviceId', (req, res) => {
  const row = db.getLatest(req.params.deviceId);
  if (!row) return res.status(404).json({ error: 'No readings for device' });
  res.json(row.data);
});

app.get('/api/readings/:deviceId/history', (req, res) => {
  const limit = Math.min(Number(req.query.limit) || 100, 1000);
  res.json(db.getHistory(req.params.deviceId, limit));
});

startMqtt({
  url: MQTT_URL,
  readingsTopic: MQTT_TOPIC,
  statusTopic: MQTT_STATUS_TOPIC,
  db,
});

app.listen(PORT, () => {
  console.log(`[api] http://localhost:${PORT}`);
  console.log(`[api] GET /api/readings  — latest sensor data`);
  console.log(`[db]  ${DB_PATH}`);
});
