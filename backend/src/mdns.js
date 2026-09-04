'use strict';

const { spawn } = require('child_process');
const os = require('os');

const MDNS_HOST = process.env.MDNS_HOST || 'crabit.local';
const MDNS_NAME = process.env.MDNS_NAME || 'crabit';

function lanIPv4() {
  const ifaces = os.networkInterfaces();
  for (const name of Object.keys(ifaces)) {
    for (const info of ifaces[name] || []) {
      if (info.family === 'IPv4' && !info.internal) {
        return info.address;
      }
    }
  }
  return null;
}

/**
 * Advertise a stable Bonjour name so phones/ESP can use http://crabit.local:3000/
 * even when the Mac DHCP address changes.
 */
function startMdnsAdvertiser(port) {
  let child = null;
  let lastIp = null;

  function stop() {
    if (child && !child.killed) {
      child.kill('SIGTERM');
    }
    child = null;
  }

  function restartIfNeeded() {
    const ip = lanIPv4();
    if (!ip) {
      console.warn('[mdns] no LAN IPv4 yet — will retry');
      return;
    }
    if (child && ip === lastIp) {
      return;
    }
    stop();
    lastIp = ip;
    // dns-sd -P Name Type Domain Port Host IPAddr
    child = spawn(
      'dns-sd',
      ['-P', MDNS_NAME, '_http._tcp', 'local', String(port), MDNS_HOST, ip],
      { stdio: ['ignore', 'ignore', 'pipe'] }
    );
    child.on('error', (err) => {
      console.warn('[mdns] dns-sd failed:', err.message);
      child = null;
    });
    child.on('exit', (code) => {
      if (code && code !== 0) {
        console.warn(`[mdns] dns-sd exited code=${code}`);
      }
      child = null;
    });
    console.log(`[mdns] advertised http://${MDNS_HOST}:${port}/ → ${ip}`);
  }

  restartIfNeeded();
  const timer = setInterval(restartIfNeeded, 30000);
  if (timer.unref) timer.unref();

  process.on('exit', stop);
  process.on('SIGINT', () => {
    stop();
    process.exit(0);
  });
  process.on('SIGTERM', () => {
    stop();
    process.exit(0);
  });

  return { host: MDNS_HOST, getIp: lanIPv4 };
}

module.exports = { startMdnsAdvertiser, lanIPv4, MDNS_HOST };
