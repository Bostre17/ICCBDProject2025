// base_implJS/server.js
const express = require('express');
const { MongoClient } = require('mongodb');
const { MeterProvider } = require('@opentelemetry/sdk-metrics');
const { ValueType } = require('@opentelemetry/api');
const { PrometheusExporter } = require('@opentelemetry/exporter-prometheus');
const { Resource } = require('@opentelemetry/resources');
const { SemanticResourceAttributes } = require('@opentelemetry/semantic-conventions');

async function startServer() {
  const mongoUrl = 'mongodb://mongo:27017';
  const client = new MongoClient(mongoUrl);
  let countersCollection;

  try {
    await client.connect();
    console.log('Connesso a MongoDB');
    const database = client.db('visitdb');
    countersCollection = database.collection('counters');
  } catch (err) {
    console.error('Impossibile connettersi a MongoDB.', err);
    process.exit(1);
  }

  const app = express();
  const port = process.env.PORT || 3000;
  const prometheusExporter = new PrometheusExporter({ port: 9464 });
  const resource = new Resource({
    [SemanticResourceAttributes.SERVICE_NAME]: 'visit-counter-base-mongodb',
    [SemanticResourceAttributes.SERVICE_VERSION]: '1.0.0',
  });
  const meterProvider = new MeterProvider({ resource: resource });
  meterProvider.addMetricReader(prometheusExporter);
  const meter = meterProvider.getMeter('visit-counter-meter');

  // --- METRICHE ESISTENTI E NUOVE ---
  
  // Metrica esistente: contatore totale visite
  const visitsCounter = meter.createCounter('visits_total', {
    description: 'Total number of visits to the web server',
  });
  
  // Metrica esistente: istogramma latenza richieste HTTP
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

  // NUOVA METRICA: Istogramma per la latenza delle query a MongoDB
  const mongoQueryLatency = meter.createHistogram('mongodb_query_latency_seconds', {
      description: 'Latenza delle query a MongoDB in secondi',
      unit: 's',
      valueType: ValueType.DOUBLE,
  });

  // Middleware per tracciare tutte le richieste
  app.use(async (req, res, next) => {
    const startTime = new Date();
    activeRequests.add(1); // Incrementa le richieste attive

    res.on('finish', () => {
        const endTime = new Date();
        const durationSeconds = (endTime - startTime) / 1000.0;
        
        // Registra la latenza della richiesta HTTP
        requestLatency.record(durationSeconds, { method: req.method, path: req.path, status_code: res.statusCode });
        
        // Registra il contatore per lo status code
        httpRequestsTotal.add(1, { method: req.method, code: res.statusCode });
        
        // Decrementa le richieste attive
        activeRequests.add(-1);
    });
    next();
  });


  app.get('/', async (req, res) => {
    // Gestione della visita e del contatore
    if (req.path === '/') {
        const dbUpdateStartTime = new Date();
        try {
            await countersCollection.findOneAndUpdate(
                { _id: "total_visits" },
                { $inc: { value: 1 } },
                { upsert: true }
            );
            visitsCounter.add(1, { 'page': '/' });
        } catch (err) {
            console.error("Errore nell'incrementare il contatore su MongoDB:", err);
        } finally {
            const dbUpdateEndTime = new Date();
            const dbDurationSeconds = (dbUpdateEndTime - dbUpdateStartTime) / 1000.0;
            // Registra la latenza della query di scrittura
            mongoQueryLatency.record(dbDurationSeconds, { operation: 'update' });
        }
    }

    let visitsCount = 0;
    const dbReadStartTime = new Date();
    try {
      const counterDoc = await countersCollection.findOne({ _id: "total_visits" });
      if (counterDoc) {
        visitsCount = counterDoc.value;
      }
    } catch (err) {
       console.error("Errore nel leggere il contatore da MongoDB:", err);
    } finally {
        const dbReadEndTime = new Date();
        const dbDurationSeconds = (dbReadEndTime - dbReadStartTime) / 1000.0;
        // Registra la latenza della query di lettura
        mongoQueryLatency.record(dbDurationSeconds, { operation: 'find' });
    }
    
    res.send(`
    <html>
      <head><title>Visit Counter - BASE con MongoDB</title></head>
      <body>
        <h1>Benvenuto nel Visit Counter BASE!</h1>
        <p>Database <strong>MongoDB</strong>.</p>
        <div style="font-size: 48px;">${visitsCount}</div>
        <p>Visite totali registrate</p>
      </body>
    </html>
    `);
  });

  app.get('/health', (req, res) => res.status(200).send('OK'));

  app.listen(port, () => {
    console.log(`Server pronto sulla porta ${port}`);
  });
}

startServer();