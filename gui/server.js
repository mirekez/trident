import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { extname, join, normalize } from 'node:path';

const port = Number(process.env.PORT || 5173);
const root = new URL('.', import.meta.url).pathname;

const types = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'application/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8'
};

createServer(async (request, response) => {
  const url = new URL(request.url, `http://${request.headers.host}`);
  const relative = url.pathname === '/' ? 'index.html' : url.pathname.slice(1);
  const path = normalize(join(root, relative));

  if (!path.startsWith(root)) {
    response.writeHead(404);
    response.end('Not found');
    return;
  }

  try {
    const body = await readFile(path);
    response.writeHead(200, { 'content-type': types[extname(path)] || 'application/octet-stream' });
    response.end(body);
  } catch {
    response.writeHead(404);
    response.end('Not found');
  }
}).listen(port, '127.0.0.1', () => {
  console.log(`GUI dev server at http://127.0.0.1:${port}`);
});
