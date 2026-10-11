const express = require('express');
const cors = require('cors');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 8080;

app.use(cors());
app.use(express.json());

// In-memory store for active relay codes:
// code -> { npsso: string | null, createdAt: number }
const codeStore = new Map();

// Expire codes after 15 minutes
const CODE_TTL_MS = 15 * 60 * 1000;

function cleanupExpiredCodes() {
  const now = Date.now();
  for (const [code, entry] of codeStore.entries()) {
    if (now - entry.createdAt > CODE_TTL_MS) {
      codeStore.delete(code);
    }
  }
}
setInterval(cleanupExpiredCodes, 60 * 1000);

// --- 1. Client App / Console endpoints ---

// Endpoint: create a new login code from the client (Desktop / Android / iOS)
function handleCreateCode(req, res) {
  const code = (req.body && req.body.code ? String(req.body.code).trim().toUpperCase() : '');
  if (!code) {
    return res.status(400).json({ result: 'error', error: 'Missing code' });
  }

  codeStore.set(code, {
    npsso: null,
    createdAt: Date.now()
  });

  console.log(`[psstream] Created code: ${code}`);
  return res.json({ result: 'success', code });
}

// Endpoint: poll for NPSSO token from the client
function handleGetTokens(req, res) {
  const code = (req.body && req.body.code ? String(req.body.code).trim().toUpperCase() : '');
  if (!code) {
    return res.status(400).json({ result: 'error', error: 'Missing code' });
  }

  const entry = codeStore.get(code);
  if (!entry) {
    return res.status(404).json({ result: 'error', error: 'Code not found or expired' });
  }

  if (!entry.npsso) {
    // Waiting for user to complete login in browser
    return res.status(404).json({ result: 'error', error: 'Tokens not ready yet' });
  }

  // Token is ready! Return it and delete code so it's one-time use
  const npsso = entry.npsso;
  codeStore.delete(code);
  console.log(`[psstream] Retrieved NPSSO token for code: ${code}`);
  return res.json({ result: 'success', npsso });
}

// Support both /psstream/* and /astraplay/* paths
app.post('/psstream/create-code', handleCreateCode);
app.post('/astraplay/create-code', handleCreateCode);

app.post('/psstream/get-tokens', handleGetTokens);
app.post('/astraplay/get-tokens', handleGetTokens);

// --- 2. Web UI endpoints (called by web browser on fedora.local) ---

function handleValidateCode(req, res) {
  const code = (req.body && req.body.code ? String(req.body.code).trim().toUpperCase() : '');
  if (!code) {
    return res.status(400).json({ result: 'error', error: 'Missing code' });
  }

  const entry = codeStore.get(code);
  if (entry) {
    return res.json({ result: 'valid', code });
  } else {
    return res.status(404).json({ result: 'invalid', error: 'Invalid or expired code' });
  }
}

function handleConnect(req, res) {
  const code = (req.body && req.body.code ? String(req.body.code).trim().toUpperCase() : '');
  const npsso = (req.body && req.body.npsso ? String(req.body.npsso).trim() : '');

  if (!code || !npsso) {
    return res.status(400).json({ result: 'error', error: 'Missing code or npsso token' });
  }

  const entry = codeStore.get(code);
  if (!entry) {
    return res.status(404).json({ result: 'error', error: 'Code not found or expired' });
  }

  entry.npsso = npsso;
  console.log(`[psstream] Connected NPSSO token for code: ${code}`);
  return res.json({ result: 'success' });
}

app.post('/astraplay/validate-code', handleValidateCode);
app.post('/psstream/validate-code', handleValidateCode);

app.post('/astraplay/connect', handleConnect);
app.post('/psstream/connect', handleConnect);

// --- 3. Local Account ID lookup endpoint (/search.php) ---
// Replaces third-party psn.flipscreen.games
app.get('/search.php', (req, res) => {
  const username = req.query.username;
  if (!username) {
    return res.json({ error: 'Username required' });
  }
  // If an encoded account ID is already given or needs formatting:
  return res.json({
    username: String(username),
    error: 'Please use the direct PSN Login method on this device or via QR code.'
  });
});

// --- 5. Web UI Static Files ---
app.use('/astraplay', express.static(path.join(__dirname, 'public')));
app.use('/psstream', express.static(path.join(__dirname, 'public')));
app.use('/', express.static(path.join(__dirname, 'public')));

// Serve the web UI for /psstream/?psstream_code=...
app.get('/psstream', (req, res) => {
  res.sendFile(path.join(__dirname, 'public', 'index.html'));
});

app.listen(PORT, '0.0.0.0', () => {
  console.log(`AstraPlay PSN Relay Server running on port ${PORT}`);
  console.log(`Local web UI available at: http://localhost:${PORT}/psstream/`);
});
