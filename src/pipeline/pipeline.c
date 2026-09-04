/*
 * This file is part of PATM.
 *
 * PATM (Pipeline Automation Tool Manager) is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * PATM is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PATM. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdio.h>

#include "core/log.h"
#include "db/db_internal.h"
#include "pipeline.h"

/*
 * _patm module — the only way tools talk to databases.
 * No credentials, no raw handles exposed.
 */

static const PatmPipelineCtx *g_ctx = NULL;
static PatmStrBuf *g_log = NULL;

static PyObject *py_log(PyObject *self, PyObject *args)
{
    const char *msg;

    (void)self;
    if (!PyArg_ParseTuple(args, "s", &msg))
        return NULL;
    if (g_log)
        patm_strbuf_printf(g_log, "%s\n", msg);
    PATM_LOG_INFO("[tool] %s", msg);
    Py_RETURN_NONE;
}

static PyObject *rows_to_pylist(const PatmResult *res)
{
    PyObject *list = PyList_New((Py_ssize_t)res->nrows);

    if (!list)
        return NULL;
    for (size_t r = 0; r < res->nrows; r++) {
        PyObject *row = PyTuple_New((Py_ssize_t)res->ncols);
        if (!row) {
            Py_DECREF(list);
            return NULL;
        }
        for (size_t c = 0; c < res->ncols; c++) {
            const char *cell = res->cells[r * res->ncols + c];
            PyObject *val =
                cell ? PyUnicode_FromString(cell) : Py_NewRef(Py_None);
            if (!val || PyTuple_SetItem(row, (Py_ssize_t)c, val)) {
                Py_XDECREF(val);
                Py_DECREF(row);
                Py_DECREF(list);
                return NULL;
            }
        }
        PyList_SetItem(list, (Py_ssize_t)r, row); /* steals row */
    }
    return list;
}

static PyObject *py_source_query(PyObject *self, PyObject *args)
{
    const char *sql;
    PatmResult res;
    PatmError err;
    PyObject *out;

    (void)self;
    if (!g_ctx || !g_ctx->source) {
        PyErr_SetString(PyExc_RuntimeError, "no source connection");
        return NULL;
    }
    if (!PyArg_ParseTuple(args, "s", &sql))
        return NULL;

    err = patm_db_query(g_ctx->source, sql, &res);
    if (!patm_is_ok(&err)) {
        PyErr_SetString(PyExc_RuntimeError, err.msg);
        return NULL;
    }
    out = rows_to_pylist(&res);
    patm_db_result_free(&res);
    return out;
}

static PyObject *py_source_tables(PyObject *self, PyObject *args)
{
    PatmStrBuf json = { 0 };
    PatmError err;
    PyObject *list;

    (void)self;
    (void)args;
    if (!g_ctx || !g_ctx->source) {
        PyErr_SetString(PyExc_RuntimeError, "no source connection");
        return NULL;
    }
    err = patm_db_list_tables(g_ctx->source, &json);
    if (!patm_is_ok(&err)) {
        patm_strbuf_free(&json);
        PyErr_SetString(PyExc_RuntimeError, err.msg);
        return NULL;
    }
    /* use Python's json.loads instead of hand-parsing in C */
    PyObject *json_mod = PyImport_ImportModule("json");
    list = NULL;
    if (json_mod) {
        PyObject *fn = PyObject_GetAttrString(json_mod, "loads");
        if (fn) {
            list = PyObject_CallFunction(fn, "s", json.data ? json.data : "[]");
            Py_DECREF(fn);
        }
        Py_DECREF(json_mod);
    }
    patm_strbuf_free(&json);
    return list;
}

static PyObject *py_target_exec(PyObject *self, PyObject *args)
{
    const char *sql;
    PatmError err;

    (void)self;
    if (!g_ctx || !g_ctx->target) {
        PyErr_SetString(PyExc_RuntimeError, "no target connection");
        return NULL;
    }
    if (!PyArg_ParseTuple(args, "s", &sql))
        return NULL;
    err = patm_db_execute(g_ctx->target, sql);
    if (!patm_is_ok(&err)) {
        PyErr_SetString(PyExc_RuntimeError, err.msg);
        return NULL;
    }
    Py_RETURN_NONE;
}

static PyObject *py_target_insert(PyObject *self, PyObject *args)
{
    const char *table;
    PyObject *columns;
    PyObject *rows;
    const PatmDbDriver *drv;
    char ident[1024];
    PatmStrBuf sql = { 0 };
    PatmError err;

    (void)self;
    if (!g_ctx || !g_ctx->target) {
        PyErr_SetString(PyExc_RuntimeError, "no target connection");
        return NULL;
    }
    if (!PyArg_ParseTuple(args, "sOO", &table, &columns, &rows))
        return NULL;
    if (!PyList_Check(columns) || !PyList_Check(rows)) {
        PyErr_SetString(PyExc_TypeError,
                        "target_insert expects (str table, list columns, "
                        "list rows)");
        return NULL;
    }

    drv = g_ctx->target->driver;
    err = drv->quote_ident(table, ident, sizeof(ident));
    if (!patm_is_ok(&err)) {
        PyErr_SetString(PyExc_ValueError, err.msg);
        return NULL;
    }

    Py_ssize_t ncols = PyList_Size(columns);
    if (ncols <= 0) {
        PyErr_SetString(PyExc_ValueError, "empty column list");
        return NULL;
    }

    /* batch INSERT in chunks of 500 */
    err = patm_strbuf_printf(&sql, "INSERT INTO %s (", ident);
    if (!patm_is_ok(&err))
        goto fail;
    for (Py_ssize_t i = 0; i < ncols; i++) {
        PyObject *col = PyList_GetItem(columns, i); /* borrowed */
        if (!col || !PyUnicode_Check(col)) {
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_TypeError, "column names must be str");
            goto fail;
        }
        const char *cstr = PyUnicode_AsUTF8(col);
        if (!cstr)
            goto fail;
        err = drv->quote_ident(cstr, ident, sizeof(ident));
        if (!patm_is_ok(&err)) {
            PyErr_SetString(PyExc_ValueError, err.msg);
            goto fail;
        }
        if (i > 0)
            err = patm_strbuf_append(&sql, ", ");
        if (patm_is_ok(&err))
            err = patm_strbuf_append(&sql, ident);
        if (!patm_is_ok(&err))
            goto fail;
    }
    err = patm_strbuf_append(&sql, ") VALUES ");
    if (!patm_is_ok(&err))
        goto fail;

    Py_ssize_t nrows_total = 0;
    for (Py_ssize_t r = 0; r < PyList_Size(rows); r++) {
        PyObject *row = PyList_GetItem(rows, r); /* borrowed */
        if (!row || !PySequence_Check(row)) {
            if (!PyErr_Occurred())
                PyErr_SetString(PyExc_TypeError, "rows must be sequences");
            goto fail;
        }
        if (PySequence_Size(row) != ncols) {
            PyErr_SetString(PyExc_ValueError,
                            "row width does not match column count");
            goto fail;
        }
        err = patm_strbuf_append(&sql, r == 0 ? "(" : ",(");
        if (!patm_is_ok(&err))
            goto fail;
        for (Py_ssize_t c = 0; c < ncols; c++) {
            PyObject *cell = PySequence_GetItem(row, c); /* new ref */
            if (!cell)
                goto fail;
            if (cell == Py_None) {
                err = patm_strbuf_append(&sql, "NULL");
            } else {
                const char *s = PyUnicode_AsUTF8(PyObject_Str(cell));
                err = s ? drv->quote_literal(&sql, s)
                        : patm_error(PATM_ERR_PYTHON, "cell conversion failed");
                if (!s && !PyErr_Occurred())
                    PyErr_SetString(PyExc_TypeError,
                                    "cells must be str/number/None");
            }
            Py_DECREF(cell);
            if (!patm_is_ok(&err))
                goto fail;
            if (c + 1 < ncols) {
                err = patm_strbuf_append(&sql, ",");
                if (!patm_is_ok(&err))
                    goto fail;
            }
        }
        err = patm_strbuf_append(&sql, ")");
        if (!patm_is_ok(&err))
            goto fail;
        nrows_total++;

        /* Execute in chunks of 500 rows. */
        if (r + 1 == PyList_Size(rows) || (r + 1) % 500 == 0) {
            err = patm_db_execute(g_ctx->target, sql.data);
            if (!patm_is_ok(&err)) {
                PyErr_SetString(PyExc_RuntimeError, err.msg);
                goto fail;
            }
            sql.len = 0;
            if (sql.data)
                sql.data[0] = '\0';
        }
    }

    patm_strbuf_free(&sql);
    return PyLong_FromSsize_t(nrows_total);

fail:
    patm_strbuf_free(&sql);
    return NULL;
}

static PyMethodDef g_patm_methods[] = {
    { "source_query", py_source_query, METH_VARARGS,
      "Run a SELECT on the source connection." },
    { "source_tables", py_source_tables, METH_NOARGS,
      "List source table names." },
    { "target_exec", py_target_exec, METH_VARARGS,
      "Execute DDL/DML on the target connection." },
    { "target_insert", py_target_insert, METH_VARARGS,
      "Insert rows into target table with safe quoting." },
    { "log", py_log, METH_VARARGS, "Write a message to the tool log." },
    { NULL, NULL, 0, NULL }
};

static struct PyModuleDef g_patm_module = {
    PyModuleDef_HEAD_INIT, "_patm", "PATM tool API", -1, g_patm_methods,
    NULL, NULL, NULL, NULL
};

static PyObject *patm_module_init(void)
{
    return PyModule_Create(&g_patm_module);
}

PatmError patm_pipeline_init(void)
{
    if (Py_IsInitialized())
        return patm_ok();
    if (PyImport_AppendInittab("_patm", patm_module_init) != 0)
        return patm_error(PATM_ERR_PYTHON, "failed to register _patm module");
    Py_Initialize();
    return patm_ok();
}

void patm_pipeline_finalize(void)
{
    if (Py_IsInitialized())
        Py_Finalize();
}

PatmError patm_pipeline_run_tool(const char *tool_path,
                                 const PatmPipelineCtx *ctx,
                                 const char *config_json,
                                 PatmStrBuf *log_out)
{
    FILE *f;
    PyObject *globals;
    PyObject *result;
    PyObject *patm_mod = NULL;
    PatmError ret = patm_ok();

    if (!tool_path || !ctx || !log_out)
        return patm_error(PATM_ERR_INVALID_ARG, "run_tool: bad args");

    f = fopen(tool_path, "rb");
    if (!f)
        return patm_error(PATM_ERR_IO, "cannot open tool '%s'", tool_path);

    globals = PyDict_New();
    if (!globals ||
        PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins())) {
        fclose(f);
        Py_XDECREF(globals);
        return patm_error(PATM_ERR_PYTHON, "tool globals setup failed");
    }

    g_ctx = ctx;
    g_log = log_out;

    /* parse config JSON and attach as _patm.config */
    patm_mod = PyImport_ImportModule("_patm");
    if (!patm_mod) {
        fclose(f);
        PyErr_Print();
        Py_DECREF(globals);
        g_ctx = NULL;
        g_log = NULL;
        return patm_error(PATM_ERR_PYTHON,
                          "internal error: _patm module missing");
    }
    {
        PyObject *json_mod = PyImport_ImportModule("json");
        PyObject *empty = NULL;
        PyObject *args;

        if (!config_json || !config_json[0])
            config_json = "{}";
        if (!json_mod) {
            PyErr_Print();
            ret = patm_error(PATM_ERR_PYTHON, "json module unavailable");
            goto done_config;
        }
        args = PyUnicode_FromString(config_json);
        if (!args) {
            ret = patm_error(PATM_ERR_MEMORY, "config copy failed");
            Py_XDECREF(json_mod);
            goto done_config;
        }
        PyObject *loads = PyObject_GetAttrString(json_mod, "loads");
        PyObject *cfg_obj =
            loads ? PyObject_CallOneArg(loads, args) : NULL;
        if (!cfg_obj) {
            PyErr_Clear();
            ret = patm_error(PATM_ERR_INVALID_ARG,
                             "run parameters are not valid JSON");
        } else if (!PyDict_Check(cfg_obj)) {
            ret = patm_error(PATM_ERR_INVALID_ARG,
                             "run parameters must be a JSON object");
        } else if (PyObject_SetAttrString(patm_mod, "config", cfg_obj)) {
            ret = patm_error(PATM_ERR_PYTHON,
                             "failed to attach run parameters");
        }
        empty = NULL; /* clarity: no extra cleanup path */
        (void)empty;
        Py_XDECREF(loads);
        Py_XDECREF(args);
        Py_XDECREF(cfg_obj);
        Py_XDECREF(json_mod);
    }
done_config:
    if (!patm_is_ok(&ret)) {
        fclose(f);
        Py_DECREF(patm_mod);
        Py_DECREF(globals);
        g_ctx = NULL;
        g_log = NULL;
        return ret;
    }

    /* make _patm available as a global so tools don't need to import it */
    if (PyDict_SetItemString(globals, "_patm", patm_mod) != 0) {
        fclose(f);
        PyErr_Print();
        Py_DECREF(patm_mod);
        Py_DECREF(globals);
        g_ctx = NULL;
        g_log = NULL;
        return patm_error(PATM_ERR_PYTHON,
                          "failed to expose _patm to tool");
    }

    result = PyRun_File(f, tool_path, Py_file_input, globals, globals);
    fclose(f);

    if (!result) {
        /* dump the traceback into the log so it shows in the UI */
        PyObject *tb_mod = PyImport_ImportModule("traceback");
        if (tb_mod) {
            PyObject *fn = PyObject_GetAttrString(tb_mod, "format_exc");
            if (fn) {
                PyObject *tb_str = PyObject_CallObject(fn, NULL);
                if (tb_str && tb_str != Py_None) {
                    const char *tb_cstr = PyUnicode_AsUTF8(tb_str);
                    if (tb_cstr && g_log)
                        patm_strbuf_printf(g_log, "%s", tb_cstr);
                }
                Py_XDECREF(tb_str);
                Py_DECREF(fn);
            }
            Py_DECREF(tb_mod);
        }
        PyErr_Clear();
        ret = patm_error(PATM_ERR_PYTHON, "tool '%s' raised an exception",
                         tool_path);
    } else {
        PATM_LOG_INFO("tool '%s' finished", tool_path);
    }
    Py_XDECREF(result);
    Py_DECREF(globals);
    Py_DECREF(patm_mod);

    g_ctx = NULL;
    g_log = NULL;
    return ret;
}
