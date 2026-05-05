# Remote Deployment Scaffold

This directory contains the remote single-server deployment scaffold for the
`my_visual_CPU` checkout at:

```text
/srv/apps/my_visual_CPU/repo
```

The deployment target is a remote development and verification host, not a
static frontend-only site. Runtime assets are intentionally kept outside the
repository:

```text
/srv/apps/my_visual_CPU/runtime-assets/linux/Image
/srv/apps/my_visual_CPU/runtime-assets/linux/rootfs.ext4
/srv/apps/my_visual_CPU/runtime-assets/linux/linux.dtb
/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike
/srv/apps/my_visual_CPU/logs/
/srv/apps/my_visual_CPU/tmp/
```

Do not fabricate Linux `Image`, rootfs, DTB, or Spike. If an asset is missing,
the frontend and smoke scripts must keep the affected route fail-closed and
report the blocker.

## Files

- `env/mycpu-frontend.env.example`
  Environment example for the systemd service.
- `systemd/mycpu-frontend.service`
  Local-only Node debug server service. It binds to `127.0.0.1:4173`; nginx is
  the public entry point.
- `nginx/mycpu.conf`
  Reverse proxy for `/`, `/console`, `/docs`, `/api/*`, and `/ws`.
- `scripts/prepare_runtime_dirs.sh`
  Creates the remote runtime directories without creating fake assets.
- `scripts/remote_smoke.sh`
  Runs remote validation and reports asset-gated blockers.
- `operations.md`
  Host-specific runbook for daily checks, restart, certificate renewal, and
  rollback on this server.

## Remote Setup

Run these commands on the remote server:

```bash
cd /srv/apps/my_visual_CPU/repo
deploy/scripts/prepare_runtime_dirs.sh
cp deploy/env/mycpu-frontend.env.example deploy/env/mycpu-frontend.env
```

Edit `deploy/env/mycpu-frontend.env` only if the remote paths differ. The
default contract is:

```bash
MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image
MYCPU_LINUX_PROTO_RUNTIME_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image
SPIKE_PATH=/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike
MYCPU_RUNTIME_TMPDIR=/srv/apps/my_visual_CPU/tmp
```

The remote service can also enable the built-in minimal auth layer:

```bash
MYCPU_AUTH_ENABLED=1
MYCPU_AUTH_ADMIN_USERNAME=admin
MYCPU_AUTH_ADMIN_PASSWORD_HASH=<scrypt-hash>
MYCPU_AUTH_SESSION_LIMIT=3
MYCPU_AUDIT_LOG_PATH=/srv/apps/my_visual_CPU/logs/audit.log
```

This auth layer is designed for the current single-session debug server model:

- no public self-registration
- at most 3 concurrent authenticated browser sessions
- one active controller for mutating actions such as `load`, `run`, `reset`,
  `terminate`, and terminal input
- additional authenticated users can observe but cannot take control until the
  current controller releases it

On a live service, rotate the admin password hash in
`deploy/env/mycpu-frontend.env` after initial deployment and keep the generated
plaintext password out of the repository. Audit lines are written to
`/srv/apps/my_visual_CPU/logs/audit.log` when `MYCPU_AUDIT_LOG_PATH` is set.

Build the simulator before starting the frontend:

```bash
cd /srv/apps/my_visual_CPU/repo/myCPU
make
```

Install the service and nginx site as root:

```bash
sudo cp /srv/apps/my_visual_CPU/repo/deploy/systemd/mycpu-frontend.service /etc/systemd/system/mycpu-frontend.service
sudo systemctl daemon-reload
sudo systemctl enable --now mycpu-frontend.service

sudo cp /srv/apps/my_visual_CPU/repo/deploy/nginx/mycpu.conf /etc/nginx/sites-available/mycpu
sudo ln -sf /etc/nginx/sites-available/mycpu /etc/nginx/sites-enabled/mycpu
sudo nginx -t
sudo systemctl reload nginx
```

The debug CLI is not exposed directly. Browser requests enter through nginx,
then nginx proxies to the local Node debug server, and the Node server spawns
`mycpu --debug-cli` as a local child process.

## Verification

Run the remote smoke from the checkout:

```bash
cd /srv/apps/my_visual_CPU/repo
deploy/scripts/remote_smoke.sh
```

The smoke always runs checks that do not require real remote assets:

```bash
git diff --check
cd frontend && node --test
cd myCPU && make test-host-debug_cli_smoke
cd myCPU && make test-host-interactive_terminal_smoke
cd myCPU && make test-host-ai_accelerator_profile_smoke
cd myCPU && make test-host-spike_differential_smoke
```

If `/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike` is executable, the
smoke also runs:

```bash
cd myCPU && SPIKE_PATH=/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike make test-host-spike_differential
```

If the Linux `Image`, `rootfs.ext4`, and `linux.dtb` exist, the smoke checks the
frontend Linux console readiness endpoint. The current frontend implementation
uses repo-generated `myCPU/workloads/linux_proto/rootfs.ext4` and
`myCPU/workloads/linux_proto/mycpu_virt.dtb` for the console route after those
build artifacts are generated; the remote asset paths are still the deployment
contract for externally supplied runtime assets.
