/*
 * Copyright 2017-2019 Baidu Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "openrasp_hook.h"

extern "C"
{
#include "ext/pdo/php_pdo_driver.h"
#include "zend_ini.h"
#include "Zend/zend_objects_API.h"
}

HOOK_FUNCTION_EX(__construct, pdo, DB_CONNECTION);
PRE_HOOK_FUNCTION_EX(query, pdo, SQL);
PRE_HOOK_FUNCTION_EX(exec, pdo, SQL);
PRE_HOOK_FUNCTION_EX(prepare, pdo, SQL_PREPARED);

extern void parse_connection_string(char *connstring, sql_connection_entry *sql_connection_p);

static char *dsn_from_uri(char *uri, char *buf, size_t buflen)
{
    php_stream *stream;
    char *dsn = NULL;

    stream = php_stream_open_wrapper(uri, "rb", REPORT_ERRORS, NULL);
    if (stream)
    {
        dsn = php_stream_get_line(stream, buf, buflen, NULL);
        php_stream_close(stream);
    }
    return dsn;
}

static void init_pdo_connection_entry(INTERNAL_FUNCTION_PARAMETERS, sql_connection_entry *sql_connection_p)
{
    char *data_source;
    size_t data_source_len;
    char *colon;
    char *username = NULL, *password = NULL;
    size_t usernamelen, passwordlen;
    zval *options = NULL;
    char alt_dsn[512];

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "s|s!s!a!", &data_source, &data_source_len,
                                         &username, &usernamelen, &password, &passwordlen, &options))
    {
        return;
    }

    sql_connection_p->set_connection_string(data_source);
    /* parse the data source name */
    colon = strchr(data_source, ':');

    if (!colon)
    {
        /* let's see if this string has a matching dsn in the php.ini */
        char *ini_dsn = NULL;

        snprintf(alt_dsn, sizeof(alt_dsn), "pdo.dsn.%s", data_source);
        if (FAILURE == cfg_get_string(alt_dsn, &ini_dsn))
        {
            return;
        }

        data_source = ini_dsn;
        colon = strchr(data_source, ':');

        if (!colon)
        {
            return;
        }
    }

    if (!strncmp(data_source, "uri:", sizeof("uri:") - 1))
    {
        /* the specified URI holds connection details */
        data_source = dsn_from_uri(data_source + sizeof("uri:") - 1, alt_dsn, sizeof(alt_dsn));
        if (!data_source)
        {
            return;
        }
        colon = strchr(data_source, ':');
        if (!colon)
        {
            return;
        }
    }
    static const char *server_names[] = {"mysql", "mssql", "oci", "pgsql"};
    int server_size = sizeof(server_names) / sizeof(server_names[0]);
    for (int index = 0; index < server_size; ++index)
    {
        if (strncmp(server_names[index], data_source, strlen(server_names[index])) == 0)
        {
            sql_connection_p->set_server((const char *)server_names[index]);
        }
    }
    if (sql_connection_p->get_server() == "mysql")
    {
        struct pdo_data_src_parser mysql_vars[] = {
            {"charset", NULL, 0},
            {"dbname", "", 0},
            {"host", "localhost", 0},
            {"port", "3306", 0},
            {"unix_socket", NULL, 0},
        };
        int matches = php_pdo_parse_data_source(colon + 1, strlen(colon + 1), mysql_vars, 5);
        sql_connection_p->set_host(mysql_vars[2].optval);
        sql_connection_p->set_using_socket(nullptr == mysql_vars[2].optval || strcmp("localhost", mysql_vars[2].optval) == 0);
        sql_connection_p->set_port(atoi(mysql_vars[3].optval));
        sql_connection_p->set_socket(SAFE_STRING(mysql_vars[4].optval));
        sql_connection_p->set_username(username);
        for (int i = 0; i < 5; i++)
        {
            if (mysql_vars[i].freeme)
            {
                efree(mysql_vars[i].optval);
            }
        }
    }
    else if (sql_connection_p->get_server() == "pgsql")
    {
        char *e, *p, *conn_str = nullptr;
        char *dhn_data_source = estrdup(colon + 1);
        e = (char *)dhn_data_source + strlen(dhn_data_source);
        p = (char *)dhn_data_source;
        while ((p = (char *)memchr(p, ';', (e - p))))
        {
            *p = ' ';
        }
        if (username && password)
        {
            spprintf(&conn_str, 0, "%s user=%s password=%s", dhn_data_source, username, password);
        }
        else if (username)
        {
            spprintf(&conn_str, 0, "%s user=%s", dhn_data_source, username);
        }
        else if (password)
        {
            spprintf(&conn_str, 0, "%s password=%s", dhn_data_source, password);
        }
        else
        {
            spprintf(&conn_str, 0, "%s", (char *)dhn_data_source);
        }
        parse_connection_string(conn_str, sql_connection_p);
        efree(conn_str);
        efree(dhn_data_source);
    }
    else
    {
        //It is not supported at present
    }
}

void pre_pdo_query_SQL(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    pdo_dbh_t *dbh = Z_PDO_DBH_P(getThis());
    char *statement;
    size_t statement_len;

    if (!ZEND_NUM_ARGS() ||
        FAILURE == zend_parse_parameters(1, "s", &statement, &statement_len))
    {
        return;
        ;
    }

    plugin_sql_check(statement, statement_len, dbh->driver->driver_name);
}

void post_pdo_query_SQL_SLOW_QUERY(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    if (Z_TYPE_P(return_value) == IS_OBJECT)
    {
        pdo_stmt_t *stmt = Z_PDO_STMT_P(return_value);
        if (!stmt || !stmt->dbh)
        {
            return;
        }
        if (stmt->row_count >= OPENRASP_CONFIG(sql.slowquery.min_rows))
        {
            slow_query_alarm(stmt->row_count);
        }
    }
}

void pre_pdo_exec_SQL(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    pdo_dbh_t *dbh = Z_PDO_DBH_P(getThis());
    char *statement;
    size_t statement_len;

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "s", &statement, &statement_len))
    {
        return;
    }

    plugin_sql_check(statement, statement_len, dbh->driver->driver_name);
}

void post_pdo_exec_SQL_SLOW_QUERY(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    if (Z_TYPE_P(return_value) == IS_LONG)
    {
        if (Z_LVAL_P(return_value) >= OPENRASP_CONFIG(sql.slowquery.min_rows))
        {
            slow_query_alarm(Z_LVAL_P(return_value));
        }
    }
}

void pre_pdo___construct_DB_CONNECTION(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    if (OPENRASP_CONFIG(security.enforce_policy) &&
        check_database_connection_username(INTERNAL_FUNCTION_PARAM_PASSTHRU, init_pdo_connection_entry, 1))
    {
        handle_block();
    }
}

void post_pdo___construct_DB_CONNECTION(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    if (!OPENRASP_CONFIG(security.enforce_policy) && Z_TYPE_P(getThis()) == IS_OBJECT)
    {
        check_database_connection_username(INTERNAL_FUNCTION_PARAM_PASSTHRU, init_pdo_connection_entry, 0);
    }
}

void pre_pdo_prepare_SQL_PREPARED(OPENRASP_INTERNAL_FUNCTION_PARAMETERS)
{
    pdo_dbh_t *dbh = Z_PDO_DBH_P(getThis());
    char *statement;
    size_t statement_len;
    zval *options = NULL;

    if (FAILURE == zend_parse_parameters(ZEND_NUM_ARGS(), "s|a", &statement,
                                         &statement_len, &options))
    {
        return;
    }
    plugin_sql_check(statement, statement_len, dbh->driver->driver_name);
}