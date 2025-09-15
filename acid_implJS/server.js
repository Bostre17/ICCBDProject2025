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
        user: 'user', host: 'postgres', database: 'visitdb', password: 'password', port: 5432,
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
    
    // --- METRICHE ESISTENTI E NUOVE ---
    const visitsCounter = meter.createCounter('visits_total', {
        description: 'Total number of visits to the web server',
    });
    const requestLatency = meter.createHistogram('request_latency_seconds', {
        description: 'Durata delle richieste HTTP in secondi', unit: 's', valueType: ValueType.DOUBLE,
    });
    // NUOVA METRICA: Gauge per le richieste attive
    const activeRequests = meter.createUpDownCounter('active_requests_total', {
        description: 'Numero di richieste HTTP attualmente attive',
    });
    // NUOVA METRICA: Contatore per status code HTTP
    const httpRequestsTotal = meter.createCounter('http_requests_total', {
        description: 'Conteggio totale delle richieste HTTP per status code e metodo',
    });
    // NUOVA METRICA: Istogramma per la latenza delle query a PostgreSQL
    const postgresQueryLatency = meter.createHistogram('postgres_query_latency_seconds', {
        description: 'Latenza delle query a PostgreSQL in secondi', unit: 's', valueType: ValueType.DOUBLE,
    });

    // Middleware per tracciare tutte le richieste
    app.use(async (req, res, next) => {
        const startTime = new Date();
        activeRequests.add(1); // Incrementa le richieste attive

        if (req.path === '/') {
            const client = await pool.connect();
            const dbStartTime = new Date();
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
                const dbEndTime = new Date();
                const dbDurationSeconds = (dbEndTime - dbStartTime) / 1000.0;
                postgresQueryLatency.record(dbDurationSeconds, { operation: 'update_transaction' });
                client.release();
            }
        }
        
        res.on('finish', () => {
            const endTime = new Date();
            const durationSeconds = (endTime - startTime) / 1000.0;
            requestLatency.record(durationSeconds, { method: req.method, path: req.path, status_code: res.statusCode });
            httpRequestsTotal.add(1, { method: req.method, code: res.statusCode });
            activeRequests.add(-1); // Decrementa le richieste attive
        });
        next();
    });

    app.get('/', async (req, res) => {
        let visitsCount = 0;
        const dbStartTime = new Date();
        try {
            const query = 'SELECT value FROM counters WHERE name = $1';
            const result = await pool.query(query, [COUNTER_NAME]);
            if (result.rows.length > 0) {
                visitsCount = result.rows[0].value;
            }
        } catch (err) {
           console.error("Errore nel leggere il contatore da PostgreSQL:", err);
        } finally {
            const dbEndTime = new Date();
            const dbDurationSeconds = (dbEndTime - dbStartTime) / 1000.0;
            postgresQueryLatency.record(dbDurationSeconds, { operation: 'select' });
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