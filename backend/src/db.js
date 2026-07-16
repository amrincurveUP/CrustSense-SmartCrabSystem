const Database = require('better-sqlite3');
const fs = require('fs');
const path = require('path');

function openDatabase(dbPath) {
  const dir = path.dirname(dbPath);
  if (!fs.existsSync(dir)) {
    fs.mkdirSync(dir, { recursive: true });
  }

  const db = new Database(dbPath);
  db.pragma('journal_mode = WAL');

  db.exec(`
    CREATE TABLE IF NOT EXISTS readings (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      device_id TEXT NOT NULL,
      payload TEXT NOT NULL,
      connected INTEGER,
      active INTEGER,
      recorded_at TEXT NOT NULL DEFAULT (datetime('now'))
    );

    CREATE INDEX IF NOT EXISTS idx_readings_device_time
      ON readings(device_id, recorded_at DESC);

    CREATE TABLE IF NOT EXISTS device_status (
      device_id TEXT PRIMARY KEY,
      status TEXT NOT NULL,
      updated_at TEXT NOT NULL DEFAULT (datetime('now'))
    );
  `);

  const insertReading = db.prepare(`
    INSERT INTO readings (device_id, payload, connected, active, recorded_at)
    VALUES (@device_id, @payload, @connected, @active, datetime('now'))
  `);

  const upsertStatus = db.prepare(`
    INSERT INTO device_status (device_id, status, updated_at)
    VALUES (@device_id, @status, datetime('now'))
    ON CONFLICT(device_id) DO UPDATE SET
      status = excluded.status,
      updated_at = excluded.updated_at
  `);

  const latestReading = db.prepare(`
    SELECT * FROM readings
    WHERE device_id = ?
    ORDER BY recorded_at DESC
    LIMIT 1
  `);

  const latestAll = db.prepare(`
    SELECT r.*
    FROM readings r
    INNER JOIN (
      SELECT device_id, MAX(id) AS max_id
      FROM readings
      GROUP BY device_id
    ) latest ON r.id = latest.max_id
    ORDER BY r.device_id
  `);

  const history = db.prepare(`
    SELECT id, device_id, connected, active, recorded_at
    FROM readings
    WHERE device_id = ?
    ORDER BY recorded_at DESC
    LIMIT ?
  `);

  const allStatus = db.prepare(`SELECT * FROM device_status ORDER BY device_id`);

  return {
    saveReading(deviceId, payload) {
      const connected = payload.connected ?? null;
      const active = payload.active ?? null;
      insertReading.run({
        device_id: deviceId,
        payload: JSON.stringify(payload),
        connected,
        active,
      });
    },
    saveStatus(deviceId, status) {
      upsertStatus.run({ device_id: deviceId, status });
    },
    getLatest(deviceId) {
      const row = latestReading.get(deviceId);
      if (!row) return null;
      return {
        device_id: row.device_id,
        connected: row.connected,
        active: row.active,
        recorded_at: row.recorded_at,
        data: JSON.parse(row.payload),
      };
    },
    getLatestAll() {
      return latestAll.all().map((row) => ({
        device_id: row.device_id,
        connected: row.connected,
        active: row.active,
        recorded_at: row.recorded_at,
        data: JSON.parse(row.payload),
      }));
    },
    getHistory(deviceId, limit = 100) {
      return history.all(deviceId, limit);
    },
    getAllStatus() {
      return allStatus.all();
    },
  };
}

module.exports = { openDatabase };
