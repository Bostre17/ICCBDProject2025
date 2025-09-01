// server.js
const express = require('express');
const { MongoClient } = require('mongodb');

// --- MODIFICA CHIAVE: Correzione degli import ---
const { MeterProvider } = require('@opentelemetry/sdk-metrics');
const { ValueType } = require('@opentelemetry/api'); // ValueType va importato da qui
// --- FINE MODIFICA ---

const { PrometheusExporter } = require('@opentelemetry/exporter-prometheus');
const { Resource } = require('@opentelemetry/resources');
const { SemanticResourceAttributes } = require('@opentelemetry/semantic-conventions');

// --- Il resto del file rimane identico a prima ---

// ... (tutto il resto del codice è corretto e rimane invariato) ...

// --- Funzione Principale Asincrona per l'Avvio ---
async function startServer() {
  // --- Configurazione e Connessione a MongoDB ---
  const mongoUrl = 'mongodb://mongo:27017';
  const client = new MongoClient(mongoUrl);
  let countersCollection;

  try {
    await client.connect();
    console.log('Connesso a MongoDB');
    const database = client.db('visitdb');
    countersCollection = database.collection('counters');
  } catch (err) {
    console.error('Impossibile connettersi a MongoDB. Il server non si avvierà.', err);
    process.exit(1);
  }

  // --- Inizializzazione Express e OpenTelemetry ---
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
  const visitsCounter = meter.createCounter('visits_total', {
    description: 'Total number of visits to the web server',
  });
  const requestLatency = meter.createHistogram('request_latency_seconds', {
    description: 'Durata delle richieste HTTP in secondi', unit: 's', valueType: ValueType.DOUBLE,
  });

  // --- Middleware Express ---
  app.use(async (req, res, next) => {
    const startTime = new Date();
    
    if (req.path === '/') {
      try {
        await countersCollection.findOneAndUpdate(
          { _id: "total_visits" },
          { $inc: { value: 1 } },
          { upsert: true }
        );
        visitsCounter.add(1, { 'page': '/' });
      } catch (err) {
        console.error("Errore nell'incrementare il contatore su MongoDB:", err);
      }
    }
  
    res.on('finish', () => {
        const endTime = new Date();
        const durationSeconds = (endTime - startTime) / 1000.0;
        requestLatency.record(durationSeconds, { method: req.method, path: req.path, status_code: res.statusCode });
    });
    
    next();
  });

  // --- Routes ---
  app.get('/', async (req, res) => {
    let visitsCount = 0;
    try {
      const counterDoc = await countersCollection.findOne({ _id: "total_visits" });
      if (counterDoc) {
        visitsCount = counterDoc.value;
      }
    } catch (err) {
       console.error("Errore nel leggere il contatore da MongoDB:", err);
    }
  
    res.send(`
    <html>
      <head><title>Visit Counter - BASE con MongoDB</title>
        <style>
          body { font-family: Arial, sans-serif; text-align: center; margin-top: 50px; }
          .counter { font-size: 48px; margin: 20px; color: #4DB33D; }
          h1 { color: #000; }
        </style>
      </head>
      <body>
        <h1>Benvenuto nel Visit Counter BASE!</h1>
        <p>Lo stato è gestito da un database <strong>MongoDB</strong>, ottimizzato per scalabilità e disponibilità.</p>
        <div class="counter">${visitsCount}</div>
        <p>Visite totali registrate</p>
      </body>
    </html>
    `);
  });

  app.get('/health', (req, res) => res.status(200).send('OK'));

  // --- Avvio del Server ---
  app.listen(port, () => {
    console.log(`Server pronto e in ascolto sulla porta ${port}`);
  });
}

// Avvia l'intera applicazione
startServer();