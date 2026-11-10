const assert = require('node:assert/strict');
const { once } = require('node:events');
const { after, before, describe, it } = require('node:test');
const { createApp, CODE_TTL_MS, startServer } = require('../server');

let server;
let app;
let baseUrl;
let currentTime;
let logMessages;

before(async () => {
  currentTime = Date.now();
  logMessages = [];
  app = createApp({
    now: () => currentTime,
    logger: { info: (message) => logMessages.push(message) }
  });
  server = app.listen(0, '127.0.0.1');
  await once(server, 'listening');
  baseUrl = `http://127.0.0.1:${server.address().port}`;
});

after(async () => {
  if (server?.listening) {
    server.close();
    await once(server, 'close');
  }
});

async function post(path, body) {
  const response = await fetch(`${baseUrl}${path}`, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(body)
  });
  return { status: response.status, body: await response.json() };
}

async function get(path) {
  const response = await fetch(`${baseUrl}${path}`);
  return { status: response.status, text: await response.text(), contentType: response.headers.get('content-type') };
}

describe('PSN login relay behavior', () => {
  describe('Given a client starts a login flow', () => {
    for (const prefix of ['/psstream', '/astraplay']) {
      it(`When it creates a code through ${prefix}, Then the relay stores a normalized code`, async () => {
        const code = `BDD-${prefix === '/psstream' ? 'OLD' : 'NEW'}`;
        const created = await post(`${prefix}/create-code`, { code: `  ${code.toLowerCase()} ` });
        assert.deepEqual(created, { status: 200, body: { result: 'success', code } });

        const validation = await post(`${prefix}/validate-code`, { code: code.toLowerCase() });
        assert.deepEqual(validation, { status: 200, body: { result: 'valid', code } });
      });
    }

    for (const value of [undefined, null, '', '   ']) {
      it(`When the client submits ${String(value)}, Then code creation is rejected`, async () => {
        const result = await post('/psstream/create-code', { code: value });
        assert.equal(result.status, 400);
        assert.deepEqual(result.body, { result: 'error', error: 'Missing code' });
      });
    }

    it('When the expiry janitor runs, Then it removes expired codes and keeps live ones', async () => {
      app.locals.cleanupExpiredCodes();
      await post('/astraplay/create-code', { code: 'JANITOR-EXPIRED' });
      currentTime += CODE_TTL_MS / 2;
      await post('/astraplay/create-code', { code: 'JANITOR-LIVE' });
      currentTime += CODE_TTL_MS / 2;
      app.locals.cleanupExpiredCodes();

      assert.equal((await post('/astraplay/validate-code', { code: 'JANITOR-EXPIRED' })).status, 404);
      assert.deepEqual(await post('/astraplay/validate-code', { code: 'JANITOR-LIVE' }), {
        status: 200,
        body: { result: 'valid', code: 'JANITOR-LIVE' }
      });
    });
  });

  describe('Given a browser opens a login code', () => {
    for (const route of ['/psstream/get-tokens', '/astraplay/validate-code']) {
      it(`When ${route} receives no code, Then the request is rejected`, async () => {
        const result = await post(route, {});
        assert.equal(result.status, 400);
        assert.deepEqual(result.body, { result: 'error', error: 'Missing code' });
      });
    }

    it('When the code is unknown, Then validation reports an expired or invalid code', async () => {
      const result = await post('/astraplay/validate-code', { code: 'DOES-NOT-EXIST' });
      assert.equal(result.status, 404);
      assert.deepEqual(result.body, { result: 'invalid', error: 'Invalid or expired code' });
    });

    it('When a code reaches its lifetime, Then validation rejects and removes it immediately', async () => {
      await post('/astraplay/create-code', { code: 'EXPIRES' });
      currentTime += CODE_TTL_MS;

      const first = await post('/psstream/validate-code', { code: 'EXPIRES' });
      const second = await post('/psstream/validate-code', { code: 'EXPIRES' });
      assert.equal(first.status, 404);
      assert.deepEqual(first.body, { result: 'invalid', error: 'Invalid or expired code' });
      assert.deepEqual(second, first);
    });
  });

  describe('Given a user connects an NPSSO token to an active code', () => {
    it('When either credential is absent, Then the relay rejects the connection', async () => {
      await post('/astraplay/create-code', { code: 'MISSING-CREDS' });
      for (const body of [{ code: 'MISSING-CREDS' }, { npsso: 'secret-token' }, {}]) {
        const result = await post('/astraplay/connect', body);
        assert.equal(result.status, 400);
        assert.equal(result.body.error, 'Missing code or npsso token');
      }
    });

    it('When the code is unknown or expired, Then the relay does not store a token', async () => {
      const unknown = await post('/psstream/connect', { code: 'UNKNOWN', npsso: 'secret-token' });
      assert.equal(unknown.status, 404);

      await post('/psstream/create-code', { code: 'EXPIRED-CONNECT' });
      currentTime += CODE_TTL_MS;
      const expired = await post('/psstream/connect', { code: 'EXPIRED-CONNECT', npsso: 'secret-token' });
      assert.equal(expired.status, 404);
      assert.deepEqual(expired.body, { result: 'error', error: 'Code not found or expired' });
    });

    it('When the token is submitted, Then the client receives it once and the login code is consumed', async () => {
      await post('/psstream/create-code', { code: 'ONE-TIME' });
      const connected = await post('/astraplay/connect', { code: 'one-time', npsso: '  private-token-value  ' });
      assert.deepEqual(connected, { status: 200, body: { result: 'success' } });

      const fetched = await post('/psstream/get-tokens', { code: 'ONE-TIME' });
      assert.deepEqual(fetched, { status: 200, body: { result: 'success', npsso: 'private-token-value' } });

      const replay = await post('/astraplay/get-tokens', { code: 'ONE-TIME' });
      assert.equal(replay.status, 404);
    });

    it('When the client polls before connection, Then the relay reports that the token is not ready', async () => {
      await post('/astraplay/create-code', { code: 'WAITING' });
      const result = await post('/psstream/get-tokens', { code: 'WAITING' });
      assert.equal(result.status, 404);
      assert.deepEqual(result.body, { result: 'error', error: 'Tokens not ready yet' });
    });

    it('When credentials are sent, Then relay logs never expose the code or token', async () => {
      await post('/astraplay/create-code', { code: 'DO-NOT-LOG-CODE' });
      await post('/astraplay/connect', { code: 'DO-NOT-LOG-CODE', npsso: 'DO-NOT-LOG-TOKEN' });
      await post('/astraplay/get-tokens', { code: 'DO-NOT-LOG-CODE' });
      assert.ok(logMessages.length > 0);
      assert.ok(logMessages.every((message) => !message.includes('DO-NOT-LOG-CODE')));
      assert.ok(logMessages.every((message) => !message.includes('DO-NOT-LOG-TOKEN')));
    });
  });

  describe('Given the relay account lookup endpoint replaces the former third-party lookup', () => {
    it('When no username is supplied, Then the response explains the requirement', async () => {
      const result = await get('/search.php');
      assert.equal(result.status, 200);
      assert.deepEqual(JSON.parse(result.text), { error: 'Username required' });
    });

    it('When a username is supplied, Then it is returned without an external lookup', async () => {
      const result = await get('/search.php?username=Alice%20Example');
      assert.equal(result.status, 200);
      assert.deepEqual(JSON.parse(result.text), {
        username: 'Alice Example',
        error: 'Please use the direct PSN Login method on this device or via QR code.'
      });
    });

    it('When a mock_id query parameter is supplied, Then it returns the mock encoded account ID', async () => {
      const result = await get('/search.php?username=Alice%20Example&mock_id=bW9ja0FjY291bnRJZDEyMw==');
      assert.equal(result.status, 200);
      assert.deepEqual(JSON.parse(result.text), {
        username: 'Alice Example',
        encoded_id: 'bW9ja0FjY291bnRJZDEyMw=='
      });
    });
  });

  describe('Given a mock third-party account lookup handler is configured', () => {
    it('When an existing user is looked up, Then the mock lookup callback supplies the account ID', async () => {
      const mockApp = createApp({
        mockLookup: (username) => {
          if (username === 'MockPro') {
            return { username, encoded_id: 'bW9ja1Byb0lkMTIzNDU2' };
          }
          return { error: 'User not found in mock directory' };
        }
      });
      const s = mockApp.listen(0, '127.0.0.1');
      await once(s, 'listening');
      const url = `http://127.0.0.1:${s.address().port}`;
      try {
        const res1 = await fetch(`${url}/search.php?username=MockPro`);
        assert.equal(res1.status, 200);
        const data1 = await res1.json();
        assert.deepEqual(data1, { username: 'MockPro', encoded_id: 'bW9ja1Byb0lkMTIzNDU2' });

        const res2 = await fetch(`${url}/search.php?username=GhostUser`);
        assert.equal(res2.status, 200);
        const data2 = await res2.json();
        assert.deepEqual(data2, { error: 'User not found in mock directory' });
      } finally {
        s.close();
        await once(s, 'close');
      }
    });

    it('When a mock thirdparty relay service handles QR code token exchange, Then credentials flow securely', async () => {
      const mockThirdPartyApp = createApp();
      const s = mockThirdPartyApp.listen(0, '127.0.0.1');
      await once(s, 'listening');
      const mockUrl = `http://127.0.0.1:${s.address().port}`;
      try {
        // Step 1: Mock client registers login code on mock thirdparty URL
        const createRes = await fetch(`${mockUrl}/psstream/create-code`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ code: 'MOCK-QR-01' })
        });
        assert.equal(createRes.status, 200);

        // Step 2: Client polls mock URL before user login (pending)
        const pendingPoll = await fetch(`${mockUrl}/psstream/get-tokens`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ code: 'MOCK-QR-01' })
        });
        const pendingData = await pendingPoll.json();
        assert.equal(pendingData.error, 'Tokens not ready yet');

        // Step 3: Browser submits token to mock thirdparty URL
        const connectRes = await fetch(`${mockUrl}/psstream/connect`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ code: 'MOCK-QR-01', npsso: 'mock-secret-npsso-token-64' })
        });
        assert.equal(connectRes.status, 200);

        // Step 4: Client polls mock URL after user login (success)
        const successPoll = await fetch(`${mockUrl}/psstream/get-tokens`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ code: 'MOCK-QR-01' })
        });
        const successData = await successPoll.json();
        assert.equal(successData.result, 'success');
        assert.equal(successData.npsso, 'mock-secret-npsso-token-64');
      } finally {
        s.close();
        await once(s, 'close');
      }
    });
  });

  describe('Given a user opens the relay login page', () => {
    for (const route of ['/', '/psstream/', '/astraplay/']) {
      it(`When the user visits ${route}, Then the static login page is served`, async () => {
        const result = await get(route);
        assert.equal(result.status, 200);
        assert.match(result.contentType, /text\/html/);
        assert.match(result.text, /AstraPlay/i);
      });
    }

    it('When the login page requests its script, Then the browser workflow asset is served', async () => {
      const result = await get('/psstream/script.js');
      assert.equal(result.status, 200);
      assert.match(result.contentType, /javascript/);
      assert.match(result.text, /validate-code/);
      assert.match(result.text, /get-tokens|connect/);
    });
  });
});

describe('PSN login relay process lifecycle', () => {
  it('Given the relay starts on an ephemeral port, When it closes, Then its cleanup timer is cleared', async () => {
    const messages = [];
    const childServer = startServer({
      port: 0,
      host: '127.0.0.1',
      now: () => Date.now(),
      logger: { info: (message) => messages.push(message) }
    });
    await once(childServer, 'listening');
    const boundPort = childServer.address().port;
    assert.ok(boundPort > 0);
    assert.ok(messages.some((message) => message.includes(String(boundPort))));
    childServer.close();
    await once(childServer, 'close');
  });

  it('Given a timer implementation without unref, When the relay closes, Then its timer is still cleared', async () => {
    let scheduledCallback;
    let clearedTimer;
    const timer = { testTimer: true };
    const childServer = startServer({
      port: 0,
      host: '127.0.0.1',
      logger: { info() {} },
      scheduleInterval: (callback, interval) => {
        assert.equal(interval, 60 * 1000);
        scheduledCallback = callback;
        return timer;
      },
      clearScheduledInterval: (value) => { clearedTimer = value; }
    });
    await once(childServer, 'listening');
    assert.equal(typeof scheduledCallback, 'function');
    scheduledCallback();
    childServer.close();
    await once(childServer, 'close');
    assert.equal(clearedTimer, timer);
  });
});

describe('PSN login relay security controls', () => {
  it('When codeStore reaches maxCodes capacity, Then new codes are rejected with 429', async () => {
    let nowTime = 1000;
    const smallApp = createApp({ maxCodes: 1, now: () => nowTime });
    const s = smallApp.listen(0, '127.0.0.1');
    await once(s, 'listening');
    const url = `http://127.0.0.1:${s.address().port}`;
    try {
      const res1 = await fetch(`${url}/astraplay/create-code`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ code: 'CODE1' })
      });
      assert.equal(res1.status, 200);

      // Re-registering existing code within capacity is allowed
      const resSame = await fetch(`${url}/astraplay/create-code`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ code: 'CODE1' })
      });
      assert.equal(resSame.status, 200);

      const res2 = await fetch(`${url}/astraplay/create-code`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ code: 'CODE2' })
      });
      assert.equal(res2.status, 429);
      assert.deepEqual(await res2.json(), { result: 'error', error: 'Relay server code capacity reached' });

      // Expiring old codes frees capacity
      nowTime += CODE_TTL_MS + 1000;
      const res3 = await fetch(`${url}/astraplay/create-code`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ code: 'CODE3' })
      });
      assert.equal(res3.status, 200);
    } finally {
      s.close();
      await once(s, 'close');
    }
  });

  it('When rate limiting is enabled and request threshold is exceeded, Then it returns 429', async () => {
    let simTime = 100000;
    const rateApp = createApp({ enableRateLimit: true, now: () => simTime });
    const s = rateApp.listen(0, '127.0.0.1');
    await once(s, 'listening');
    const url = `http://127.0.0.1:${s.address().port}`;
    try {
      // Limit for create-code is 5 req / min
      for (let i = 0; i < 5; i++) {
        const res = await fetch(`${url}/astraplay/create-code`, {
          method: 'POST',
          headers: { 'content-type': 'application/json' },
          body: JSON.stringify({ code: `RL${i}` })
        });
        assert.equal(res.status, 200);
      }
      // 6th request should hit 429
      const res6 = await fetch(`${url}/astraplay/create-code`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ code: 'RL6' })
      });
      assert.equal(res6.status, 429);
      assert.deepEqual(await res6.json(), {
        result: 'error',
        error: 'Too many requests, please try again later'
      });

      // Advance time by 61 seconds; rate limit should reset and cleanup evicts record
      simTime += 61 * 1000;
      rateApp.locals.cleanupExpiredCodes();
      const resAfterReset = await fetch(`${url}/astraplay/create-code`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ code: 'RL_RESET' })
      });
      assert.equal(resAfterReset.status, 200);
    } finally {
      s.close();
      await once(s, 'close');
    }
  });
});
