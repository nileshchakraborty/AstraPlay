#!/usr/bin/env bash
# Install a per-user host proxy for AstraPlay's embedded Waydroid login relay.
set -euo pipefail

for command_name in ip waydroid socat systemctl; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Missing $command_name. Install it on the Linux host first." >&2
        exit 1
    fi
done

bin_dir="${HOME}/.local/bin"
unit_dir="${HOME}/.config/systemd/user"
mkdir -p "$bin_dir" "$unit_dir"
proxy_path="$bin_dir/waydroid-relay-proxy.sh"
unit_path="$unit_dir/waydroid-relay-proxy.service"
backup_suffix=".bak-$(date +%Y%m%d%H%M%S)"
[[ ! -f "$proxy_path" ]] || cp "$proxy_path" "${proxy_path}${backup_suffix}"
[[ ! -f "$unit_path" ]] || cp "$unit_path" "${unit_path}${backup_suffix}"

cat > "$proxy_path" <<'PROXY'
#!/usr/bin/env bash
set -euo pipefail

host_ip=$(ip -4 route get 1.1.1.1 | awk '{for (i = 1; i <= NF; i++) if ($i == "src") {print $(i + 1); exit}}')
container_ip=$(waydroid status | awk '/IP address:/ {print $3; exit}')
if [[ -z "$host_ip" || -z "$container_ip" ]]; then
    echo "Waiting for a host LAN address and a running Waydroid container" >&2
    exit 1
fi

waydroid prop set persist.astraplay.lan_ip "$host_ip"
exec socat TCP4-LISTEN:25525,fork,reuseaddr "TCP4:${container_ip}:25525"
PROXY
chmod 755 "$proxy_path"

cat > "$unit_path" <<'UNIT'
[Unit]
Description=AstraPlay Waydroid phone login relay

[Service]
Type=simple
ExecStart=%h/.local/bin/waydroid-relay-proxy.sh
Restart=always
RestartSec=10

[Install]
WantedBy=default.target
UNIT

systemctl --user daemon-reload
systemctl --user enable waydroid-relay-proxy.service
systemctl --user restart waydroid-relay-proxy.service
echo "Relay installed. Keep AstraPlay's Phone QR screen open, then test http://<host-LAN-IP>:25525/ from another device on the same Wi-Fi."
echo "If your LAN IP changes, run: systemctl --user restart waydroid-relay-proxy.service"
