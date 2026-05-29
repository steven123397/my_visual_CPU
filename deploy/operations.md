# Remote Operations Runbook

This runbook is for the live remote deployment on this host.

Runtime root:

```text
/srv/apps/my_visual_CPU
```

Repo checkout:

```text
/srv/apps/my_visual_CPU/repo
```

Current public domain:

```text
https://my-visual-cpu.site
```

Local upstream:

```text
http://127.0.0.1:4173
```

## Current Layout

- systemd service:
  `mycpu-frontend.service`
- nginx site:
  `/etc/nginx/sites-available/mycpu`
- nginx symlink:
  `/etc/nginx/sites-enabled/mycpu`
- service env file:
  `/srv/apps/my_visual_CPU/repo/deploy/env/mycpu-frontend.env`
- runtime assets:
  - `/srv/apps/my_visual_CPU/runtime-assets/linux/Image`
  - `/srv/apps/my_visual_CPU/runtime-assets/linux/rootfs.ext4`
  - `/srv/apps/my_visual_CPU/runtime-assets/linux/linux.dtb`
  - `/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike`
- logs:
  - `/srv/apps/my_visual_CPU/logs/nginx-access.log`
  - `/srv/apps/my_visual_CPU/logs/nginx-error.log`
  - `/srv/apps/my_visual_CPU/logs/audit.log`

## Upgrade From the `a8869a8` Deployment Baseline

Use this flow when the live server already runs the `a8869a8` deployment and the
repo should move to the current `main`.

Preflight:

```bash
cd /srv/apps/my_visual_CPU/repo
git rev-parse --short HEAD
git status --short --branch

ls -lh \
  /srv/apps/my_visual_CPU/runtime-assets/linux/Image \
  /srv/apps/my_visual_CPU/runtime-assets/linux/rootfs.ext4 \
  /srv/apps/my_visual_CPU/runtime-assets/linux/linux.dtb \
  /srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike
```

Verify the auth hash before restarting. Remote production configs should not run
with auth disabled unless `MYCPU_PUBLIC_UNAUTH_OK=1` is explicitly present for a
development-only deployment:

```bash
cd /srv/apps/my_visual_CPU/repo
if rg -q '^MYCPU_AUTH_ENABLED=0' deploy/env/mycpu-frontend.env; then
  rg '^MYCPU_PUBLIC_UNAUTH_OK=1' deploy/env/mycpu-frontend.env
else
  rg '^MYCPU_AUTH_ENABLED=1' deploy/env/mycpu-frontend.env
  rg '^MYCPU_AUTH_ADMIN_PASSWORD_HASH=scrypt\\$' deploy/env/mycpu-frontend.env
fi
```

Upgrade the checkout and rebuild:

```bash
cd /srv/apps/my_visual_CPU/repo
git fetch origin
git checkout main
git pull --ff-only origin main

deploy/scripts/prepare_runtime_dirs.sh

cd myCPU
make
make build-workload WORKLOAD_NAME=linux_proto LINUX_PROTO_ROOTFS_MODE=block
install -m 644 workloads/linux_proto/rootfs.ext4 /srv/apps/my_visual_CPU/runtime-assets/linux/rootfs.ext4
install -m 644 workloads/linux_proto/mycpu_virt.dtb /srv/apps/my_visual_CPU/runtime-assets/linux/linux.dtb
cd ..
```

Reinstall templates only after reviewing local edits:

```bash
sudo install -m 644 /srv/apps/my_visual_CPU/repo/deploy/systemd/mycpu-frontend.service /etc/systemd/system/mycpu-frontend.service
sudo systemctl daemon-reload
sudo systemctl restart mycpu-frontend.service

sudo install -m 644 /srv/apps/my_visual_CPU/repo/deploy/nginx/mycpu.conf /etc/nginx/sites-available/mycpu
sudo nginx -t
sudo systemctl reload nginx
```

Post-upgrade checks:

```bash
systemctl status --no-pager mycpu-frontend.service
curl -k --resolve my-visual-cpu.site:443:127.0.0.1 https://my-visual-cpu.site/api/tests
deploy/scripts/remote_smoke.sh
```

The upgrade should not overwrite `runtime-assets/`, `logs/`, `tmp/`, or the
host-local `deploy/env/mycpu-frontend.env`. `/api/tests` should return `401`
before login when the remote auth layer is enabled; the browser must log in
before using `/console`.

## Daily Checks

Check service status:

```bash
systemctl status --no-pager mycpu-frontend.service
ss -ltnp | rg '127\.0\.0\.1:4173'
journalctl -u mycpu-frontend.service -n 100 --no-pager
```

Check nginx status:

```bash
sudo nginx -t
systemctl status --no-pager nginx
sudo tail -n 100 /srv/apps/my_visual_CPU/logs/nginx-access.log
sudo tail -n 100 /srv/apps/my_visual_CPU/logs/nginx-error.log
```

Check local routing:

```bash
curl -I -H 'Host: my-visual-cpu.site' http://127.0.0.1/
curl -k -I --resolve my-visual-cpu.site:443:127.0.0.1 https://my-visual-cpu.site/
curl -k --resolve my-visual-cpu.site:443:127.0.0.1 https://my-visual-cpu.site/api/tests
```

Expected results:

- HTTP returns `301` to `https://my-visual-cpu.site/`
- HTTPS `/` returns `200`
- `/api/tests` returns `401` before login, then `200` after browser login
- `diagnostics.linuxConsole.status` is `ready` when `Image` exists

Check auth audit activity:

```bash
sudo tail -n 100 /srv/apps/my_visual_CPU/logs/audit.log
```

## Restart Operations

Restart only the frontend service:

```bash
sudo systemctl restart mycpu-frontend.service
systemctl status --no-pager mycpu-frontend.service
```

Rotate the admin password hash:

```bash
node -e "const crypto=require('crypto'); const password=process.argv[1]; const N=16384,r=8,p=1; const salt=crypto.randomBytes(16); const hash=crypto.scryptSync(password,salt,64,{N,r,p}); console.log(['scrypt',N,r,p,salt.toString('base64url'),hash.toString('base64url')].join('$'))" 'new-strong-password'
sudoedit /srv/apps/my_visual_CPU/repo/deploy/env/mycpu-frontend.env
sudo systemctl restart mycpu-frontend.service
```

Reload nginx after config-only changes:

```bash
sudo nginx -t
sudo systemctl reload nginx
```

Full nginx restart if reload is not enough:

```bash
sudo nginx -t
sudo systemctl restart nginx
```

## Runtime Asset Checks

Check that required assets still exist:

```bash
ls -lh \
  /srv/apps/my_visual_CPU/runtime-assets/linux/Image \
  /srv/apps/my_visual_CPU/runtime-assets/linux/rootfs.ext4 \
  /srv/apps/my_visual_CPU/runtime-assets/linux/linux.dtb \
  /srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike
```

Check repo-generated Linux runtime artifacts used by the frontend route:

```bash
ls -lh \
  /srv/apps/my_visual_CPU/repo/myCPU/workloads/linux_proto/rootfs.ext4 \
  /srv/apps/my_visual_CPU/repo/myCPU/workloads/linux_proto/mycpu_virt.dtb
```

Rebuild repo-generated Linux artifacts if needed:

```bash
cd /srv/apps/my_visual_CPU/repo/myCPU
make build-workload WORKLOAD_NAME=linux_proto LINUX_PROTO_ROOTFS_MODE=block
install -m 644 workloads/linux_proto/rootfs.ext4 /srv/apps/my_visual_CPU/runtime-assets/linux/rootfs.ext4
install -m 644 workloads/linux_proto/mycpu_virt.dtb /srv/apps/my_visual_CPU/runtime-assets/linux/linux.dtb
```

Do not fabricate `Image` or `spike`.

## Certificate Renewal

The host timer is enabled:

```bash
systemctl status --no-pager certbot.timer
```

Recommended dry-run for this site only:

```bash
sudo certbot renew --cert-name my-visual-cpu.site --dry-run
```

Renew this site immediately if needed:

```bash
sudo certbot renew --cert-name my-visual-cpu.site
```

List installed certificates:

```bash
sudo certbot certificates
```

Important host-specific caveat:

- `sudo certbot renew --dry-run` on this host currently returns non-zero because
  `campus2hand.site` fails validation.
- `my-visual-cpu.site` dry-run/renewal should be checked with the
  `--cert-name my-visual-cpu.site` form above so that `trading-system` renewal
  issues do not get misread as a myCPU outage.

## Rollback

### Roll back nginx only

Disable the public myCPU site:

```bash
sudo rm -f /etc/nginx/sites-enabled/mycpu
sudo nginx -t
sudo systemctl reload nginx
```

Restore it later:

```bash
sudo ln -sfn /etc/nginx/sites-available/mycpu /etc/nginx/sites-enabled/mycpu
sudo nginx -t
sudo systemctl reload nginx
```

### Roll back the frontend service only

Stop public upstream while keeping files:

```bash
sudo systemctl stop mycpu-frontend.service
```

Disable it across reboots:

```bash
sudo systemctl disable mycpu-frontend.service
```

Bring it back:

```bash
sudo systemctl enable --now mycpu-frontend.service
```

### Roll back to a previous repo checkout

Before changing code, note current state:

```bash
cd /srv/apps/my_visual_CPU/repo
git rev-parse HEAD
git status --short --branch
```

If the target commit already exists locally:

```bash
cd /srv/apps/my_visual_CPU/repo
git checkout <target-commit-or-branch>
sudo systemctl restart mycpu-frontend.service
```

If the env or nginx template changed in that target revision, re-install them:

```bash
sudo install -m 644 /srv/apps/my_visual_CPU/repo/deploy/systemd/mycpu-frontend.service /etc/systemd/system/mycpu-frontend.service
sudo systemctl daemon-reload
sudo systemctl restart mycpu-frontend.service

sudo install -m 644 /srv/apps/my_visual_CPU/repo/deploy/nginx/mycpu.conf /etc/nginx/sites-available/mycpu
sudo nginx -t
sudo systemctl reload nginx
```

Do not use destructive git rollback commands unless you explicitly intend to
discard local repo changes.

## Remote Validation Commands

Frontend and Linux ready:

```bash
curl -k --resolve my-visual-cpu.site:443:127.0.0.1 https://my-visual-cpu.site/api/tests
```

Linux runtime guardrail:

```bash
cd /srv/apps/my_visual_CPU/repo/myCPU
MYCPU_RUN_LINUX_PROTO_RUNTIME=1 \
MYCPU_LINUX_PROTO_RUNTIME_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image \
python3 -m unittest \
  tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested
```

Linux frontend e2e guardrail:

```bash
cd /srv/apps/my_visual_CPU/repo
MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E=1 \
MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image \
node --test frontend/tests/debug_server_e2e.test.mjs
```

## Incident Notes

If `https://my-visual-cpu.site` is down but `127.0.0.1:4173` is healthy, focus
on nginx and certificate state.

If nginx is healthy but `/api/tests` does not show `linux_proto_console`, check:

- `MYCPU_LINUX_PROTO_CONSOLE_IMAGE` in
  `/srv/apps/my_visual_CPU/repo/deploy/env/mycpu-frontend.env`
- readability and existence of
  `/srv/apps/my_visual_CPU/runtime-assets/linux/Image`
- repo-generated
  `/srv/apps/my_visual_CPU/repo/myCPU/workloads/linux_proto/rootfs.ext4`
- repo-generated
  `/srv/apps/my_visual_CPU/repo/myCPU/workloads/linux_proto/mycpu_virt.dtb`

If `certbot renew --dry-run` fails, check whether the failure belongs to
`my-visual-cpu.site` or another site on the same host before taking action on
the myCPU deployment.
