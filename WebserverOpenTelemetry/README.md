# WebServerCounter - Web Server C++ con Monitoraggio Avanzato

![License](https://img.shields.io/badge/license-MIT-blue.svg)

Un semplice web server C++ che conta le visite e offre un sistema completo di monitoraggio attraverso Prometheus e una implementazione dimostrativa di OpenTelemetry. Questo progetto è pensato come esempio didattico per illustrare integrazione di metriche e tracing in applicazioni C++ moderne.

## 📑 Indice
- [Caratteristiche](#-caratteristiche)
- [Struttura del Progetto](#-struttura-del-progetto)
- [Requisiti](#-requisiti)
- [Installazione e Avvio](#-installazione-e-avvio)
- [Architettura](#-architettura)
- [Endpoints](#-endpoints)
- [Prometheus](#-prometheus)
- [OpenTelemetry](#-opentelemetry)
- [Docker](#-docker)
- [Guida allo Sviluppo](#-guida-allo-sviluppo)
- [FAQ](#-faq)

## 🚀 Caratteristiche

- Server HTTP leggero basato sulla libreria cpp-httplib
- Conteggio delle visite totali e per percorso
- Endpoints HTML interattivi per visualizzare le statistiche
- Esposizione di metriche in formato Prometheus
- Implementazione minimale di OpenTelemetry per tracciamento e metriche
- Containerizzazione completa con Docker e Docker Compose
- Stack di monitoraggio integrato con Prometheus

## 📂 Struttura del Progetto

```
webservercounter/
├── CMakeLists.txt               # Configurazione CMake
├── CMakePresets.json            # Presets per build su diverse piattaforme
├── WebServer.cpp                # Codice sorgente principale
├── include/                     # Directory per le librerie di terze parti
│   └── httplib.h                # Libreria HTTP header-only (da aggiungere)
├── docker-compose.yml           # Configurazione Docker Compose
├── prometheus.yml               # Configurazione Prometheus
└── otel-collector-config.yaml   # Configurazione OpenTelemetry Collector (opzionale)
```

## 📋 Requisiti

Per build manuale:
- C++17 compiler (GCC 7+, Clang 5+, MSVC 19.14+)
- CMake 3.10 o superiore
- pthread library (per Linux)
- [cpp-httplib](https://github.com/yhirose/cpp-httplib) (header-only)

Per Docker:
- Docker Engine
- Docker Compose

## 🛠️ Installazione e Avvio

### Utilizzo con Docker (raccomandato)

1. Clonare il repository:
   ```bash
   git clone https://github.com/yourusername/webservercounter.git
   cd webservercounter
   ```

2. Scaricare la libreria cpp-httplib:
   ```bash
   mkdir -p include
   curl -o include/httplib.h https://raw.githubusercontent.com/yhirose/cpp-httplib/master/httplib.h
   ```

3. Avviare con Docker Compose:
   ```bash
   docker-compose up --build
   ```

4. Accedere al server:
   - Web Server: http://localhost:8080
   - Prometheus: http://localhost:9090

### Build Manuale

1. Clonare il repository e aggiungere la libreria come sopra.

2. Configurare e compilare con CMake:
   ```bash
   mkdir build && cd build
   cmake ..
   cmake --build .
   ```

3. Eseguire il server:
   ```bash
   ./webserver
   ```

## 🏗️ Architettura

Il sistema è composto da tre componenti principali:

1. **Web Server C++**: gestisce le richieste HTTP, traccia le visite e espone metriche.
2. **Prometheus**: raccoglie e memorizza le metriche per analisi e visualizzazione.
3. **OpenTelemetry (implementazione demo)**: fornisce tracciamento distribuito.

```
┌─────────────────┐     ┌───────────────┐
│                 │     │               │
│   Web Server    │────▶│   Prometheus  │
│    (C++)        │     │               │
│                 │     │               │
└─────────────────┘     └───────────────┘
         │                     │
         │                     │
         ▼                     ▼
┌─────────────────────────────────────┐
│           Visualizzazione           │
│      (Prometheus Web UI)            │
└─────────────────────────────────────┘
```

## 📌 Endpoints

Il server espone i seguenti endpoints:

- **`/`** - Home page con contatore totale visite
- **`/stats`** - Statistiche dettagliate per percorso
- **`/metrics`** - Metriche in formato Prometheus
- **`/traces`** - Informazioni sulle tracce OpenTelemetry

## 📊 Prometheus

### Concetti Chiave

Prometheus è un sistema di monitoraggio e alerting open-source progettato per affidabilità e scalabilità. Funziona con un modello di raccolta pull-based e una potente query language (PromQL).

### Configurazione

Il file `prometheus.yml` configura Prometheus per raccogliere metriche dal nostro server:

```yaml
global:
  scrape_interval: 15s  # Intervallo di raccolta dati

scrape_configs:
  - job_name: 'webserver'
    static_configs:
      - targets: ['webserver:8080']
    metrics_path: /metrics
```

### Metriche Esposte

Il server espone le seguenti metriche principali:

1. **`visit_counter_total`**: Contatore totale delle visite al server
2. **`path_visits_total{path="..."}`**: Visite per specifico percorso
3. **`otel_visit_counter_total`**: Contatore totale (versione OpenTelemetry)
4. **`otel_path_*_visits`**: Metriche specifiche per percorso (OpenTelemetry)

### Utilizzo in Prometheus

1. Accedere all'interfaccia web di Prometheus: http://localhost:9090
2. Esempi di query:
   - `visit_counter_total` - Visualizza il contatore totale
   - `rate(visit_counter_total[1m])` - Tasso di visite al minuto
   - `path_visits_total{path="/stats"}` - Visite alla pagina stats

## 📡 OpenTelemetry

### Concetti Chiave

OpenTelemetry (OTEL) è un framework standard per l'osservabilità delle applicazioni che comprende:

- **Traces**: registrazione di eventi collegati che seguono una richiesta attraverso il sistema
- **Metrics**: misurazioni quantitative dello stato del sistema
- **Logs**: registrazioni di eventi discreti

### Implementazione Minima

Questo progetto include una implementazione minimale di OpenTelemetry per scopi didattici:

- `otel::Span`: rappresenta un'unità di lavoro discreta
- `otel::Metric`: traccia contatori con etichette
- `otel::MetricsRegistry`: gestisce centralmente le metriche

### Esempio di Tracciamento

Ogni richiesta HTTP viene tracciata tramite uno span:

```cpp
auto span = std::make_unique<otel::Span>("handle_root_request");
span->SetAttribute("http.method", "GET");
span->SetAttribute("http.path", "/");
// ... elaborazione della richiesta ...
span->SetAttribute("http.response_time_ms", duration);
```

### Configurazione Collector (Opzionale)

Il file `otel-collector-config.yaml` fornisce una configurazione per un OpenTelemetry Collector completo (non incluso nella configurazione Docker di base).

## 🐳 Docker

### Concetti e Integrazione

Docker viene utilizzato per semplificare il deployment e garantire un ambiente coerente:

1. **Container Web Server**: compila ed esegue l'applicazione C++
2. **Container Prometheus**: raccoglie e memorizza le metriche

### Docker Compose

Il file `docker-compose.yml` orchestra l'intera infrastruttura:

```yaml
version: '3'
services:
  webserver:
    build: .
    ports:
      - "8080:8080"
    restart: unless-stopped
    networks:
      - monitoring

  prometheus:
    image: prom/prometheus:latest
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml
    ports:
      - "9090:9090"
    restart: unless-stopped
    networks:
      - monitoring

networks:
  monitoring:
    driver: bridge
```


### Aggiungere Nuove Metriche

Per creare una nuova metrica personalizzata:

```cpp
// Nel costruttore o all'avvio
my_custom_metric = otel::MetricsRegistry::Instance().CreateCounter(
    "my_custom_metric", "Descrizione della metrica");

// Dove necessario per incrementare la metrica
my_custom_metric->Add(1, {{"label_key", "label_value"}});
```

## Scelte implementative

**Q: Che librerie esterne sono utilizzate?**  
A: Il progetto usa principalmente `cpp-httplib` come libreria HTTP header-only. Le funzionalità di OpenTelemetry sono implementate nativamente per scopi didattici.

**Q: Come posso visualizzare le tracce?**  
A: In questa implementazione minimale, le tracce vengono stampate su console. Negli stdout dei container Docker o nel terminale se eseguito manualmente.

**Q: Posso usare questo in produzione?**  
A: Questo è un progetto dimostrativo. Per un uso in produzione consigliamo:
   - Implementare l'SDK ufficiale OpenTelemetry C++
   - Aggiungere gestione degli errori robusta
   - Configurare TLS/HTTPS
   - Implementare rate limiting e sicurezza aggiuntiva


Creato come progetto dimostrativo per l'integrazione di metriche e tracing in applicazioni C++ moderne.
