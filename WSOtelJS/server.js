// server.js
const express = require('express');
const { MeterProvider } = require('@opentelemetry/sdk-metrics');
const { PrometheusExporter } = require('@opentelemetry/exporter-prometheus');
const { Resource } = require('@opentelemetry/resources');
const { SemanticResourceAttributes } = require('@opentelemetry/semantic-conventions');

// Configurazione OpenTelemetry con Prometheus
const prometheusExporter = new PrometheusExporter({
  port: 9464, // Porta per l'endpoint Prometheus metrics
});

// Creazione del resource per identificare l'applicazione
const resource = new Resource({
  [SemanticResourceAttributes.SERVICE_NAME]: 'visit-counter',
  [SemanticResourceAttributes.SERVICE_VERSION]: '1.0.0',
});

// Configurazione del MeterProvider
const meterProvider = new MeterProvider({
  resource: resource,
});
meterProvider.addMetricReader(prometheusExporter);

// Creazione dei meter e counter
const meter = meterProvider.getMeter('visit-counter-meter');
const visitsCounter = meter.createCounter('visits_total', {
  description: 'Total number of visits to the web server',
});

// Inizializzazione Express
const app = express();
const port = process.env.PORT || 3000;

// Contatore in memoria (utile per visualizzare rapidamente)
let visitsCount = 0;

// Middleware per contare le visite
app.use((req, res, next) => {
  if (req.path === '/') {
    visitsCount++;
    visitsCounter.add(1, { 'page': 'home' });
  }
  next();
});

// Route principale
app.get('/', (req, res) => {
  res.send(`
    <html>
      <head>
        <title>Visit Counter</title>
        <style>
          body {
            font-family: Arial, sans-serif;
            text-align: center;
            margin-top: 50px;
          }
          .counter {
            font-size: 48px;
            margin: 20px;
          }
          .info {
            margin-top: 30px;
            color: #666;
          }
        </style>
      </head>
      <body>
        <h1>Benvenuto nel Visit Counter</h1>
        <p>Questa pagina conta le visite che riceve</p>
        <div class="counter">${visitsCount}</div>
        <p>Visite totali registrate</p>
        <div class="info">
          <p>Le metriche sono disponibili all'endpoint Prometheus: <a href="http://localhost:9464/metrics" target="_blank">http://localhost:9464/metrics</a></p>
          <p>Per vedere solo le metriche specifiche di questa applicazione: <a href="http://localhost:9464/metrics?name=visits_total" target="_blank">http://localhost:9464/metrics?name=visits_total</a></p>
        </div>
      </body>
    </html>
  `);
});

// Avvio del server
app.listen(port, () => {
  console.log(`Server in ascolto sulla porta ${port}`);
  console.log(`Metrics esposte su http://localhost:9464/metrics`);
});