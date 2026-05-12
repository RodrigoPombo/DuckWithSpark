-- Working with Schema Folders (e.g., dbo/)
-- OneLake often stores tables in schema folders like: Tables/dbo/table_name

-- Setup
INSTALL delta;
LOAD delta;
LOAD 'build/release/extension/spark_duckdb/spark_duckdb.duckdb_extension';

SET spark_lakehouse_workspace_id = '74e51969-0e25-4d5c-9b97-29ccdf12fcf1';
SET spark_lakehouse_id = '1bf0fd14-f77f-4e50-aff2-e25ed0116357';
SET spark_auth_mode = 'azure_cli';

-- Create table in dbo schema via Spark
SELECT * FROM spark_execute('
  CREATE OR REPLACE TABLE dbo.dimension_date AS
  SELECT
    date_key,
    date_value,
    year,
    quarter,
    month,
    day_of_week
  FROM existing_date_table
');

-- Query using schema prefix - resolves to: Tables/dbo/dimension_date
SELECT * FROM dbo.dimension_date
WHERE year = 2024
LIMIT 10;

-- Alternative: Use slash notation (same result)
-- This is useful when schema names have special characters
SELECT * FROM "dbo/dimension_date"
WHERE year = 2024
LIMIT 10;

-- Join across schemas
SELECT
  d.date_value,
  d.day_of_week,
  s.total_sales
FROM dbo.dimension_date d
JOIN sales_summary s
  ON d.date_key = DATE_TRUNC('day', s.month)
WHERE d.year = 2024
ORDER BY d.date_value;
