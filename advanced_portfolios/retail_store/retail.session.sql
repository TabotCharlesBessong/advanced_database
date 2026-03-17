
-- DROP TABLE IF EXISTS retail_sales;

-- CREATE TABLE IF NOT EXISTS retail_sales(
--   transactions_id INT PRIMARY KEY,
--   sale_date DATE,
--   sale_time TIME,
--   customer_id INT,
--   gender VARCHAR(15),
--   age INT,
--   category VARCHAR(15),
--   quantiy INT,
--   price_per_unit FLOAT,
--   cogs FLOAT,
--   total_sale FLOAT
-- );

-- SELECT * FROM retail_sales;

-- ALTER TABLE retail_sales RENAME COLUMN transaction_id TO transactions_id;

-- ALTER TABLE public.retail_sales ADD COLUMN IF NOT EXISTS quantiy INT;