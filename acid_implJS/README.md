# Visit Counter Web Server con OpenTelemetry e Prometheus

Questa applicazione è un server web semplice che conta le visite ricevute e le espone come metriche utilizzando OpenTelemetry e Prometheus.

## Caratteristiche

- Web server in Node.js con Express
- Conteggio visite visualizzato in una pagina web
- Metriche esposte tramite OpenTelemetry per Prometheus
- Configurazione Docker completa con Prometheus e Grafana
- Dashboard per visualizzare le statistiche delle visite

## Requisiti

- Docker e Docker Compose

## Installazione e avvio

1. Clona il repository
2. Esegui il seguente comando nella directory principale:

```bash
docker-compose up -d
```

## Accesso ai servizi

- **Web Server**: http://localhost:3000
- **Metriche Prometheus**: http://localhost:9464/metrics
- **Interfaccia Prometheus**: http://localhost:9090
- **Interfaccia Grafana**: http://localhost:3001 (username: admin, password: admin)

## Struttura del progetto

- `server.js` - Il codice dell'applicazione Node.js
- `package.json` - Dipendenze del progetto
- `Dockerfile` - Configurazione per costruire l'immagine Docker
- `docker-compose.yml` - Configurazione per orchestrare i container
- `prometheus.yml` - Configurazione di Prometheus

## Metriche disponibili

La principale metrica esposta è `visits_total`, che rappresenta il numero totale di visite alla pagina principale del server.

## Guida all'utilizzo delle tecnologie

### OpenTelemetry

[OpenTelemetry](https://opentelemetry.io/) è un framework open source per la raccolta di dati telemetrici (metriche, tracce e log) dalle applicazioni. In questo progetto viene utilizzato per:

#### Concetti fondamentali di OpenTelemetry

1. **Resource**: Rappresenta l'entità che produce la telemetria (in questo caso, il nostro server web)
2. **MeterProvider**: Responsabile della creazione e gestione dei meter
3. **Meter**: Factory per la creazione di strumenti di misurazione come counter, gauge, ecc.
4. **Counter**: Misura che può solo incrementare, utilizzata in questo caso per contare le visite

#### Come è implementato nel progetto

```javascript
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

// Incremento del contatore ad ogni visita
visitsCounter.add(1, { 'page': 'home' });
```

#### Estensioni possibili

- Aggiungere altre metriche come `gauge` per misurare valori che possono aumentare o diminuire
- Implementare tracciamento distribuito per seguire un'azione attraverso più servizi
- Aggiungere attributi alle metriche per segmentazione e filtri (es. `user_agent`, `country`, ecc.)

### Prometheus

[Prometheus](https://prometheus.io/) è un sistema di monitoraggio e alerting open source, progettato specificamente per ambienti cloud e microservizi.

#### Concetti fondamentali di Prometheus

1. **Metrics**: Dati numerici time series che rappresentano lo stato del sistema
2. **Scraping**: Processo con cui Prometheus raccoglie le metriche dagli endpoint esposti
3. **PromQL**: Linguaggio di query per interrogare e analizzare le metriche
4. **Alerting**: Sistema per creare avvisi basati su condizioni dei dati

#### Configurazione in questo progetto

Il file `prometheus.yml` configura il servizio Prometheus:

```yaml
global:
  scrape_interval: 15s  # Frequenza con cui raccogliere le metriche

scrape_configs:
  - job_name: 'visit-counter'  # Nome del job che identifica la fonte
    static_configs:
      - targets: ['visit-counter:9464']  # Endpoint delle metriche
```

#### Come utilizzare Prometheus nel progetto

1. Accedi all'interfaccia web di Prometheus all'indirizzo http://localhost:9090
2. Utilizza la barra di ricerca per inserire query PromQL come:
   - `visits_total`: Visualizza il contatore totale delle visite
   - `rate(visits_total[5m])`: Calcola il tasso di visite negli ultimi 5 minuti
3. Esplora i grafici generati e utilizza le funzioni temporali per analizzare i dati

#### Query PromQL utili

- `visits_total`: Numero totale di visite
- `rate(visits_total[1m])`: Tasso di visite al minuto
- `sum(rate(visits_total[5m])) by (page)`: Tasso di visite raggruppato per pagina
- `increase(visits_total[1h])`: Incremento delle visite nell'ultima ora

### Grafana

[Grafana](https://grafana.com/) è una piattaforma open source per la visualizzazione e il monitoraggio dei dati metrici. Permette di creare dashboard interattive e avvisi.

#### Concetti fondamentali di Grafana

1. **Datasource**: Fonte dei dati (in questo caso Prometheus)
2. **Dashboard**: Insieme di pannelli che visualizzano dati
3. **Panel**: Singola visualizzazione (grafico, tabella, ecc.) all'interno di una dashboard
4. **Query Editor**: Interfaccia per scrivere query che estraggono dati dalla fonte

#### Configurazione della dashboard per il visit-counter

1. Accedi a Grafana (http://localhost:3001) usando le credenziali admin/admin
2. Configura la data source Prometheus:
   - Vai su Configuration > Data Sources > Add data source
   - Seleziona Prometheus
   - Imposta l'URL su `http://prometheus:9090` (usa il nome del servizio nel network Docker)
   - Clicca su "Save & Test"

3. Crea una nuova dashboard:
   - Clicca su "Create" > "Dashboard"
   - Aggiungi un nuovo pannello cliccando su "Add panel"
   - Seleziona "Prometheus" come fonte dati
   - Nella query inserisci `visits_total`
   - Personalizza il titolo, la legenda e altre impostazioni di visualizzazione
   - Salva il pannello e la dashboard

#### Pannelli consigliati per la dashboard

1. **Contatore di visite totali**:
   - Visualizzazione: Stat
   - Query: `visits_total`
   - Opzioni: Mostra l'ultimo valore

2. **Grafico visite nel tempo**:
   - Visualizzazione: Time series
   - Query: `visits_total`
   - Opzioni: Mostra il trend di crescita

3. **Tasso di visite al minuto**:
   - Visualizzazione: Time series o Gauge
   - Query: `rate(visits_total[1m])`
   - Opzioni: Imposta una soglia di colore per evidenziare valori alti/bassi

4. **Visite nell'ultima ora**:
   - Visualizzazione: Stat
   - Query: `increase(visits_total[1h])`
   - Opzioni: Mostra l'ultimo valore con trend

#### Funzionalità avanzate di Grafana

- **Templating**: Usa variabili per creare dashboard dinamiche
- **Alerting**: Configura avvisi basati su condizioni delle metriche
- **Annotations**: Aggiungi eventi temporali ai grafici
- **Panel linking**: Collega pannelli per navigazione dettagliata
- **Plugins**: Estendi Grafana con visualizzazioni e integrazioni aggiuntive

## Best Practices per il monitoraggio

1. **Pensare alle metriche fin dalla progettazione**: Identificare KPI importanti per l'applicazione
2. **Usare etichette/attributi significativi**: Permette di filtrare e aggregare i dati in modo efficace
3. **Definire soglie di alerting**: Creare avvisi per condizioni anomale (es. picchi improvvisi di visite)
4. **Mantenere dashboard minimaliste**: Concentrarsi su metriche veramente utili
5. **Impostare retention dei dati appropriata**: Bilanciare precisione storica e utilizzo di risorse

## Risoluzione dei problemi comuni

### OpenTelemetry
- **Metriche non visibili**: Verificare la configurazione dell'esportatore e che il server sia avviato
- **Errori di tipo**: Assicurarsi di utilizzare versioni compatibili delle librerie OpenTelemetry

### Prometheus
- **Target in stato "down"**: Controllare connettività di rete tra i container e correttezza dell'URL
- **Metriche mancanti**: Verificare che l'endpoint `/metrics` sia accessibile e che restituisca dati

### Grafana
- **Datasource non raggiungibile**: Controllare l'URL del datasource (usare nomi di servizio nel network Docker)
- **Grafici vuoti**: Verificare la correttezza delle query PromQL