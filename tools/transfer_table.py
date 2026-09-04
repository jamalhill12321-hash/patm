# PATM tool: transfer-table
#
# Copies a table (all columns, all rows) from the SOURCE connection to the
# TARGET connection. Row data moves through _patm.target_insert(), which
# quotes identifiers and literals on the C side per-dialect.
#
# Run parameters (_patm.config):
#
#   source_table  (required)  table name on the source server
#   target_table  (optional)  table name on the target server,
#                             defaults to source_table's short name
#   batch_size    (optional)  rows per insert chunk, default 500
#
# The target table must already exist with a compatible schema.


def main():
    src = _patm.config.get("source_table")
    if not src:
        raise RuntimeError(
            "missing run parameter 'source_table' "
            '(e.g. {"source_table": "demo"})')
    tgt = _patm.config.get("target_table") or src.split(".")[-1]
    try:
        batch = int(_patm.config.get("batch_size", 500))
        if batch <= 0:
            raise ValueError
    except (TypeError, ValueError):
        raise RuntimeError("run parameter 'batch_size' must be a "
                           "positive integer")

    if tgt not in _patm.source_tables():
        _patm.log("note: '%s' not present on source; transferring anyway"
                  % tgt)

    offset = 0
    total = 0
    columns = None
    while True:
        sql = "SELECT * FROM %s LIMIT %d OFFSET %d" % (src, batch, offset)
        rows = _patm.source_query(sql)
        if not rows:
            break
        if columns is None:
            width = len(rows[0])
            columns = ["col%d" % i for i in range(width)]
            _patm.log("detected %d column(s); positional insert" % width)
        _patm.target_insert(tgt, columns, rows)
        total += len(rows)
        offset += batch

    _patm.log("transferred %d row(s) into target table '%s'"
              % (total, tgt))


main()
