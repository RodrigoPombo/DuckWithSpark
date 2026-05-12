# Spark DuckDB Extension (C++)

DuckDB extension for executing Spark SQL via Microsoft Fabric Livy API.

## Overview

This extension allows DuckDB to offload heavy data engineering operations (DML/DDL) to Spark. It creates and manipulates Delta tables in OneLake via Spark.

**For reads**, users should use the built-in DuckDB Delta extension directly.

## Architecture

- **Writes (DML/DDL)**: This extension → Livy API → Spark → OneLake Delta tables
- **Reads (DQL)**: Users directly use DuckDB's Delta extension
- **Session Variables**: Configure once, use for all Spark operations

## Key Benefits

✅ **Offload heavy writes to Spark** - Distributed processing for DML/DDL  
✅ **Session variables** - Set once, use everywhere  
✅ **OneLake native** - Creates Delta tables in OneLake  
✅ **Simple API** - One function: `spark_execute()`  
✅ **C++ implementation** - Full access to DuckDB internals  

## Configuration

Set session variables once - used by both Spark execution AND Delta reading:

```sql
-- These variables are shared between spark_execute() and delta_scan()
SET spark_lakehouse_workspace_id = '74e51969-0e25-4d5c-9b97-29ccdf12fcf1';
SET spark_lakehouse_id = '1bf0fd14-f77f-4e50-aff2-e25ed0116357';
SET spark_auth_mode = 'azure_cli';  -- or 'service_principal', 'access_token'
```

For service principal auth:
```sql
SET spark_tenant_id = 'your-tenant-id';
SET spark_client_id = 'your-client-id';
SET spark_client_secret = 'your-client-secret';
```

## Function

### spark_execute(sql_text VARCHAR)
Execute DML/DDL operation via Spark and return execution status (not data).

**Use for:** CREATE TABLE, INSERT, UPDATE, DELETE, MERGE operations

```sql
-- DDL Example: Create table via Spark
SELECT * FROM spark_execute('
  CREATE OR REPLACE TABLE customer_summary AS
  SELECT customer_id, SUM(revenue) as total_revenue
  FROM raw_sales_data
  GROUP BY customer_id
');
-- Returns: | status | execution_time_sec | message             |
--          | ok     | 45.2               | Statement completed |

-- DML Example: Insert data via Spark
SELECT * FROM spark_execute('
  INSERT INTO customer_summary
  SELECT customer_id, SUM(revenue)
  FROM new_sales_data
  GROUP BY customer_id
');
```

### Reads: Use DuckDB Delta Extension

For reading OneLake tables, use DuckDB's built-in Delta extension.

Refer to Delta extension documentation for reading from OneLake paths.

## Typical Workflows

### Workflow 1: Processing Existing OneLake Data

If your data is already in OneLake (from other pipelines, Fabric notebooks, etc.):

```sql
-- Configure extension
SET spark_lakehouse_workspace_id = 'your-workspace-id';
SET spark_lakehouse_id = 'your-lakehouse-id';
SET spark_auth_mode = 'azure_cli';

-- Use Spark for heavy transformations on existing tables
SELECT * FROM spark_execute('
  CREATE OR REPLACE TABLE sales_aggregated AS
  SELECT region, product, SUM(revenue) as total_revenue
  FROM sales_transactions  -- existing OneLake table
  WHERE year = 2024
  GROUP BY region, product
');

-- Read results with DuckDB Delta extension
-- (refer to Delta extension docs for reading OneLake paths)
```

### Workflow 2: Load Local Data → OneLake → Spark Processing

If you need to load local data first, use DuckDB's Delta extension to write to OneLake:

```sql
-- Step 1: Load local data into DuckDB
CREATE TABLE local_data AS 
SELECT * FROM read_csv('data.csv');

-- Step 2: Write to OneLake using Delta extension
-- Format: abfss://<workspace-id>@onelake.dfs.fabric.microsoft.com/<lakehouse-id>/Tables/<table-name>
COPY local_data TO 'abfss://74e51969...@onelake.dfs.fabric.microsoft.com/1bf0fd14.../Tables/my_table' 
  (FORMAT DELTA);

-- Step 3: Process with Spark (for heavy operations)
SELECT * FROM spark_execute('
  CREATE OR REPLACE TABLE my_table_aggregated AS
  SELECT category, COUNT(*) as count, AVG(value) as avg_value
  FROM my_table
  GROUP BY category
');

-- Step 4: Read results back
-- (use Delta extension to read from OneLake)
```

### When to Use Spark vs Local DuckDB

**Use Spark (spark_execute) for:**
- ✅ Heavy aggregations on large OneLake tables (100M+ rows)
- ✅ Complex transformations requiring distributed processing
- ✅ CREATE TABLE / INSERT / UPDATE / DELETE / MERGE on OneLake tables
- ✅ Operations that take advantage of Spark's parallelism

**Use Local DuckDB for:**
- ✅ Lightweight operations on small datasets
- ✅ Reading and filtering OneLake tables (via Delta extension)
- ✅ Joins with local data
- ✅ Exploratory queries and analysis

## Complete Example Workflow

```sql
-- Step 1: Load Spark extension
LOAD 'path/to/spark_duckdb.duckdb_extension';

-- Step 2: Configure
SET spark_lakehouse_workspace_id = '74e51969-0e25-4d5c-9b97-29ccdf12fcf1';
SET spark_lakehouse_id = '1bf0fd14-f77f-4e50-aff2-e25ed0116357';
SET spark_auth_mode = 'azure_cli';

-- Step 3: Create table via Spark (DDL - returns status only)
SELECT * FROM spark_execute('
  CREATE OR REPLACE TABLE customer_summary AS
  SELECT 
    customer_id,
    COUNT(*) as order_count,
    SUM(total_amount) as lifetime_value
  FROM orders
  WHERE order_date >= ''2023-01-01''
  GROUP BY customer_id
');
-- Output: | status | execution_time_sec | message             |
--         | ok     | 45.2               | Statement completed |

-- Step 4: Insert more data via Spark (DML - returns status only)
SELECT * FROM spark_execute('
  INSERT INTO customer_summary
  SELECT customer_id, COUNT(*), SUM(total_amount)
  FROM new_orders
  GROUP BY customer_id
');

-- For reading: Use DuckDB Delta extension (separate from this extension)
```

## Building

### Prerequisites
- CMake 3.5+
- C++17 compiler
- OpenSSL
- DuckDB sources (as submodule or system install)

### Build Steps
```bash
# Clone with DuckDB submodule
git clone --recurse-submodules <repo-url>
cd spark_duckdb_cpp

# Build
make release

# The extension will be at:
# build/release/extension/spark_duckdb/spark_duckdb.duckdb_extension
```

## Loading

```sql
-- Load the extension
LOAD 'build/release/extension/spark_duckdb/spark_duckdb.duckdb_extension';

-- Or install permanently
INSTALL 'build/release/extension/spark_duckdb/spark_duckdb.duckdb_extension';
LOAD spark_duckdb;
```

## Dependencies

- **Azure authentication**: Azure CLI installed or service principal credentials
- **DuckDB Delta extension**: For reading OneLake tables (install with `INSTALL delta`)
- **Microsoft Fabric Livy API**: For Spark execution
- **OpenSSL**: For HTTPS requests
