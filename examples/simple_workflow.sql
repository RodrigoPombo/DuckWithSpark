-- Simple Workflow Example
-- This demonstrates the natural table access pattern

-- 1. Load extensions
INSTALL delta;
LOAD delta;
LOAD 'build/release/extension/spark_duckdb/spark_duckdb.duckdb_extension';

-- 2. Configure your lakehouse (once per session)
SET spark_lakehouse_workspace_id = '74e51969-0e25-4d5c-9b97-29ccdf12fcf1';
SET spark_lakehouse_id = '1bf0fd14-f77f-4e50-aff2-e25ed0116357';
SET spark_auth_mode = 'azure_cli';

-- 3. Create a table via Spark (heavy aggregation)
SELECT * FROM spark_execute('
  CREATE OR REPLACE TABLE sales_summary AS
  SELECT
    product_category,
    DATE_TRUNC(''month'', sale_date) as month,
    COUNT(*) as transaction_count,
    SUM(sale_amount) as total_sales,
    AVG(sale_amount) as avg_sale
  FROM raw_sales
  WHERE sale_date >= ''2023-01-01''
  GROUP BY product_category, DATE_TRUNC(''month'', sale_date)
');

-- 4. Query the table naturally - NO paths needed!
-- This automatically resolves to delta_scan() with the correct abfss:// path
SELECT * FROM sales_summary
WHERE total_sales > 100000
ORDER BY month DESC, total_sales DESC
LIMIT 20;

-- 5. Join with local DuckDB tables
CREATE TEMP TABLE target_categories AS
  SELECT 'Electronics' as category, 0.15 as discount
  UNION ALL
  SELECT 'Clothing', 0.20
  UNION ALL
  SELECT 'Home', 0.10;

SELECT
  s.product_category,
  s.month,
  s.total_sales,
  t.discount,
  s.total_sales * (1 - t.discount) as discounted_total
FROM sales_summary s
JOIN target_categories t ON s.product_category = t.category
ORDER BY s.month DESC, discounted_total DESC;

-- 6. Insert incremental data via Spark
SELECT * FROM spark_execute('
  INSERT INTO sales_summary
  SELECT
    product_category,
    DATE_TRUNC(''month'', sale_date) as month,
    COUNT(*) as transaction_count,
    SUM(sale_amount) as total_sales,
    AVG(sale_amount) as avg_sale
  FROM raw_sales
  WHERE sale_date >= ''2024-01-01''
  GROUP BY product_category, DATE_TRUNC(''month'', sale_date)
');

-- 7. Query updated data
SELECT
  product_category,
  COUNT(*) as month_count,
  SUM(total_sales) as category_total,
  AVG(avg_sale) as overall_avg_sale
FROM sales_summary
GROUP BY product_category
ORDER BY category_total DESC;

-- 8. Create dimension table
SELECT * FROM spark_execute('
  CREATE OR REPLACE TABLE dim_products AS
  SELECT DISTINCT
    product_id,
    product_name,
    product_category,
    unit_price
  FROM raw_sales
');

-- 9. Query dimension table
SELECT * FROM dim_products
WHERE product_category = 'Electronics'
ORDER BY unit_price DESC
LIMIT 10;

-- 10. Complex analytical query combining both
SELECT
  p.product_name,
  p.product_category,
  SUM(s.transaction_count) as times_sold,
  SUM(s.total_sales) as revenue
FROM sales_summary s
JOIN dim_products p ON s.product_category = p.product_category
GROUP BY p.product_name, p.product_category
HAVING SUM(s.transaction_count) > 100
ORDER BY revenue DESC
LIMIT 50;
