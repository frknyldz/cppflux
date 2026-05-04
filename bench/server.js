const http    = require('http');
const cluster = require('cluster');
const os      = require('os');

const WORKERS = os.cpus().length;  // one process per logical core

const sleep = ms => new Promise(r => setTimeout(r, ms));

async function dbFetch(key)          { await sleep(10); return `db_result:${key}`; }
async function serviceCall(raw)      { await sleep(10); return `enriched:${raw}`; }
async function formatResponse(s)     { return `formatted:${s}\n`; }

if (cluster.isPrimary) {
    console.log(`Node cluster :8082  workers=${WORKERS}`);
    for (let i = 0; i < WORKERS; i++) cluster.fork();
} else {
    http.createServer(async (req, res) => {
        if (req.url === '/ping') {
            res.end('pong\n');

        } else if (req.url === '/echo' && req.method === 'POST') {
            const chunks = [];
            for await (const chunk of req) chunks.push(chunk);
            const body = Buffer.concat(chunks).toString();
            res.end(body || '(empty)\n');

        } else if (req.url === '/pipeline') {
            const raw      = await dbFetch('pipeline');
            const enriched = await serviceCall(raw);
            const response = await formatResponse(enriched);
            res.end(response);

        } else {
            res.writeHead(404);
            res.end('Not Found\n');
        }
    }).listen(8082);
}
