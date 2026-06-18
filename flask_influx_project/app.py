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


def _parse_float(val):
    if val is None or val == "":
        return None
    try:
        return float(val)
    except (TypeError, ValueError):
        return None


def _parse_int(val):
    if val is None or val == "":
        return None
    try:
        return int(float(val))
    except (TypeError, ValueError):
        return None


def _resolve_timestamp(data):
    ts = _parse_int(data.get("timestamp"))
    if ts and ts > 0:
        return datetime.fromtimestamp(ts, tz=timezone.utc)
    return datetime.now(timezone.utc)


@app.get("/healthz")
def healthz():
    return "OK", 200


@app.post("/update_data")
def update_data():
    data = {}
    if request.is_json:
        data = request.get_json(silent=True) or {}
    data.update(request.form.to_dict())

    temperature = _parse_float(data.get("temperature"))
    pressure    = _parse_float(data.get("pressure"))      # hPa (come inviato dall'Arduino)
    altitude    = _parse_float(data.get("altitude"))
    humidity    = _parse_float(data.get("humidity"))
    fint_src    = _parse_int(data.get("fintSrc")) or 0
    lightning_dist = _parse_int(data.get("flightningDistKm")) or 0
    lightning_energy = _parse_int(data.get("flightningEnergyVal")) or 0

    ts_dt = _resolve_timestamp(data)
    ts_iso = ts_dt.isoformat().replace("+00:00", "Z")

    print(
        f"[update_data] ts={ts_iso} T={temperature} P={pressure} A={altitude} H={humidity} "
        f"fintSrc={fint_src} flightningDistKm={lightning_dist} flightningEnergyVal={lightning_energy}",
        flush=True,
    )

    line = (
        f"{ts_iso},{temperature},{pressure},{altitude},{humidity},"
        f"{fint_src},{lightning_dist},{lightning_energy}\n"
    )
    with _lock:
        with open(OUTPUT_FILE, "a", encoding="utf-8") as f:
            f.write(line)

    if write_api is not None:
        try:
            p = Point("sensor").tag("source", "arduino")
            if temperature is not None:
                p = p.field("temperature", temperature)
            if pressure is not None:
                p = p.field("pressure", pressure)
            if altitude is not None:
                p = p.field("altitude", altitude)
            if humidity is not None:
                p = p.field("humidity", humidity)
            p = (
                p.field("fint_src", fint_src)
                .field("lightning_dist_reg", lightning_dist)
                .field("lightning_energy", lightning_energy)
                .time(ts_dt)
            )
            write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=p)
        except Exception as e:
            print(f"[influx] write failed: {e}", flush=True)

    return "Dati ricevuti con successo!", 200


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=4999)
