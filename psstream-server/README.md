# AstraPlay PSN Token Relay Server (fedora.local)

This directory contains a lightweight, self-hosted Node.js / Express clone of the AstraPlay login token relay service. It replaces the external `xbgamestream.com` relay server with your own local service running on `fedora.local`.

## Features
- **Zero Third-Party Relays:** PSN token authentication stays strictly on your local network (`fedora.local:8080`).
- **No Metrics or Donation Checks:** Clean, transparent, open-source relay with no external telemetry or payment pings.
- **Full Compatibility:** Supports all AstraPlay endpoints (`/psstream/create-code`, `/psstream/get-tokens`, `/astraplay/validate-code`, `/astraplay/connect`) and serves the clean mobile-friendly auth UI.

---

## Deployment on `fedora.local`

### Option 1: Run with Docker Compose (Recommended)
1. Copy the `psstream-server` folder to your Fedora machine (or clone the repo on `fedora.local`):
   ```bash
   scp -r psstream-server user@fedora.local:~/psstream-server
   ```
2. On `fedora.local`:
   ```bash
   cd ~/psstream-server
   docker compose up -d --build
   ```

### Option 2: Run directly with Node.js
On `fedora.local`:
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

The AstraPlay client is configured to default to `http://fedora.local:8080`.
You can also override the URL dynamically using the environment variable:
```bash
export ASTRAPLAY_RELAY_URL="http://fedora.local:8080"
```
