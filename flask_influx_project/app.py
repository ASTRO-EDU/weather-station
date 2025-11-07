from flask import Flask, request
from datetime import datetime, timezone
import os, threading

from influxdb_client import InfluxDBClient, Point, WriteOptions

app = Flask(__name__)

OUTPUT_FILE = os.environ.get("OUTPUT_FILE", "measurement.txt")
INFLUX_URL   = os.environ.get("INFLUX_URL", "http://influxdb:8086")
INFLUX_TOKEN = os.environ.get("INFLUX_TOKEN", "")
INFLUX_ORG   = os.environ.get("INFLUX_ORG", "")
INFLUX_BUCKET= os.environ.get("INFLUX_BUCKET", os.environ.get("DOCKER_INFLUXDB_INIT_BUCKET", "MONITORING"))

_lock = threading.Lock()

influx_client = None
write_api = None
if INFLUX_URL and INFLUX_TOKEN and INFLUX_ORG and INFLUX_BUCKET:
    influx_client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG, timeout=10_000)
    write_api = influx_client.write_api(write_options=WriteOptions(batch_size=1))

@app.get("/healthz")
def healthz():
    return "OK", 200

@app.post("/update_data")
def update_data():
    data = {}
    if request.is_json:
        data = request.get_json(silent=True) or {}
    data.update(request.form.to_dict())

    valore = data.get("valore")
    temperature = data.get("temperature")
    pressure    = data.get("pressure")
    altitude    = data.get("altitude")
    fintSrc = data.get("fintSrc")
    flightningDistKm = data.get("flightningDistKm")
    flightningEnergyVal = data.get("flightningEnergyVal")

    # --- normalizzazione della pressure ---
    if pressure is not None:
        try:
            pressure = float(pressure) / 100.0
            pressure = str(pressure)   # riconversione in stringa
        except ValueError:
            pressure = None


    ts_dt = datetime.now(timezone.utc)

    ts_iso = ts_dt.isoformat().replace("+00:00","Z")

    print(
        f"[update_data] ts={ts_iso} valore={valore} T={temperature} P={pressure} A={altitude} fsrc={fintSrc} flightningDistKm={flightningDistKm} flightningEnergyVal={flightningEnergyVal}",
        flush=True
    )

    line = f"{ts_iso},{temperature},{pressure},{altitude},{fintSrc},{flightningDistKm},{flightningEnergyVal}\n"
    with _lock:
        with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
            f.write(line)

    if write_api is not None:
        try:
            p = (
                Point("sensor")
                .tag("source", "flask")
                .field("temperature", float(temperature) if temperature is not None else None)
                .field("pressure", float(pressure) if pressure is not None else None)
                .field("altitude", float(altitude) if altitude is not None else None)
                .time(ts_dt)
            )
            write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=p)
        except Exception as e:
            print(f"[influx] write failed: {e}", flush=True)

    return "Dati ricevuti con successo!", 200

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=4999)
