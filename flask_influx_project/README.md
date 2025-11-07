# Flask + InfluxDB Project

Questo progetto avvia un'app Flask che riceve dati via POST e li salva sia in un file `measurement.txt`
sia in InfluxDB (bucket MONITORING).

## Avvio

```bash
docker compose up -d --build
```

Flask: [http://localhost:4999](http://localhost:4999)  
InfluxDB UI: [http://localhost:8096](http://localhost:8096)

## Test

```bash
curl -X POST http://localhost:4999/update_data       -H "Content-Type: application/x-www-form-urlencoded"       --data-urlencode "valore=123"       --data-urlencode "temperature=21.45"       --data-urlencode "pressure=1008.23"       --data-urlencode "altitude=542.10"
```

## Fermare i containers

Per fermare e rimuovere i container avviati da `docker-compose`:

```bash
docker compose down
```

Se vuoi solo fermarli senza rimuovere volumi/rete:

```bash
docker compose stop
```
