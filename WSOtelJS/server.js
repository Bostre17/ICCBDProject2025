// server.js
const express = require('express');
const { createClient } = require('redis');
const { MeterProvider } = require('@opentelemetry/sdk-metrics');
const { PrometheusExporter } = require('@opentelemetry/exporter-prometheus');
const { Resource } = require('@opentelemetry/resources');
const { SemanticResourceAttributes } = require('@opentelemetry/semantic-conventions');

// --- Configurazione Redis ---
const redisClient = createClient({
  // Il nome 'redis' verrà risolto da Docker Compose all'IP del container Redis
  url: 'redis://redis:6379'
});

redisClient.on('error', (err) => console.log('Redis Client Error', err));
redisClient.connect();

const TOTAL_VISITS_KEY = 'total_visits'; // Chiave per il contatore totale su Redis

// --- Configurazione OpenTelemetry con Prometheus ---
const prometheusExporter = new PrometheusExporter({
  port: 9464, // Porta per l'endpoint Prometheus metrics
});

const resource = new Resource({
  [SemanticResourceAttributes.SERVICE_NAME]: 'visit-counter-scalable',
  [SemanticResourceAttributes.SERVICE_VERSION]: '2.0.0',
});

const meterProvider = new MeterProvider({
  resource: resource,
});
meterProvider.addMetricReader(prometheusExporter);

const meter = meterProvider.getMeter('visit-counter-meter');
const visitsCounter = meter.createCounter('visits_total', {
  description: 'Total number of visits to the web server',
});

// --- Inizializzazione Express ---
const app = express();
const port = process.env.PORT || 3000;

// --- Middleware per contare le visite ---
// Questo middleware si applica a tutte le richieste in arrivo
app.use(async (req, res, next) => {
  // Incrementa il contatore totale su Redis in modo atomico
  // e aggiorna la metrica OpenTelemetry
  await redisClient.incr(TOTAL_VISITS_KEY);
  visitsCounter.add(1, { 'page': req.path });
  next();
});

// --- Route principale ---
app.get('/', async (req, res) => {
  // Ottiene il valore corrente del contatore da Redis
  const visitsCount = await redisClient.get(TOTAL_VISITS_KEY) || 0;

  res.send(`
    <html>
      <head>
        <title>Visit Counter Scalabile</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
          .counter { font-size: 48px; margin: 20px; color: #2c3e50; }
          h1 { color: #3498db; }
        </style>
      </head>
      <body>
        <h1>Benvenuto nel Visit Counter Scalabile!</h1>
        <p>Questa applicazione è servita da una delle tante repliche dietro un load balancer.</p>
        <div class="counter">${visitsCount}</div>
        <p>Visite totali registrate (stato condiviso su Redis)</p>
      </body>
    </html>
  `);
});

// --- Endpoint di Health Check ---
// Usato dal load balancer per verificare se l'istanza è attiva
app.get('/health', (req, res) => {
    res.status(200).send('OK');
});


// --- Avvio del server ---
app.listen(port, () => {
  console.log(`Server in ascolto sulla porta ${port}`);
  console.log(`Metrics esposte su http://localhost:9464/metrics`);
});