# _patm Python Module Reference

This is the API your tool script uses to talk to databases. PATM injects `_patm` automatically — you don't import it.

`_patm` is always available as a global. No import needed.

## Attributes

### _patm.config

A dict holding the run parameters the user entered in the "Run Tool" dialog. Keys and values are strings.

```python
table = _patm.config.get("table")
output = _patm.config.get("output") or "default.csv"
```

If the user hasn't set a parameter, `.get()` returns `None`.

## Functions

### _patm.source_query(sql)

Runs a SELECT on the source connection. Returns a list of tuples, one per row. Each cell is a string, or `None` for SQL NULL.

```python
rows = _patm.source_query("SELECT id, name FROM users")
for row in rows:
    print(row[0], row[1])  # (id, name)
```

Raises `RuntimeError` if the query fails.

### _patm.source_tables()

Returns a list of table names on the source connection (strings).

```python
tables = _patm.source_tables()
if "users" in tables:
    rows = _patm.source_query("SELECT * FROM users")
```

### _patm.target_exec(sql)

Runs DDL or DML on the target connection. No return value. Use this for CREATE TABLE, UPDATE, DELETE, etc.

```python
_patm.target_exec("CREATE TABLE IF NOT EXISTS backup (id INT, name TEXT)")
_patm.target_exec("DELETE FROM old_data WHERE archived = 'yes'")
```

Raises `RuntimeError` if the statement fails.

### _patm.target_insert(table, columns, rows)

Inserts rows into a target table. Handles quoting automatically per-dialect.

- `table` — string, target table name
- `columns` — list of column name strings
- `rows` — list of tuples (one per row)

```python
columns = ["id", "name", "email"]
rows = [
    (1, "Alice", "alice@example.com"),
    (2, "Bob", None),
]
_patm.target_insert("users", columns, rows)
```

Internally batches inserts in chunks of 500 rows. Raises `RuntimeError` or `ValueError` on bad input.

### _patm.log(message)

Writes a line to the tool log shown in the PATM UI. Takes one string argument.

```python
_patm.log("processing table: %s" % table)
_patm.log("done, exported %d rows" % total)
```

## Example: Minimal Tool

```python
# Reads all rows from "source_table" on the source,
# inserts them into "target_table" on the target.

src = _patm.config.get("source_table")
tgt = _patm.config.get("target_table")

if not src or not tgt:
    raise RuntimeError("need source_table and target_table in config")

rows = _patm.source_query("SELECT * FROM %s" % src)
if not rows:
    _patm.log("source table is empty")
else:
    width = len(rows[0])
    columns = ["col%d" % i for i in range(width)]
    _patm.target_insert(tgt, columns, rows)
    _patm.log("copied %d rows" % len(rows))
```

## Notes

- You cannot access database connections directly. All access goes through the functions above.
- Credentials are never exposed to your script.
- `_patm.config` is read-only. You can read values but not modify the config.
- Each tool run is isolated. One execution of your script, one call.
- Standard Python 3 libraries are available (csv, json, os, re, etc.).
