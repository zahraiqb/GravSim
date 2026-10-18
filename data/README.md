# Initial conditions

Built-in scenarios use J2000 mean orbital elements from E. M. Standish, *Keplerian Elements for
Approximate Positions of the Major Planets* (JPL Solar System Dynamics, Table 1), converted to
state vectors by `state_from_elements`. Accuracy is ~arcminutes over centuries: fine for
visualisation and dynamics studies, **not** an ephemeris.

## Using real JPL Horizons vectors
1. Go to https://ssd.jpl.nasa.gov/horizons/app.html, choose *Vector Table*, target = each body,
   centre = `@0` (solar-system barycentre), reference plane = ecliptic, units = AU and AU/day,
   same epoch for all bodies. Note the epoch and the GM values Horizons reports.
2. Write a CSV (see `example_two_body.csv`): `name,kind,mass_msun,x,y,z,vx,vy,vz`
3. Run `gravsim --csv your.csv --years 50 --method dopri5 --out out.csv`.

Record the epoch, source and retrieval date next to every dataset you commit.
