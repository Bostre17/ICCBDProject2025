// acid_implJS/server.js
const express = require('express');
const { Pool } = require('pg');
const { MeterProvider } = require('@opentelemetry/sdk-metrics');
const { ValueType } = require('@opentelemetry/api');
const { PrometheusExporter } = require('@opentelemetry/exporter-prometheus');
const { Resource } = require('@opentelemetry/resources');
const { SemanticResourceAttributes } = require('@opentelemetry/semantic-conventions');

async function startServer() {
    const app = express();
    const port = process.env.PORT || 3000;

    const pool = new Pool({
        user: 'user',
        host: 'postgres',
        database: 'visitdb',
        password: 'password',
        port: 5432,
    });

    try {
        const client = await pool.connect();
        console.log('Connesso a PostgreSQL');
        client.release();
    } catch (err) {
        console.error('Impossibile connettersi a PostgreSQL.', err);
        process.exit(1);
    }
    
    const COUNTER_NAME = 'total';

    const prometheusExporter = new PrometheusExporter({ port: 9464 });
    const resource = new Resource({
        [SemanticResourceAttributes.SERVICE_NAME]: 'visit-counter-acid',
        [SemanticResourceAttributes.SERVICE_VERSION]: '1.0.0',
    });
    const meterProvider = new MeterProvider({ resource: resource });
    meterProvider.addMetricReader(prometheusExporter);
    const meter = meterProvider.getMeter('visit-counter-meter');
    const visitsCounter = meter.createCounter('visits_total', {
        description: 'Total number of visits to the web server',
    });
    const requestLatency = meter.createHistogram('request_latency_seconds', {
        description: 'Durata delle richieste HTTP in secondi', unit: 's', valueType: ValueType.DOUBLE,
    });

    app.use(async (req, res, next) => {
        const startTime = new Date();
        if (req.path === '/') {
            const client = await pool.connect();
            try {
                await client.query('BEGIN');
                const query = 'UPDATE counters SET value = value + 1 WHERE name = $1';
                await client.query(query, [COUNTER_NAME]);
                await client.query('COMMIT');
                visitsCounter.add(1, { 'page': '/' });
            } catch (err) {
                await client.query('ROLLBACK');
                console.error("Errore nella transazione, rollback eseguito:", err);
            } finally {
                client.release();
            }
        }
        res.on('finish', () => {
            const endTime = new Date();
            const durationSeconds = (endTime - startTime) / 1000.0;
            requestLatency.record(durationSeconds, { method: req.method, path: req.path, status_code: res.statusCode });
        });
        next();
    });

    app.get('/', async (req, res) => {
        let visitsCount = 0;
        try {
            const query = 'SELECT value FROM counters WHERE name = $1';
            const result = await pool.query(query, [COUNTER_NAME]);
            if (result.rows.length > 0) {
                visitsCount = result.rows[0].value;
            }
        } catch (err) {
           console.error("Errore nel leggere il contatore da PostgreSQL:", err);
        }
        res.send(`
        <html><head><title>Visit Counter - Versione ACID</title></head>
            <body><h1>Benvenuto!</h1><p>Database <strong>PostgreSQL</strong> con transazioni ACID.</p>
            <div style="font-size: 48px;">${visitsCount}</div><p>Visite totali</p>
            </body></html>
        `);
    });

    app.get('/health', (req, res) => res.status(200).send('OK'));

    app.listen(port, () => {
        console.log(`Server ACID pronto sulla porta ${port}`);
    });
}

startServer();