import crypto from 'node:crypto';

const WS_GUID = '258EAFA5-E914-47DA-95CA-C5AB0DC85B11';

function frameText(text) {
  const payload = Buffer.from(text);
  if (payload.length < 126) {
    return Buffer.concat([Buffer.from([0x81, payload.length]), payload]);
  }
  if (payload.length < 65536) {
    const header = Buffer.alloc(4);
    header[0] = 0x81;
    header[1] = 126;
    header.writeUInt16BE(payload.length, 2);
    return Buffer.concat([header, payload]);
  }

  const header = Buffer.alloc(10);
  header[0] = 0x81;
  header[1] = 127;
  header.writeBigUInt64BE(BigInt(payload.length), 2);
  return Buffer.concat([header, payload]);
}

export function createWebSocketHub(server) {
  const sockets = new Set();

  server.on('upgrade', (request, socket) => {
    if (request.url !== '/ws') {
      socket.destroy();
      return;
    }

    const key = request.headers['sec-websocket-key'];
    if (!key) {
      socket.destroy();
      return;
    }

    const accept = crypto.createHash('sha1').update(`${key}${WS_GUID}`).digest('base64');
    socket.write(
      [
        'HTTP/1.1 101 Switching Protocols',
        'Upgrade: websocket',
        'Connection: Upgrade',
        `Sec-WebSocket-Accept: ${accept}`,
        '',
        '',
      ].join('\r\n'));

    sockets.add(socket);
    const cleanup = () => sockets.delete(socket);
    socket.on('close', cleanup);
    socket.on('end', cleanup);
    socket.on('error', cleanup);
    socket.on('data', (chunk) => {
      if ((chunk[0] & 0x0F) === 0x8) {
        socket.end();
        cleanup();
      }
    });
  });

  return {
    broadcast(payload) {
      const frame = frameText(JSON.stringify(payload));
      for (const socket of sockets) {
        if (socket.destroyed) {
          sockets.delete(socket);
          continue;
        }
        socket.write(frame);
      }
    },
    close() {
      for (const socket of sockets) {
        socket.end();
      }
      sockets.clear();
    },
  };
}
