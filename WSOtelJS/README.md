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

### Configurazione di una dashboard Grafana

1. Accedi a Grafana (http://localhost:3001)
2. Vai su Configuration > Data Sources
3. Aggiungi Prometheus come fonte dati (URL: http://prometheus:9090)
4. Crea una nuova dashboard con un grafico che mostri la metrica `visits_total`

## Struttura del progetto

- `server.js` - Il codice dell'applicazione Node.js
- `package.json` - Dipendenze del progetto
- `Dockerfile` - Configurazione per costruire l'immagine Docker
- `docker-compose.yml` - Configurazione per orchestrare i container
- `prometheus.yml` - Configurazione di Prometheus

## Metriche disponibili

La principale metrica esposta è `visits_total`, che rappresenta il numero totale di visite alla pagina principale del server.