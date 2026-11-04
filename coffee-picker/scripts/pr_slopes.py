"""How steep is Puerto Rico's coffee belt? Writes results/pr_slopes.json.

    pip install rasterio
    python scripts/pr_slopes.py

Reads a window of the USGS 3DEP 1/3 arc-second (~10 m) elevation tile over the
western Cordillera Central, from 67 W eastward (Maricao, Lares, Yauco,
Adjuntas, Utuado, Jayuya, Ciales, Orocovis; Las Marias lies in the next tile
west and is left out), straight from the USGS bucket. Takes the slope of every
cell between 250 and 1000 m elevation.

That band is a stand-in for coffee land: it also holds forest and pasture,
and a 10 m grid smooths the banks and terraces a foot actually meets. Farm
boundaries from PROCAFE would replace it.
"""

import json
import os

import numpy as np

TILE = ("/vsicurl/https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/13/TIFF/current/"
        "n19w067/USGS_13_n19w067.tif")
BOUNDS = (-67.0, 18.05, -66.35, 18.35)   # lon/lat: west, south, east, north
BAND_M = (250.0, 1000.0)
OUT = os.path.join(os.path.dirname(__file__), "..", "results", "pr_slopes.json")


def grades(z, res_deg, lat):
    """Slope grade [%] of each cell of a lat/lon elevation grid."""
    dy = res_deg * 111320.0
    dx = res_deg * 111320.0 * np.cos(np.radians(lat))
    gy, gx = np.gradient(z, dy, dx)
    return 100.0 * np.hypot(gx, gy), dx * dy


def main():
    import rasterio
    from rasterio.windows import from_bounds

    with rasterio.open(TILE) as src:
        z = src.read(1, window=from_bounds(*BOUNDS, src.transform)).astype(np.float64)
        res = src.res[0]
    z[z < -1000] = np.nan
    g, cell = grades(z, res, 0.5 * (BOUNDS[1] + BOUNDS[3]))
    band = (z >= BAND_M[0]) & (z <= BAND_M[1]) & np.isfinite(g)
    g = g[band]
    edges = list(range(0, 130, 10))
    hist, _ = np.histogram(np.clip(g, 0, 129.9), bins=edges)
    out = {
        "source": "USGS 3DEP 1/3 arc-second, tile n19w067",
        "bounds_lonlat": BOUNDS, "elevation_band_m": BAND_M,
        "area_km2": round(band.sum() * cell / 1e6),
        "percentiles": {f"p{q}": round(float(np.percentile(g, q)), 1) for q in (10, 25, 50, 75, 90, 95)},
        "share_at_or_below": {str(s): round(float((g <= s).mean()), 4) for s in range(10, 130, 10)},
        "histogram": {"edges": edges, "share": [round(float(h / g.size), 4) for h in hist]},
    }
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "w") as f:
        json.dump(out, f, indent=1)
    print(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
