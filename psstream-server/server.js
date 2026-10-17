const express = require('express');
const cors = require('cors');
const path = require('path');

const PORT = process.env.PORT || 25525;
const CODE_TTL_MS = 15 * 60 * 1000;
const CLEANUP_INTERVAL_MS = 60 * 1000;

function normalizeCode(value) {
  return value == null ? '' : String(value).trim().toUpperCase();
}

function createApp({ now = Date.now, logger = console } = {}) {
  const app = express();
  const codeStore = new Map();

  app.use(cors());
  app.use(express.json());

  function findActiveCode(code) {
    const entry = codeStore.get(code);
    if (!entry) return undefined;
    if (now() - entry.createdAt >= CODE_TTL_MS) {
      codeStore.delete(code);
      return undefined;
    }
    return entry;
  }

  function cleanupExpiredCodes() {
    const currentTime = now();
    for (const [code, entry] of codeStore.entries()) {
      if (currentTime - entry.createdAt >= CODE_TTL_MS) {
        codeStore.delete(code);
      }
    }
  }

  function handleCreateCode(req, res) {
    const code = normalizeCode(req.body && req.body.code);
    if (!code) {
      return res.status(400).json({ result: 'error', error: 'Missing code' });
    }

    codeStore.set(code, { npsso: null, createdAt: now() });
    logger.info('[psstream] Created login code');
    return res.json({ result: 'success', code });
  }

  function handleGetTokens(req, res) {
    const code = normalizeCode(req.body && req.body.code);
    if (!code) {
      return res.status(400).json({ result: 'error', error: 'Missing code' });
    }

    const entry = findActiveCode(code);
    if (!entry) {
      return res.status(404).json({ result: 'error', error: 'Code not found or expired' });
    }

    if (!entry.npsso) {
      return res.status(404).json({ result: 'error', error: 'Tokens not ready yet' });
    }

    const npsso = entry.npsso;
    codeStore.delete(code);
    logger.info('[psstream] Retrieved one-time login token');
    return res.json({ result: 'success', npsso });
  }

  function handleValidateCode(req, res) {
    const code = normalizeCode(req.body && req.body.code);
    if (!code) {
      return res.status(400).json({ result: 'error', error: 'Missing code' });
    }

    if (findActiveCode(code)) {
      return res.json({ result: 'valid', code });
    }
    return res.status(404).json({ result: 'invalid', error: 'Invalid or expired code' });
  }

  function handleConnect(req, res) {
    const code = normalizeCode(req.body && req.body.code);
    const npsso = req.body && req.body.npsso != null ? String(req.body.npsso).trim() : '';
    if (!code || !npsso) {
      return res.status(400).json({ result: 'error', error: 'Missing code or npsso token' });
    }

    const entry = findActiveCode(code);
    if (!entry) {
      return res.status(404).json({ result: 'error', error: 'Code not found or expired' });
    }

    entry.npsso = npsso;
    logger.info('[psstream] Connected login token');
    return res.json({ result: 'success' });
  }

  // Keep both route prefixes compatible with existing AstraPlay and Chiaki clients.
  for (const prefix of ['/psstream', '/astraplay']) {
    app.post(`${prefix}/create-code`, handleCreateCode);
    app.post(`${prefix}/get-tokens`, handleGetTokens);
    app.post(`${prefix}/validate-code`, handleValidateCode);
    app.post(`${prefix}/connect`, handleConnect);
  }

  app.get('/search.php', (req, res) => {
    const username = req.query.username;
    if (!username) {
      return res.json({ error: 'Username required' });
    }
    return res.json({
      username: String(username),
      error: 'Please use the direct PSN Login method on this device or via QR code.'
    });
  });

  app.use('/astraplay', express.static(path.join(__dirname, 'public')));
  app.use('/psstream', express.static(path.join(__dirname, 'public')));
  app.use('/', express.static(path.join(__dirname, 'public')));

  // Return the cleanup function for deterministic tests and graceful embedding.
  app.locals.cleanupExpiredCodes = cleanupExpiredCodes;
  return app;
}

function startServer({
  port = PORT,
  host = '0.0.0.0',
  now = Date.now,
  logger = console,
  scheduleInterval = setInterval,
  clearScheduledInterval = clearInterval
} = {}) {
  const app = createApp({ now, logger });
  const cleanupTimer = scheduleInterval(() => app.locals.cleanupExpiredCodes(), CLEANUP_INTERVAL_MS);
  if (cleanupTimer && typeof cleanupTimer.unref === 'function') cleanupTimer.unref();

  const server = app.listen(port, host, () => {
    logger.info(`AstraPlay PSN Relay Server running on port ${server.address().port}`);
    logger.info(`Local web UI available at: http://localhost:${server.address().port}/psstream/`);
  });
  server.once('close', () => clearScheduledInterval(cleanupTimer));
  return server;
}

module.exports = { CODE_TTL_MS, createApp, startServer };
