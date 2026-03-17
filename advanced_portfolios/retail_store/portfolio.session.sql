-- SELECT * FROM retail_sales;

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

-- ALTER TABLE public.retail_sales ADD COLUMN IF NOT EXISTS quantiy INT;

-- \copy public.retail_sales
-- (transaction_id, sale_date, sale_time, customer_id, gender, age, category, quantity, price_per_unit, cogs, total_sale)
-- FROM 'retail_store.csv'
-- WITH (FORMAT csv, HEADER true, DELIMITER ',', ENCODING 'UTF8');

-- \copy public.retail_sales
-- (transaction_id, sale_date, sale_time, customer_id, gender, age, category, quantity, price_per_unit, cogs, total_sale)
-- FROM 'C:\\Users\\GROUP\\Downloads\\retail_store.csv'
-- WITH (FORMAT csv, HEADER true, DELIMITER ',', ENCODING 'UTF8');

-- icacls "C:\Users\GROUP\Downloads\retail_store.csv" /grant Everyone:F

-- SELECT COUNT(*) AS "Total transactions" FROM retail_sales;

-- identify null values
-- SELECT * FROM retail_sales WHERE transactions_id IS NULL;

-- SELECT * FROM retail_sales WHERE transactions_id IS NULL OR sale_time IS NULL OR customer_id IS NULL OR category IS NULL;

-- getting unique number of customers
-- SELECT COUNT(DISTINCT customer_id) AS "Unique customers" FROM retail_sales;

-- getting unique number of categories
-- SELECT COUNT(DISTINCT category) AS "Unique categories" FROM retail_sales;

-- getting unique number of transactions per category
-- SELECT category, COUNT(*) AS "Number of transactions" FROM retail_sales GROUP BY category;

-- getting total sales per category
-- SELECT category, SUM(total_sale) AS "Total sales" FROM retail_sales GROUP BY category;

-- NULL CHECK
-- SELECT * FROM retail_sales
-- WHERE 
--     sale_date IS NULL OR sale_time IS NULL OR customer_id IS NULL OR 
--     gender IS NULL OR age IS NULL OR category IS NULL OR 
--     quantiy IS NULL OR price_per_unit IS NULL OR cogs IS NULL;

-- delete all NULL values
-- DELETE FROM retail_sales
-- WHERE 
--     sale_date IS NULL OR sale_time IS NULL OR customer_id IS NULL OR
--     gender IS NULL OR age IS NULL OR category IS NULL OR
--     quantiy IS NULL OR price_per_unit IS NULL OR cogs IS NULL;

-- data analysis and findings

-- getting all sales made on '2022-11-05'
-- SELECT * FROM retail_sales WHERE sale_date = '2022-11-05';

-- getting all sales made in the month of November in the clothing category
-- SELECT * FROM retail_sales WHERE category = 'Clothing' AND EXTRACT(MONTH FROM sale_date) = 11;

-- SELECT * FROM retail_sales WHERE category = 'Clothing' AND TO_CHAR(sale_date, 'YYYY-MM') = '2022-11' AND quantiy >= 4;

-- getting total sales for each category
-- SELECT category, SUM(total_sale) AS "Total sales", COUNT(*) AS "Total Orders" FROM retail_sales GROUP BY category;

-- getting average age of users who purchased from the 'Bueaty' category

-- SELECT ROUND(AVG(age), 2) as "Average age of customers in Beauty category" FROM retail_sales WHERE category = 'Beauty';

-- getting total number of transactions by each gender in each category
-- SELECT category, gender, COUNT(*) AS "Total transactions" FROM retail_sales GROUP BY category, gender ORDER BY category ASC;

-- getting average sales in each month and finding the best sales month
-- SELECT year,month,avg_sale FROM (SELECT EXTRACT(YEAR FROM sale_date) as year, EXTRACT(MONTH FROM sale_date) as month,AVG(total_sale) as avg_sale,RANK() OVER(PARTITION BY EXTRACT(YEAR FROM sale_date) ORDER BY AVG(total_sale) DESC) AS RANK FROM retail_sales GROUP BY 1,2) AS "T1" WHERE RANK = 1;

-- getting number of unique customers who purchased items from each category
-- SELECT category, COUNT(DISTINCT customer_id) as cnt_unique_cs FROM retail_sales GROUP BY category;

-- getting informations based on shifts

-- WITH hourly_sale AS (
--   SELECT *,
--   CASE WHEN EXTRACT(HOUR FROM sale_time) < 12 THEN 'Morning' WHEN EXTRACT(HOUR FROM sale_time) BETWEEN 12 AND 17 THEN 'Afternoon' ELSE 'Evening' END AS shift FROM retail_sales
-- )
-- SELECT shift, COUNT(*) AS total_orders FROM hourly_sale GROUP BY shift;