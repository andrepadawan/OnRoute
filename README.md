<p align="center">
  <img src="branding/onroute_logo_dark_svg.svg" alt="OnRoute" width="280">
</p>

Live GPS tracking for event shuttles. A Raspberry Pi with a GPS module posts its coordinates to a FastAPI server,
which shows them on an embeddable Leaflet map. An authenticated admin panel sets the hours during which the service
is active and the points of interest shown on the map — outside those hours the `/coordinates` endpoint returns
`notactive`, so the device can stay on without leaking the location.

Italian version: [README_IT.md](README_IT.md).

## Stack

Raspberry Pi (Zero 2 W) + gpsd, Python 3.9, FastAPI, Pydantic v2, Jinja2, Leaflet 1.9. 
Deployed on PythonAnywhere.
HTTP Basic auth for the admin, bearer token for the device.

## Layout

```
RPiGPS/      # device-side: gpsd reader + HTTP poster
Server/      # FastAPI app, JSON persistence, admin templates + JS
branding/    # logos
```

## Running it

Server, locally:

```bash
python -m venv .venv && source .venv/bin/activate
pip install -r requirements.txt
cp Server/.env.example Server/.env   # fill in values
cd Server && uvicorn api:app --reload
```

You'll need to compile: `ADMIN_USERNAME`, `ADMIN_PASSWORD`, `DEVICE_TOKEN`, and `ENV` (`development` exposes `/docs`).
Admin panel is at `/admin`, public map at `/embed`.

Device:

```bash
cd RPiGPS
cp .env.example .env   # set URL_SITO_GPS + DEVICE_TOKEN
python networking.py
```

On macOS there's no `gpsd`, so if you set `ENV=development` the device reads from `mock_gps_coordinates.txt` instead.

## Status

Work in progress. What's next, roughly in order:

- cursor on map showing shuttle’s position (needs gps orientation)
- modify-POI endpoint and a bit of UI polish (ugly af rn)
- ruff + PEP 8 cleanup (some method names are camelCase from early commits)
- LED status on the Pi (service active / idle)
- small test suite for `ScheduleManager` and `MapManager`
- android app

## License

MIT.

Made by [andrepadawan](https://github.com/andrepadawan), a bachelor Computer Engineering student at Politecnico di Torino.