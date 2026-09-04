# PATM tool: export-csv
#
# Exports a table from the SOURCE connection to a CSV file.
# Reads its run parameters from _patm.config (set in the Run Tool dialog):
#
#   table       (required)  source table name, e.g. "public.demo"
#   output      (optional)  CSV file path, default "export.csv"
#   batch_size  (optional)  rows per fetch batch, default 1000
#
# Only reads from the source; never writes to any server.

import csv
import os

DEFAULT_BATCH = 1000


def require_table():
    table = _patm.config.get("table")
    if not table:
        raise RuntimeError(
            "missing run parameter 'table' (e.g. {\"table\": \"demo\"})")
    return table


def main():
    table = require_table()
    output = _patm.config.get("output") or "export.csv"
    try:
        batch = int(_patm.config.get("batch_size", DEFAULT_BATCH))
        if batch <= 0:
            raise ValueError
    except (TypeError, ValueError):
        raise RuntimeError("run parameter 'batch_size' must be a "
                           "positive integer")

    known = _patm.source_tables()
    short = table.split(".")[-1]
    if table not in known and short in known:
        table = short
    elif table not in known and short not in known:
        _patm.log("warning: '%s' is not in the source base-table list"
                  % table)

    total = 0
    offset = 0
    with open(output, "w", newline="") as fh:
        writer = csv.writer(fh)
        while True:
            sql = "SELECT * FROM %s LIMIT %d OFFSET %d" % (table, batch,
                                                           offset)
            rows = _patm.source_query(sql)
            if not rows:
                break
            for row in rows:
                writer.writerow(["" if c is None else c for c in row])
            total += len(rows)
            offset += batch

    size = os.path.getsize(output) if os.path.exists(output) else 0
    _patm.log("exported %d row(s) to %s (%d bytes)"
              % (total, output, size))


main()
