# AstraPlay PSN Token Relay Server

This directory contains a lightweight, self-hosted Node.js / Express implementation
of the AstraPlay login token relay service. Run it on a machine on your own network
(any host you control — the examples below use `relay.example.com` as a placeholder;
never commit a personal LAN hostname to the repo).

## Features
- **Zero Third-Party Relays:** PSN token authentication stays on your own network.
- **No Metrics or Donation Checks:** Clean, transparent, open-source relay with no external telemetry or payment pings.
- **Full Compatibility:** Supports all AstraPlay endpoints (`/psstream/create-code`, `/psstream/get-tokens`, `/astraplay/validate-code`, `/astraplay/connect`) and serves the clean mobile-friendly auth UI.

---

## Deployment on your own server

### Option 1: Run with Docker Compose (Recommended)
1. Copy the `psstream-server` folder to your server (or clone the repo there):
   ```bash
   scp -r psstream-server user@relay.example.com:~/psstream-server
   ```
2. On your server:
   ```bash
   cd ~/psstream-server
   docker compose up -d --build
   ```

### Option 2: Run directly with Node.js
On your server:
```bash
cd ~/psstream-server
npm install
node server.js
```

### Option 3: Run as a Systemd Service
Create `/etc/systemd/system/astraplay-relay.service`:
```ini
[Unit]
Description=AstraPlay PSN Token Relay Server
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/opt/psstream-server
ExecStart=/usr/bin/node /opt/psstream-server/server.js
Restart=always
Environment=PORT=8080

[Install]
WantedBy=multi-user.target
```
Enable and start:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now astraplay-relay
```

---

## Client Configuration

Each AstraPlay client must be told where your relay runs. Replace
`https://relay.example.com:8080` with your own host in every case, and keep
personal hostnames out of version control.

- **Desktop (Qt):** the app uses its embedded relay automatically. To point it
  at a remote relay instead:
  ```bash
  export ASTRAPLAY_RELAY_URL="https://relay.example.com:8080"
  ```
- **Android:** bake the URL in at build time (it cannot be changed at runtime):
  ```bash
  ./gradlew :app:assembleRelease -PAstraPlayRelayUrl=https://relay.example.com:8080
  ```
  Without this flag the QR/TV login flow reports that no relay is configured.
  See `android/BUILD_FOR_GOOGLE_PLAY.md`.
- **iOS:** set `ASTRAPLAY_RELAY_URL` in your Xcode scheme's environment (or CI
  environment). Without it, relay login calls fail gracefully and the UI
  reports that login could not start.
