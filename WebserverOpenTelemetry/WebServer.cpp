// WebServer.cpp
// Un semplice web server C++ che conta le visite e offre metriche per Prometheus.
// Include una integrazione minimale (custom e non ufficiale) con OpenTelemetry
// per mostrare il concetto di Tracing e Metriche.
// Utilizza la libreria header-only cpp-httplib per la gestione del server HTTP.

#include <iostream> // Per input/output su console (std::cout)
#include <string>   // Per la gestione delle stringhe (std::string)
#include <atomic>   // Per contatori atomici thread-safe (std::atomic)
#include <thread>   // Per la gestione dei thread (anche se cpp-httplib gestisce i suoi)
#include <mutex>    // Per la protezione delle risorse condivise (std::mutex, std::lock_guard)
#include <chrono>   // Per misurare il tempo (std::chrono) - utile per durate degli span
#include <ctime>    // Per ottenere il tempo corrente per i log (std::time, std::localtime)
#include <sstream>  // Per costruire stringhe dinamicamente (std::stringstream) - utile per HTML e metriche
#include <map>      // Per memorizzare contatori per percorso (std::map) - mantiene l'ordine per percorso
#include <iomanip>  // Per formattare l'output del tempo (std::put_time)
#include <unordered_map> // Per memorizzare i valori delle metriche OTEL con label (std::unordered_map) - accesso più veloce
#include <vector>   // Per collezioni dinamiche, es. attributi span (std::vector)
#include <memory>   // Per la gestione smart dei puntatori (std::unique_ptr)

// Includiamo la libreria web server header-only.
// Assicurati che il file "httplib.h" sia nel percorso di inclusione del tuo compilatore.
// Link utile: https://github.com/yhirose/cpp-httplib
#include "httplib.h"

// --- Implementazione Minimale di OpenTelemetry ---
// NOTA: Questa è una implementazione estremamente semplificata e *custom*
// del concetto di OpenTelemetry. NON è la libreria ufficiale OpenTelemetry C++.
// Serve solo a dimostrare i concetti di Span e Metriche logging le informazioni su console.
// In una vera applicazione, si userebbe l'SDK ufficiale che esporta i dati a un Collector.
// Link utile (SDK ufficiale OpenTelemetry C++): https://opentelemetry.io/docs/instrumentation/cpp/

namespace otel {

    // Rappresenta il contesto di una traccia e di uno span.
    // Identifica in modo univoco uno span e la traccia a cui appartiene.
    class SpanContext {
    public:
        std::string trace_id; // Identificatore unico della traccia
        std::string span_id;  // Identificatore unico dello span all'interno della traccia

        SpanContext() {
            // Generazione IDs casuali (semplice, non adatta a produzione)
            // Gli IDs OpenTelemetry sono tipicamente generati in modo distribuito e sicuro.
            char trace_id_buffer[33]; // 32 caratteri esadecimali + null terminator
            char span_id_buffer[17];  // 16 caratteri esadecimali + null terminator
            // %032x formatta un intero come esadecimale con leading zeros fino a 32 caratteri
            std::snprintf(trace_id_buffer, sizeof(trace_id_buffer), "%032x", std::rand());
            std::snprintf(span_id_buffer, sizeof(span_id_buffer), "%016x", std::rand());
            trace_id = trace_id_buffer;
            span_id = span_id_buffer;
        }
    };

    // Rappresenta un attributo chiave-valore associato a uno span o una metrica.
    struct Attribute {
        std::string key;
        std::string value;

        // Costruttori per diversi tipi di valore
        Attribute(const std::string& k, const std::string& v) : key(k), value(v) {}
        Attribute(const std::string& k, int v) : key(k), value(std::to_string(v)) {}
        Attribute(const std::string& k, long v) : key(k), value(std::to_string(v)) {}
        Attribute(const std::string& k, double v) : key(k), value(std::to_string(v)) {}
    };

    // Rappresenta uno Span, un'unità di lavoro discreta in una traccia.
    // Ha un nome, un contesto, un tempo di inizio/fine e attributi.
    class Span {
    private:
        std::string name;       // Nome dello span (es. "handle_request", "database_query")
        SpanContext context;    // Contesto di traccia/span
        std::chrono::time_point<std::chrono::high_resolution_clock> start_time; // Tempo di inizio dello span
        std::vector<Attribute> attributes; // Attributi associati allo span
        bool ended = false;     // Flag per assicurarsi che End() sia chiamato una sola volta

    public:
        // Costruttore: Inizia lo span registrando il tempo corrente.
        Span(const std::string& n) : name(n), start_time(std::chrono::high_resolution_clock::now()) {}

        // Metodi per aggiungere attributi allo span.
        void SetAttribute(const std::string& key, const std::string& value) {
            attributes.push_back(Attribute(key, value));
        }

        void SetAttribute(const std::string& key, int value) {
            attributes.push_back(Attribute(key, value));
        }

        void SetAttribute(const std::string& key, long value) {
            attributes.push_back(Attribute(key, value));
        }

        void SetAttribute(const std::string& key, double value) {
            attributes.push_back(Attribute(key, value));
        }

        // Termina lo span: calcola la durata e "esporta" (in questo caso, logga) le informazioni.
        void End() {
            if (ended) return; // Evita doppie chiamate a End

            auto end_time = std::chrono::high_resolution_clock::now();
            // Calcola la durata in millisecondi
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

            // Log delle informazioni dello span su console.
            // In una vera integrazione, questi dati verrebbero inviati a un OpenTelemetry Collector.
            std::cout << "[OTEL] Span: " << name
                << ", TraceID: " << context.trace_id
                << ", SpanID: " << context.span_id
                << ", Duration: " << duration << "ms" << std::endl;

            std::cout << "[OTEL] Attributes: ";
            for (const auto& attr : attributes) {
                std::cout << attr.key << "=" << attr.value << " ";
            }
            std::cout << std::endl;

            ended = true;
        }

        // Distruttore: Assicura che End() venga chiamato automaticamente
        // quando lo span esce dallo scope (es. alla fine di una funzione o handler).
        // Questo implementa il pattern RAII (Resource Acquisition Is Initialization).
        ~Span() {
            if (!ended) {
                End();
            }
        }
    };

    // Rappresenta una Metrica (contatore) in stile OpenTelemetry minimale.
    // Supporta label per distinguere valori diversi della stessa metrica.
    class Metric {
    private:
        std::string name;        // Nome della metrica (es. "request_count")
        std::string description; // Descrizione della metrica
        // Memorizza i valori della metrica associati a diverse combinazioni di label.
        // Le label vengono concatenate in una singola stringa chiave per l'unordered_map.
        std::unordered_map<std::string, int64_t> values;
        std::mutex mutex;        // Mutex per proteggere l'accesso concorrente alla mappa values

    public:
        // Costruttore: Inizializza nome e descrizione della metrica.
        Metric(const std::string& n, const std::string& desc) : name(n), description(desc) {}

        // Aggiunge un valore al contatore, opzionalmente con label.
        // Utilizza un mutex per garantire thread-safety.
        void Add(int64_t value, const std::unordered_map<std::string, std::string>& labels = {}) {
            std::lock_guard<std::mutex> lock(mutex); // Blocca il mutex per accesso esclusivo

            // Costruisce una chiave stringa unica basata sulle label fornite.
            // L'ordine delle label nella chiave influenza se le metriche vengono raggruppate correttamente
            // se si usassero unordered_map all'esterno. Usiamo un map per l'output Prometheus
            // per garantire un ordine coerente delle label, ma l'unordered_map interna è per l'aggiunta.
            std::string label_key = "";
            // Ordina le label per garantire una chiave consistente
            std::map<std::string, std::string> sorted_labels(labels.begin(), labels.end());
            for (const auto& label : sorted_labels) {
                // Formato chiave: key1:value1;key2:value2;
                label_key += label.first + ":" + label.second + ";";
            }

            // Incrementa il valore per la combinazione di label specificata.
            values[label_key] += value;
        }

        // Restituisce le metriche in formato testo compatibile con Prometheus.
        // Link utile: https://prometheus.io/docs/instrumenting/exposition_formats/
        std::string GetPrometheusFormat() const {
            std::stringstream ss;

            // Linee HELP e TYPE per la metrica
            ss << "# HELP " << name << " " << description << "\n";
            ss << "# TYPE " << name << " counter\n"; // Questa implementazione gestisce solo contatori

            // Output dei valori per ogni combinazione di label
            // Utilizziamo un map temporaneo per ordinare le label per l'output Prometheus
            std::map<std::string, int64_t> ordered_values(values.begin(), values.end());

            for (const auto& pair : ordered_values) {
                ss << name;

                // Aggiunge le label se presenti
                if (!pair.first.empty()) {
                    ss << "{";
                    bool first = true;
                    std::string label_str = pair.first;
                    std::string delimiter = ";";
                    size_t pos = 0;
                    std::string token;

                    // Parsa la chiave delle label (es. "path:/;")
                    while ((pos = label_str.find(delimiter)) != std::string::npos) {
                        token = label_str.substr(0, pos);
                        if (!token.empty()) {
                            size_t colon_pos = token.find(":");
                            if (colon_pos != std::string::npos) {
                                std::string key = token.substr(0, colon_pos);
                                std::string value = token.substr(colon_pos + 1);

                                if (!first) {
                                    ss << ",";
                                }
                                // Formato label Prometheus: key="value"
                                ss << key << "=\"" << value << "\"";
                                first = false;
                            }
                        }
                        label_str.erase(0, pos + delimiter.length());
                    }
                    ss << "}";
                }

                // Aggiunge il valore della metrica
                ss << " " << pair.second << "\n";
            }

            return ss.str();
        }
    };

    // Registry Singleton per gestire tutte le metriche OpenTelemetry (minimali).
    // Assicura che ci sia una singola istanza del registry per creare e accedere alle metriche.
    class MetricsRegistry {
    private:
        // Mappa che memorizza le metriche per nome.
        // std::unique_ptr assicura la corretta gestione della memoria.
        std::unordered_map<std::string, std::unique_ptr<Metric>> metrics;
        std::mutex mutex; // Mutex per proteggere l'accesso concorrente alla mappa metrics

        // Costruttore privato per impedire l'instanziazione diretta (Singleton).
        MetricsRegistry() = default;

    public:
        // Cancella costruttori di copia e assegnazione per garantire che sia Singleton.
        MetricsRegistry(const MetricsRegistry&) = delete;
        MetricsRegistry& operator=(const MetricsRegistry&) = delete;
        MetricsRegistry(MetricsRegistry&&) = delete;
        MetricsRegistry& operator=(MetricsRegistry&&) = delete;

        // Metodo statico per ottenere l'unica istanza del registry.
        static MetricsRegistry& Instance() {
            static MetricsRegistry instance; // Creata al primo utilizzo (thread-safe in C++11+)
            return instance;
        }

        // Crea o restituisce un contatore esistente.
        // Thread-safe grazie al mutex.
        Metric* CreateCounter(const std::string& name, const std::string& description) {
            std::lock_guard<std::mutex> lock(mutex); // Blocca il mutex

            // Se la metrica non esiste, la crea.
            if (metrics.find(name) == metrics.end()) {
                metrics[name] = std::make_unique<Metric>(name, description);
            }

            // Restituisce un puntatore alla metrica.
            return metrics[name].get();
        }

        // Restituisce le metriche di tutte le metriche registrate in formato Prometheus.
        std::string GetAllMetrics() const {
            std::stringstream ss;
            // Nota: L'accesso a 'metrics' qui non necessita di lock perché questo metodo
            // viene chiamato solo da getPrometheusMetrics() che ha già un lock sul VisitCounter,
            // e l'aggiunta di nuove metriche avviene solo in CreateCounter che ha il proprio lock.
            // Tuttavia, in un sistema più complesso con letture/scritture più libere, un lock qui sarebbe necessario.

            for (const auto& metric_pair : metrics) {
                // Chiama GetPrometheusFormat() per ogni metrica registrata
                ss << metric_pair.second->GetPrometheusFormat() << "\n";
            }

            return ss.str();
        }
    };
} // namespace otel

// --- Classe per il conteggio delle visite ---
// Questa classe gestisce il conteggio delle visite totali e per specifici percorsi.
// È thread-safe per l'utilizzo in un server multi-thread.
class VisitCounter {
private:
    // Contatore totale delle visite. Usiamo std::atomic per thread-safety senza mutex aggiuntivi
    // per operazioni semplici di incremento/lettura.
    std::atomic<int> total_counter{ 0 };

    // Mutex per proteggere l'accesso alla mappa path_counters,
    // poiché le operazioni sulla mappa (inserimento, accesso) non sono atomiche.
    std::mutex path_mutex;
    // Mappa che memorizza il numero di visite per ogni percorso visitato.
    std::map<std::string, int> path_counters; // std::map mantiene i percorsi ordinati

    // Riferimenti ai contatori OpenTelemetry gestiti dal registry.
    // Questi puntatori NON sono gestiti da questa classe (non li creiamo con new/delete),
    // ma li otteniamo dal MetricsRegistry singleton.
    otel::Metric* visit_counter; // Contatore OTEL per le visite totali
    std::map<std::string, otel::Metric*> path_metrics; // Mappa per i contatori OTEL per percorso

public:
    // Costruttore: Inizializza la classe e registra il contatore totale delle visite
    // nel registry OpenTelemetry minimale.
    VisitCounter() {
        // Ottiene l'istanza unica del MetricsRegistry e crea (o ottiene) il contatore totale.
        // Questo viene fatto una volta sola all'avvio del server.
        visit_counter = otel::MetricsRegistry::Instance().CreateCounter(
            "otel_visit_counter_total", "Numero totale di visite al server (OTEL)");
    }

    // Incrementa il contatore totale delle visite in modo thread-safe.
    // Restituisce il valore DOPO l'incremento.
    int incrementTotal() {
        int count = ++total_counter; // Operazione atomica di pre-incremento

        // Aggiorna il contatore OpenTelemetry corrispondente.
        // Add(1) è thread-safe internamente alla classe Metric.
        visit_counter->Add(1);

        return count;
    }

    // Incrementa il contatore delle visite per un percorso specifico in modo thread-safe.
    void incrementPath(const std::string& path) {
        std::lock_guard<std::mutex> lock(path_mutex); // Blocca il mutex prima di accedere alla mappa

        path_counters[path]++; // Incrementa il contatore per il percorso specifico

        // Logica per aggiornare o creare un contatore OpenTelemetry per il percorso.
        // Costruisce un nome metrico "pulito" dal percorso.
        std::string metric_name = "otel_path_" + path.substr(1) + "_visits";
        if (path == "/") {
            metric_name = "otel_path_root_visits";
        }
        else {
            // Sostituisci '/' con '_' per nomi metrici validi Prometheus/OTEL
            for (char& c : metric_name) {
                if (c == '/') c = '_';
            }
        }


        // Se non esiste già un contatore OTEL per questo percorso, lo crea.
        if (path_metrics.find(path) == path_metrics.end()) {
            std::string description = "Visite al percorso " + path + " (OTEL)";
            // Crea (o ottiene) il contatore OTEL specifico per questo percorso
            path_metrics[path] = otel::MetricsRegistry::Instance().CreateCounter(metric_name, description);
        }

        // Aggiunge 1 al contatore OTEL per questo percorso, usando il percorso come label.
        path_metrics[path]->Add(1, { {"path", path} });
    }

    // Restituisce il contatore totale delle visite in modo thread-safe.
    int getTotal() const {
        return total_counter.load(); // Operazione atomica di lettura
    }

    // Restituisce una copia della mappa dei contatori per percorso.
    // La copia è necessaria per evitare modifiche alla mappa originale mentre viene letta.
    // Viene usato il lock per garantire una copia consistente.
    std::map<std::string, int> getPathCounters() {
        std::lock_guard<std::mutex> lock(path_mutex); // Blocca il mutex durante la copia
        return path_counters;
    }

    // Genera le metriche in formato testo compatibile con Prometheus.
    // Include sia i contatori "nativi" che quelli gestiti tramite l'OTEL Registry minimale.
    // Protegge l'accesso ai contatori con il mutex path_mutex (che copre anche total_counter
    // indirettamente per questo scopo specifico).
    std::string getPrometheusMetrics() {
        std::stringstream ss;
        std::lock_guard<std::mutex> lock(path_mutex); // Blocca il mutex per leggere i contatori

        // --- Metriche "Native" (generate direttamente qui) ---

        // Metrica totale visite
        ss << "# HELP visit_counter_total Numero totale di visite al server\n";
        ss << "# TYPE visit_counter_total counter\n";
        ss << "visit_counter_total " << total_counter << "\n\n"; // Legge il valore atomico

        // Metriche per percorso (dalla mappa path_counters)
        ss << "# HELP path_visits_total Numero di visite per percorso\n";
        ss << "# TYPE path_visits_total counter\n";
        for (const auto& pair : path_counters) {
            // Formato con label: metric_name{label_key="label_value"} value
            ss << "path_visits_total{path=\"" << pair.first << "\"} " << pair.second << "\n";
        }

        // --- Metriche OpenTelemetry (dal Registry minimale) ---
        // Aggiunge l'output delle metriche registrate tramite l'OTEL Registry.
        // Queste sono già formattate in stile Prometheus dalla classe Metric.
        ss << "\n" << otel::MetricsRegistry::Instance().GetAllMetrics();

        return ss.str();
    }
};

// --- Funzione principale ---
int main() {
    // Inizializzazione del seed per la generazione di numeri casuali.
    // Usato per generare gli ID di span e traccia nella implementazione OTEL minimale.
    // È importante farlo una volta sola all'avvio del programma.
    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    // Inizializzazione del server HTTP con la libreria httplib.
    httplib::Server server;
    const int PORT = 8080; // Porta su cui il server ascolterà

    // Inizializzazione dell'oggetto VisitCounter per tracciare le visite.
    VisitCounter counter;

    // --- Definizione degli Endpoint ---

    // Endpoint principale ("/")
    server.Get("/", [&](const httplib::Request& req, httplib::Response& res) {
        // Avvia uno span OpenTelemetry minimale per questa richiesta.
        // Lo span terminerà automaticamente quando unique_ptr span esce dallo scope
        // alla fine di questo handler (grazie al distruttore dello Span).
        auto span = std::make_unique<otel::Span>("handle_root_request");

        // Aggiunge attributi standard HTTP allo span per fornire contesto sulla richiesta.
        span->SetAttribute("http.method", "GET");
        span->SetAttribute("http.path", "/");
        span->SetAttribute("http.remote_ip", req.remote_addr); // Indirizzo IP del client

        // Misura il tempo di inizio per calcolare la durata totale della gestione della richiesta.
        // Questa misurazione è separata dalla durata dello span OTEL (anche se spesso coincidono).
        auto start_time = std::chrono::high_resolution_clock::now();

        // Incrementa i contatori delle visite (totale e per il percorso corrente).
        int count = counter.incrementTotal();
        counter.incrementPath("/");

        // Logging della visita su console standard.
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&time), "%F %T") << "] " // Formatta data e ora
            << "Visita da " << req.remote_addr << " al percorso: / "
            << "(Totale visite: " << count << ")" << std::endl;

        // Generazione della risposta HTML per la homepage.
        std::stringstream html;
        html << "<!DOCTYPE html>"
            << "<html><head><title>Contatore Visite</title>"
            << "<meta charset='UTF-8'>"
            // Stili CSS semplici per rendere la pagina leggibile
            << "<style>"
            << "body { font-family: Arial, sans-serif; margin: 40px; line-height: 1.6; }"
            << "h1 { color: #333; }"
            << ".counter { font-size: 2em; font-weight: bold; color: #2c3e50; }"
            << ".links { margin-top: 20px; }"
            << ".links a { margin-right: 15px; color: #3498db; text-decoration: none; }"
            << ".links a:hover { text-decoration: underline; }"
            << "</style></head><body>"
            << "<h1>Web Server C++ con monitoraggio visite</h1>"
            << "<p>Questa pagina &egrave; stata visitata <span class='counter'>" << count << "</span> volte.</p>"
            << "<div class='links'>"
            // Link agli altri endpoint
            << "<a href='/stats'>Visualizza statistiche dettagliate</a> | "
            << "<a href='/metrics'>Metriche Prometheus</a> | "
            << "<a href='/traces'>Info sulle tracce OpenTelemetry</a>" // Link per info tracce
            << "</div></body></html>";

        // Imposta il contenuto della risposta e il tipo MIME.
        res.set_content(html.str(), "text/html; charset=UTF-8");

        // Calcola la durata della gestione della richiesta.
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();

        // Aggiunge attributi relativi alla risposta allo span OTEL.
        span->SetAttribute("http.response_time_ms", duration);
        span->SetAttribute("http.status_code", 200); // Stato HTTP 200 OK per successo

        // Lo span (gestito da unique_ptr) esce dallo scope qui e il suo distruttore chiama End().
        // L'informazione della traccia viene loggata.
        });

    // Endpoint per le statistiche dettagliate ("/stats")
    server.Get("/stats", [&](const httplib::Request& req, httplib::Response& res) {
        // Avvia uno span OpenTelemetry per questa richiesta.
        auto span = std::make_unique<otel::Span>("handle_stats_request");

        // Aggiunge attributi HTTP.
        span->SetAttribute("http.method", "GET");
        span->SetAttribute("http.path", "/stats");
        span->SetAttribute("http.remote_ip", req.remote_addr);

        auto start_time = std::chrono::high_resolution_clock::now();

        // Incrementa i contatori.
        int count = counter.incrementTotal();
        counter.incrementPath("/stats");

        // Logging della visita.
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&time), "%F %T") << "] "
            << "Visita da " << req.remote_addr << " al percorso: /stats "
            << "(Totale visite: " << count << ")" << std::endl;

        // Ottiene i contatori per percorso.
        auto path_counters = counter.getPathCounters(); // Ottiene una copia della mappa

        // Generazione della risposta HTML per le statistiche.
        std::stringstream html;
        html << "<!DOCTYPE html>"
            << "<html><head><title>Statistiche Visite</title>"
            << "<meta charset='UTF-8'>"
            // Stili CSS per la tabella
            << "<style>"
            << "body { font-family: Arial, sans-serif; margin: 40px; line-height: 1.6; }"
            << "h1, h2 { color: #333; }"
            << "table { border-collapse: collapse; width: 100%; margin-top: 20px; }"
            << "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }"
            << "th { background-color: #f2f2f2; }"
            << "tr:nth-child(even) { background-color: #f9f9f9; }" // Riga alternata
            << ".counter { font-size: 1.2em; font-weight: bold; color: #2c3e50; }"
            << ".back-link { margin-top: 20px; }"
            << ".back-link a { color: #3498db; text-decoration: none; }"
            << ".back-link a:hover { text-decoration: underline; }"
            << "</style></head><body>"
            << "<h1>Statistiche Dettagliate</h1>"
            << "<p>Visite totali: <span class='counter'>" << counter.getTotal() << "</span></p>"
            << "<h2>Visite per percorso:</h2>"
            << "<table><tr><th>Percorso</th><th>Visite</th></tr>"; // Intestazione tabella

        // Popola la tabella con i dati dei contatori per percorso.
        for (const auto& pair : path_counters) {
            html << "<tr><td>" << pair.first << "</td><td>" << pair.second << "</td></tr>";
        }

        html << "</table>"
            << "<div class='back-link'><a href='/'>Torna alla home</a></div>" // Link per tornare indietro
            << "</body></html>";

        res.set_content(html.str(), "text/html; charset=UTF-8");

        // Calcola e registra la durata nello span OTEL.
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        span->SetAttribute("http.response_time_ms", duration);
        span->SetAttribute("http.status_code", 200); // Stato HTTP OK
        });

    // Endpoint per le metriche Prometheus ("/metrics")
    server.Get("/metrics", [&](const httplib::Request& req, httplib::Response& res) {
        // Avvia uno span OpenTelemetry per questa richiesta.
        auto span = std::make_unique<otel::Span>("handle_metrics_request");

        // Aggiunge attributi HTTP.
        span->SetAttribute("http.method", "GET");
        span->SetAttribute("http.path", "/metrics");
        span->SetAttribute("http.remote_ip", req.remote_addr);

        auto start_time = std::chrono::high_resolution_clock::now();

        // Incrementa i contatori (anche la visita all'endpoint metrics conta).
        counter.incrementTotal();
        counter.incrementPath("/metrics");

        // Logging della visita.
        auto now = std::chrono::system_clock::now();
        std::time_t time = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << std::put_time(std::localtime(&time), "%F %T") << "] "
            << "Visita da " << req.remote_addr << " al percorso: /metrics"
            << std::endl;

        // Genera e restituisce le metriche in formato Prometheus.
        res.set_content(counter.getPrometheusMetrics(), "text/plain"); // Tipo MIME corretto per metriche

        // Calcola e registra la durata nello span OTEL.
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        span->SetAttribute("http.response_time_ms", duration);
        span->SetAttribute("http.status_code", 200); // Stato HTTP OK
        });

    // Endpoint informativo sulle tracce OpenTelemetry ("/traces")
    // Questo endpoint non mostra le tracce direttamente (dato che vanno su console),
    // ma spiega dove trovarle.
    server.Get("/traces", [&](const httplib::Request& req, httplib::Response& res) {
        // Avvia uno span per questa richiesta (anche questa richiesta viene tracciata).
        auto span = std::make_unique<otel::Span>("handle_traces_request");
        span->SetAttribute("http.method", "GET");
        span->SetAttribute("http.path", "/traces");
        span->SetAttribute("http.remote_ip", req.remote_addr);

        // Incrementa i contatori.
        counter.incrementTotal();
        counter.incrementPath("/traces");

        // Logging della visita (opzionale, già coperto dal logging generale).
        // Potresti aggiungere un log specifico qui se necessario.

        // Generazione della pagina HTML informativa.
        std::stringstream html;
        html << "<!DOCTYPE html>"
            << "<html><head><title>OpenTelemetry Traces Info</title>"
            << "<meta charset='UTF-8'>"
            << "<style>"
            << "body { font-family: Arial, sans-serif; margin: 40px; line-height: 1.6; }"
            << "h1 { color: #333; }"
            << ".note { background-color: #f8f9fa; border-left: 4px solid #4285f4; padding: 15px; margin-top: 20px; }"
            << ".note p { margin: 0 0 10px 0; }" // Spazio tra i paragrafi nella nota
            << ".note p:last-child { margin-bottom: 0; }" // Nessun margine inferiore sull'ultimo paragrafo
            << ".back-link { margin-top: 20px; }"
            << ".back-link a { color: #3498db; text-decoration: none; }"
            << ".back-link a:hover { text-decoration: underline; }"
            << "</style></head><body>"
            << "<h1>OpenTelemetry Traces</h1>"
            << "<div class='note'>"
            << "<p>Questa applicazione include una integrazione minimale di OpenTelemetry Tracing e Metrics.</p>"
            << "<p>A causa della sua implementazione semplice (non usa un OTel Collector reale), le informazioni delle tracce (Span) e delle metriche OpenTelemetry <strong>vengono stampate direttamente sulla console standard (stdout) del server</strong>.</p>"
            << "<p>Se stai eseguendo l'applicazione in un container Docker, puoi visualizzare le tracce usando il comando <code>docker logs [nome-del-container]</code>.</p>"
            << "<p>Cerca le linee che iniziano con <code>[OTEL]</code>.</p>"
            << "</div>"
            << "<div class='back-link'><a href='/'>Torna alla home</a></div>"
            << "</body></html>";

        res.set_content(html.str(), "text/html; charset=UTF-8");

        // Registra lo stato HTTP nello span OTEL.
        span->SetAttribute("http.status_code", 200); // Stato HTTP OK
        });

    // Messaggi informativi all'avvio del server.
    std::cout << "Server avviato sulla porta " << PORT << std::endl;
    std::cout << "Endpoint disponibili:" << std::endl;
    std::cout << "  - http://localhost:" << PORT << "/ (Homepage con contatore totale)" << std::endl;
    std::cout << "  - http://localhost:" << PORT << "/stats (Statistiche dettagliate per percorso)" << std::endl;
    std::cout << "  - http://localhost:" << PORT << "/metrics (Metriche in formato Prometheus)" << std::endl;
    std::cout << "  - http://localhost:" << PORT << "/traces (Info su dove trovare i dati OpenTelemetry)" << std::endl;
    std::cout << "  - OpenTelemetry integrato in modalità minimale (output su console)." << std::endl;

    // Avvia il server in modalità di ascolto.
    // "0.0.0.0" fa sì che il server ascolti su tutte le interfacce di rete disponibili,
    // utile specialmente se eseguito all'interno di un container Docker.
    bool success = server.listen("0.0.0.0", PORT);

    // Se listen restituisce false, significa che c'è stato un errore (es. porta già in uso).
    if (!success) {
        std::cerr << "Errore nell'avvio del server sulla porta " << PORT << ". Assicurati che la porta non sia già in uso." << std::endl;
        return 1; // Indica un errore all'uscita
    }


    return 0; // Indica che il programma è terminato con successo
}