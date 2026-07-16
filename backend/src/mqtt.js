const mqtt = require('mqtt');

function deviceIdFromTopic(topic, prefix) {
  // smart-aquaculture/{device_id}/readings
  const parts = topic.split('/');
  if (parts.length >= 2 && parts[0] === prefix.split('/')[0]) {
    return parts[1];
  }
  return 'unknown';
}

function startMqtt({ url, readingsTopic, statusTopic, db, onReading }) {
  const client = mqtt.connect(url);

  client.on('connect', () => {
    console.log(`[mqtt] connected → ${url}`);
    client.subscribe([readingsTopic, statusTopic], (err) => {
      if (err) console.error('[mqtt] subscribe error', err);
      else console.log(`[mqtt] subscribed: ${readingsTopic}, ${statusTopic}`);
    });
  });

  client.on('message', (topic, buf) => {
    const text = buf.toString();
    const base = readingsTopic.replace('/+/readings', '');

    if (topic.endsWith('/readings')) {
      const deviceId = deviceIdFromTopic(topic, readingsTopic);
      try {
        const payload = JSON.parse(text);
        db.saveReading(deviceId, payload);
        if (onReading) onReading(deviceId, payload);
        console.log(`[mqtt] reading ${deviceId} connected=${payload.connected} active=${payload.active}`);
      } catch (e) {
        console.error('[mqtt] invalid JSON from', topic, e.message);
      }
      return;
    }

    if (topic.endsWith('/status')) {
      const deviceId = deviceIdFromTopic(topic, statusTopic);
      db.saveStatus(deviceId, text);
      console.log(`[mqtt] status ${deviceId} → ${text}`);
    }
  });

  client.on('error', (err) => console.error('[mqtt] error', err.message));

  return client;
}

module.exports = { startMqtt };
